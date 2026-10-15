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

class MysticReversal : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Gain control of a spell. You may make new choices for it." The spell is
    // a target (CR 355.9.a.2), chosen as this is played.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    static bool qualifies(const ChainItem& ci) { return ci.is_spell; }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return anyChainItem(state, qualifies);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto spell = pickChainItem(ctx, "Mystic Reversal: gain control of a spell",
                                         chainItemChoices(ctx, qualifies));
        if (!spell.has_value()) return;
        if (finalizingChoices(ctx)) return;
        if (*spell == kInvalidId) return;
        for (auto& ci : ctx.state.chain.items) {
            if (ci.id != *spell) continue;
            // Gain control: the chain item and the spell's card are mine; the
            // spell resolves under my control.
            ci.controller = ctx.controller;
            if (ctx.state.objectExists(ci.source))
                ctx.state.getObject(ci.source).controller = ctx.controller;
            break;
        }
        RIFT_TRACE(ctx.events, "MYSTIC REVERSAL: gained control of the chosen spell");
        // "You may make new choices for it" (CR 750-755): its new controller
        // re-chooses its targets (keeping one is choosing it again).
        ctx.executor.makeNewChoices(*spell);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 80;
        d.def_id = R"RB(ogn-080-298)RB";
        d.name = R"RB(Mystic Reversal)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-080/298)RB";
        d.collector_number = 80;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Gain control of a spell. You may make new choices for it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/298fe91f9d76086b7d77880e11016ed46389b61b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_80(CardRegistry& r) {
    r.registerCard(80, std::make_unique<MysticReversal>());
}

} // namespace riftbound
