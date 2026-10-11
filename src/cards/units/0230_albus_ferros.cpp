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

class AlbusFerros : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, spend any number of buffs. For each buff spent,
    // channel 1 rune exhausted." Spending is what the ability does as it
    // resolves (it is not a cost paid in advance), so the buffs are chosen
    // then, one unit at a time, and the runes follow the buffs actually spent.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // One entry per buff: a unit that holds several can be picked for each.
        std::vector<GameObjectId> buffs;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != ctx.controller || !obj.location.has_value())
                continue;
            for (int i = 0; i < obj.buff_count; ++i) buffs.push_back(id);
        }
        // "any number": as many as there are buffs.
        const auto chosen = selectSeveral(ctx, "Albus Ferros: spend a buff from", buffs);
        if (!chosen.has_value()) return;   // waiting for the agent
        int spent = 0;
        for (auto id : *chosen) {
            auto& obj = ctx.state.getObject(id);
            if (!obj.location.has_value() || obj.buff_count <= 0) continue;
            ctx.executor.spendBuff(id, ctx.controller);   // CR 427.2; "When you spend a buff"
            ++spent;
        }
        if (spent <= 0) return;
        ctx.executor.channelRunes(ctx.controller, spent, /*enter_exhausted=*/true);
        RIFT_TRACE(ctx.events, "ALBUS FERROS: spend " + std::to_string(spent) +
                            " buffs -> channel " + std::to_string(spent) + " runes exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 230;
        d.def_id = R"RB(ogn-230-298)RB";
        d.name = R"RB(Albus Ferros)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-230/298)RB";
        d.collector_number = 230;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, spend any number of buffs. For each buff spent, channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ec0be8d2a79196b689fccf9ee42ce8baa7e9c35e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_230(CardRegistry& r) {
    r.registerCard(230, std::make_unique<AlbusFerros>());
}

} // namespace riftbound
