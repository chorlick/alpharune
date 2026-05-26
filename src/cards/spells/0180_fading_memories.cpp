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

class FadingMemories : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    bool needsPlayTimeTarget() const override { return true; }

    static std::vector<GameObjectId> legalTargets(const GameState& state) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value()) continue;
            if (obj.isUnit() && obj.isAtBattlefield()) {
                out.push_back(id);
            } else if (obj.isGear()) {
                out.push_back(id);
            }
        }
        return out;
    }

    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/) const override {
        return legalTargets(state);
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            picked = pickTarget(ctx, "Fading Memories: give [Temporary]",
                                 legalTargets(ctx.state));
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& obj = ctx.state.getObject(picked);
        obj.keywords.set(Keyword::Temporary);
        ctx.events.logTrace("FADING MEMORIES: " + obj.name + " gains [Temporary]");
        // The start-of-Beginning-Phase Temporary sweep (game_engine.cpp
        // beginningStep) now kills both units AND gear, so a gear given
        // [Temporary] here is auto-killed via EffectExecutor::killObject.
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 180;
        d.def_id = R"RB(ogn-180-298)RB";
        d.name = R"RB(Fading Memories)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-180/298)RB";
        d.collector_number = 180;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(Give a unit at a battlefield or a gear [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a2b366942f1a7ec1f4bd3c1b17e10356214705d7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_180(CardRegistry& r) {
    r.registerCard(180, std::make_unique<FadingMemories>());
}

} // namespace riftbound
