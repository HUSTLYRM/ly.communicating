#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>

namespace ly::communicating {
    static_assert(sizeof(std::uint8_t) == sizeof(char), "sizeof(std::uint8_t) != sizeof(char)");

    /// @brief 字节的概念，认为内存的最小单位是 8 二进制位字节
    using byte_type = std::uint8_t;

    using size_type = std::size_t;
    using byte_span = std::span<byte_type>;
    using const_byte_span = std::span<const byte_type>;

    template<size_type size>
    using byte_array = std::array<byte_type, size>;

    struct const_byte_span_formatter : public std::formatter<byte_type> {
        auto format(const const_byte_span& span, std::format_context& ctx) const {
            if (span.empty()) return ctx.out();
            const auto count = span.size();
            if (count == 1) return std::formatter<byte_type>::format(span.front(), ctx);

            for (const auto& element : span.subspan(0, count - 1)) {
                const auto it = std::formatter<byte_type>::format(element, ctx);
                std::format_to(it, " ");
            }
            return std::formatter<byte_type>::format(span.back(), ctx);
        }
    };
}

template<>
struct std::formatter<ly::communicating::const_byte_span> : ly::communicating::const_byte_span {};
