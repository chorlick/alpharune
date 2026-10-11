#pragma once
/// @file keyed_slots.h
/// GameObject::card_counters and GameObject::string_state: a card's own
/// memory on an object, read and written by name (plans/09 item 9.KK1).
///
/// They were `std::unordered_map<std::string, V>`: every lookup built and
/// hashed a std::string (a heap allocation for a name over 15 characters),
/// every object carried 56 bytes of table whether it had a counter or not, and
/// a GameState copy allocated a bucket array, a node and usually a key string
/// per counter. Now:
///
///  - a name is interned once per process into a small integer (`SlotKey`);
///    a per-thread cache keyed by the name's address answers the usual lookup,
///    a string literal, without hashing it;
///  - an object with no counter holds one null pointer, and its copy is that
///    pointer; counters live four to a block.
///
/// The spelling card code uses is the map's and did not change:
/// `obj.card_counters["x"]`, `.find("x")` / `.end()` / `it->second`,
/// `.count("x")`, `.erase("x")`, `.at("x")`, `.empty()`. As with the map,
/// `operator[]` adds a zero entry, and **a reference to an entry stays valid
/// while that entry exists** (cards keep `int& last = counters["x"]` across
/// another insertion): entries never move.
///
/// Not provided, because nothing used it: iteration in the map's order.
/// `forEach` visits the entries in an unspecified order.

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace riftbound {

/// An interned counter name. 0 is "no name" (a free entry).
using SlotKey = uint32_t;

namespace detail {
/// The key of `name` and the pooled copy of the name. Takes the pool's lock.
struct SlotKeyEntry {
    SlotKey key = 0;
    const std::string* name = nullptr;
};
SlotKeyEntry internSlotKey(std::string_view name);
} // namespace detail

/// The name a key was made from (for a log line or a debugger). Throws
/// std::out_of_range for a key no name was interned as.
const std::string& slotKeyName(SlotKey key);

/// The key of a counter name. The same name gives the same key in every
/// thread, for the life of the process.
inline SlotKey slotKey(std::string_view name) {
    // Keyed by where the characters are. The usual caller passes a string
    // literal, whose address is its identity; the stored name is compared on
    // a hit, so a different name built at a remembered address (a temporary
    // std::string) misses instead of being mistaken for the old one.
    struct Slot {
        const char* data = nullptr;
        std::size_t size = 0;
        detail::SlotKeyEntry entry;
    };
    constexpr std::size_t kSlots = 256;
    thread_local Slot cache[kSlots];
    const auto address = reinterpret_cast<std::uintptr_t>(name.data());
    Slot& slot = cache[(address ^ (address >> 8)) & (kSlots - 1)];
    if (slot.data == name.data() && slot.size == name.size() && slot.entry.name != nullptr &&
        std::memcmp(slot.entry.name->data(), name.data(), name.size()) == 0) {
        return slot.entry.key;
    }
    slot = Slot{name.data(), name.size(), detail::internSlotKey(name)};
    return slot.entry.key;
}

template <class V>
class KeyedSlots {
public:
    /// One entry: `it->second` is the value, as in the map this replaced.
    struct Entry {
        SlotKey first = 0;
        V second{};
    };
    using iterator = Entry*;
    using const_iterator = const Entry*;
    using mapped_type = V;

    KeyedSlots() noexcept = default;
    KeyedSlots(const KeyedSlots& other) { copyFrom(other); }
    KeyedSlots(KeyedSlots&& other) noexcept = default;
    KeyedSlots& operator=(const KeyedSlots& other) {
        if (this != &other) {
            head_.reset();
            copyFrom(other);
        }
        return *this;
    }
    KeyedSlots& operator=(KeyedSlots&& other) noexcept = default;

    /// The entry named `name`, added with a value-initialized V if absent.
    V& operator[](std::string_view name) {
        const SlotKey key = slotKey(name);
        Entry* free_entry = nullptr;
        Block* last = nullptr;
        for (Block* b = head_.get(); b; b = b->next.get()) {
            for (Entry& e : b->entries) {
                if (e.first == key) return e.second;
                if (e.first == 0 && !free_entry) free_entry = &e;
            }
            last = b;
        }
        if (!free_entry) {
            auto block = std::make_unique<Block>();
            free_entry = &block->entries[0];
            if (last) last->next = std::move(block);
            else head_ = std::move(block);
        }
        free_entry->first = key;
        free_entry->second = V{};
        return free_entry->second;
    }

    iterator find(std::string_view name) { return findKey(slotKey(name)); }
    const_iterator find(std::string_view name) const { return findKey(slotKey(name)); }
    iterator end() noexcept { return nullptr; }
    const_iterator end() const noexcept { return nullptr; }

    std::size_t count(std::string_view name) const { return find(name) ? 1 : 0; }
    bool contains(std::string_view name) const { return find(name) != nullptr; }

    V& at(std::string_view name) {
        if (Entry* e = find(name)) return e->second;
        throw std::out_of_range("KeyedSlots::at: no entry \"" + std::string(name) + "\"");
    }
    const V& at(std::string_view name) const {
        if (const Entry* e = find(name)) return e->second;
        throw std::out_of_range("KeyedSlots::at: no entry \"" + std::string(name) + "\"");
    }

    /// Removes the entry named `name`; 1 if there was one.
    std::size_t erase(std::string_view name) {
        Entry* e = find(name);
        if (!e) return 0;
        e->first = 0;
        e->second = V{};
        if (empty()) head_.reset();  // nothing left to keep in place
        return 1;
    }

    /// Removes the entry `it` points at (from find); a null `it` is ignored.
    void erase(const_iterator it) {
        if (!it) return;
        Entry* e = const_cast<Entry*>(it);
        e->first = 0;
        e->second = V{};
        if (empty()) head_.reset();
    }

    bool empty() const noexcept { return size() == 0; }
    std::size_t size() const noexcept {
        std::size_t n = 0;
        for (const Block* b = head_.get(); b; b = b->next.get())
            for (const Entry& e : b->entries) n += e.first != 0;
        return n;
    }
    void clear() noexcept { head_.reset(); }

    /// `visit(key, value)` for every entry, in an unspecified order.
    template <class Visit>
    void forEach(Visit&& visit) const {
        for (const Block* b = head_.get(); b; b = b->next.get())
            for (const Entry& e : b->entries)
                if (e.first != 0) visit(e.first, e.second);
    }

    /// The same names with the same values.
    friend bool operator==(const KeyedSlots& a, const KeyedSlots& b) {
        if (a.size() != b.size()) return false;
        bool same = true;
        a.forEach([&](SlotKey key, const V& value) {
            const Entry* other = b.findKey(key);
            if (!other || !(other->second == value)) same = false;
        });
        return same;
    }

private:
    static constexpr int kPerBlock = 4;
    struct Block {
        Entry entries[kPerBlock];
        std::unique_ptr<Block> next;
    };

    Entry* findKey(SlotKey key) const noexcept {
        for (Block* b = head_.get(); b; b = b->next.get())
            for (Entry& e : b->entries)
                if (e.first == key) return &e;
        return nullptr;
    }

    void copyFrom(const KeyedSlots& other) {
        std::unique_ptr<Block>* tail = &head_;
        for (const Block* b = other.head_.get(); b; b = b->next.get()) {
            auto block = std::make_unique<Block>();
            for (int i = 0; i < kPerBlock; ++i) block->entries[i] = b->entries[i];
            *tail = std::move(block);
            tail = &(*tail)->next;
        }
    }

    std::unique_ptr<Block> head_;
};

} // namespace riftbound
