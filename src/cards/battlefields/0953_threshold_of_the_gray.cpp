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

class ThresholdOfTheGray : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When combat starts here, the attacker and defender each [Add] [1]."
    TriggerType triggerType() const override { return TriggerType::WhenCombatStartsHere; }
    // "... each [Add] [1]" resolves as it triggers (CR 430.2; D2).
    bool triggeredAbilityAdds(TriggerType) const override { return true; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto here = battlefieldOfCard(ctx.state, ctx.source);
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.id != here) continue;
            if (bf.attacker) ctx.executor.addFloatingEnergy(*bf.attacker, 1);
            if (bf.defender) ctx.executor.addFloatingEnergy(*bf.defender, 1);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 953;
        d.def_id = R"RB(ven-166-166)RB";
        d.name = R"RB(Threshold of the Gray)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-166/166)RB";
        d.collector_number = 166;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When combat starts here, the attacker and defender each [Add] [1].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5521aac82f036f1070326c743a9f653955d67bb9-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_953(CardRegistry& r) {
    r.registerCard(953, std::make_unique<ThresholdOfTheGray>());
}

}  // namespace riftbound
