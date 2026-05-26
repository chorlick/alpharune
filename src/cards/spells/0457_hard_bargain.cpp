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

class HardBargain : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // Pure counter — no secondary effect. Not playable unless there's a
    // spell on the chain to potentially counter.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        switch (ri.resume_point) {
        case 0: {
            if (ctx.state.chain.items.empty()) {
                ctx.events.logTrace("HARD BARGAIN: nothing on chain to counter — no-op");
                return;
            }
            auto& top = ctx.state.chain.items.back();
            if (!top.is_spell) {
                ctx.events.logTrace("HARD BARGAIN: top of chain isn't a spell — no-op");
                return;
            }
            PlayerId target_controller = top.controller;
            GameObjectId target_source = top.source;
            ri.resume_data = {static_cast<int32_t>(target_source)};

            // Count target controller's ready runes — they need 2 to save.
            auto& tps = ctx.state.player(target_controller);
            int ready_runes = 0;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isRune() || obj.controller != target_controller) continue;
                if (!obj.location.has_value()) continue;
                if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
                if (std::get<BaseLocation>(*obj.location).player != target_controller) continue;
                if (!obj.is_exhausted) ++ready_runes;
            }
            (void)tps;

            if (ready_runes < 2) {
                // Can't afford the rescue cost — counter immediately.
                if (!ctx.state.chain.items.empty()) {
                    revertCounteredPlay(ctx, ctx.state.chain.items.back());  // CR 425.1.b
                }
                ctx.state.chain.items.pop_back();
                if (ctx.state.objectExists(target_source)) {
                    auto& obj = ctx.state.getObject(target_source);
                    ctx.events.logTrace("HARD BARGAIN: countered " + obj.name +
                                         " (" + toString(target_controller) +
                                         " can't afford 2E to save)");
                    obj.zone = ZoneType::Trash;
                    obj.location = std::nullopt;
                    ctx.state.player(obj.owner).trash.push_back(target_source);
                }
                return;
            }

            // Offer the target's controller a binary choice: pay 2 to save,
            // or let it be countered. Two MakeChoice intents — chosen_objects
            // is empty for "let it die," contains the target_source for "pay."
            std::vector<Intent> choices;
            Intent pay;
            pay.type = IntentType::MakeChoice;
            pay.player = target_controller;
            pay.chosen_objects = {target_source};
            choices.push_back(pay);
            Intent decline;
            decline.type = IntentType::MakeChoice;
            decline.player = target_controller;
            choices.push_back(decline);
            std::string spell_name = ctx.state.objectExists(target_source)
                ? ctx.state.getObject(target_source).name : "spell";
            ctx.executor.requestChoice(target_controller, std::move(choices),
                                        "Hard Bargain: pay 2 to save " + spell_name +
                                        "? [pay | decline]");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (ri.resume_data.empty()) return;
            auto target_source = static_cast<GameObjectId>(ri.resume_data[0]);
            if (!ctx.state.objectExists(target_source)) return;

            // Determine target controller from the (still on-chain) item.
            // The top of `items` is still our target — case 0 didn't pop it.
            PlayerId target_controller = ctx.state.objectExists(target_source)
                ? ctx.state.getObject(target_source).controller
                : opponent(ctx.controller);

            bool paid = (choice && !choice->chosen_objects.empty() &&
                          choice->chosen_objects[0] == target_source);

            if (paid) {
                // Exhaust 2 ready runes from target_controller's base.
                int exhausted = 0;
                for (auto& [id, obj] : ctx.state.objects) {
                    if (exhausted >= 2) break;
                    if (!obj.isRune() || obj.controller != target_controller) continue;
                    if (!obj.location.has_value()) continue;
                    if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
                    if (std::get<BaseLocation>(*obj.location).player != target_controller) continue;
                    if (obj.is_exhausted) continue;
                    obj.is_exhausted = true;
                    ++exhausted;
                    ctx.events.logTrace("  HARD BARGAIN COST: exhaust " + obj.name +
                                         " (id=" + std::to_string(id) + ")");
                }
                ctx.events.logTrace(std::string("HARD BARGAIN: ") +
                                     toString(target_controller) +
                                     " paid 2E to save " +
                                     ctx.state.getObject(target_source).name);
                // Spell stays on chain — no pop.
            } else {
                // Counter — pop + trash.
                if (!ctx.state.chain.items.empty()) {
                    revertCounteredPlay(ctx, ctx.state.chain.items.back());  // CR 425.1.b
                    ctx.state.chain.items.pop_back();
                }
                auto& obj = ctx.state.getObject(target_source);
                ctx.events.logTrace("HARD BARGAIN: countered " + obj.name +
                                     " (controller declined to pay)");
                obj.zone = ZoneType::Trash;
                obj.location = std::nullopt;
                ctx.state.player(obj.owner).trash.push_back(target_source);
            }
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 457;
        d.def_id = R"RB(sfd-136-221)RB";
        d.name = R"RB(Hard Bargain)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-136/221)RB";
        d.collector_number = 136;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
[Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Counter a spell unless its controller pays [2].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2b09887a6e0d2f109aefea62ca6744488794c233-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_457(CardRegistry& r) {
    r.registerCard(457, std::make_unique<HardBargain>());
}

} // namespace riftbound
