#pragma once

#include <atomic>
#include <memory>
#include <optional>

namespace ly::communicating {
    template<typename TItem>
    class shared_atomic_optional_item {
    public:
        using item_type = TItem;
        using item_ptr_type = std::shared_ptr<std::atomic<item_type>>; // removed optional

    private:
        item_ptr_type item;

    public:
		template<typename ...TArgs>
        static shared_atomic_optional_item<item_type> make(TArgs&&... args) {
            return { std::make_shared<std::atomic<item_type>>(std::forward<TArgs>(args)...) };
        }

        shared_atomic_optional_item() : item() {}

        shared_atomic_optional_item(item_ptr_type item) : item(std::move(item)) {}

        bool is_valid() const noexcept {
            return item != nullptr;
        }

        bool get(item_type& result) noexcept {
            if (!is_valid()) return false;
            result = (*item).load();
            return true;
        }

        bool set(const item_type& result) noexcept {
            if (!is_valid()) return false;
            (*item).store(result);
            return true;
        }
    };
}