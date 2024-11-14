#include <concepts>
#include <iostream>

#include <ly/communicating/core/basic_tasks.hpp>

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

int main() {
	std::array<int, 3> a = { 1, 2, 3 };
	const auto span = std::span<const std::uint8_t>{ reinterpret_cast<const std::uint8_t*>(&a[0]), sizeof(a) };
	std::cout << ly::communicating::byte_span_format("{:0>2X} ", span);

	return 0;
}