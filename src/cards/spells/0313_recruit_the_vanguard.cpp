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

class RecruitTheVanguard : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Play four 1 [M] Recruit unit tokens. (They can be played to your base
    //  or to battlefields you control.)" Each token's place is its
    //  controller's choice; nothing is asked when the base is the only one.
    //  Progress lives on the resolving item: resume_point kAwait while a
    //  prompt is out, resume_data = the places chosen so far.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<int> places;
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_point == kDone) return;
            const auto dests = tokenPlayDests(ctx.state, ctx.controller);
            if (ri.resume_point == kAwait) {
                int d = takenValue(ctx, 0);
                if (std::find(dests.begin(), dests.end(), d) == dests.end()) d = 0;
                ri.resume_data.push_back(d);
            } else {
                ri.resume_data.clear();
            }
            if (ri.resume_data.size() < kTokens && dests.size() > 1) {
                std::vector<Intent> options;
                for (int d : dests) options.push_back(valChoice(ctx.controller, d, destLabel(d)));
                ctx.executor.requestChoice(ctx.controller, std::move(options),
                    "Recruit the Vanguard: where to play Recruit " +
                    std::to_string(ri.resume_data.size() + 1) + " of 4", PromptKind::Location);
                ri.resume_point = kAwait;
                return;
            }
            ri.resume_point = kDone;
            places.assign(ri.resume_data.begin(), ri.resume_data.end());
        }
        places.resize(kTokens, 0);
        for (int d : places) {
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                     1, {"Recruit"}, KeywordSet{},
                                     destLocation(ctx.controller, d),
                                     /*enter_ready=*/false);
        }
        RIFT_TRACE(ctx.events, "RECRUIT THE VANGUARD: played four 1[M] Recruit tokens");
    }
private:
    static constexpr size_t kTokens = 4;
    enum : int { kAwait = 21, kDone = 22 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 313;
        d.def_id = R"RB(ogs-015-024)RB";
        d.name = R"RB(Recruit the Vanguard)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-015/024)RB";
        d.collector_number = 15;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 6;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Play four 1 [M] Recruit unit tokens. (They can be played to your base or to battlefields you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/81d1c47459606f7b627778cce9b5f0e44d80f7fa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_313(CardRegistry& r) {
    r.registerCard(313, std::make_unique<RecruitTheVanguard>());
}

} // namespace riftbound
