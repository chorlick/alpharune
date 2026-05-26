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

class Brutalizer : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        bool ok = standardEquip(ctx, ctx.source, unit, /*energy=*/0, Domain::Calm);
        if (ok && ctx.state.objectExists(ctx.source)) {
            ctx.state.getObject(ctx.source).card_counters["__brutalizer_attach_turn"] =
                ctx.state.turn.turn_number;
        }
        return ok;
    }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [gid, gear] : state.objects) {
            if (gear.card_def_id != cardDefId()) continue;
            if (gear.controller != controller || !gear.attached_to.has_value()) continue;
            auto it = gear.card_counters.find("__brutalizer_attach_turn");
            if (it == gear.card_counters.end()) continue;
            if (it->second != state.turn.turn_number) continue;  // not attached this turn
            GameObjectId unit_id = *gear.attached_to;
            if (!state.objectExists(unit_id)) continue;
            GameObject::AuraEffect ae;
            ae.source = gid;
            ae.might_bonus = 2;
            state.getObject(unit_id).aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 365;
        d.def_id = R"RB(sfd-042-221)RB";
        d.name = R"RB(Brutalizer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-042/221)RB";
        d.collector_number = 42;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.might_bonus = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [G] ([G]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(If this was attached to me this turn, I have an additional +2 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3a0de7eec3de501f79f33c09b43c7fe42721d10d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_365(CardRegistry& r) {
    r.registerCard(365, std::make_unique<Brutalizer>());
}

} // namespace riftbound
