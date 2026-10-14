#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class EmperorSDivide : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move any number of friendly units at a battlefield to their base."
    // The units (zero allowed, CR 355.13) are chosen as the spell is played,
    // before its cost and before anyone responds (CR 355.5, 355.15; testing
    // phase G17): the first one names the battlefield, the rest are at that
    // battlefield. Played from facedown, only units at its battlefield (CR
    // 811.1.d.2): with none there it is played choosing none (judge ruling
    // 2026-10-11, plans/09 9.Z4: CR 355.13 wins over 811.1.d).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object, [&](const GameObject& u) {
            return u.isAtBattlefield() && effectCanMoveToBase(state, controller, u);
        });
    }
    // Only a unit this spell can move to its base (not one that "can't move to
    // base", not at "Units can't move from here to base": effectCanMoveToBase).
    static std::vector<GameObjectId> movable(const CardContext& ctx,
                                             std::optional<BattlefieldId> at) {
        auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            return o.controller == ctx.controller && o.isAtBattlefield() &&
                   (!at.has_value() || o.battlefieldId() == at) &&
                   effectCanMoveToBase(ctx.state, ctx.controller, o);
        });
        return atHiddenBattlefield(ctx.state, ctx.source, std::move(out));
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The first unit (or none): it names the battlefield.
        const auto first = selectSeveralTargets(*this, ctx, "Emperor's Divide: move a unit to base",
                                         movable(ctx, std::nullopt), /*max_picks=*/1, kFirstSlot);
        if (!first.has_value()) return;          // waiting for the agent
        std::vector<GameObjectId> units = *first;
        if (!units.empty()) {
            const auto bf = ctx.state.getObject(units.front()).battlefieldId();
            std::vector<GameObjectId> rest;
            for (auto id : movable(ctx, bf))
                if (id != units.front()) rest.push_back(id);
            const auto more = selectSeveralTargets(*this, ctx,
                                                   "Emperor's Divide: move another unit there to base",
                                            rest, /*max_picks=*/-1, kRestSlot);
            if (!more.has_value()) return;
            units.insert(units.end(), more->begin(), more->end());
        }
        if (finalizingChoices(ctx)) return;      // choices end here (CR 355)
        for (auto uid : units) {
            if (!ctx.state.objectExists(uid)) continue;
            ctx.executor.moveToBase(uid);
            if (ctx.state.getObject(uid).isAtBase())
                RIFT_TRACE(ctx.events, "EMPEROR'S DIVIDE: moved " +
                                    ctx.state.getObject(uid).name + " to base");
        }
    }
    static constexpr size_t kFirstSlot = Card::kChoiceStateSlot + 1;   // 7, 8
    static constexpr size_t kRestSlot = Card::kChoiceStateSlot + 3;    // 9, 10, ...
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 366;
        d.def_id = R"RB(sfd-043-221)RB";
        d.name = R"RB(Emperor's Divide)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-043/221)RB";
        d.collector_number = 43;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Move any number of friendly units at a battlefield to their base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/80f367f06549d597c7b5a77be73eb647eab34f59-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_366(CardRegistry& r) {
    r.registerCard(366, std::make_unique<EmperorSDivide>());
}

} // namespace riftbound
