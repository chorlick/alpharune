#include "engine/observer_view.h"

#include "cards/card_registry.h"
#include "engine/effect_executor.h"
#include "engine/resource_payment.h"
#include "openspiel/action_vocab.h"

#include <algorithm>

namespace riftbound {

namespace {

constexpr SpendFor kPurposes[4] = {SpendFor::Other, SpendFor::Unit, SpendFor::Spell,
                                   SpendFor::Gear};

void put(std::vector<int64_t>& out, int64_t v) { out.push_back(v); }

}  // namespace

ObserverView::ObserverView(const GameState& state, PlayerId observer, const CardDB& db,
                           const CardRegistry* registry)
    : state_(state), observer_(observer), db_(db), registry_(registry),
      knowledge_(state, observer) {}

std::vector<GameObjectId> ObserverView::ownHand() const {
    return knowledge::sortedHand(state_, observer_);
}

const std::vector<GameObjectId>& ObserverView::publicPile(PlayerId owner, ZoneType zone) const {
    static const std::vector<GameObjectId> kNone;
    const auto& ps = state_.player(owner);
    switch (zone) {
        case ZoneType::Trash:      return ps.trash;
        case ZoneType::Banishment: return ps.banishment;
        default:                   return kNone;
    }
}

std::vector<CardKnowledge> ObserverView::knownInHand(PlayerId owner) const {
    std::vector<CardKnowledge> out;
    if (owner == observer_) return out;   // its own hand is simply visible
    for (const auto& k : knowledge_.knownHiddenCards(owner))
        if (k.last_known_zone == ZoneType::Hand && k.zone_confirmed) out.push_back(k);
    return out;
}

int ObserverView::handSize(PlayerId owner) const {
    return static_cast<int>(state_.player(owner).hand.size());
}
int ObserverView::deckSize(PlayerId owner) const {
    return static_cast<int>(state_.player(owner).main_deck.size());
}
int ObserverView::runeDeckSize(PlayerId owner) const {
    return static_cast<int>(state_.player(owner).rune_deck.size());
}

std::vector<ChainItemFacts> ObserverView::chain() const {
    std::vector<ChainItemFacts> out;
    const auto& items = state_.chain.items;
    auto positionOf = [&](ChainItemId id) {
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].id == id) return static_cast<int>(i);
        return -1;
    };
    auto facts = [&](const ChainItem& ci, int position, bool resolving) {
        ChainItemFacts f;
        f.position = position;
        f.resolving = resolving;
        f.controller = ci.controller;
        f.source = ci.source;
        if (ci.source != kInvalidId && state_.objectExists(ci.source) &&
            knowledge::identityVisible(state_, ci.source, observer_))
            f.card = ci.card_def_id != kInvalidId ? ci.card_def_id
                                                  : state_.getObject(ci.source).card_def_id;
        using K = ChainItemFacts::Kind;
        f.kind = ci.is_equip ? K::Equip
               : ci.keyword_ability.has_value() ? K::Keyword
               : ci.is_spell ? K::Spell
               : ci.is_permanent ? K::Permanent
               : ci.is_activated_ability ? K::Activated
                                         : K::Triggered;
        f.ability_index = ci.ability_index;
        f.trigger = ci.fired_trigger;
        f.finalized = ci.status == ChainItemStatus::Finalized;
        f.targets = ci.targets;
        f.chosen = ci.chosen_objects;
        f.chosen_item = positionOf(ci.chosen_item);
        f.subject = ci.triggering_subject;
        return f;
    };
    for (size_t i = 0; i < items.size(); ++i) out.push_back(facts(items[i], static_cast<int>(i), false));
    if (state_.chain.resuming.has_value())
        out.push_back(facts(*state_.chain.resuming, -1, true));
    return out;
}

PermanentFacts ObserverView::permanent(GameObjectId id) const {
    PermanentFacts f;
    if (!state_.objectExists(id)) return f;
    const GameObject& o = state_.getObject(id);
    f.id = id;
    if (knowledge::identityVisible(state_, id, observer_)) f.card = o.card_def_id;
    f.controller = o.controller;
    f.location = o.location.get();
    f.might = o.current_might;
    for (uint32_t k = 0; k < static_cast<uint32_t>(Keyword::Count); ++k)
        if (o.hasKeyword(static_cast<Keyword>(k))) f.keywords |= 1u << k;
    f.assault = o.assault_value;
    f.shield = o.shield_value;
    f.deflect = o.deflect_value;
    f.damage = o.damage_marked;
    f.lethal = damageKillsIn(state_, registry_, o);
    f.exhausted = o.is_exhausted;
    f.stunned = o.is_stunned;
    f.buffs = o.buff_count;
    f.empowered = o.empowered;
    f.designation = o.combat_designation;
    if (o.attached_to.has_value()) f.attached_to = *o.attached_to;
    return f;
}

std::vector<PermanentFacts> ObserverView::board() const {
    std::vector<std::pair<std::vector<int64_t>, GameObjectId>> seen;
    for (const auto& [id, obj] : state_.objects.inPlay())
        seen.push_back({openspiel::visibleObjectDescription(state_, id, observer_), id});
    std::stable_sort(seen.begin(), seen.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<PermanentFacts> out;
    out.reserve(seen.size());
    for (const auto& [what, id] : seen) out.push_back(permanent(id));
    return out;
}

std::vector<BattlefieldFacts> ObserverView::battlefields() const {
    std::vector<BattlefieldFacts> out;
    for (const auto& bf : state_.battlefields) {
        BattlefieldFacts f;
        f.id = bf.id;
        if (bf.card_object_id != kInvalidId && state_.objectExists(bf.card_object_id))
            f.card = state_.getObject(bf.card_object_id).card_def_id;
        f.controller = bf.controller;
        f.contested = bf.is_contested;
        f.contested_by = bf.contested_by;
        f.showdown_staged = bf.showdown_staged;
        f.showdown_in_progress = bf.showdown_in_progress;
        f.combat_staged = bf.combat_staged;
        f.combat_in_progress = bf.combat_in_progress;
        f.attacker = bf.attacker;
        f.defender = bf.defender;
        f.facedown = static_cast<int>(bf.facedown.size());
        out.push_back(f);
    }
    return out;
}

ResourceFacts ObserverView::resources(PlayerId player) const {
    ResourceFacts r;
    r.pool = state_.player(player).rune_pool;
    for (auto id : resource::runesInBase(state_, player)) {
        if (state_.getObject(id).is_exhausted) ++r.exhausted_runes;
        else ++r.ready_runes;
    }
    for (int k = 0; k < 4; ++k) {
        r.energy[k] = resource::payableEnergy(state_, player, kPurposes[k]);
        r.power_any[k] = resource::payablePower(state_, player, Domain::Count, kPurposes[k]);
    }
    for (int d = 0; d < static_cast<int>(Domain::Count); ++d)
        r.power_by_domain.push_back(
            resource::payablePower(state_, player, static_cast<Domain>(d), SpendFor::Other));
    return r;
}

TimingFacts ObserverView::timing() const {
    TimingFacts t;
    const auto& ts = state_.turn;
    t.phase = ts.phase;
    t.turn_number = ts.turn_number;
    t.turn_player = ts.turn_player;
    t.starting_player = ts.starting_player;
    t.ns = ts.ns_state;
    t.oc = ts.oc_state;
    t.priority_holder = ts.priority_holder;
    t.focus_holder = ts.focus_holder;
    return t;
}

std::vector<DeckModel::PoolCard> ObserverView::opponentCardPool(const DeckModel& model) const {
    return model.unseenPool(state_, opponent(observer_), observer_);
}

std::vector<int64_t> ObserverView::fingerprint() const {
    std::vector<int64_t> out;
    out.reserve(1024);
    auto card = [&](GameObjectId id) -> int64_t {
        return id != kInvalidId && state_.objectExists(id) &&
                       knowledge::identityVisible(state_, id, observer_)
                   ? static_cast<int64_t>(state_.getObject(id).card_def_id.raw()) : -1;
    };
    auto object = [&](GameObjectId id) {
        if (id == kInvalidId || !state_.objectExists(id)) { put(out, -7); return; }
        const auto d = openspiel::visibleObjectDescription(state_, id, observer_);
        put(out, static_cast<int64_t>(d.size()));
        out.insert(out.end(), d.begin(), d.end());
    };
    auto optPlayer = [&](const std::optional<PlayerId>& p) { put(out, p ? rel(*p) : -1); };
    auto location = [&](const std::optional<LocationId>& l) {
        if (!l) { put(out, -1); return; }
        if (const auto* b = std::get_if<BaseLocation>(&*l)) { put(out, 100 + rel(b->player)); return; }
        put(out, static_cast<int64_t>(std::get<BattlefieldLocation>(*l).id.raw()));
    };
    // A list of known cards as rows; `as_set`: the rows sorted by what they
    // say (the list's own order breaks ties between copies by object id).
    auto knowledgeList = [&](const std::vector<CardKnowledge>& ks, bool as_set) {
        std::vector<std::vector<int64_t>> rows;
        for (const auto& k : ks)
            rows.push_back({static_cast<int64_t>(k.card_def_id.raw()),
                            static_cast<int>(k.last_known_zone), k.zone_confirmed,
                            static_cast<int>(k.deck_position), k.deck_rank, k.deck_rank_span});
        if (as_set) std::sort(rows.begin(), rows.end());
        put(out, static_cast<int64_t>(rows.size()));
        for (const auto& r : rows) out.insert(out.end(), r.begin(), r.end());
    };

    // (Section markers, -1001.., make a difference easy to place.)
    // The turn and the timing window.
    put(out, -1001);
    const TimingFacts t = timing();
    put(out, static_cast<int>(t.phase));
    put(out, t.turn_number);
    put(out, rel(t.turn_player));
    put(out, rel(t.starting_player));
    put(out, static_cast<int>(t.ns));
    put(out, static_cast<int>(t.oc));
    optPlayer(t.priority_holder);
    optPlayer(t.focus_holder);
    put(out, state_.effectiveVictoryScore());

    for (PlayerId p : {observer_, opponent(observer_)}) {
        const auto& ps = state_.player(p);
        put(out, -1002);
        put(out, ps.score);
        put(out, ps.xp);
        put(out, handSize(p));
        put(out, deckSize(p));
        put(out, runeDeckSize(p));
        for (ZoneType z : {ZoneType::Trash, ZoneType::Banishment}) {
            const auto& pile = publicPile(p, z);
            put(out, static_cast<int64_t>(pile.size()));
            for (auto id : pile) put(out, card(id));
        }
        put(out, card(ps.champion_zone));
        put(out, card(ps.legend_zone));
        // Resources.
        put(out, -1003);
        const ResourceFacts r = resources(p);
        put(out, r.pool.energy); put(out, r.pool.unit_energy); put(out, r.pool.spell_energy);
        put(out, r.pool.showdown_energy); put(out, r.pool.universal_power);
        put(out, r.pool.spell_power); put(out, r.pool.gear_power);
        for (int d = 0; d < static_cast<int>(Domain::Count); ++d) put(out, r.pool.power[d]);
        put(out, r.ready_runes); put(out, r.exhausted_runes);
        for (int k = 0; k < 4; ++k) { put(out, r.energy[k]); put(out, r.power_any[k]); }
        for (int v : r.power_by_domain) put(out, v);
        // What the observer knows of their hidden zones.
        put(out, -1004);
        knowledgeList(knowledge_.knownHiddenCards(p), true);
        knowledgeList(knowledge_.knownDeckTop(p), false);
        knowledgeList(knowledge_.knownDeckBottom(p), false);
        knowledgeList(knowledge_.knownRuneDeckTop(p), false);
        knowledgeList(knowledge_.knownRuneDeckBottom(p), false);
        for (ZoneType z : {ZoneType::Hand, ZoneType::MainDeck, ZoneType::FacedownZone})
            put(out, knowledge_.unknownCount(p, z));
    }
    // Its own hand and the compositions it knows.
    put(out, -1005);
    for (auto id : ownHand()) put(out, card(id));
    for (const auto& [c, n] : knowledge_.ownDeckComposition()) { put(out, c.raw()); put(out, n); }
    put(out, -2);
    for (const auto& [c, n] : knowledge_.ownRuneDeckComposition()) { put(out, c.raw()); put(out, n); }
    put(out, -2);

    // The board, with the derived facts.
    put(out, -1006);
    for (const auto& b : battlefields()) {
        put(out, static_cast<int64_t>(b.id.raw()));
        put(out, static_cast<int64_t>(b.card.raw()));
        optPlayer(b.controller);
        put(out, b.contested); put(out, rel(b.contested_by));
        put(out, b.showdown_staged); put(out, b.showdown_in_progress);
        put(out, b.combat_staged); put(out, b.combat_in_progress);
        optPlayer(b.attacker); optPlayer(b.defender);
        put(out, b.facedown);
    }
    put(out, -1007);
    {
        // One row per object, the rows as a set: two objects that look the
        // same are told apart only by what they are attached to, and the
        // board's order breaks such ties by object id.
        std::vector<std::vector<int64_t>> rows;
        for (const auto& f : board()) {
            std::vector<int64_t> row;
            std::swap(out, row);
            object(f.id);
            put(out, f.might); put(out, f.keywords);
            put(out, f.assault); put(out, f.shield); put(out, f.deflect);
            put(out, f.damage); put(out, f.lethal);
            put(out, f.exhausted); put(out, f.stunned); put(out, f.buffs); put(out, f.empowered);
            put(out, static_cast<int>(f.designation));
            location(f.location);
            if (f.attached_to != kInvalidId) object(f.attached_to); else put(out, -3);
            std::swap(out, row);
            rows.push_back(std::move(row));
        }
        std::sort(rows.begin(), rows.end());
        for (const auto& r : rows) out.insert(out.end(), r.begin(), r.end());
    }
    // The chain with its links.
    put(out, -1008);
    for (const auto& c : chain()) {
        put(out, c.position); put(out, c.resolving); put(out, rel(c.controller));
        object(c.source);
        put(out, static_cast<int64_t>(c.card.raw()));
        put(out, static_cast<int>(c.kind)); put(out, c.ability_index);
        put(out, static_cast<int>(c.trigger)); put(out, c.finalized);
        put(out, static_cast<int64_t>(c.targets.size()));
        for (auto id : c.targets) object(id);
        put(out, static_cast<int64_t>(c.chosen.size()));
        for (auto id : c.chosen) object(id);
        put(out, c.chosen_item);
        if (c.subject != kInvalidId) object(c.subject); else put(out, -3);
    }
    return out;
}

}  // namespace riftbound
