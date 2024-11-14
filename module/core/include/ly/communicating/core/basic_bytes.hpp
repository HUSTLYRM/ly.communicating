#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <sstream>

namespace ly::communicating {
	using byte_type = std::uint8_t;
	using size_type = std::size_t;
	using byte_span = std::span<byte_type>;
	using const_byte_span = std::span<const byte_type>;

	template<size_type size>
	using byte_array = std::array<byte_type, size>;

	inline void byte_span_format(std::ostream& stream, std::string_view format, const_byte_span span) {
		for (const auto byte : span)
			stream << std::vformat(format, std::make_format_args(byte));
	}

	inline std::string byte_span_format(std::string_view format, const_byte_span span) {
		std::stringstream stream;
		byte_span_format(stream, format, span);
		return stream.str();
	}
}
