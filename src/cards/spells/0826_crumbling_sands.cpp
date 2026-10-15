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

class CrumblingSands : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Counter a spell if an opponent has played another spell this turn."
    // The spell is a target (CR 355.9.a.2), chosen as this is played; the
    // condition is read as it resolves.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    static bool qualifies(const ChainItem& ci) { return ci.is_spell; }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return anyChainItem(state, qualifies);
    }
    /// Has an opponent of `me` played a spell this turn OTHER than `target`?
    /// The target itself counts toward its controller's spells_played_this_turn,
    /// so it needs 2 when it is theirs.
    static bool opponentPlayedAnotherSpell(const GameState& state, PlayerId me,
                                           const ChainItem& target) {
        const int played = state.player(opponent(me)).spells_played_this_turn;
        return played >= (target.controller == me ? 1 : 2);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto spell = pickChainItem(ctx, "Crumbling Sands: counter a spell",
                                         chainItemChoices(ctx, qualifies));
        if (!spell.has_value()) return;
        if (finalizingChoices(ctx)) return;
        const ChainItem* item = *spell != kInvalidId ? chainItem(ctx.state, *spell) : nullptr;
        if (!item) return;
        if (!opponentPlayedAnotherSpell(ctx.state, ctx.controller, *item)) {
            RIFT_TRACE(ctx.events, "CRUMBLING SANDS: no other opposing spell this turn — no counter");
            return;
        }
        counterChosenItem(ctx, *spell);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 826;
        d.def_id = R"RB(ven-039-166)RB";
        d.name = R"RB(Crumbling Sands)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-039/166)RB";
        d.collector_number = 39;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter a spell if an opponent has played another spell this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0d7491bdb2e9d18f64a5b53a16623b26a7cac6a1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_826(CardRegistry& r) {
    r.registerCard(826, std::make_unique<CrumblingSands>());
}

}  // namespace riftbound
