#include "cards/card.h"
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

// "[Repeat] [P] ... Look at the top 2 cards of your Main Deck. Draw one and
//  recycle the other." ([Repeat] is engine-handled.)

class CalledShot : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            int n = std::min(2, static_cast<int>(ps.main_deck.size()));
            if (n == 0) return;
            if (n == 1) {  // only one card — just draw it
                ctx.executor.drawCards(ctx.controller, 1);
                return;
            }
            // Look at (reveal privately) the top 2, offer one to draw.
            std::vector<Intent> choices;
            for (int i = 0; i < n; ++i) {
                GameObjectId cid = ps.main_deck[ps.main_deck.size() - 1 - i];
                if (!ctx.state.objectExists(cid)) continue;
                const auto& obj = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{cid, obj.card_def_id, obj.owner,
                                                  false, ctx.controller, ZoneType::MainDeck});
                Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
                c.chosen_objects = {cid}; choices.push_back(std::move(c));
            }
            if (choices.empty()) return;
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Called Shot: draw one (recycle the other)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            GameObjectId drawn = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) drawn = choice->chosen_objects[0];
            int n = std::min(2, static_cast<int>(ps.main_deck.size()));
            std::vector<GameObjectId> top;
            for (int i = 0; i < n; ++i)
                top.push_back(ps.main_deck[ps.main_deck.size() - 1 - i]);
            if (drawn == kInvalidId && !top.empty()) drawn = top[0];
            // Move drawn card to hand; recycle the other.
            for (auto cid : top) {
                auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), cid);
                if (it != ps.main_deck.end()) ps.main_deck.erase(it);
            }
            if (drawn != kInvalidId && ctx.state.objectExists(drawn)) {
                auto& d = ctx.state.getObject(drawn);
                d.zone = ZoneType::Hand;
                d.location = std::nullopt;
                ps.hand.push_back(drawn);
            }
            std::vector<GameObjectId> rest;
            for (auto cid : top) if (cid != drawn) rest.push_back(cid);
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
            ctx.events.logTrace("CALLED SHOT: drew one of top 2, recycled the other");
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 443;
        d.def_id = R"RB(sfd-122-221)RB";
        d.name = R"RB(Called Shot)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-122/221)RB";
        d.collector_number = 122;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Repeat] [P] (You may pay the additional cost to repeat this spell's effect.)
Look at the top 2 cards of your Main Deck. Draw one and recycle the other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8fb07f0b3c0c697e324fb016f4cf7e4288aa4594-744x1039.png?accountingTag=RB)RB";
        d.banned = true;  // tournament ban (formerly cards/ban-list.csv)
        return d;
    }();
};

}  // anonymous namespace

void register_card_443(CardRegistry& r) {
    r.registerCard(443, std::make_unique<CalledShot>());
}

} // namespace riftbound
