#include "rules/deck_model.h"

#include "engine/knowledge_tracker.h"

#include <algorithm>

namespace riftbound {

bool isMainDeckType(const CardDef& d) {
    return d.card_type == CardType::Unit || d.card_type == CardType::Spell ||
           d.card_type == CardType::Gear;
}

int CopyCounts::remaining(const CardDef& d) const {
    if (d.super_type == SuperType::Signature && signatures >= kSignatureCards) return 0;
    if (d.unlimited_copies) return -1;
    const int cap = d.keywords.has(Keyword::Unique) ? 1 : kCopiesPerName;
    auto it = by_name.find(d.name);
    return std::max(0, cap - (it == by_name.end() ? 0 : it->second));
}

void CopyCounts::add(const CardDef& d) {
    if (d.super_type == SuperType::Signature) ++signatures;
    if (!d.unlimited_copies) ++by_name[d.name];
}

DeckModel::DeckModel(const CardDB& db) : db_(db), builder_(db) {
    std::vector<const CardDef*> defs;
    for (const auto& [id, d] : db.all()) defs.push_back(&d);
    std::sort(defs.begin(), defs.end(),
              [](const CardDef* a, const CardDef* b) { return a->id < b->id; });

    auto fillHidden = [&](LegendPool& pool) {
        for (CardDefId id : pool.cards)
            if (db_.get(id).keywords.has(Keyword::Hidden)) pool.hidden.push_back(id);
    };
    for (const CardDef* d : defs) {
        if (!d->banned && !d->isTokenCard() && isMainDeckType(*d)) any_.cards.push_back(d->id);
    }
    std::vector<CardDefId> all_runes;
    for (const CardDef* d : defs) {
        if (!d->banned && !d->isTokenCard() && d->card_type == CardType::Rune)
            all_runes.push_back(d->id);
    }
    if (!all_runes.empty()) any_.runes.push_back(all_runes);
    fillHidden(any_);

    for (const CardDef* d : defs) {
        if (d->card_type != CardType::Legend) continue;
        const RandomDeckBuilder::LegendCards* cards = builder_.legendCards(d->id);
        if (!cards) continue;
        LegendPool pool;
        pool.found = true;
        pool.cards = cards->main;
        pool.cards.insert(pool.cards.end(), cards->signatures.begin(), cards->signatures.end());
        std::sort(pool.cards.begin(), pool.cards.end());
        pool.runes = cards->runes;
        fillHidden(pool);
        pools_.emplace(d->id, std::move(pool));
    }
}

const CardDef* DeckModel::def(CardDefId id) const {
    auto it = db_.all().find(id);
    return it == db_.all().end() ? nullptr : &it->second;
}

const DeckModel::LegendPool& DeckModel::poolFor(CardDefId legend) const {
    auto it = pools_.find(legend);
    return it == pools_.end() ? any_ : it->second;
}

CardDefId DeckModel::legendOf(const GameState& state, PlayerId owner) {
    const auto& ps = state.player(owner);
    if (ps.legend_zone != kInvalidId && state.objectExists(ps.legend_zone))
        return state.getObject(ps.legend_zone).card_def_id;
    return kInvalidId;
}

CopyCounts DeckModel::seenCounts(const GameState& state, PlayerId owner,
                                 PlayerId observer) const {
    const knowledge::GameKnowledgeView view(state, observer);
    CopyCounts counts;
    std::vector<GameObjectId> ids;
    for (const auto& [id, obj] : state.objects)
        if (obj.owner == owner && !obj.is_token) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (auto id : ids) {
        const CardKnowledge k = view.about(id);
        if (!k.identity_known) continue;
        const CardDef* d = def(k.card_def_id);
        if (d && isMainDeckType(*d)) counts.add(*d);
    }
    return counts;
}

std::vector<DeckModel::PoolCard> DeckModel::unseenPool(const GameState& state, PlayerId owner,
                                                       PlayerId observer) const {
    const LegendPool& pool = poolFor(legendOf(state, owner));
    const CopyCounts seen = seenCounts(state, owner, observer);
    std::vector<PoolCard> out;
    for (CardDefId id : pool.cards) {
        const CardDef* d = def(id);
        if (!d) continue;
        const int left = seen.remaining(*d);
        if (left != 0) out.push_back({id, left});
    }
    return out;
}

}  // namespace riftbound
