#pragma once

#include "byte_reader.hpp"
#include "byte_writer.hpp"

namespace ly::communicating {
    class byte_rwer : public byte_reader, public byte_writer {};

    template<typename T>
    concept is_rwer = is_reader<T> && is_writer<T>;

    class byte_rwer_provider {
    public:
        using rwer_type = byte_rwer;

        virtual ~byte_rwer_provider() = default;
        [[nodiscard]] virtual std::optional<std::shared_ptr<rwer_type>> get_rwer() = 0;
    };

    template<typename T, typename rwer_type = typename T::rwer_type>
    concept is_rwer_provider = is_rwer<rwer_type> && requires(T& t) {
        { t.get_rwer() } -> std::same_as<std::optional<std::shared_ptr<rwer_type>>>;
    };
}
