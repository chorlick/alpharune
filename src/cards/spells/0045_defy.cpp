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

class Defy : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Counter a spell that costs no more than [4] and no more than [A]."
    // Both caps are on the spell's printed cost: at most 4 Energy and at most
    // one Power. The spell is a target (CR 355.9.a.2), chosen as Defy is
    // played (CR 355.5) among the finalized spells on the chain that qualify.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    static bool qualifies(const ChainItem& ci, const CardDB& db) {
        if (!ci.is_spell || ci.card_def_id == kInvalidId) return false;
        const auto& d = db.get(ci.card_def_id);
        return d.energy_cost <= 4 && d.power_cost <= 1;
    }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/,
                         const CardDB& db) const override {
        return anyChainItem(state, [&](const ChainItem& ci) { return qualifies(ci, db); });
    }
    // Without the card table only the half the state shows: a spell is there.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return anyChainItem(state, [](const ChainItem& ci) { return ci.is_spell; });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const CardDB& db = ctx.executor.cardDB();
        const auto spell = pickChainItem(
            ctx, "Defy: counter a spell that costs no more than [4] and [A]",
            chainItemChoices(ctx, [&](const ChainItem& ci) { return qualifies(ci, db); }));
        if (!spell.has_value()) return;
        if (finalizingChoices(ctx)) return;
        if (*spell != kInvalidId) counterChosenItem(ctx, *spell);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 45;
        d.def_id = R"RB(ogn-045-298)RB";
        d.name = R"RB(Defy)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-045/298)RB";
        d.collector_number = 45;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter a spell that costs no more than [4] and no more than [A].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4989bfcc4bd7be77051f0c2c349a981ba9c273e0-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_45(CardRegistry& r) {
    r.registerCard(45, std::make_unique<Defy>());
}

} // namespace riftbound
