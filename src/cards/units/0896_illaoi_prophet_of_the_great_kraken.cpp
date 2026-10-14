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

class IllaoiProphetOfTheGreatKraken : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // "When you play me or when I score, play a [1] [M] Tentacle unit token
    //  from Bilgewater." Scoring = conquering or holding.
    // No location is printed: the token goes to the controller's base or a
    // battlefield they control, their choice (CR 355.2.a).
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer, TriggerType::WhenIHold};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const int dest = pickTokenDest(*this, ctx, "Illaoi: play the Tentacle to");
        if (dest < 0) return;   // waiting for the agent (or nowhere to play it)
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Tentacle", 1,
                                 // CR 187.10: 1 Might and the Bilgewater tag only.
                                 {"Bilgewater"}, KeywordSet{},
                                 destLocation(ctx.controller, std::max(0, dest)),
                                 /*enter_ready=*/false,
                                 tokenPlayLocations(ctx));
        RIFT_TRACE(ctx.events, "ILLAOI, PROPHET: 1[M] Tentacle token");
    }
    // "I have +1 [M] for each token unit you control."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            int tokens = 0;
            for (const auto& [id, obj] : state.objects.onBoard())
                if (obj.isUnit() && obj.isToken() && obj.controller == controller &&
                    obj.location.has_value()) ++tokens;
            if (tokens > 0) auraMight(self, sid, tokens);
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 896;
        d.def_id = R"RB(ven-109-166)RB";
        d.name = R"RB(Illaoi, Prophet of the Great Kraken)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-109/166)RB";
        d.collector_number = 109;
        d.artist = R"RB(Michal Ivan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB", R"RB(Illaoi)RB"};
        d.energy_cost = 6;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me or when I score, play a [1] [M] Tentacle unit token from Bilgewater.
I have +1 [M] for each token unit you control.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/86fccf469e67fb89312e0c860842e2801a8b12e7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_896(CardRegistry& r) {
    r.registerCard(896, std::make_unique<IllaoiProphetOfTheGreatKraken>());
}

}  // namespace riftbound
