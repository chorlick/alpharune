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
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .energy = 0, .power = 1, .power_domain = Domain::Calm};
    }

    // "If this was attached to me this turn, I have an additional +2 [M]."
    // However it got attached — Equip, or for free by another card (Rell,
    // Jax, Forge of the Fluft) — so the attachment is noticed here, in the
    // aura pass that follows it, rather than stamped by onEquip: each
    // Brutalizer remembers its bearer and the turn that bearer changed.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        static const char* const kBearer = "__brutalizer_bearer";
        static const char* const kTurn = "__brutalizer_attach_turn";
        const int this_turn = state.turn.turn_number + 1;
        // (Every object: finds its own instances by card id; one off the board has its counters cleared.)
        for (auto& [gid, gear] : state.objects) {
            if (gear.card_def_id != cardDefId() || gear.controller != controller) continue;
            if (!gear.location.has_value() || !gear.attached_to.has_value()) {
                gear.card_counters.erase(kBearer);
                gear.card_counters.erase(kTurn);
                continue;
            }
            const GameObjectId unit_id = *gear.attached_to;
            if (gear.card_counters[kBearer] != static_cast<int>(unit_id) + 1) {
                gear.card_counters[kBearer] = static_cast<int>(unit_id) + 1;
                gear.card_counters[kTurn] = this_turn;
            }
            if (gear.card_counters[kTurn] != this_turn) continue;  // not attached this turn
            if (!state.objectExists(unit_id)) continue;
            auto& unit = state.getObject(unit_id);
            if (!unit.isUnit() || !unit.location.has_value()) continue;
            auraMight(unit, gid, 2);
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
