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

class Moonfall : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // First battlefield where the controller has a unit.
    std::optional<BattlefieldId> chooseBattlefield(CardContext& ctx) const {
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (bf) return bf;
        }
        return std::nullopt;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto bf = chooseBattlefield(ctx);
        if (!bf) return;  // no battlefield where you have units → fizzle
        PlayerId opp = opponent(ctx.controller);

        // "You may move up to one enemy unit to that battlefield."
        auto find_movable_enemy = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != opp) continue;
                if (!obj.location.has_value()) continue;
                auto ubf = obj.battlefieldId();
                if (ubf && *ubf == *bf) continue;  // already there
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_movable_enemy() != kInvalidId; };
        int conf = confirmOptional(ctx,
            "Moonfall: move an enemy unit to that battlefield?", still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf == 1) {
            GameObjectId mover = find_movable_enemy();
            if (mover != kInvalidId && ctx.state.objectExists(mover)) {
                ctx.executor.moveToBattlefield(mover, *bf);
                ctx.events.logTrace("MOONFALL: moved an enemy unit to the battlefield");
            }
        }

        // "Then give enemy units there -2 [M] this turn."
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            auto ubf = obj.battlefieldId();
            if (!ubf || *ubf != *bf) continue;
            ctx.executor.giveTemporaryMight(id, -2);
        }
        ctx.events.logTrace("MOONFALL: -2 [M] to enemy units at the battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 755;
        d.def_id = R"RB(unl-198-219)RB";
        d.name = R"RB(Moonfall)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-198/219)RB";
        d.collector_number = 198;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Diana)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a battlefield where you have units. You may move up to one enemy unit to that battlefield. Then give enemy units there -2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2eda8d4bbee47857b1916599b5c7d5fa7ca400dc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_755(CardRegistry& r) {
    r.registerCard(755, std::make_unique<Moonfall>());
}

} // namespace riftbound
