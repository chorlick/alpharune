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

class DecreeOfUnity : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Kill an enemy Chaos ([P]) unit or gear."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_enemy = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.location.has_value()) continue;
            if (!obj.isUnit() && !obj.isGear()) continue;
            if (obj.controller == controller || obj.untargetable_by_enemy) continue;
            if (std::find(obj.domains.begin(), obj.domains.end(), Domain::Chaos) ==
                obj.domains.end()) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId picked = pickTarget(ctx, "Decree of Unity",
                                         enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // An attached Equipment is unattached first so its bearer's Might and
        // attachment list don't keep a dead gear.
        if (ctx.state.getObject(picked).isGear()) ctx.executor.unattachGear(picked);
        ctx.executor.killObject(picked);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 918;
        d.def_id = R"RB(ven-131-166)RB";
        d.name = R"RB(Decree of Unity)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-131/166)RB";
        d.collector_number = 131;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Kill an enemy Chaos ([P]) unit or gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/220161ac48f1307e4d74c372d549fea18829a86b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_918(CardRegistry& r) {
    r.registerCard(918, std::make_unique<DecreeOfUnity>());
}

}  // namespace riftbound
