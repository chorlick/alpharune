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

class Confront : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // "Units you play this turn enter ready. Draw 1."
        // ENGINE LIMITATION: there is no per-player "units enter ready this
        // turn" flag in PlayerState, and the resolvePermanent enter-state path
        // only consults the per-Card entersReadyOnPlay() hook — neither can be
        // driven by a spell without engine edits (out of scope). The previous
        // impl erroneously readied the spell object itself; that is removed.
        // The forward-looking enter-ready clause is left unimplemented; the
        // Draw 1 is honored.
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 129;
        d.def_id = R"RB(ogn-129-298)RB";
        d.name = R"RB(Confront)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-129/298)RB";
        d.collector_number = 129;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Units you play this turn enter ready. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/691a38a7c344fdb5ee86e34d41321d99754286a7-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_129(CardRegistry& r) {
    r.registerCard(129, std::make_unique<Confront>());
}

} // namespace riftbound
