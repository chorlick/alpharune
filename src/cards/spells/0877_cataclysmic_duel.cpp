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

class CataclysmicDuel : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Each player chooses a unit they control. Kill the rest."
    // Resumable: the controller chooses, then the opponent (each from their
    // own units); every other unit on the board is then killed at once.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        const PlayerId opp = opponent(ctx.controller);
        const auto none = static_cast<int32_t>(kInvalidId.raw());
        auto unitsOf = [&](PlayerId p) {
            std::vector<GameObjectId> out;
            for (const auto& [id, obj] : ctx.state.objects.onBoard())
                if (obj.isUnit() && obj.controller == p && obj.location.has_value())
                    out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        auto prompt = [&](PlayerId p, int next) {
            auto units = unitsOf(p);
            if (units.empty()) return false;
            std::vector<Intent> opts;
            for (auto u : units) opts.push_back(objChoice(p, u));
            ctx.executor.requestChoice(p, std::move(opts),
                                       "Cataclysmic Duel: choose a unit you control to keep", PromptKind::ChooseObject);
            ri.resume_point = next;
            return true;
        };
        // The answer of player `p`, defaulting to their first unit if the
        // recorded choice is missing or no longer one of their units.
        auto answer = [&](PlayerId p) {
            GameObjectId picked = takenObject(ctx);
            auto units = unitsOf(p);
            if (std::find(units.begin(), units.end(), picked) == units.end())
                picked = units.empty() ? kInvalidId : units.front();
            return static_cast<int32_t>(picked.raw());
        };
        switch (ri.resume_point) {
        case 0:
            ri.resume_data = {none, none};
            if (prompt(ctx.controller, 1)) return;
            [[fallthrough]];
        case 1:
            if (ri.resume_point == 1) ri.resume_data[0] = answer(ctx.controller);
            if (prompt(opp, 2)) return;
            [[fallthrough]];
        case 2:
            if (ri.resume_point == 2) ri.resume_data[1] = answer(opp);
            break;
        default:
            return;
        }
        ri.resume_point = 3;
        // Collect before killing (killing mutates zones / fires events).
        std::vector<GameObjectId> doomed;
        for (const auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (static_cast<int32_t>(id.raw()) == ri.resume_data[0] ||
                static_cast<int32_t>(id.raw()) == ri.resume_data[1]) continue;
            doomed.push_back(id);
        }
        std::sort(doomed.begin(), doomed.end());
        RIFT_TRACE(ctx.events, "CATACLYSMIC DUEL: killing " + std::to_string(doomed.size()) +
                            " unchosen unit(s)");
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, doomed);   // they die at the same time
            for (auto id : doomed) ctx.executor.killObject(id);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 877;
        d.def_id = R"RB(ven-090-166)RB";
        d.name = R"RB(Cataclysmic Duel)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-090/166)RB";
        d.collector_number = 90;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 8;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Each player chooses a unit they control. Kill the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1fb5fcd8ca8be36b5aba234040b7428aab682576-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_877(CardRegistry& r) {
    r.registerCard(877, std::make_unique<CataclysmicDuel>());
}

}  // namespace riftbound
