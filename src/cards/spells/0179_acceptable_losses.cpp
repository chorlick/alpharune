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

class AcceptableLosses : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each player kills one of their gear." Each player chooses which of
    // THEIR OWN gear (not a target: CR 355.10.e), as the spell resolves, the
    // controller first; then both are killed at the same time (CR 370.1.a.2;
    // testing phase R4, as Cull the Weak does for units). The lists are in
    // ascending object ids (boardObjects).
    static constexpr size_t kMineSlot = 7;
    static constexpr size_t kTheirsSlot = 9;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto own_gear = [&](PlayerId p) {
            return boardObjects(ctx.state, [p](const GameObject& o) {
                return o.isGear() && o.controller == p;
            });
        };
        const PlayerId opp = opponent(ctx.controller);
        // At resolution: each player chooses (CR 355.10.e; batch M1).
        const auto mine = selectObjectAs(ctx, ctx.controller,
                                         "Acceptable Losses: kill one of your gear",
                                         own_gear(ctx.controller), kMineSlot);
        if (!mine.has_value()) return;   // waiting on the controller
        // At resolution: each player chooses (CR 355.10.e; batch M1).
        const auto theirs = selectObjectAs(ctx, opp, "Acceptable Losses: kill one of your gear",
                                           own_gear(opp), kTheirsSlot);
        if (!theirs.has_value()) return;   // waiting on the opponent
        std::vector<GameObjectId> doomed;
        for (auto [id, p] : {std::pair{*mine, ctx.controller}, std::pair{*theirs, opp}})
            if (id != kInvalidId && ctx.state.objectExists(id) &&
                ctx.state.getObject(id).location.has_value() &&
                ctx.state.getObject(id).controller == p)
                doomed.push_back(id);
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, doomed);   // they die at the same time
            for (auto id : doomed) ctx.executor.killObject(id);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 179;
        d.def_id = R"RB(ogn-179-298)RB";
        d.name = R"RB(Acceptable Losses)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-179/298)RB";
        d.collector_number = 179;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Each player kills one of their gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b2b470bab1ae511ab9de0b1ce576e2050532a081-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_179(CardRegistry& r) {
    r.registerCard(179, std::make_unique<AcceptableLosses>());
}

} // namespace riftbound
