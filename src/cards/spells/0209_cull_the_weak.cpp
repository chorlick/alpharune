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

class CullTheWeak : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Each player kills one of their units." Nothing is targeted (CR
    // 355.10.e): each player chooses among their own units as the spell
    // resolves, the caster first; both units are then killed together.
    // Progress lives on the resolving item: resume_point says whose prompt is
    // out, resume_data = the units chosen so far.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId players[2] = {ctx.controller, opponent(ctx.controller)};
        auto own_units = [&](PlayerId p) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects.onBoard())
                if (obj.isUnit() && obj.controller == p && obj.location.has_value())
                    out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (no chain item to resume): nobody can be
            // asked, so each player's first unit, like the choice helpers'
            // default.
            std::vector<GameObjectId> doomed;
            for (PlayerId p : players)
                if (auto units = own_units(p); !units.empty()) doomed.push_back(units.front());
            {   // the batch is for these kills only
                DeathBatch together(ctx.executor, doomed);   // they die at the same time
                for (auto id : doomed) ctx.executor.killObject(id);
            }
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        int next = 0;
        if (ri.resume_point == kAwaitCaster || ri.resume_point == kAwaitOpponent) {
            const int who = ri.resume_point == kAwaitCaster ? 0 : 1;
            const auto units = own_units(players[who]);
            GameObjectId unit = takenObject(ctx);
            if (std::find(units.begin(), units.end(), unit) == units.end())
                unit = units.empty() ? kInvalidId : units.front();
            if (unit != kInvalidId) ri.resume_data.push_back(static_cast<int32_t>(unit.raw()));
            next = who + 1;
        } else {
            ri.resume_data.clear();
        }
        for (int who = next; who < 2; ++who) {
            const auto units = own_units(players[who]);
            if (units.empty()) continue;
            std::vector<Intent> options;
            for (auto id : units) options.push_back(objChoice(players[who], id));
            ctx.executor.requestChoice(players[who], std::move(options),
                                       "Cull the Weak: kill one of your units", PromptKind::ChooseObject);
            ri.resume_point = who == 0 ? kAwaitCaster : kAwaitOpponent;
            return;
        }
        ri.resume_point = kDone;
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, objectIdsFromRaw(ri.resume_data));   // they die at the same time
            for (auto id : ri.resume_data)
                ctx.executor.killObject(GameObjectId::fromRaw(id));
        }
    }
private:
    enum : int { kAwaitCaster = 21, kAwaitOpponent = 22, kDone = 23 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 209;
        d.def_id = R"RB(ogn-209-298)RB";
        d.name = R"RB(Cull the Weak)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-209/298)RB";
        d.collector_number = 209;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.ability_text = R"RB(Each player kills one of their units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/49338a4b31727c6ef50e7dbc54e7004dcd2b6f4c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_209(CardRegistry& r) {
    r.registerCard(209, std::make_unique<CullTheWeak>());
}

} // namespace riftbound
