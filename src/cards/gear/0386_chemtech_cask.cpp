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

class ChemtechCask : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play a spell on an opponent's turn, you may exhaust me to
    // play a Gold gear token exhausted."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Optional + cost = exhaust self. Legal only on the OPPONENT's turn
        // and while I'm ready (so I can pay the exhaust cost).
        auto still_legal = [&]() -> bool {
            if (!ctx.state.objectExists(ctx.source)) return false;
            const auto& self = ctx.state.getObject(ctx.source);
            if (self.is_exhausted) return false;
            return ctx.state.turn.turn_player != ctx.controller;
        };
        int conf = confirmOptional(ctx, "Chemtech Cask: exhaust to make a Gold token?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf == 0) return;   // declined / not legal
        ctx.executor.exhaustObject(ctx.source);  // pay the cost
        createGoldExhausted(ctx);
        ctx.events.logTrace("CHEMTECH CASK: exhausted -> Gold gear token (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 386;
        d.def_id = R"RB(sfd-063-221)RB";
        d.name = R"RB(Chemtech Cask)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-063/221)RB";
        d.collector_number = 63;
        d.artist = R"RB(Six More Vodka & 黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 1;
        d.ability_text = R"RB(When you play a spell on an opponent's turn, you may exhaust me to play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/39071feea6d3739a834a83b1841f496bdfc3a59a-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_386(CardRegistry& r) {
    r.registerCard(386, std::make_unique<ChemtechCask>());
}

} // namespace riftbound
