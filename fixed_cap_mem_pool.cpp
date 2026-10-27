#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T>
class ObjectPool final {
private:
    static constexpr std::size_t npos =
        std::numeric_limits<std::size_t>::max();

    struct Slot {
        alignas(T) std::byte storage[sizeof(T)];

        std::size_t next_free = npos;
        bool occupied = false;
    };

    std::vector<Slot> slots_;
    std::size_t free_head_ = npos;
    std::size_t available_ = 0;

    T* object_at(std::size_t index) noexcept {
        return std::launder(
            reinterpret_cast<T*>(slots_[index].storage)
        );
    }

    bool locate(const T* object, std::size_t& index) const noexcept {
        if (object == nullptr || slots_.empty()) {
            return false;
        }

        const auto base =
            reinterpret_cast<std::uintptr_t>(slots_.data());

        const auto address =
            reinterpret_cast<std::uintptr_t>(object);

        const std::size_t stride = sizeof(Slot);

        if (address < base) {
            return false;
        }

        const std::uintptr_t offset = address - base;

        if (offset % stride != offsetof(Slot, storage)) {
            return false;
        }

        index = offset / stride;

        return index < slots_.size();
    }

public:
    explicit ObjectPool(std::size_t capacity)
        : slots_(capacity),
          free_head_(capacity == 0 ? npos : 0),
          available_(capacity) {

        static_assert(
            std::is_nothrow_destructible_v<T>,
            "T must have a non-throwing destructor."
        );

        for (std::size_t i = 0; i < capacity; ++i) {
            slots_[i].next_free =
                (i + 1 < capacity) ? i + 1 : npos;
        }
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    ~ObjectPool() noexcept {
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            if (slots_[i].occupied) {
                std::destroy_at(object_at(i));
            }
        }
    }

    template <typename... Args>
    T* create(Args&&... args) {
        if (free_head_ == npos) {
            return nullptr;
        }

        const std::size_t index = free_head_;
        Slot& slot = slots_[index];

        free_head_ = slot.next_free;
        --available_;

        try {
            T* object = std::construct_at(
                reinterpret_cast<T*>(slot.storage),
                std::forward<Args>(args)...
            );

            slot.occupied = true;
            slot.next_free = npos;

            return object;
        } catch (...) {
            slot.next_free = free_head_;
            free_head_ = index;
            ++available_;
            throw;
        }
    }

    bool destroy(T* object) noexcept {
        std::size_t index = 0;

        if (!locate(object, index)) {
            return false;
        }

        Slot& slot = slots_[index];

        if (!slot.occupied) {
            return false;
        }

        std::destroy_at(object_at(index));

        slot.occupied = false;
        slot.next_free = free_head_;
        free_head_ = index;
        ++available_;

        return true;
    }

    std::size_t capacity() const noexcept {
        return slots_.size();
    }

    std::size_t available() const noexcept {
        return available_;
    }
};