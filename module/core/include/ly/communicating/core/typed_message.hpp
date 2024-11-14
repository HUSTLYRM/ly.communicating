#pragma once

#include <cstring>

#include "basic_bytes.hpp"

namespace ly::communicating {
	template<size_type data_size>
	struct typed_message {
		static constexpr auto DataSize = data_size;
		static constexpr auto FullSize = DataSize + 3;

		byte_type head;
		byte_type type;
		byte_array<DataSize> data;
		byte_type tail;

		byte_span as_span() noexcept {
			return { reinterpret_cast<byte_type*>(this), sizeof(*this) };
		}

		const_byte_span as_span() const noexcept {
			return { reinterpret_cast<const byte_type*>(this), sizeof(*this) };
		}

		explicit operator byte_span() noexcept {
			return as_span();
		}

		explicit operator const_byte_span() const noexcept {
			return as_span();
		}

		template<typename T>
		T& data_as() noexcept {
			static_assert(sizeof(T) == DataSize, "inconsistent size");
			return *reinterpret_cast<T*>(&data);
		}

		template<typename T>
		const T& data_as() const noexcept {
			static_assert(sizeof(T) == DataSize, "inconsistent size");
			return *reinterpret_cast<const T*>(&data);
		}

		template<typename T>
		void data_from(const T& t) noexcept {
			static_assert(sizeof(T) == DataSize, "inconsistent size");
			std::memcpy(&data, &t, sizeof(T));
		}

		template<typename T>
		void data_to(T& t) const noexcept {
			static_assert(sizeof(T) == DataSize, "inconsistent size");
			std::memcpy(&t, &data, sizeof(T));
		}
	};

	template<typename T>
	using typed_message_wrap = typed_message<sizeof(T)>;
}
