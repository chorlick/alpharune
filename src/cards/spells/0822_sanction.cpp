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

class Sanction : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose one — Empower a unit. Disempower it at end of turn. /
    //  Disempower a unit that's [Empowered]. Empower it at end of turn."
    // The mode, then a unit that mode can affect, are chosen as the spell is
    // played (CR 355) — never a unit first and a mode that cannot use it.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !chooseableUnits(state, controller,
                                [](const GameObject&) { return true; }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto any = chooseableUnits(ctx.state, ctx.controller,
                                   [](const GameObject&) { return true; });
        auto empowered = chooseableUnits(ctx.state, ctx.controller,
                                         [](const GameObject& o) { return o.isEmpowered(); });
        uint32_t mask = (any.empty() ? 0u : 1u) | (empowered.empty() ? 0u : 2u);
        int mode = pickMode(ctx, "Sanction", 2,
                            {"Empower a unit (disempower it at end of turn)",
                             "Disempower an Empowered unit (empower it at end of turn)"},
                            mask);
        if (mode < 0) return;   // -1 waiting, -2 no legal mode
        GameObjectId picked = pickTarget(ctx, "Sanction", mode == 0 ? any : empowered);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (mode == 0) {
            ctx.executor.empower(picked);
            ctx.state.getObject(picked).empower_flip_eot = -1;
        } else if (ctx.executor.disempower(picked)) {
            ctx.state.getObject(picked).empower_flip_eot = 1;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 822;
        d.def_id = R"RB(ven-035-166)RB";
        d.name = R"RB(Sanction)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-035/166)RB";
        d.collector_number = 35;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose one —
Empower a unit. Disempower it at end of turn.Disempower a unit that's [Empowered]. Empower it at end of turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/daef24792dddefd0971b34d01bd60bc3b11adcca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_822(CardRegistry& r) {
    r.registerCard(822, std::make_unique<Sanction>());
}

}  // namespace riftbound
