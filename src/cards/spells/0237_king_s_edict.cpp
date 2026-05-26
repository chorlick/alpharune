#include "cards/card.h"
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

class KingSEdict : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Starting with the next player, each other player chooses a unit you don't
    //  control that hasn't been chosen for this spell. Kill those units."
    // In 1v1 this means: the opponent chooses one of their OWN units to be
    // killed. The opponent's choice isn't surfaced through the caster's agent,
    // so we approximate their rational pick (lowest-Might unit they control).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        GameObjectId victim = kInvalidId;
        int best_might = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            // "a unit you don't control" — not controlled by the caster.
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (obj.controller != opp) continue;  // 1v1: opponent's own unit
            if (victim == kInvalidId || obj.current_might < best_might) {
                victim = id;
                best_might = obj.current_might;
            }
        }
        if (victim != kInvalidId) {
            ctx.executor.killObject(victim);
            ctx.events.logTrace("KING'S EDICT: opponent's chosen unit killed");
        }
    }
    // No play-time target: the killed unit is the OPPONENT's choice, not the
    // caster's, so we don't expose a caster-chosen target.
    bool hasLegalTargets(const GameState&, PlayerId) const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 237;
        d.def_id = R"RB(ogn-237-298)RB";
        d.name = R"RB(King's Edict)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-237/298)RB";
        d.collector_number = 237;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Starting with the next player, each other player chooses a unit you don't control that hasn't been chosen for this spell. Kill those units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ec1b8b98e1ca7a34939322883309c71605bb8bc5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_237(CardRegistry& r) {
    r.registerCard(237, std::make_unique<KingSEdict>());
}

} // namespace riftbound
