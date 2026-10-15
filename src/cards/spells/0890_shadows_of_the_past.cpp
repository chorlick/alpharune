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

class ShadowsOfThePast : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Return up to 2 units from trashes to their owners' hands."
    // The units are targets in public zones (CR 355.10.a.1), up to two, zero
    // allowed (CR 355.13), chosen as the spell is played, in its finalize
    // pass, before its cost: one prompt each over the units in BOTH players'
    // trashes, with "None". A chosen unit that has left its trash as the
    // spell resolves is a mistarget; the other is still returned.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // cards in trashes, not units on the board
    }
    static std::vector<GameObjectId> trashUnits(const GameState& state) {
        std::vector<GameObjectId> out;
        for (const auto& ps : state.players)
            for (auto cid : ps.trash)
                if (state.objectExists(cid) && state.getObject(cid).isUnit()) out.push_back(cid);
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto picks = selectSeveralTargets(*this, ctx,
            "Shadows of the Past: return a unit from a trash (up to two)",
            trashUnits(ctx.state), /*max_picks=*/2);
        if (!picks.has_value()) return;          // waiting for the agent
        if (finalizingChoices(ctx)) return;      // choices end here (CR 355)
        for (auto picked : *picks) {
            auto& obj = ctx.state.getObject(picked);
            auto& owner = ctx.state.player(obj.owner);
            owner.trash.erase(std::remove(owner.trash.begin(), owner.trash.end(), picked),
                              owner.trash.end());
            deck::putInHand(ctx.state, picked);
            RIFT_TRACE(ctx.events, "SHADOWS OF THE PAST: returned " + obj.name +
                                " from trash to its owner's hand");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 890;
        d.def_id = R"RB(ven-103-166)RB";
        d.name = R"RB(Shadows of the Past)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-103/166)RB";
        d.collector_number = 103;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Return up to 2 units from trashes to their owners' hands.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/25e2130938a893833fcb8e29287adbd230c3b67f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_890(CardRegistry& r) {
    r.registerCard(890, std::make_unique<ShadowsOfThePast>());
}

}  // namespace riftbound
