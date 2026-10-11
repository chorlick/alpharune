#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class ExperimentalHexplate : public SimpleEquipGear {
public:
    ExperimentalHexplate() : SimpleEquipGear(Domain::Mind) {}
    const CardDef& def() const override { return def_; }
    // Equipped: "I am a Mech." The bearer has the Mech tag for as long as this
    // is attached. The tag is put on as the gear is attached (so "your Mechs"
    // effects see it at once), re-asserted every aura pass for attach paths
    // that do not go through onEquip (Weaponmaster, Grandmaster at Arms), and
    // taken off again in onUnattached — only if this gear is what added it:
    // card_counters[kGaveMech] on the gear holds that bearer's id + 1.
    void onEquipped(CardContext& ctx, GameObjectId /*unit*/) override {
        grantMech(ctx.state, ctx.source);
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [gid, gear] : state.objects.onBoard()) {
            if (gear.card_def_id != cardDefId() || gear.controller != controller) continue;
            if (!gear.location.has_value()) continue;
            grantMech(state, gid);
        }
    }
    void onUnattached(CardContext& ctx, GameObjectId unit) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& gear = ctx.state.getObject(ctx.source);
        auto it = gear.card_counters.find(kGaveMech);
        if (it == gear.card_counters.end()) return;
        const bool mine = it->second == static_cast<int>(unit) + 1;
        gear.card_counters.erase(it);
        if (!mine || !ctx.state.objectExists(unit)) return;
        auto& tags = ctx.state.getObject(unit).tags;
        tags.erase(std::remove(tags.begin(), tags.end(), std::string("Mech")), tags.end());
    }
private:
    static constexpr const char* kGaveMech = "__hexplate_gave_mech";
    static void grantMech(GameState& state, GameObjectId gear_id) {
        auto& gear = state.getObject(gear_id);
        if (!gear.attached_to.has_value() || !state.objectExists(*gear.attached_to)) return;
        auto& bearer = state.getObject(*gear.attached_to);
        if (!bearer.isUnit() || !bearer.location.has_value()) return;
        if (hasTag(bearer, "Mech")) return;   // already a Mech (printed, or another Hexplate)
        bearer.tags.push_back("Mech");
        gear.card_counters[kGaveMech] = static_cast<int>(*gear.attached_to) + 1;
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 396;
        d.def_id = R"RB(sfd-073-221)RB";
        d.name = R"RB(Experimental Hexplate)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-073/221)RB";
        d.collector_number = 73;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.might_bonus = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [B] ([B]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(I am a Mech.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/940a5e9796d4ac10a238d9b2612545c04bd9a570-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_396(CardRegistry& r) {
    r.registerCard(396, std::make_unique<ExperimentalHexplate>());
}

} // namespace riftbound
