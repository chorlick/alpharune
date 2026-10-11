#include "core/keyed_slots.h"

#include <deque>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace riftbound {

namespace {

// Every counter name of the process. Names are few (one per counter a card or
// the engine keeps, plus the handful built at run time from a player or a
// turn), so the table is small and never shrinks.
struct SlotKeyTable {
    struct Hash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };
    std::shared_mutex mutex;
    // Keys are views of the strings in `names`, whose addresses never change.
    std::unordered_map<std::string_view, SlotKey, Hash, std::equal_to<>> keys;
    std::deque<std::string> names;  // names[key - 1]

    static SlotKeyTable& instance() {
        static SlotKeyTable* table = new SlotKeyTable;  // never destroyed
        return *table;
    }
};

} // namespace

detail::SlotKeyEntry detail::internSlotKey(std::string_view name) {
    auto& table = SlotKeyTable::instance();
    {
        std::shared_lock<std::shared_mutex> lock(table.mutex);
        auto it = table.keys.find(name);
        if (it != table.keys.end()) return {it->second, &table.names[it->second - 1]};
    }
    std::unique_lock<std::shared_mutex> lock(table.mutex);
    auto it = table.keys.find(name);
    if (it != table.keys.end()) return {it->second, &table.names[it->second - 1]};
    table.names.emplace_back(name);
    const SlotKey key = static_cast<SlotKey>(table.names.size());
    table.keys.emplace(std::string_view(table.names.back()), key);
    return {key, &table.names.back()};
}

const std::string& slotKeyName(SlotKey key) {
    auto& table = SlotKeyTable::instance();
    std::shared_lock<std::shared_mutex> lock(table.mutex);
    if (key == 0 || key > table.names.size())
        throw std::out_of_range("slotKeyName: no such key " + std::to_string(key));
    return table.names[key - 1];
}

} // namespace riftbound
