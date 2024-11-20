#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>

namespace cango::utility {
    template<typename item_type>
    using triple_item_array = std::array<item_type, 3>;

    namespace details {
        struct triple_item_pool_utils {
            enum usage_flag : std::uint8_t {
                Empty = 0,
                Full  = 1,
                Busy  = 2
            };

            std::atomic_uint8_t writer_index{0};
            triple_item_array<std::atomic_uint8_t> flags{};
        };
    }

    template<std::size_t item_size>
    class nonblock_triple_byte_pool final : details::triple_item_pool_utils {
        static_assert(sizeof(char) == sizeof(std::uint8_t), "byte size assertion failed");

        std::array<std::array<std::uint8_t, item_size>, 3> items{};

    public:
        template<typename item_type>
            requires std::is_trivially_copyable_v<item_type> && (sizeof(item_type) == item_size)
        void push(const item_type &item) noexcept {
            std::uint8_t index{writer_index};
            std::uint8_t busy;
            do {
                busy = Busy;
                index = (index + 1) % 3;
            }
            while (flags[index].compare_exchange_weak(busy, Busy));
            std::memcpy(items[index].data(), &item, item_size);
            flags[index] = Full;
            writer_index = index;
        }

        template<typename item_type>
            requires std::is_trivially_copyable_v<item_type> && (sizeof(item_type) == item_size)
        [[nodiscard]] bool pop(item_type &item) noexcept {
            for (std::uint8_t index = 0; index < 3; ++index) {
                if (std::uint8_t full = Full;
                    !flags[index].compare_exchange_weak(full, Busy))
                    continue;
                std::memcpy(&item, items[index].data(), item_size);
                flags[index] = Empty;
                return true;
            }
            return false;
        }
    };

    class nonblock_triple_byte_pool_dynamic final : details::triple_item_pool_utils {
        std::array<std::vector<std::uint8_t>, 3> items{};

    public:
        [[nodiscard]] std::size_t get_item_size() const noexcept { return items[0].size(); }
        void set_item_size() noexcept { std::ranges::for_each(items, [](auto &item) { item.resize(0); }); }

        template<typename item_type>
            requires std::is_trivially_copyable_v<item_type>
        void push(const item_type &item) noexcept {
            if (sizeof(item_type) != get_item_size()) return;

            std::uint8_t index{writer_index};
            std::uint8_t busy;
            do {
                busy = Busy;
                index = (index + 1) % 3;
            }
            while (flags[index].compare_exchange_weak(busy, Busy));
            std::memcpy(items[index], &item, get_item_size());
            flags[index] = Full;
            writer_index = index;
        }

        template<typename item_type>
            requires std::is_trivially_copyable_v<item_type>
        [[nodiscard]] bool pop(item_type &item) noexcept {
            if (sizeof(item_type) != get_item_size()) return false;

            for (std::uint8_t index = 0; index < 3; ++index) {
                if (std::uint8_t full = Full;
                    !flags[index].compare_exchange_weak(full, Busy))
                    continue;
                std::memcpy(&item, items[index], get_item_size());
                flags[index] = Empty;
                return true;
            }
            return false;
        }
    };

    template<typename item_type>
        requires std::default_initializable<item_type> && std::is_copy_assignable_v<item_type>
    class nonblock_triple_item_pool final : details::triple_item_pool_utils {
        triple_item_array<item_type> items{};

    public:
        void push(const item_type &item) noexcept {
            std::uint8_t index{writer_index};
            std::uint8_t busy;
            do {
                busy = Busy;
                index = (index + 1) % 3;
            }
            while (flags[index].compare_exchange_weak(busy, Busy));
            items[index] = item;
            flags[index] = Full;
            writer_index = index;
        }

        [[nodiscard]] bool pop(item_type &item) noexcept {
            for (std::uint8_t index = 0; index < 3; ++index) {
                if (std::uint8_t full = Full;
                    !flags[index].compare_exchange_weak(full, Busy))
                    continue;
                item = items[index];
                flags[index] = Empty;
                return true;
            }
            return false;
        }
    };
}
