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

class TricksyTentacles : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move any number of enemy units with the same controller and a total
    //  Might of 8 or less to a single location."
    // Resumable: repeated unit picks (each with a "Done" option) against a
    // Might budget of 8, then the one location they all move to (their
    // controller's base or any battlefield). 1v1: every enemy unit has the
    // same controller. resume_data = {remaining budget, picked ids...};
    // resume_point 1 = a unit is being asked, 2 = the location, 3 = done.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        auto candidates = [&]() {
            auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                if (o.controller == ctx.controller) return false;
                if (o.current_might > ri.resume_data[0]) return false;
                for (size_t i = 1; i < ri.resume_data.size(); ++i)
                    if (static_cast<GameObjectId>(ri.resume_data[i]) == o.id) return false;
                return true;
            });
            std::sort(out.begin(), out.end());
            // Deflect (CR 809): only units whose cost the controller can pay.
            return offerableChoices(ctx, out);
        };
        auto promptUnit = [&]() {
            auto c = candidates();
            if (c.empty()) return false;
            std::vector<Intent> opts;
            for (auto id : c) opts.push_back(objChoice(ctx.controller, id));
            // "any number" includes zero (CR 355.13): "Done" is on every
            // prompt, and with no unit to choose the spell still resolves.
            opts.push_back(valChoice(ctx.controller, 0, "Done"));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                "Tricksy Tentacles: move an enemy unit (Might left: " +
                std::to_string(ri.resume_data[0]) + ")");
            ri.resume_point = 1;
            return true;
        };
        const int n = 1 + static_cast<int>(ctx.state.battlefields.size());
        auto promptLocation = [&]() {
            if (ri.resume_data.size() <= 1) { ri.resume_point = 3; return; }   // nothing chosen
            std::vector<Intent> opts;
            for (int d = 0; d < n; ++d)
                opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Tricksy Tentacles: move them where?");
            ri.resume_point = 2;
        };
        if (ri.resume_point == 0) {
            ri.resume_data = {8};
            if (!promptUnit()) ri.resume_point = 3;
            return;
        }
        if (ri.resume_data.empty()) return;
        if (ri.resume_point == 1) {
            GameObjectId picked = takenObject(ctx);
            auto legal = candidates();
            if (picked == kInvalidId ||
                std::find(legal.begin(), legal.end(), picked) == legal.end()) {
                promptLocation();   // "Done"
                return;
            }
            commitChosenObject(ctx, picked);   // Deflect, "chosen" bookkeeping
            ri.resume_data[0] -= ctx.state.getObject(picked).current_might;
            ri.resume_data.push_back(static_cast<int32_t>(picked));
            if (!promptUnit()) promptLocation();
            return;
        }
        if (ri.resume_point == 2) {
            ri.resume_point = 3;
            int d = takenValue(ctx, 0);
            if (d < 0 || d >= n) d = 0;
            for (size_t i = 1; i < ri.resume_data.size(); ++i) {
                auto id = static_cast<GameObjectId>(ri.resume_data[i]);
                if (!ctx.state.objectExists(id)) continue;
                const auto& u = ctx.state.getObject(id);
                if (!u.location.has_value()) continue;
                // Already there: it does not move.
                const auto dests = moveDests(ctx.state, u);
                if (std::find(dests.begin(), dests.end(), d) == dests.end()) continue;
                moveTo(ctx, id, d);
            }
            ctx.events.logTrace("TRICKSY TENTACLES: moved the chosen enemy units to " + destLabel(d));
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 616;
        d.def_id = R"RB(unl-054-219)RB";
        d.name = R"RB(Tricksy Tentacles)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-054/219)RB";
        d.collector_number = 54;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Move any number of enemy units with the same controller and a total Might of 8 or less to a single location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7e1e77df44c076599fa780f7fd01a4550f67af45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_616(CardRegistry& r) {
    r.registerCard(616, std::make_unique<TricksyTentacles>());
}

} // namespace riftbound
