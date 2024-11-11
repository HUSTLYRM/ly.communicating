#pragma once

#include <memory>
#include <concepts>
#include <optional>

#include "basic_bytes.hpp"

namespace ly::communicating {
    class byte_writer {
    public:
        virtual ~byte_writer() = default;
        [[nodiscard]] virtual size_type write(const_byte_span buffer) = 0;
    };

    template<typename T>
    concept is_writer = requires(T& t, const_byte_span& buffer) {
        { t.write(buffer) } -> std::same_as<size_type>;
    };

    class byte_writer_provider {
    public:
        using writer_type = byte_writer;

        virtual ~byte_writer_provider() = default;
        [[nodiscard]] virtual std::optional<std::shared_ptr<writer_type>> get_writer() = 0;
    };

    template<typename T, typename writer_type = typename T::writer_type>
    concept is_writer_provider = is_writer<typename T::writer_type> && requires(T& t) {
        { t.get_writer() } -> std::same_as<std::optional<std::shared_ptr<writer_type>>>;
    };
}