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

// "[1][B]: Draw 1.
//  [4][B][B][B][B], [E]: Score 1 point.
//  Use my abilities only while I'm at a battlefield." ([B] = Mind power.)

class RenataGlascMastermind : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (obj.isAtBattlefield()) return true;
        }
        return false;
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {
            // [1][B]: Draw 1.
            { .cost = {.energy = 1, .power = 1, .power_domain = Domain::Mind},
              .targets = {}, .is_action = false, .is_reaction = false },
            // [4][B][B][B][B], [E]: Score 1 point.
            { .cost = {.exhaust = true, .energy = 4, .power = 4, .power_domain = Domain::Mind},
              .targets = {}, .is_action = false, .is_reaction = false },
        };
    }
    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (ability_index == 0) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("RENATA MASTERMIND: [1][B] -> draw 1");
        } else {
            ctx.state.player(ctx.controller).score += 1;
            ctx.events.logTrace("RENATA MASTERMIND: [4][B][B][B][B],[E] -> score 1 point");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 410;
        d.def_id = R"RB(sfd-088-221)RB";
        d.name = R"RB(Renata Glasc, Mastermind)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-088/221)RB";
        d.collector_number = 88;
        d.artist = R"RB(League of Legends)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Renata Glasc)RB", R"RB(Zaun)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB([1][B]: Draw 1.
[4][B][B][B][B], [E]: Score 1 point.
Use my abilities only while I'm at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/be8f7e08562c076e8947aafc3ecd202051d17c02-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_410(CardRegistry& r) {
    r.registerCard(410, std::make_unique<RenataGlascMastermind>());
}

} // namespace riftbound
