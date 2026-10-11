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
    // The controller picks the cards, one prompt each, and may stop early.
    // Progress lives on the resolving item: resume_point kAwait while a prompt
    // is out, resume_data[0] = cards returned so far.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        auto& ps = ctx.state.player(ctx.controller);
        const CardDB& db = ctx.executor.cardDB();
        auto hidden_in_trash = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : ps.trash) {
                if (!ctx.state.objectExists(cid)) continue;
                const auto& obj = ctx.state.getObject(cid);
                if (obj.card_def_id == kInvalidId) continue;
                if (db.get(obj.card_def_id).keywords.has(Keyword::Hidden)) out.push_back(cid);
            }
            return out;
        };
        bool stop = false;
        if (ri.resume_point == kAwait) {
            const auto cards = hidden_in_trash();
            const GameObjectId cid = takenObject(ctx);
            if (std::find(cards.begin(), cards.end(), cid) == cards.end()) {
                stop = true;   // "Return no more"
            } else {
                auto& obj = ctx.state.getObject(cid);
                ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), cid), ps.trash.end());
                obj.zone = ZoneType::Hand;
                obj.location = std::nullopt;
                ps.hand.push_back(cid);
                ++ri.resume_data[0];
                ctx.events.logTrace("GUERILLA WARFARE: returned " + obj.name +
                                     " from trash to hand");
            }
        } else {
            ri.resume_data.assign(1, 0);
        }
        if (!stop && ri.resume_data[0] < 2) {
            const auto cards = hidden_in_trash();
            if (!cards.empty()) {
                std::vector<Intent> options;
                // "up to two" includes none (CR 355.13), on the first prompt
                // too; the second sentence happens either way.
                options.push_back(valChoice(ctx.controller, 0, "Return no more"));
                for (auto cid : cards) options.push_back(objChoice(ctx.controller, cid));
                ctx.executor.requestChoice(ctx.controller, std::move(options),
                    "Guerilla Warfare: return a [Hidden] card from your trash (up to two)");
                ri.resume_point = kAwait;
                return;
            }
        }
        ri.resume_point = kDone;
        ps.hide_ignoring_costs_this_turn = true;
    }
private:
    enum : int { kAwait = 21, kDone = 22 };
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
