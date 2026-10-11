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

class DariusExecutioner_243 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.player(ctx.controller).cards_played_this_turn < 2) return;
        ctx.executor.readyObject(ctx.source);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "Other friendly units have +1 [M] here." — every OTHER friendly unit at
    // the battlefield I'm at.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            auto my_bf = self.battlefieldId();
            if (!my_bf) return;  // "here" — only while I'm at a battlefield
            for (auto& [uid, u] : state.objects) {
                if (uid == sid) continue;  // "other"
                if (!u.isUnit() || u.controller != controller) continue;
                if (u.battlefieldId() != my_bf) continue;
                auraMight(u, sid, 1);
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 243;
        d.def_id = R"RB(ogn-243-298)RB";
        d.name = R"RB(Darius, Executioner)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-243/298)RB";
        d.collector_number = 243;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Trifarian)RB", R"RB(Darius)RB", R"RB(Noxus)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion] — When you play me, ready me. (Get the effect if you've played another card this turn)
Other friendly units have +1 [M] here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b71cf13a07074a6eccbe88ae6c74133d989cb68-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_243(CardRegistry& r) {
    r.registerCard(243, std::make_unique<DariusExecutioner_243>());
}

} // namespace riftbound
