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

class JayceHammerInHand : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "When I become ready, choose one to give me this turn — [Assault 2] /
    // [Deflect 2] / [Ganking]". (The three keywords are granted options, not
    // printed base keywords, so def_ does not set them.)
    // (WhenIBecomeReady covers both effect readies and the Awaken phase.)
    TriggerType triggerType() const override { return TriggerType::WhenIBecomeReady; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto on_board = [&]() { return ctx.state.getObject(ctx.source).location.has_value(); };
        static const std::vector<std::string> kModes = {"Assault 2", "Deflect 2", "Ganking"};
        // Off the board there is no keyword to give: no legal mode. No return
        // ahead of pickMode — it stores "no mode" on the chain item, so the
        // resolution asks nothing if I am on the board again by then.
        int mode = pickMode(ctx, "Jayce, Hammer in Hand: choose a keyword", 3, kModes,
                            on_board() ? 0x7u : 0u);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (mode < 0 || !on_board()) return;          // waiting / no mode / I left the board
        switch (mode) {
        case 0: ctx.executor.giveTemporaryKeyword(ctx.source, Keyword::Assault, 2); break;
        case 1: ctx.executor.giveTemporaryKeyword(ctx.source, Keyword::Deflect, 2); break;
        default: ctx.executor.giveTemporaryKeyword(ctx.source, Keyword::Ganking, 0); break;
        }
        ctx.events.logTrace("JAYCE, HAMMER IN HAND: became ready -> [" + kModes[mode] + "] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 875;
        d.def_id = R"RB(ven-088-166)RB";
        d.name = R"RB(Jayce, Hammer in Hand)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-088/166)RB";
        d.collector_number = 88;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Jayce)RB", R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I become ready, choose one to give me this turn —
[Assault 2] (+2 [M] while I'm an attacker.)[Deflect 2] (Opponents must pay [A][A] to choose me with a spell or ability.)[Ganking] (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2dcecc0aab2b62e48388177cc27d220fd70b8a78-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_875(CardRegistry& r) {
    r.registerCard(875, std::make_unique<JayceHammerInHand>());
}

}  // namespace riftbound
