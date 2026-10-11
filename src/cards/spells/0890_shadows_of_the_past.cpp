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
    // Resumable: up to two sequential picks over the units in BOTH players'
    // trashes, each with a "Done" option.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        auto candidates = [&]() {
            std::vector<GameObjectId> out;
            for (const auto& ps : ctx.state.players)
                for (auto cid : ps.trash)
                    if (ctx.state.objectExists(cid) && ctx.state.getObject(cid).isUnit())
                        out.push_back(cid);
            return out;
        };
        auto prompt = [&]() {
            auto c = candidates();
            if (c.empty()) return false;
            std::vector<Intent> opts;
            for (auto id : c) opts.push_back(objChoice(ctx.controller, id));
            opts.push_back(valChoice(ctx.controller, 0, "Done"));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Shadows of the Past: return a unit from a trash");
            ri.resume_point = 1;
            return true;
        };
        if (ri.resume_point == 0) {
            ri.resume_data = {0};
            if (!prompt()) ri.resume_point = 2;
            return;
        }
        if (ri.resume_point != 1) return;
        GameObjectId picked = takenObject(ctx);
        auto legal = candidates();
        if (picked == kInvalidId ||
            std::find(legal.begin(), legal.end(), picked) == legal.end()) {
            ri.resume_point = 2;   // "Done"
            return;
        }
        auto& obj = ctx.state.getObject(picked);
        auto& owner = ctx.state.player(obj.owner);
        owner.trash.erase(std::remove(owner.trash.begin(), owner.trash.end(), picked),
                          owner.trash.end());
        obj.zone = ZoneType::Hand;
        obj.location = std::nullopt;
        owner.hand.push_back(picked);
        ctx.events.logTrace("SHADOWS OF THE PAST: returned " + obj.name +
                            " from trash to its owner's hand");
        if (ri.resume_data.empty()) ri.resume_data = {0};
        if (++ri.resume_data[0] >= 2 || !prompt()) ri.resume_point = 2;
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
