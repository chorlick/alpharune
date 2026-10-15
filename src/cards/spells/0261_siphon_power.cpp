#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class SiphonPower : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a battlefield. Give friendly units there +1 [M] this turn and
    //  enemy units there -1 [M] this turn, to a minimum of 1 [M]."
    // The battlefield is the target (CR 355.10.b / 355.10.d: "Kill all units
    // at a battlefield" targets a battlefield, not the units), chosen as the
    // spell is played (CR 355.5), in its finalize pass before its cost. So a
    // battlefield with no unit can be chosen, no unit's [Deflect] is owed,
    // and units that come or go in response are the ones there as it
    // resolves (testing phase R8: a unit stood for the battlefield).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // a battlefield, not a unit
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const int mode = pickBattlefield(*this, ctx, "Siphon Power: choose a battlefield");
        if (mode == -1) return;               // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (mode < 0 || static_cast<size_t>(mode) >= ctx.state.battlefields.size()) return;
        const BattlefieldId bf = ctx.state.battlefields[static_cast<size_t>(mode)].id;
        std::vector<GameObjectId> there;
        for (auto& [id, obj] : ctx.state.objects.onBoard())
            if (obj.isUnit() && obj.battlefieldId() == std::optional<BattlefieldId>{bf})
                there.push_back(id);
        std::sort(there.begin(), there.end());
        for (auto id : there) {
            if (ctx.state.getObject(id).controller == ctx.controller)
                ctx.executor.giveTemporaryMight(id, 1);
            else
                ctx.executor.giveTemporaryMight(id, -1, /*minimum=*/1);
        }
        RIFT_TRACE(ctx.events, "SIPHON POWER: friendly +1 / enemy -1 (min 1) at one BF");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 261;
        d.def_id = R"RB(ogn-266-298)RB";
        d.name = R"RB(Siphon Power)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-266/298)RB";
        d.collector_number = 266;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Viktor)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a battlefield. Give friendly units there +1 [M] this turn and enemy units there -1 [M] this turn, to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b449170ba312711c82708d1fea2b044822ce5eaa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_261(CardRegistry& r) {
    r.registerCard(261, std::make_unique<SiphonPower>());
}

} // namespace riftbound
