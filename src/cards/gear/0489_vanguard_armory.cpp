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

class VanguardArmory : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    // "[E]: Play three 1 [M] Recruit unit tokens. (You may play them to
    // different locations.)" Each token goes where a unit may be played: the
    // controller's base or a battlefield they control. With more than one
    // such location the controller is asked once per token; the tokens are
    // played together once all three are placed.
    // resume_point kAsking / kDone; the picks are resume_data[kFirstPick..].
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto dests = [&]() {
            std::vector<int> out{0};   // 0 = base, 1 + i = battlefield i (as destLabel)
            for (const auto& bf : ctx.state.battlefields)
                if (bf.controller.has_value() && *bf.controller == ctx.controller)
                    out.push_back(1 + static_cast<int>(bf.id.raw()));
            return out;
        };
        auto play_all = [&](const std::vector<int>& where) {
            for (int d : where) {
                const LocationId loc = d <= 0
                    ? LocationId{BaseLocation{ctx.controller}}
                    : LocationId{BattlefieldLocation{BattlefieldId::fromRaw(d - 1)}};
                ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                         /*might=*/1, /*tags=*/{"Recruit"}, KeywordSet{},
                                         loc, /*enter_ready=*/false);
            }
            RIFT_TRACE(ctx.events, "VANGUARD ARMORY: [E] -> three 1[M] Recruit tokens");
        };
        const auto options = dests();
        if (!ctx.state.chain.resuming.has_value() || options.size() == 1) {
            // Nothing to choose (or no chain item to suspend on): all to base.
            if (ctx.state.chain.resuming.has_value()) {
                if (ctx.state.chain.resuming->resume_point == kDone) return;
                ctx.state.chain.resuming->resume_point = kDone;
            }
            play_all({0, 0, 0});
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        while (ri.resume_data.size() < kFirstPick) ri.resume_data.push_back(0);
        if (ri.resume_point == kAsking) {
            int d = takenValue(ctx, 0);
            if (std::find(options.begin(), options.end(), d) == options.end()) d = 0;
            ri.resume_data.push_back(d);
        }
        const size_t placed = ri.resume_data.size() - kFirstPick;
        if (placed < 3) {
            std::vector<Intent> opts;
            for (int d : options) opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                "Vanguard Armory: play Recruit " + std::to_string(placed + 1) + " of 3 where?", PromptKind::Location);
            ri.resume_point = kAsking;
            return;
        }
        ri.resume_point = kDone;
        play_all({ri.resume_data[kFirstPick], ri.resume_data[kFirstPick + 1],
                  ri.resume_data[kFirstPick + 2]});
    }
private:
    static constexpr int kAsking = 20;
    static constexpr int kDone = 21;
    static constexpr size_t kFirstPick = 7;   // slots 0..6 belong to the choice helpers
    const CardDef def_ = [] {
        CardDef d;
        d.id = 489;
        d.def_id = R"RB(sfd-168-221)RB";
        d.name = R"RB(Vanguard Armory)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-168/221)RB";
        d.collector_number = 168;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([E]: Play three 1 [M] Recruit unit tokens. (You may play them to different locations.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4f4d735ff77ea599307e142196338d438fcedc05-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_489(CardRegistry& r) {
    r.registerCard(489, std::make_unique<VanguardArmory>());
}

} // namespace riftbound
