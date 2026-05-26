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

class EmperorOfTheSands : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Find an on-board instance of this legend controlled by `controller`.
        bool present = false;
        GameObjectId self_id = kInvalidId;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller) continue;
            present = true;
            self_id = sid;
            break;
        }
        if (!present) return;
        // Grant [Weaponmaster] to friendly units tagged "Sand Soldier".
        for (auto& [uid, u] : state.objects) {
            if (!u.isUnit() || u.controller != controller) continue;
            bool is_sand = false;
            for (auto& tag : u.tags) {
                if (tag == "Sand Soldier") { is_sand = true; break; }
            }
            if (!is_sand) continue;
            GameObject::AuraEffect ae;
            ae.source = self_id;
            ae.keyword = Keyword::Weaponmaster;
            u.aura_effects.push_back(ae);
        }
    }

    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1};
    }
    // "Use only if you've played an Equipment this turn."
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        return state.player(controller).equipment_played_this_turn > 0;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sand Soldier",
                                 2, {"Sand Soldier"}, KeywordSet{}, loc, false);
        ctx.events.logTrace("EMPEROR OF THE SANDS: created 2[M] Sand Soldier token");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 512;
        d.def_id = R"RB(sfd-197-221)RB";
        d.name = R"RB(Emperor of the Sands)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-197/221)RB";
        d.collector_number = 197;
        d.artist = R"RB(蛋费鸡丁)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Azir)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB(Your Sand Soldiers have [Weaponmaster].
[1], [E]: Play a 2 [M] Sand Soldier unit token to your base. Use only if you've played an Equipment this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0472274c49f6540858758ebf9bd2f107a601541a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_512(CardRegistry& r) {
    r.registerCard(512, std::make_unique<EmperorOfTheSands>());
}

} // namespace riftbound
