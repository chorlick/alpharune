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

class SpiritSRefuge : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play this, buff a friendly unit." Its controller picks the
    // unit as the ability is finalized (CR 383.3); a trigger is never handed
    // targets.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        const auto friendly = boardObjects(ctx.state, [me](const GameObject& o) {
            return o.isUnit() && o.controller == me;
        });
        const GameObjectId t = pickTarget(ctx, "Spirit's Refuge: buff a friendly unit", friendly);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.buffUnit(t);
    }
    // "Friendly buffed units have [Deflect] if they didn't already."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Active only while an on-board instance exists. One instance is
        // enough and more add nothing.
        GameObjectId self_id = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            self_id = id;
            break;
        }
        if (self_id == kInvalidId) return;
        for (auto& [uid, u] : state.objects) {
            if (!u.isUnit() || u.controller != controller || !u.location.has_value())
                continue;
            if (u.buff_count <= 0) continue;  // "buffed" = has a +1 [M] buff
            // "if they didn't already": this hook cannot know — another
            // card's hook may add [Deflect] after it ran (Allay, Jax; Fiora
            // in the later Might-dependent hook) — so the grant is marked
            // "only if absent" and the engine drops it, once every hook of
            // the pass has run, from a unit that has [Deflect] printed, "this
            // turn", from an Equipment or from any other effect. The unit
            // then costs the [A] its own [Deflect] says, never one more.
            auraKeywordIfAbsent(u, self_id, Keyword::Deflect, 1);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 63;
        d.def_id = R"RB(ogn-063-298)RB";
        d.name = R"RB(Spirit's Refuge)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-063/298)RB";
        d.collector_number = 63;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play this, buff a friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff.)
Friendly buffed units have [Deflect] if they didn't already. (Opponents must pay [A] to choose those units with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/26d5e54580d1f17ed6c35a66ac6a90ce56c99ec8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_63(CardRegistry& r) {
    r.registerCard(63, std::make_unique<SpiritSRefuge>());
}

} // namespace riftbound
