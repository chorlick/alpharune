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

class Deceiver : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    // "you may discard 1 and exhaust me to ...": the "may" opens the effect and
    // the cost follows it at once, so both are settled as the ability is
    // finalized (CR 383.3.a / 383.3.b) — before anyone can respond.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "there" is the battlefield that was conquered or held: the trigger's
    // subject is that battlefield's card object.
    static std::optional<BattlefieldId> scoredBattlefield(GameState& state,
                                                           PlayerId /*controller*/) {
        if (!state.chain.resuming.has_value()) return std::nullopt;
        return battlefieldOfCard(state, state.chain.resuming->triggering_subject);
    }

    // Progress is kept on the resolving chain item in resume_data slots past
    // the ones the choice helpers own (0..6): [kPhase] = where we are.
    // confirmOptional asks the "may"; what the token copies is the reflexive
    // trigger's choice (onReflexiveTrigger).
    static constexpr size_t kPhase = 7;
    static constexpr size_t kToken = 8;
    static constexpr size_t kBf = 9;
    enum : int32_t { kConfirm = 0, kAwaitDiscard = 1, kCopy = 2, kFinished = 3, kPaid = 4 };

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        // (Re-fetched on every use: the helpers may grow resume_data.)
        auto slot = [&](size_t i) -> int32_t& {
            auto& rd = ctx.state.chain.resuming->resume_data;
            while (rd.size() <= kBf) rd.push_back(0);
            return rd[i];
        };
        if (slot(kPhase) == kFinished) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        // Both halves of the cost must be payable: discard 1 and exhaust me.
        // (Once it is paid, being exhausted no longer argues against the effect.)
        auto can_pay = [&]() {
            return costAlreadyPaid(ctx) || slot(kPhase) >= kCopy ||
                   (ctx.state.objectExists(ctx.source) &&
                    !ctx.state.getObject(ctx.source).is_exhausted && !ps.hand.empty());
        };

        // "you may discard 1 and exhaust me": optional (CR 383.3.a).
        const int go = confirmOptional(
            ctx, "Deceiver: discard 1 and exhaust me to play a Reflection", can_pay);
        if (go == -1) return;           // waiting
        if (go == 1 && slot(kPhase) == kConfirm) {
            std::vector<Intent> choices;
            for (auto card_id : ps.hand) choices.push_back(objChoice(ctx.controller, card_id));
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "discard 1 (Deceiver)");
            slot(kPhase) = kAwaitDiscard;
            return;
        }
        if (go == 1 && slot(kPhase) == kAwaitDiscard) {
            auto choice = ctx.executor.takeChoice();
            if (!can_pay()) { slot(kPhase) = kFinished; return; }
            GameObjectId discard = ps.hand.front();
            if (choice && !choice->chosen_objects.empty() &&
                std::find(ps.hand.begin(), ps.hand.end(), choice->chosen_objects[0]) !=
                    ps.hand.end())
                discard = choice->chosen_objects[0];
            // Pay the cost: exhaust me, discard the chosen card.
            if (costDueNow(ctx)) {
                ctx.executor.exhaustObject(ctx.source);
                ctx.executor.applyDiscard(ctx.controller, discard);
            }
            slot(kPhase) = kPaid;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go != 1) { slot(kPhase) = kFinished; return; }   // declined, or can't pay

        if (slot(kPhase) == kPaid) {
            auto bf = scoredBattlefield(ctx.state, ctx.controller);
            LocationId loc = bf.has_value()
                ? LocationId{BattlefieldLocation{*bf}}
                : LocationId{BaseLocation{ctx.controller}};
            // CR 187.6: a 0 [M] Reflection token; played ready, "there".
            // ([Temporary] is given by the reflexive trigger below, not here.)
            GameObjectId token = ctx.executor.createToken(
                ctx.controller, CardType::Unit, "Reflection", 0, {}, KeywordSet{}, loc,
                /*enter_ready=*/true);
            ctx.events.logTrace("DECEIVER: created ready Reflection token there");
            slot(kPhase) = kFinished;
            // "Then do this: It becomes a copy of another unit there. Give it
            // [Temporary]." — a reflexive trigger (CR 388): it goes on the
            // chain now, with the token in play, as a pending item of its own
            // (CR 389.1). Which unit is chosen when it is finalized, and
            // players may respond before it resolves.
            ctx.executor.queueReflexiveTrigger(
                ctx.source, ctx.controller, kBecomeACopy,
                {static_cast<int32_t>(token), bf.has_value() ? static_cast<int32_t>(*bf) + 1 : 0});
        }
    }

    // `data` = {the Reflection token, its battlefield + 1 (0 = none)}.
    static constexpr int kBecomeACopy = 0;
    bool hasReflexiveTrigger() const override { return true; }
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        if (data.size() < 2) return;
        const auto token = static_cast<GameObjectId>(data[0]);
        auto token_in_play = [&]() {
            return ctx.state.objectExists(token) &&
                   ctx.state.getObject(token).location.has_value();
        };
        // "another unit there": any controller's unit at that battlefield
        // (units that can't be chosen by enemy spells and abilities are not
        // offered).
        std::vector<GameObjectId> models;
        if (data[1] != 0 && token_in_play()) {
            const auto bf = static_cast<BattlefieldId>(data[1] - 1);
            models = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.id != token && o.battlefieldId() == bf;
            });
            std::sort(models.begin(), models.end());
        }
        GameObjectId model = kInvalidId;
        if (!models.empty()) {
            model = pickTarget(ctx, "Deceiver: copy which unit there?", models);
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 388.4)
        if (!token_in_play()) return;         // the token left in response
        if (model != kInvalidId && ctx.state.objectExists(model) &&
            ctx.state.getObject(model).location.has_value()) {
            ctx.executor.copyUnit(token, model);
            ctx.events.logTrace("DECEIVER: token became a copy of a unit there");
        }
        // "Give it [Temporary]." — after the copy, which overwrites keywords.
        if (token_in_play()) ctx.state.getObject(token).keywords.set(Keyword::Temporary);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 756;
        d.def_id = R"RB(unl-199-219)RB";
        d.name = R"RB(Deceiver)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-199/219)RB";
        d.collector_number = 199;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(LeBlanc)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you conquer or hold, you may discard 1 and exhaust me to play a ready Reflection unit token there. Then do this: It becomes a copy of another unit there. Give it [Temporary].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c17d29602d61d6702372fec4db44c6fb2a29e8a2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_756(CardRegistry& r) {
    r.registerCard(756, std::make_unique<Deceiver>());
}

} // namespace riftbound
