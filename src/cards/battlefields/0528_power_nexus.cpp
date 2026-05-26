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

// "When you hold here, you may pay [A][A][A][A] to score 1 point."

class PowerNexus : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        int conf = confirmOptional(ctx, "Power Nexus: pay [A][A][A][A] to score 1?",
                                   [&]() { return ps.rune_pool.totalPower() >= 4; });
        if (conf != 1) return;
        if (ps.rune_pool.totalPower() < 4) return;
        for (int i = 0; i < 4; ++i) spendOnePower(ps);
        ps.score++;
        ctx.events.logTrace("POWER NEXUS: paid [A][A][A][A] -> score 1 point");
    }
private:
    static void spendOnePower(PlayerState& ps) {
        if (ps.rune_pool.universal_power > 0) { ps.rune_pool.universal_power--; return; }
        for (int d = 0; d < static_cast<int>(Domain::Count); ++d) {
            if (ps.rune_pool.power[d] > 0) { ps.rune_pool.power[d]--; return; }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 528;
        d.def_id = R"RB(sfd-214-221)RB";
        d.name = R"RB(Power Nexus)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-214/221)RB";
        d.collector_number = 214;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, you may pay [A][A][A][A] to score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/653db9a5f2aead0191c8d61da3d1876fb894b545-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_528(CardRegistry& r) {
    r.registerCard(528, std::make_unique<PowerNexus>());
}

} // namespace riftbound
