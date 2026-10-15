#pragma once
/// @file object_table.h
/// The object pool of a game (GameObjectId -> GameObject) and its board index.
///
/// ObjectTable is the `GameState::objects` container. It behaves as the
/// `std::unordered_map<GameObjectId, GameObject>` it wraps — same lookups, same
/// iteration order, references to objects stay valid for the life of the
/// object — and adds lists of the objects that are in play, kept by where they
/// are (plans/06-performance.md item 6.10). Before, every board query ("the
/// units at this battlefield", "this player's runes", "anything in play that
/// triggers on this") walked all ~150 objects of a game, deck and trash
/// included, and a single cleanup or legal-action pass asked dozens of them.
///
/// How the index stays right
/// -------------------------
/// It is not maintained by the code that moves objects; that would be one
/// forgotten call away from a stale answer (there are over a thousand places,
/// tests included, that write `obj.zone` or `obj.location`). Instead the two
/// fields the index is built from, GameObject::zone and GameObject::location,
/// are TrackedField / TrackedLocation (tracked_field.h): assigning either one,
/// by any route, reports the object to the table it lives in (IndexSlot), and
/// the next query brings the lists up to date. Between two changes of a zone
/// or a location any number of queries share one update. Everything else a
/// query filters on (controller, card type, exhausted, ...) is read from the
/// object when the query is asked, so it cannot be stale.
///
/// The lists are in the table's iteration order, so a query returns what the
/// walk it replaces returned, in the same order. An update does not walk the
/// objects (batch 5c-perf: a walk per update was 5% of a game): the index
/// keeps the table's entries in that order (rebuilt when an object is added
/// or removed, the only changes that can reorder the table) and, per entry,
/// which lists it is in; an update reads again only the objects whose zone or
/// location changed and refills the lists from those per-entry facts.
///
/// Using a range
/// -------------
/// `onBoard()`, `inPlay()`, `atLocation()` return a BoardRange: the objects
/// that were there when the range was taken. It iterates like the table itself
/// (`for (auto& [id, obj] : objects.onBoard())`), so a walk over every object
/// that skipped the ones not on the board becomes a walk over the board by
/// naming the range. Use it for loops whose body cannot move an object (reads,
/// flag updates, aura bookkeeping). A loop that kills, moves, plays or creates
/// objects must collect ids first, as it had to before: a range does not
/// follow such changes (it stays safe to iterate — the lists it views are kept
/// until it is done — but it is the board as it was). Building with
/// -DRIFTBOUND_CHECK_BOARD_INDEX=ON turns that case into an error and makes
/// every query recompute the lists and compare them with the index.
///
/// Threads
/// -------
/// A `const` table may be queried from several threads at once, like any
/// other const data of the engine (CardDB, CardRegistry): the first query of
/// a stale table rebuilds the lists under a mutex, the others wait for it,
/// and a clean table is read without taking the mutex (one atomic load). As
/// everywhere, changing a state while another thread reads it is not safe.
///
/// The index holds nothing about what an id IS (it stores pointers to the
/// table's entries and compares LocationId values), so making GameObjectId /
/// BattlefieldId strong types (plans/05 item 5.5) changes nothing here but
/// the spelling of the key type.

#include "game_object.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace riftbound {

/// A run of objects out of the board index. `Entry` is the table's
/// (id, object) pair, const or not: iterating gives what iterating the table
/// gives, for the objects of the run only.
template <class Entry>
class BoardRange {
public:
    class const_iterator {
    public:
        using value_type = Entry;
        using reference = Entry&;
        using pointer = Entry*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        const_iterator() = default;
        explicit const_iterator(Entry* const* at) : at_(at) {}
        Entry& operator*() const { return **at_; }
        Entry* operator->() const { return *at_; }
        const_iterator& operator++() { ++at_; return *this; }
        const_iterator operator++(int) { const_iterator was = *this; ++at_; return was; }
        bool operator==(const const_iterator& o) const { return at_ == o.at_; }
        bool operator!=(const const_iterator& o) const { return at_ != o.at_; }
    private:
        Entry* const* at_ = nullptr;
    };
    using iterator = const_iterator;
    using value_type = Entry;

    BoardRange() = default;
    BoardRange(const BoardRange& o) : first_(o.first_), last_(o.last_), pins_(o.pins_) {
        if (pins_) pins_->fetch_add(1, std::memory_order_relaxed);
    }
    BoardRange& operator=(const BoardRange& o) {
        if (o.pins_) o.pins_->fetch_add(1, std::memory_order_relaxed);
        if (pins_) pins_->fetch_sub(1, std::memory_order_release);
        first_ = o.first_;
        last_ = o.last_;
        pins_ = o.pins_;
        return *this;
    }
    ~BoardRange() { if (pins_) pins_->fetch_sub(1, std::memory_order_release); }

    const_iterator begin() const { return const_iterator(first_); }
    const_iterator end() const { return const_iterator(last_); }
    std::size_t size() const { return static_cast<std::size_t>(last_ - first_); }
    bool empty() const { return first_ == last_; }
    Entry& operator[](std::size_t i) const { return *first_[i]; }

private:
    friend class ObjectTable;
    BoardRange(Entry* const* f, Entry* const* l, std::atomic<int>* pins)
        : first_(f), last_(l), pins_(pins) {
        if (pins_) pins_->fetch_add(1, std::memory_order_relaxed);
    }
    Entry* const* first_ = nullptr;
    Entry* const* last_ = nullptr;
    // The lists this range views are in use while it lives: a rebuild leaves
    // them alone and makes new ones (ObjectTable::lists). Atomic: ranges over
    // a const table may be taken on several threads.
    std::atomic<int>* pins_ = nullptr;
};

class ObjectTable {
public:
    using Map = std::unordered_map<GameObjectId, GameObject>;
    using key_type = Map::key_type;
    using mapped_type = Map::mapped_type;
    using value_type = Map::value_type;
    using size_type = Map::size_type;
    using iterator = Map::iterator;
    using const_iterator = Map::const_iterator;
    using Range = BoardRange<value_type>;
    using ConstRange = BoardRange<const value_type>;

    ObjectTable() = default;
    // A copy has objects of its own: they report to the copy's index.
    ObjectTable(const ObjectTable& other) : map_(other.map_) { adoptAll(); }
    ObjectTable& operator=(const ObjectTable& other) {
        if (this != &other) {
            map_ = other.map_;
            adoptAll();
        }
        return *this;
    }
    // A move takes the objects and the index they report to together.
    ObjectTable(ObjectTable&&) noexcept = default;
    ObjectTable& operator=(ObjectTable&&) noexcept = default;

    // ── The map ──
    iterator begin() { return map_.begin(); }
    iterator end() { return map_.end(); }
    const_iterator begin() const { return map_.begin(); }
    const_iterator end() const { return map_.end(); }
    const_iterator cbegin() const { return map_.cbegin(); }
    const_iterator cend() const { return map_.cend(); }
    size_type size() const { return map_.size(); }
    bool empty() const { return map_.empty(); }
    iterator find(GameObjectId id) { return map_.find(id); }
    const_iterator find(GameObjectId id) const { return map_.find(id); }
    size_type count(GameObjectId id) const { return map_.count(id); }
    bool contains(GameObjectId id) const { return map_.find(id) != map_.end(); }
    GameObject& at(GameObjectId id) { return map_.at(id); }
    const GameObject& at(GameObjectId id) const { return map_.at(id); }

    /// The object `id`, created (default) if there is none.
    GameObject& operator[](GameObjectId id) {
        auto [it, inserted] = map_.try_emplace(id);
        if (inserted) adopt(it->second);
        return it->second;
    }
    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        auto r = map_.emplace(std::forward<Args>(args)...);
        if (r.second) adopt(r.first->second);
        return r;
    }
    size_type erase(GameObjectId id) {
        const size_type n = map_.erase(id);
        if (n > 0) markStale();
        return n;
    }
    iterator erase(const_iterator pos) {
        markStale();
        return map_.erase(pos);
    }
    void clear() {
        map_.clear();
        markStale();
    }

    // ── The board index ──
    /// Every object with a location: units, gear and runes at a base or a
    /// battlefield (attached and facedown ones included).
    Range onBoard() { return range<value_type>(Which::Board); }
    ConstRange onBoard() const { return range<const value_type>(Which::Board); }
    /// The board and each player's Legend Zone: everything whose card can
    /// have an ability or a continuous effect working.
    Range inPlay() { return range<value_type>(Which::InPlay); }
    ConstRange inPlay() const { return range<const value_type>(Which::InPlay); }
    /// Every object that is NOT in a pile: not in a Main Deck, a Rune Deck, a
    /// Trash or Banishment (and anything with a location, whatever its zone
    /// says). That is the board, the Legend and Champion Zones, the hands,
    /// the chain, facedown cards and the battlefield cards — everything a
    /// Cleanup can have something to do with (GameEngine::cleanup: an object
    /// in a pile carries no status, no damage and no continuous effect, CR
    /// 124). About a quarter of a game's objects.
    Range offPiles() { return range<value_type>(Which::OffPiles); }
    ConstRange offPiles() const { return range<const value_type>(Which::OffPiles); }
    /// Whether `obj` is in a pile in the sense of offPiles().
    static bool inPile(const GameObject& obj) {
        if (obj.location.has_value()) return false;
        const ZoneType z = obj.zone.get();
        return z == ZoneType::MainDeck || z == ZoneType::RuneDeck || z == ZoneType::Trash ||
               z == ZoneType::Banishment;
    }
    /// The objects at one location.
    Range atLocation(const LocationId& loc) { return rangeAt<value_type>(loc); }
    ConstRange atLocation(const LocationId& loc) const { return rangeAt<const value_type>(loc); }

    /// How often the lists were rebuilt (for tests and benchmarks).
    std::size_t indexRebuilds() const { return index_ ? index_->rebuilds : 0; }

private:
    using List = std::vector<value_type*>;

    // Which lists an entry is in (Index::where).
    static constexpr uint8_t kOnBoard = 1, kInPlay = 2, kOffPiles = 4;
    static uint8_t whereOf(const GameObject& obj) {
        const bool located = obj.location.has_value();
        uint8_t w = 0;
        if (located) w |= kOnBoard;
        if (located || obj.zone == ZoneType::LegendZone) w |= kInPlay;
        if (!inPile(obj)) w |= kOffPiles;
        return w;
    }

    /// One build of the lists.
    struct Lists {
        std::atomic<int> pins{0}; // BoardRanges viewing these lists
        List board;
        List in_play;
        List off_piles;
        // One entry per location that holds, or held, an object. A game has
        // two bases and a handful of battlefields: a linear search beats a map.
        std::vector<std::pair<LocationId, List>> by_location;
        List none;                // what at() answers for a location never used

        const List& at(const LocationId& loc) const {
            for (const auto& [l, list] : by_location)
                if (l == loc) return list;
            return none;
        }
        List& slot(const LocationId& loc) {
            for (auto& [l, list] : by_location)
                if (l == loc) return list;
            by_location.emplace_back(loc, List{});
            return by_location.back().second;
        }
        void fill(const Map& map) {
            board.clear();
            in_play.clear();
            off_piles.clear();
            for (auto& entry : by_location) entry.second.clear();
            for (const value_type& centry : map) {
                // The table is not const to whoever asks through a non-const
                // accessor; a const accessor hands out const entries.
                value_type* entry = const_cast<value_type*>(&centry);
                const GameObject& obj = centry.second;
                const bool located = obj.location.has_value();
                if (located) {
                    board.push_back(entry);
                    slot(*obj.location).push_back(entry);
                }
                if (located || obj.zone == ZoneType::LegendZone) in_play.push_back(entry);
                if (!inPile(obj)) off_piles.push_back(entry);
            }
        }
        /// The lists from the index's per-entry facts (no object is read).
        template <class Index>
        void fillFrom(const Index& ix) {
            board.clear();
            in_play.clear();
            off_piles.clear();
            for (auto& entry : by_location) entry.second.clear();
            // by_location[k] is the index's locations[k] (both only grow).
            for (std::size_t k = by_location.size(); k < ix.locations.size(); ++k)
                by_location.emplace_back(ix.locations[k], List{});
            const std::size_t n = ix.order.size();
            for (std::size_t i = 0; i < n; ++i) {
                const uint8_t w = ix.where[i];
                if (w == 0) continue;
                value_type* entry = ix.order[i];
                if (w & kOnBoard) {
                    board.push_back(entry);
                    by_location[ix.at[i]].second.push_back(entry);
                }
                if (w & kInPlay) in_play.push_back(entry);
                if (w & kOffPiles) off_piles.push_back(entry);
            }
        }
        bool sameAs(const Lists& o) const {
            if (board != o.board || in_play != o.in_play || off_piles != o.off_piles) return false;
            for (const auto& [loc, list] : by_location)
                if (o.at(loc) != list) return false;
            for (const auto& [loc, list] : o.by_location)
                if (at(loc) != list) return false;
            return true;
        }
    };

    struct Index {
        std::atomic<bool> stale{true};   // IndexSlot::touch raises it
        std::mutex rebuilding;           // held by the one thread that rebuilds
        std::size_t rebuilds = 0;
        std::unique_ptr<Lists> current;
        // Lists a BoardRange was still iterating when the board changed and
        // was asked again: kept until that range is done.
        std::vector<std::unique_ptr<Lists>> retired;

        // What an update is made from.
        std::deque<IndexSlot> slots;        // one per object adopted (stable addresses)
        std::vector<IndexSlot*> changed;    // objects whose zone or location changed since
        bool reordered = true;              // an object was added or removed since
        std::vector<value_type*> order;     // the table's entries, in its iteration order
        std::vector<uint8_t> where;         // per entry (by rank): kOnBoard | kInPlay | kOffPiles
        std::vector<uint16_t> at;           // per entry on the board: its place in `locations`
        std::vector<LocationId> locations;  // every location an entry has been listed at

        uint16_t locationIndex(const LocationId& loc) {
            for (std::size_t k = 0; k < locations.size(); ++k)
                if (locations[k] == loc) return static_cast<uint16_t>(k);
            locations.push_back(loc);
            return static_cast<uint16_t>(locations.size() - 1);
        }
        void relist(std::size_t rank) {
            const GameObject& obj = order[rank]->second;
            where[rank] = whereOf(obj);
            if (obj.location.has_value()) at[rank] = locationIndex(*obj.location);
        }
    };

    // The index object exists from the first object on (adopt) and for every
    // copy (adoptAll); only a table that never held an object has none, and
    // its queries answer from an empty static list without creating one —
    // a const query never allocates the index.
    Index& ensureIndex() {
        if (!index_) index_ = std::make_unique<Index>();
        return *index_;
    }
    // An object added or removed: the table may have a new order.
    void markStale() {
        if (!index_) return;
        index_->reordered = true;
        index_->stale.store(true, std::memory_order_release);
    }
    static void reportTo(Index& ix, GameObject& obj) {
        IndexSlot& slot = ix.slots.emplace_back();
        slot.stale = &ix.stale;
        slot.changed = &ix.changed;
        obj.zone.reportTo(&slot);
        obj.location.reportTo(&slot);
    }
    void adopt(GameObject& obj) {
        Index& ix = ensureIndex();
        reportTo(ix, obj);
        ix.reordered = true;
        ix.stale.store(true, std::memory_order_release);
    }
    void adoptAll() {
        Index& ix = ensureIndex();
        // Every object of the table gets a slot of its own; whatever the
        // assignment that brought them reported to the old slots is moot.
        ix.changed.clear();
        ix.slots.clear();
        for (auto& [id, obj] : map_) reportTo(ix, obj);
        ix.reordered = true;
        ix.stale.store(true, std::memory_order_release);
    }
    /// Brings the per-entry facts up to date: the whole order when an object
    /// was added or removed, else the objects that changed.
    void update(Index& ix) const {
        if (ix.reordered) {
            ix.order.clear();
            for (const value_type& centry : map_)
                ix.order.push_back(const_cast<value_type*>(&centry));
            ix.where.assign(ix.order.size(), 0);
            ix.at.assign(ix.order.size(), 0);
            for (std::size_t i = 0; i < ix.order.size(); ++i) {
                ix.order[i]->second.location.slot_->rank = static_cast<uint32_t>(i);
                ix.relist(i);
            }
            for (IndexSlot* s : ix.changed) s->queued = false;
            ix.reordered = false;
        } else {
            for (IndexSlot* s : ix.changed) {
                s->queued = false;
                ix.relist(s->rank);
            }
        }
        ix.changed.clear();
    }

    /// The lists, rebuilt first if an object moved since they were built.
    /// nullptr for a table that never held an object.
    const Lists* lists() const {
        Index* ixp = index_.get();
        if (!ixp) return nullptr;
        Index& ix = *ixp;
        if (ix.stale.load(std::memory_order_acquire)) {
            // One thread rebuilds; a reader that finds the table stale at the
            // same moment waits here and then finds it clean.
            const std::lock_guard<std::mutex> lock(ix.rebuilding);
            if (ix.stale.load(std::memory_order_relaxed)) {
                if (ix.current && ix.current->pins.load(std::memory_order_acquire) > 0) {
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
                    throw std::logic_error("ObjectTable: an object changed zone or location inside "
                                           "a loop over a BoardRange, and the board was asked again");
#endif
                    ix.retired.push_back(std::move(ix.current));
                }
                ix.retired.erase(std::remove_if(ix.retired.begin(), ix.retired.end(),
                                     [](const std::unique_ptr<Lists>& l) {
                                         return l->pins.load(std::memory_order_acquire) == 0;
                                     }),
                                 ix.retired.end());
                if (!ix.current) ix.current = std::make_unique<Lists>();
                update(ix);
                ix.current->fillFrom(ix);
                ++ix.rebuilds;
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
                {
                    Lists fresh;
                    fresh.fill(map_);
                    if (!fresh.sameAs(*ix.current))
                        throw std::logic_error("ObjectTable: the updated board index differs from "
                                               "a walk over the table");
                }
#endif
                ix.stale.store(false, std::memory_order_release);
            }
        }
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
        else {
            // The index says it is current: recompute and compare, and check
            // that every object of the table reports to this index.
            Lists fresh;
            fresh.fill(map_);
            bool ok = fresh.sameAs(*ix.current);
            for (const auto& entry : map_) {
                const IndexSlot* slot = entry.second.location.slot_;
                ok = ok && slot && entry.second.zone.reportsTo(slot) && slot->stale == &ix.stale &&
                     slot->rank < ix.order.size() && ix.order[slot->rank] == &entry;
            }
            if (!ok)
                throw std::logic_error("ObjectTable: the board index is stale (an object's zone "
                                       "or location changed without the table being told)");
        }
#endif
        return ix.current.get();
    }

    enum class Which { Board, InPlay, OffPiles };
    template <class Entry>
    BoardRange<Entry> range(Which which) const {
        const Lists* l = lists();
        if (!l) return BoardRange<Entry>();
        return range<Entry>(*l, which == Which::Board ? l->board
                                   : which == Which::InPlay ? l->in_play : l->off_piles);
    }
    template <class Entry>
    BoardRange<Entry> rangeAt(const LocationId& loc) const {
        const Lists* l = lists();
        if (!l) return BoardRange<Entry>();
        return range<Entry>(*l, l->at(loc));
    }
    template <class Entry>
    static BoardRange<Entry> range(const Lists& lists, const List& list) {
        value_type* const* first = list.data();
        return BoardRange<Entry>(first, first + list.size(),
                                 const_cast<std::atomic<int>*>(&lists.pins));
    }

    Map map_;
    mutable std::unique_ptr<Index> index_;
};

}  // namespace riftbound
