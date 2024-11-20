#include <concepts>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>

#include <ly/communicating/core/basic_tasks.hpp>
#include <ly/communicating/core/sao_item.hpp>
#include <ly/communicating/core/triple_pool.hpp>

namespace ly::communicating {
	/// @brief 标准读取流程，需要额外的 item 与 buffer，一个用来存储当前获取的物品，一个用来存储当前获得的字节
	/// @details
	///		缺点，（不必要的？）额外空间
	///		优点，函数调用少，每个对象都像一个简单的映射函数一样，只专注做一件事
	int standard_reader_run(auto& reader, auto& packer, auto& sink, auto& item, auto& buffer) {
		if (!reader.read(buffer)) return -1;			// 获取字节，填满缓冲区 o -> bytes
		if (!packer.pack(buffer, item)) return -2;		// 包装缓冲区字节成包裹 bytes -> item
		if (!sink.set(item)) return -3;					// 将包裹投递到指定位置	item -> o
		return 0;
	}

	/// @brief 极简读取流程，由于一般包装器的实现都有内部缓冲区，用于选取合适字节区段，所以直接将包装器内部的缓冲区用于读取和投递
	/// @details
	///		缺点
	///			- 对于 packer 提高要求，使功能耦合于此任务
	///			- packer 的 as_item 函数并不安全，且由于运行时解析对象，所以不可优化，多了一次函数调用
	/// 
	///		优点
	///			- packer 提供的 as_buffer 是可优化的
	///			- 由于 read, pack 连续两次操作面向的都是 packer 内部的连续内存，所以缓存命中率也会有所改善
	///			- 由于少了 item 与 buffer，相比标准流程减少了数据由 buffer -> pack, packed_bytes -> item 的两次内存拷贝
	///			- 减少的内存拷贝时间会远大于多的一次函数调用
	/// 
	///		也就是说，无论是时间还是空间均优于标准流程，但是我不喜欢 :(
	int optimized_reader_run(auto& reader, auto& packer, auto& sink) {
		if (!reader.read(packer.as_buffer())) return -1;// 包装器提供内部缓冲区，读取器填满缓冲区
		if (!packer.pack()) return -2;					// 包装器解析内部缓冲区，生成包裹
		if (!sink.put(packer.as_item())) return -3;		// 包装器提供包裹，投递到指定位置
		return 0;
	}

	int standard_writer_run(auto& writer, auto& unpacker, auto& source, auto& item, auto& buffer) {
		if (!source.get(item)) return -1;
		if (!unpacker.unpack(item, buffer)) return -2;
		if (!writer.write(buffer)) return -3;
		return 0;
	}

	int optimized_writer_run(auto& writer, auto& unpacker, auto& source) {
		if (!source.get(unpacker.as_item())) return -1;
		if (!unpacker.unpack()) return -2;
		if (!writer.write(unpacker.as_buffer())) return -3;
		return 0;
	}
}

namespace {
	using namespace std::chrono_literals;

	template<typename TItem>
	using tri_item = cango::utility::nonblock_triple_item_pool<TItem>;

	template<typename TItem>
	class sa_item final {
		std::shared_ptr<std::atomic<TItem>> Item{ std::make_shared<std::atomic<TItem>>() };

	public:
		using item_type = TItem;
		void push(const item_type& item) noexcept {
			*Item = item;
		}

		[[nodiscard]] bool pop(item_type& item) noexcept {
			item = *Item;
			return true;
		}
	};

	template<std::size_t extra_size = 10>
	struct fake_data {
		std::uint32_t index;
		std::uint8_t extra[extra_size];
	};

	using clock_type = std::chrono::high_resolution_clock;
	using time_type = clock_type::time_point;

	template<typename data_type>
	struct pushing_info {
		time_type generated_time;
		time_type pushed_time;
		data_type data{};
	};

	template<typename data_type>
	struct pulling_info {
		time_type before_acquired_time;
		time_type acquired_time;
		data_type data{};
		bool is_lost{ true };
	};

	std::chrono::microseconds to_us(const auto& duration) {
		return std::chrono::duration_cast<std::chrono::microseconds>(duration);
	}

	struct summary_info {
		std::chrono::microseconds push_cost;
		std::chrono::microseconds pull_cost;
		std::chrono::microseconds push_pull_delay;
		bool is_lost{ true };

		[[nodiscard]] std::string format(std::size_t i) const noexcept {
			return is_lost ? std::format("{}, nan, nan", pull_cost.count()) :
				std::format("{}, {}, {}", push_cost.count(), pull_cost.count(), push_pull_delay.count());
		}

		[[nodiscard]] std::string format_average(std::size_t count) const noexcept {
			return std::format("average: w({}) r({}) delay({})",
				push_cost.count() / count, pull_cost.count() / count, push_pull_delay.count() / count);
		}
	};

	template<typename data_type, std::size_t count>
	struct io_tasks {
		using pushing_info = pushing_info<data_type>;
		using pulling_info = pulling_info<data_type>;
		using pushing_table = std::array<pushing_info, count>;
		using pulling_table = std::array<pulling_info, count>;

		/// @brief 生成，写入数据。每次写入后调用 sleeper
		static void write_task(auto& sink, pushing_table& table, auto& sleep) {
			for (std::size_t i = 0; i < count; i++) {
				pushing_info info{};
				info.data.index = i;
				info.generated_time = clock_type::now();
				sink.push(info.data);
				info.pushed_time = clock_type::now();
				table[i] = info;
				sleep();
			}

			// 通知读取任务结束
			pushing_info info{};
			info.data.index = count;
			for (auto i = 0; i < 10; i++) {
				std::this_thread::sleep_for(100ms);
				sink.push(info.data);
			}
		}

		/// @brief 读取，保存数据。在没有读取到数据时调用 idle_sleep，在读取到数据后调用 work_sleep
		static void read_task(auto& source, pulling_table& table, auto& idle_sleep, auto& work_sleep) {
			auto last_index = count;
			while (true) {
				pulling_info info{};
				info.before_acquired_time = clock_type::now();
				if (!source.pop(info.data) || info.data.index == last_index) {
					idle_sleep();
					continue;
				}
				info.acquired_time = clock_type::now();
				for (; info.data.index != 0 && last_index < info.data.index - 1; last_index++)
					std::cout << "lost ";
				std::cout << info.data.index << ' ';
				last_index = info.data.index;
				if (info.data.index >= count) break;
				info.is_lost = false;
				table[info.data.index] = info;
				work_sleep();
			}
			std::cout << std::endl;
		}

		static void summary(std::ostream& stream, const time_type begin, const pushing_table& tWrite, const pulling_table& tRead) {
			const auto invalid_duration = begin - begin;

			std::vector<summary_info> sum;
			sum.reserve(count);

			for (std::size_t i = 0; i < count; i++) {
				auto& pushing = tWrite[i];
				auto& pulling = tRead[i];

				const auto is_lost = pulling.is_lost;
				const auto push_cost = pushing.pushed_time - pushing.generated_time;
				const auto pull_cost = is_lost ? invalid_duration : pulling.acquired_time - pulling.before_acquired_time;
				const auto push_pull_delay = is_lost ? invalid_duration : pulling.acquired_time - pushing.pushed_time;

				summary_info info{
					to_us(push_cost),
					to_us(pull_cost),
					to_us(push_pull_delay),
					is_lost
				};
				if (!is_lost) sum.emplace_back(info);
				stream << info.format(i) << std::endl;
			}

			std::chrono::microseconds total_push_cost{};
			std::chrono::microseconds total_pull_cost{};
			std::chrono::microseconds total_push_pull_delay{};
			for (const auto& info : sum) {
				total_push_cost += info.push_cost;
				total_pull_cost += info.pull_cost;
				total_push_pull_delay += info.push_pull_delay;
			}

			summary_info average{
				total_push_cost,
				total_pull_cost,
				total_push_pull_delay
			};
			const auto loss_count = count - sum.size();
			stream << average.format_average(count) << std::endl;
			stream << std::format("loss: {}/{}", loss_count, count) << std::endl;
		}

		struct executor {
			pushing_table t_write;
			pulling_table t_read;
			time_type begin;

			void execute(auto& item, auto& sleep, auto& idle_sleep, auto& work_sleep) {
				std::thread write_thread{ [&item, this, &sleep] {
					write_task(item, t_write, sleep);
				} };
				std::thread read_thread{ [&item, this, &idle_sleep, &work_sleep] {
					read_task(item, t_read, idle_sleep, work_sleep);
				} };
				begin = clock_type::now();
				write_thread.join();
				read_thread.join();
			}

			void summary(std::ostream& stream) const {
				io_tasks::summary(stream, begin, t_write, t_read);
			}
		};
	};

	template<std::size_t ms>
	void sleep() {
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	template<template<typename> typename target_type, std::size_t extra_data = 10, std::size_t count = 1000, std::size_t w_ms = 10, std::size_t ri_ms = 1, std::size_t rw_ms = 5>
	void fake_data_benchmark(std::string_view filepath) {
		using data_type = fake_data<extra_data>;
		using task_type = io_tasks<data_type, count>;
		typename task_type::executor executor;
		target_type<data_type> target;
		std::ofstream stream{ filepath.data() };
		executor.execute(target, sleep<w_ms>, sleep<ri_ms>, sleep<rw_ms>);
		executor.summary(stream);
	};
}

int main() {

	// group 1
	// 10 字节额外数据，1000 次写入，写入间隔 10ms，读取空闲间隔 1ms，读取工作间隔 5ms
	fake_data_benchmark<sa_item>("sa1_10_1000_10_1_5.txt");
	fake_data_benchmark<tri_item>("tri1_10_1000_10_1_5.txt");


	// group 2
	// 10 字节额外数据，1000 次写入，写入间隔 2ms，读取空闲间隔 1ms，读取工作间隔 10ms
	fake_data_benchmark<sa_item, 10, 1000, 2, 1, 10>("sa1_10_1000_2_1_10.txt");
	fake_data_benchmark<tri_item, 10, 1000, 2, 1, 10>("tri1_10_1000_2_1_10.txt");


	// group 3
	// 10 字节额外数据，1000 次写入，写入间隔 2ms，读取空闲间隔 0ms，读取工作间隔 10ms
	fake_data_benchmark<sa_item, 10, 1000, 2, 0, 10>("sa1_10_1000_2_0_10.txt");
	fake_data_benchmark<tri_item, 10, 1000, 2, 0, 10>("tri1_10_1000_2_0_10.txt");


	return 0;
}