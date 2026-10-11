#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class ResonatingStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a battlefield you control and a unit you control at a different
    //  location. Move that unit to that battlefield and give it +2 [M] this turn."
    // The battlefield, then a unit that is not there, are chosen as the spell
    // is played (CR 355). Played from facedown, the battlefield is the one it
    // was hidden at (CR 811.1.d.2); the unit, which by its own restriction
    // cannot be there, is chosen freely (811.1.d.4).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    static bool controls(const BattlefieldState& bf, PlayerId p) {
        return bf.controller.has_value() && *bf.controller == p;
    }
    /// Units `p` controls that are somewhere other than battlefield `bf`.
    static std::vector<GameObjectId> unitsElsewhere(const GameState& state, PlayerId p,
                                                    BattlefieldId bf) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != p || !obj.location.has_value()) continue;
            auto at = obj.battlefieldId();
            if (at.has_value() && *at == bf) continue;
            out.push_back(id);
        }
        return out;
    }
    // The unit it chooses: one I control that is somewhere other than a
    // battlefield I control (the play gate's view of the pick below).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        for (const auto& bf : state.battlefields) {
            if (!controls(bf, controller)) continue;
            const auto units = unitsElsewhere(state, controller, bf.id);
            if (std::find(units.begin(), units.end(), object) != units.end())
                return FinalizeChoice::Yes;
        }
        return FinalizeChoice::No;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (const auto& bf : state.battlefields)
            if (controls(bf, controller) && !unitsElsewhere(state, controller, bf.id).empty())
                return true;
        return false;
    }
    // Played from facedown, the battlefield is the one it was hidden at: I
    // must control it and have a unit somewhere else.
    bool hasLegalChoicesHiddenAt(const GameState& state, PlayerId controller, const CardDB&,
                                 BattlefieldId battlefield) const override {
        for (const auto& bf : state.battlefields)
            if (bf.id == battlefield)
                return controls(bf, controller) &&
                       !unitsElsewhere(state, controller, bf.id).empty();
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& bfs = ctx.state.battlefields;
        const int n = static_cast<int>(std::min<size_t>(bfs.size(), 32));
        const auto hidden_at = facedownPlayBattlefield(ctx.state, ctx.source);
        uint32_t mask = 0;
        std::vector<std::string> labels;
        for (int i = 0; i < n; ++i) {
            std::string name = "BF#" + std::to_string(static_cast<int>(bfs[i].id));
            if (ctx.state.objectExists(bfs[i].card_object_id))
                name = ctx.state.getObject(bfs[i].card_object_id).name;
            labels.push_back(name);
            if (hidden_at.has_value() && *hidden_at != bfs[i].id) continue;
            if (controls(bfs[i], ctx.controller) &&
                !unitsElsewhere(ctx.state, ctx.controller, bfs[i].id).empty())
                mask |= (1u << i);
        }
        int mode = pickMode(ctx, "Resonating Strike: battlefield", n, labels, mask);
        if (mode < 0 || mode >= n) return;   // -1 waiting, -2 nothing legal
        const BattlefieldId dest = bfs[mode].id;
        GameObjectId unit = pickTarget(ctx, "Resonating Strike: unit to move",
                                       unitsElsewhere(ctx.state, ctx.controller, dest));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        ctx.executor.moveToBattlefield(unit, dest);
        ctx.executor.giveTemporaryMight(unit, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 821;
        d.def_id = R"RB(ven-034-166)RB";
        d.name = R"RB(Resonating Strike)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-034/166)RB";
        d.collector_number = 34;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hidden);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
[Reaction] (Play on your turn or in showdowns.)
Choose a battlefield you control and a unit you control at a different location. Move that unit to that battlefield and give it +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1a625de128019d65d41dd0af4f8d189d5de530a5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_821(CardRegistry& r) {
    r.registerCard(821, std::make_unique<ResonatingStrike>());
}

}  // namespace riftbound
