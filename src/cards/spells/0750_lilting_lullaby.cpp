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

class LiltingLullaby : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // Lullaby is a pure counter for legality purposes: its lockout is scoped
    // to "Its controller" — the controller of the countered spell. Peek-and-
    // pop: the spell countered is the top of the chain, so a spell under an
    // ability cannot be reached and does not make this playable.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return !state.chain.items.empty() && state.chain.items.back().is_spell;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        const auto& top = ctx.state.chain.items.back();
        if (!top.is_spell) return;
        PlayerId target_controller = top.controller;
        counterChainTop(ctx);
        ctx.state.player(target_controller).cant_play_spells_this_turn = true;
        ctx.events.logTrace("LILTING LULLABY: countered + " +
                             std::string(toString(target_controller)) +
                             " can't play spells this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 750;
        d.def_id = R"RB(unl-190-219)RB";
        d.name = R"RB(Lilting Lullaby)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-190/219)RB";
        d.collector_number = 190;
        d.artist = R"RB(Wild Blue Studio/莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Lillia)RB"};
        d.energy_cost = 2;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter a spell. Its controller can't play spells this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/272b71b493575e38ff8888ec187cd33e54c0eacc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_750(CardRegistry& r) {
    r.registerCard(750, std::make_unique<LiltingLullaby>());
}

} // namespace riftbound
