#pragma once
/// @file tracked_field.h
/// GameObject fields that tell the object table when they change.
///
/// GameObject::zone and GameObject::location are what the board index of
/// ObjectTable (object_table.h) is built from. They read like the plain
/// `ZoneType` and `std::optional<LocationId>` they hold; assigning one raises
/// the "stale" flag of the table the object lives in, so no code that moves an
/// object can leave the index behind. A GameObject that is in no table (a
/// local copy, a snapshot) reports to nobody.
///
/// Copying a field copies its value only: the copy belongs to no table until a
/// table adopts the object it is part of (ObjectTable does that for every
/// object it inserts or copies). Assigning to a field keeps the table it
/// already reports to.
///
/// The flag is atomic because readers of a const table may be on other
/// threads (object_table.h, "Threads"); a write to a field is still a write
/// to the state, which needs the state to oneself as any other write does.

#include "types.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <utility>

namespace riftbound {

class ObjectTable;

/// A value of type T (an enum here) whose assignments are reported.
template <class T>
class TrackedField {
public:
    TrackedField() = default;
    TrackedField(const T& v) : value_(v) {}                       // NOLINT: reads as a T
    TrackedField(const TrackedField& o) : value_(o.value_) {}     // a copy reports to nobody
    TrackedField& operator=(const TrackedField& o) { set(o.value_); return *this; }
    TrackedField& operator=(const T& v) { set(v); return *this; }

    operator const T&() const { return value_; }                  // NOLINT: reads as a T
    const T& get() const { return value_; }

private:
    friend class ObjectTable;
    void set(const T& v) {
        if (value_ == v) return;
        value_ = v;
        if (stale_) stale_->store(true, std::memory_order_release);
    }
    void reportTo(std::atomic<bool>* stale) { stale_ = stale; }
    bool reportsTo(const std::atomic<bool>* stale) const { return stale_ == stale; }

    T value_{};
    std::atomic<bool>* stale_ = nullptr;
};

/// `std::optional<LocationId>` whose assignments are reported. Read access is
/// the optional's; there is no way to change the location in place (operator*
/// and operator-> are const), so every change is an assignment or reset().
class TrackedLocation {
public:
    using Value = std::optional<LocationId>;

    TrackedLocation() = default;
    TrackedLocation(std::nullopt_t) {}                                  // NOLINT
    TrackedLocation(const Value& v) : value_(v) {}                      // NOLINT
    TrackedLocation(const LocationId& v) : value_(v) {}                 // NOLINT
    TrackedLocation(const BaseLocation& v) : value_(LocationId{v}) {}   // NOLINT
    TrackedLocation(const BattlefieldLocation& v) : value_(LocationId{v}) {}   // NOLINT
    TrackedLocation(const TrackedLocation& o) : value_(o.value_) {}     // a copy reports to nobody

    TrackedLocation& operator=(const TrackedLocation& o) { set(o.value_); return *this; }
    TrackedLocation& operator=(std::nullopt_t) { set(std::nullopt); return *this; }
    TrackedLocation& operator=(const Value& v) { set(v); return *this; }
    TrackedLocation& operator=(const LocationId& v) { set(Value{v}); return *this; }
    TrackedLocation& operator=(const BaseLocation& v) { set(Value{LocationId{v}}); return *this; }
    TrackedLocation& operator=(const BattlefieldLocation& v) {
        set(Value{LocationId{v}});
        return *this;
    }
    void reset() { set(std::nullopt); }

    bool has_value() const { return value_.has_value(); }
    explicit operator bool() const { return value_.has_value(); }
    const LocationId& operator*() const { return *value_; }
    const LocationId* operator->() const { return &*value_; }
    const LocationId& value() const { return value_.value(); }
    LocationId value_or(const LocationId& other) const { return value_.value_or(other); }
    operator const Value&() const { return value_; }                    // NOLINT
    const Value& get() const { return value_; }

    friend bool operator==(const TrackedLocation& a, const TrackedLocation& b) {
        return a.value_ == b.value_;
    }
    friend bool operator==(const TrackedLocation& a, const Value& b) { return a.value_ == b; }
    friend bool operator==(const TrackedLocation& a, const LocationId& b) {
        return a.value_.has_value() && *a.value_ == b;
    }
    friend bool operator==(const TrackedLocation& a, std::nullopt_t) {
        return !a.value_.has_value();
    }

private:
    friend class ObjectTable;
    void set(const Value& v) {
        if (value_ == v) return;
        value_ = v;
        if (stale_) stale_->store(true, std::memory_order_release);
    }
    void reportTo(std::atomic<bool>* stale) { stale_ = stale; }
    bool reportsTo(const std::atomic<bool>* stale) const { return stale_ == stale; }

    Value value_;
    std::atomic<bool>* stale_ = nullptr;
};

/// GameObject::zone: a TrackedField<ZoneType> that also counts the object's
/// lives. CR 359.3.e.2 / 359.3.e.4: "A target is illegal as the spell resolves
/// if ... it has changed Zones to or from a Non-Board Zone" — "it's not treated
/// as the same object" even when it has come back (CR 124.1). Every change of
/// zone other than between the two board zones (a Base and a Battlefield)
/// starts a new life; `incarnation()` is its number. A chain item stamps the
/// incarnation of what it chooses (GameState::stampChosen) and a choice whose
/// object has had a new life since is no longer legal (GameState::
/// stillTheChosenObject; testing phase G14). The counter sits in the padding
/// after the one-byte zone, so a GameObject is no larger for it.
class TrackedZone {
public:
    TrackedZone() = default;
    TrackedZone(ZoneType v) : value_(v) {}                                  // NOLINT
    TrackedZone(const TrackedZone& o) : value_(o.value_), incarnation_(o.incarnation_) {}
    TrackedZone& operator=(const TrackedZone& o) {
        set(o.value_);
        incarnation_ = o.incarnation_;
        return *this;
    }
    TrackedZone& operator=(ZoneType v) { set(v); return *this; }

    operator const ZoneType&() const { return value_; }                    // NOLINT
    const ZoneType& get() const { return value_; }
    uint32_t incarnation() const { return incarnation_; }

    static bool isBoardZone(ZoneType z) {
        return z == ZoneType::Base || z == ZoneType::BattlefieldZone;
    }

private:
    friend class ObjectTable;
    void set(ZoneType v) {
        if (value_ == v) return;
        if (!(isBoardZone(value_) && isBoardZone(v))) ++incarnation_;
        value_ = v;
        if (stale_) stale_->store(true, std::memory_order_release);
    }
    void reportTo(std::atomic<bool>* stale) { stale_ = stale; }
    bool reportsTo(const std::atomic<bool>* stale) const { return stale_ == stale; }

    ZoneType value_{};
    uint32_t incarnation_ = 0;
    std::atomic<bool>* stale_ = nullptr;
};

}  // namespace riftbound
