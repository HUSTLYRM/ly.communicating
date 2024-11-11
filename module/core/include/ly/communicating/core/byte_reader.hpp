#pragma once

#include <memory>
#include <concepts>
#include <optional>

#include "basic_bytes.hpp"

namespace ly::communicating {
    class byte_reader {
    public:
        virtual ~byte_reader() = default;
        [[nodiscard]] virtual size_type read(byte_span buffer) = 0;
    };

    template<typename T>
    concept is_reader = requires(T& t, byte_span& buffer) {
        { t.read(buffer) } -> std::same_as<size_type>;
    };

    class byte_reader_provider {
    public:
        using reader_type = byte_reader;

        virtual ~byte_reader_provider() = default;
        [[nodiscard]] virtual std::optional<std::shared_ptr<reader_type>> get_reader() = 0;
    };

    template<typename T, typename reader_type = typename T::reader_type>
    concept is_reader_provider = is_reader<reader_type> && requires(T& t) {
        { t.get_reader() } -> std::same_as<std::optional<std::shared_ptr<reader_type>>>;
    };
}

