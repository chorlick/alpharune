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

class BloodharborRipper : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true,
                                  .must_be_at_battlefield = true};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0])) {
            ctx.executor.bounceToHand(targets[0]);
            ctx.events.logTrace("BLOODHARBOR RIPPER: bounced a friendly unit");
        }
        // Play a Gold gear token, exhausted. createToken with the real Gold
        // card_def_id (326) so its [Reaction] kill-for-[A] ability dispatches.
        auto loc = BaseLocation{ctx.controller};
        auto tid = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                            0, {"Gold"}, KeywordSet{}, loc,
                                            /*enter_ready=*/false);
        if (ctx.state.objectExists(tid)) {
            auto& tok = ctx.state.getObject(tid);
            tok.card_def_id = kGoldGearCardDefId;
            tok.is_exhausted = true;  // "exhausted"
        }
        ctx.events.logTrace("BLOODHARBOR RIPPER: created exhausted Gold gear token");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 746;
        d.def_id = R"RB(unl-185-219)RB";
        d.name = R"RB(Bloodharbor Ripper)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-185/219)RB";
        d.collector_number = 185;
        d.artist = R"RB(TSWCK逍)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Pyke)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([1], [E]: Return a friendly unit at a battlefield to its owner's hand. Play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8ca1bfde8d898a11a6abc73bbab8fc7092b0dcb4-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_746(CardRegistry& r) {
    r.registerCard(746, std::make_unique<BloodharborRipper>());
}

} // namespace riftbound
