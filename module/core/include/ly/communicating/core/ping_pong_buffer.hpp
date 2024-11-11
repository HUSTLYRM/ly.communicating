#pragma once

#include <stdexcept>

#include "basic_bytes.hpp"

namespace ly::communicating {
    using byte_verifier = bool(*)(const_byte_span);

    [[nodiscard]] inline bool find_head_byte(const_byte_span &span, byte_type head) noexcept {
        const auto head_index = std::ranges::find(span, head);
        if (head_index == span.end()) return false;
        span = span.subspan(head_index - span.begin());
        return true;
    }

    /// @brief 基于 PingPong 思想的运行时双数据包大小的内存缓冲器。
    ///		输入一个数据包的字节数，经过检验后输出数据包。
    ///		当一个数据包大小的数据到来时，将把所有字节加入缓冲区，然后在缓冲区中检查是否有完整的数据包，如果有则输出数据包。
    ///		如果没有则储存本次数据包等待下次数据包到来。
    ///	@tparam verify 用于检验数据包是否符合要求，如果不符合要求，此类的 Examine 函数将返回 false
    ///	@note
    ///		初始化时需要提供`数据包的头字节`和 有效的、长度为两倍数据包大小的`连续内存`
    /// @todo
    ///		成员管理有很大问题，等待调整
    template<byte_verifier verify>
    class ping_pong_span final {
        /// @brief 优化函数 @c FillPingWith0 ，避免重复清空 PingSpan
        bool IsLastMessageFoundInPongSpan{false};

        byte_span ping;
        byte_span pong;
        byte_span full;

    public:
        byte_type head{'!'};

        /// @brief 给定头字节和完整的内存区间，构造 @c PingPongExchanger
        ///	@param fullSpan size 至少大于 2，否则会抛出 @c std::runtime_error
        /// @exception std::runtime_error 当给定的参数不符合要求时抛出异常
        explicit ping_pong_span(const byte_span fullSpan) :
            ping(fullSpan.data(), fullSpan.size() / 2),
            pong(fullSpan.data() + ping.size(), ping.size()),
            full(fullSpan) {
            if (ping.empty()) throw std::invalid_argument("sizeof fullSpan must be at least 2");
            std::ranges::fill(ping, 0);
        }

        byte_span get_reader_span() const noexcept {
            return pong;
        }

        /// @brief 假设已经写入所有数据到读取缓冲区(即 Pong 缓冲区)，现在执行交换流程，并检查是否可以输出
        ///	@param destination 用于输出数据的位置，大小必须大于或等于 @c PongSpan.size()
        [[nodiscard]] bool examine(byte_span destination) noexcept {
            if (destination.size() < ping.size()) return false;

            // 如果 PongSpan 中有完整的数据包，则直接输出
            if (pong.front() == head && verify(pong)) {
                std::ranges::copy(pong, destination.begin());
                if (!IsLastMessageFoundInPongSpan) {
                    IsLastMessageFoundInPongSpan = true;
                    std::ranges::fill(ping, 0);
                }
                return true;
            }

            IsLastMessageFoundInPongSpan = false;
            const_byte_span message_span;
            const auto successful = find_head_byte(message_span, head) && verify(message_span);
            if (successful) std::ranges::copy(message_span, destination.begin());
            std::ranges::copy(pong, ping.begin()); // 无论成功与否，都利用新数据覆盖 PingSpan
            return successful;
        }
    };

    template<typename TMessage, byte_verifier verify>
    struct reader_toolkit final {
        using msg_type = TMessage;

        byte_array<sizeof(TMessage) * 2> buffer{};
        ping_pong_span<verify> ping_pong{buffer};
        byte_span reader_span{ping_pong.get_reader_span()};

        byte_array<sizeof(TMessage)> result_buffer{};

        TMessage &result() noexcept { return *reinterpret_cast<msg_type *>(result_buffer.data()); }

        const TMessage &result() const noexcept { return *reinterpret_cast<const msg_type *>(result_buffer.data()); }

        byte_span result_span() noexcept { return result_buffer; }

        const_byte_span result_span() const noexcept { return result_buffer; }
    };
}
