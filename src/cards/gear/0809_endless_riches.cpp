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

class EndlessRiches : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "When you play this, banish your hand and trash, then [Burn 7]."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto banishAll = [&](std::vector<GameObjectId>& zone) {
            std::vector<GameObjectId> cards;
            cards.swap(zone);
            for (auto id : cards) {
                if (!ctx.state.objectExists(id)) continue;
                auto& card = ctx.state.getObject(id);
                card.zone = ZoneType::Banishment;
                card.location = std::nullopt;
                auto& owner = ctx.state.player(card.owner);
                owner.banishment.push_back(id);
                owner.cards_banished_this_turn++;
                RIFT_TRACE(ctx.events, "ENDLESS RICHES: banished " + card.name);
                // "When you banish a card you own" watchers.
                ctx.events.emit(ObjectStateChangedEvent{id, "banished_owned"});
            }
        };
        banishAll(ps.hand);
        banishAll(ps.trash);
        ctx.executor.burn(ctx.controller, 7);
    }
    // "Skip your Draw Phase." / "You may play cards from your trash." / "If a
    // card would go to your trash from anywhere other than your Main Deck,
    // banish it instead." - continuous flags, re-asserted every aura pass.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject&) {
            auto& ps = state.player(controller);
            ps.skip_draw_phase = true;
            ps.may_play_from_trash = true;
            ps.trash_goes_to_banish = true;
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 809;
        d.def_id = R"RB(ven-022-166)RB";
        d.name = R"RB(Endless Riches)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-022/166)RB";
        d.collector_number = 22;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play this, banish your hand and trash, then [Burn 7].
Skip your Draw Phase.
You may play cards from your trash.
If a card would go to your trash from anywhere other than your Main Deck, banish it instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e2c33bf55162086acd458775a5db3d976f137b61-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_809(CardRegistry& r) {
    r.registerCard(809, std::make_unique<EndlessRiches>());
}

}  // namespace riftbound
