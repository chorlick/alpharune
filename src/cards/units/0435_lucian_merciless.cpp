#include "cards/card.h"
#include "cards/units/weaponmaster_base.h"
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

// [Weaponmaster] is the shared keyword ability (weaponmaster_base.h).
class LucianMerciless : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer};
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (ctx.firing_trigger == TriggerType::WhenIConquer) {
            if (finalizingChoices(ctx)) return;   // no choice in this branch (CR 383.3)
            firstConquerReady(ctx);
        } else {
            WeaponmasterUnit::onTrigger(ctx, targets);
        }
    }

    void firstConquerReady(CardContext& ctx) {
        auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;   // "ready me": only on the board
        int turn_id = ctx.state.turn.turn_number + 1;  // +1 so 0 = "never"
        int& last = self.card_counters["__lucian_conquer_ready_turn"];
        if (last == turn_id) return;  // already fired this turn
        last = turn_id;
        ctx.executor.readyObject(ctx.source);
        RIFT_TRACE(ctx.events, "LUCIAN MERCILESS: first conquer this turn -> ready me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 435;
        d.def_id = R"RB(sfd-113-221)RB";
        d.name = R"RB(Lucian, Merciless)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-113/221)RB";
        d.collector_number = 113;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Lucian)RB", R"RB(Demacia)RB", R"RB(Sentinel)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)
The first time I conquer each turn, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bc192b6e22e7a277c53f035809c59db4548a0fd5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_435(CardRegistry& r) {
    r.registerCard(435, std::make_unique<LucianMerciless>());
}

} // namespace riftbound
