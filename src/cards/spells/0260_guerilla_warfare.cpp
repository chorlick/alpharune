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

class GuerillaWarfare : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Return up to two cards with [Hidden] from your trash to your hand. You
    //  can hide cards ignoring costs this turn."
    // The cards are targets (the trash is public, CR 355.10.a.1), up to two,
    // zero allowed (CR 355.13), chosen as the spell is played (CR 355.5;
    // testing phase D67).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // cards in a trash, not units
    }
    static std::vector<GameObjectId> hiddenInTrash(const CardContext& ctx) {
        std::vector<GameObjectId> out;
        const CardDB& db = ctx.executor.cardDB();
        for (auto cid : ctx.state.player(ctx.controller).trash) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& obj = ctx.state.getObject(cid);
            if (obj.card_def_id == kInvalidId) continue;
            if (db.get(obj.card_def_id).keywords.has(Keyword::Hidden)) out.push_back(cid);
        }
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto picks = selectSeveralTargets(*this, ctx,
            "Guerilla Warfare: return a [Hidden] card from your trash (up to two)",
            hiddenInTrash(ctx), /*max_picks=*/2);
        if (!picks.has_value()) return;          // waiting for the agent
        if (finalizingChoices(ctx)) return;      // choices end here (CR 355)
        auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : *picks) {
            if (!ctx.state.objectExists(cid)) continue;
            ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), cid), ps.trash.end());
            deck::putInHand(ctx.state, cid);
            RIFT_TRACE(ctx.events, "GUERILLA WARFARE: returned " +
                                 ctx.state.getObject(cid).name + " from trash to hand");
        }
        ps.hide_ignoring_costs_this_turn = true;
    }
private:
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 260;
        d.def_id = R"RB(ogn-264-298)RB";
        d.name = R"RB(Guerilla Warfare)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-264/298)RB";
        d.collector_number = 264;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Teemo)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB(Return up to two cards with [Hidden] from your trash to your hand. You can hide cards ignoring costs this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3a1f70e49f1fdbac907a3395f517e96717e8afea-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_260(CardRegistry& r) {
    r.registerCard(260, std::make_unique<GuerillaWarfare>());
}

} // namespace riftbound
