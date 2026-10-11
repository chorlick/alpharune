#pragma once
/// @file interned.h
/// Values a GameObject carries that are the same for many objects and almost
/// never change: its name, its tags, its domains (plans/09 item 9.KK1).
///
/// A GameState is copied for every search node, and each copy used to
/// allocate a string, a vector of strings and a vector of domains per object.
/// An `Interned<T>` is one pointer to an immortal, shared, constant `T`: a
/// copy is a pointer copy and frees nothing.
///
/// It reads as the `T` it stands for — `obj.name + "..."`, `obj.name == "X"`,
/// `for (auto& t : obj.tags)`, `obj.tags.empty()`, passing it where a
/// `const T&` is wanted — so code that reads these members did not change.
/// Writing is by assignment (`obj.name = def.name`, `obj.tags = {...}`) or by
/// the few edits card code does (`push_back`, `clear`, `edit`): each makes the
/// new value and looks it up in the pool. Nothing holds a non-const reference
/// to the shared value, so one object's edit never shows in another.
///
/// The pool is process-wide and never shrinks. What goes into it is bounded by
/// the card table plus what effects make at run time (token names, a copied
/// unit's tags), not by the number of games played. Threads may intern at the
/// same time (a mutex guards the pool; a per-thread cache in front of it makes
/// the common case — the same CardDef member again — a pointer compare and an
/// equality check).

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace riftbound {

namespace detail {

/// The pool of every `T` ever interned. std::set: its nodes never move.
template <class T>
struct InternPool {
    std::mutex mutex;
    std::set<T> values;
    static InternPool& instance() {
        static InternPool* pool = new InternPool;  // never destroyed: objects of
        return *pool;                              // static duration may point in
    }
};

/// The pooled value equal to `v` (added if new). Slow path: takes the mutex.
template <class T>
const T* internInPool(const T& v) {
    auto& pool = InternPool<T>::instance();
    std::lock_guard<std::mutex> lock(pool.mutex);
    return &*pool.values.insert(v).first;
}

template <class T>
const T* emptyInterned() {
    static const T* empty = internInPool(T{});
    return empty;
}

/// The pooled value equal to `v`. A per-thread cache keyed by `v`'s address
/// answers the usual caller, which interns the same CardDef member again and
/// again; a hit is checked for equality, so a different value that happens to
/// live at a remembered address is never mistaken for the old one.
template <class T>
const T* intern(const T& v) {
    struct Slot { const T* source = nullptr; const T* pooled = nullptr; };
    constexpr std::size_t kSlots = 256;
    thread_local Slot cache[kSlots];
    const auto address = reinterpret_cast<std::uintptr_t>(&v);
    Slot& slot = cache[((address >> 3) ^ (address >> 11)) & (kSlots - 1)];
    if (slot.source == &v && *slot.pooled == v) return slot.pooled;
    const T* pooled = internInPool(v);
    slot = Slot{&v, pooled};
    return pooled;
}

} // namespace detail

template <class T>
class Interned {
public:
    using value_type = typename T::value_type;
    using const_iterator = typename T::const_iterator;
    using iterator = const_iterator;
    using size_type = typename T::size_type;

    Interned() : p_(detail::emptyInterned<T>()) {}
    /// Explicit: a `T` and an `Interned<T>` in one conditional expression then
    /// have one common type, the T.
    explicit Interned(const T& v) : p_(detail::intern(v)) {}
    Interned(const Interned&) noexcept = default;
    Interned& operator=(const Interned&) noexcept = default;

    /// `obj.name = def.name`: the usual write, answered by the per-thread cache.
    Interned& operator=(const T& v) {
        p_ = detail::intern(v);
        return *this;
    }
    /// `obj.name = "Recruit"`, `obj.name = some_string_view_like`.
    template <class U>
        requires(!std::same_as<std::remove_cvref_t<U>, Interned> &&
                 !std::same_as<std::remove_cvref_t<U>, T> &&
                 std::constructible_from<T, U &&>)
    Interned& operator=(U&& u) {
        const T made(std::forward<U>(u));
        p_ = detail::internInPool(made);
        return *this;
    }
    /// `obj.tags = {"Mech", "Poro"}`.
    Interned& operator=(std::initializer_list<value_type> items) {
        const T made(items);
        p_ = detail::internInPool(made);
        return *this;
    }

    // ── Reading: as the T it stands for ──
    operator const T&() const noexcept { return *p_; }
    const T& get() const noexcept { return *p_; }
    const T& operator*() const noexcept { return *p_; }
    const T* operator->() const noexcept { return p_; }

    const_iterator begin() const noexcept { return p_->begin(); }
    const_iterator end() const noexcept { return p_->end(); }
    size_type size() const noexcept { return p_->size(); }
    bool empty() const noexcept { return p_->empty(); }
    decltype(auto) operator[](size_type i) const { return (*p_)[i]; }
    decltype(auto) at(size_type i) const { return p_->at(i); }
    decltype(auto) front() const { return p_->front(); }
    decltype(auto) back() const { return p_->back(); }

    // ── Writing: make the new value, look it up ──
    /// Copies the value, hands the copy to `change`, interns the result.
    template <class Change>
    void edit(Change&& change) {
        T next = *p_;
        change(next);
        p_ = detail::internInPool(next);
    }
    void clear() { p_ = detail::emptyInterned<T>(); }
    template <class V>
    void push_back(V&& v) {
        edit([&](T& next) { next.push_back(std::forward<V>(v)); });
    }

    // ── Comparing ──
    // Two interned values are equal exactly when they are the same pooled value.
    friend bool operator==(const Interned& a, const Interned& b) noexcept { return a.p_ == b.p_; }
    friend bool operator==(const Interned& a, const T& b) { return *a.p_ == b; }
    friend bool operator<(const Interned& a, const Interned& b) { return *a.p_ < *b.p_; }

    // ── A name is a string ──
    friend bool operator==(const Interned& a, const char* b)
        requires std::same_as<T, std::string> { return *a.p_ == b; }
    friend bool operator==(const Interned& a, std::string_view b)
        requires std::same_as<T, std::string> { return std::string_view(*a.p_) == b; }
    friend std::string operator+(const Interned& a, const Interned& b)
        requires std::same_as<T, std::string> { return *a.p_ + *b.p_; }
    friend std::string operator+(const Interned& a, const std::string& b)
        requires std::same_as<T, std::string> { return *a.p_ + b; }
    friend std::string operator+(const std::string& a, const Interned& b)
        requires std::same_as<T, std::string> { return a + *b.p_; }
    friend std::string operator+(std::string&& a, const Interned& b)
        requires std::same_as<T, std::string> { return std::move(a) + *b.p_; }
    friend std::string operator+(const Interned& a, const char* b)
        requires std::same_as<T, std::string> { return *a.p_ + b; }
    friend std::string operator+(const char* a, const Interned& b)
        requires std::same_as<T, std::string> { return a + *b.p_; }
    friend std::string operator+(const Interned& a, char b)
        requires std::same_as<T, std::string> { return *a.p_ + b; }
    friend std::string operator+(char a, const Interned& b)
        requires std::same_as<T, std::string> { return a + *b.p_; }
    const char* c_str() const noexcept requires std::same_as<T, std::string> { return p_->c_str(); }
    size_type length() const noexcept requires std::same_as<T, std::string> { return p_->size(); }
    template <class... A>
    auto find(A&&... a) const requires std::same_as<T, std::string> {
        return p_->find(std::forward<A>(a)...);
    }
    template <class... A>
    auto rfind(A&&... a) const requires std::same_as<T, std::string> {
        return p_->rfind(std::forward<A>(a)...);
    }
    template <class... A>
    auto compare(A&&... a) const requires std::same_as<T, std::string> {
        return p_->compare(std::forward<A>(a)...);
    }
    template <class... A>
    std::string substr(A&&... a) const requires std::same_as<T, std::string> {
        return p_->substr(std::forward<A>(a)...);
    }

    /// Prints as the value does.
    template <class Stream>
        requires requires(Stream& os, const T& v) { os << v; }
    friend Stream& operator<<(Stream& os, const Interned& v) {
        os << *v.p_;
        return os;
    }

private:
    const T* p_;
};

/// JSON (nlohmann, found by ADL): as the value.
template <class Json, class T>
    requires requires(Json& j, const T& v) { j = v; }
void to_json(Json& j, const Interned<T>& v) { j = v.get(); }

} // namespace riftbound
