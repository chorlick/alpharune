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

class DecreeOfDiscord : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Return any number of enemy Order ([Y]) units with total Might 5 or less
    //  to their owners' hands."
    // The units are targets chosen as a group (CR 355.11.a; zero allowed,
    // 355.13) as the spell is played, in its finalize pass, before its cost:
    // one prompt per unit (each with "Done") against the Might budget, built
    // by hand, so each goes through offerableChoices / commitChosenObject. As
    // it resolves, the chosen units still legal are returned together; if
    // they no longer have total Might 5 or less, the controller chooses a
    // legal subset of them (CR 355.11.b).
    // Progress: resume_point kPicking / kAwait / kChosen / kDone;
    // resume_data[kCountSlot] = units picked, then the units; the subset
    // helper's slots follow them.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // enemy units only
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point != kPicking && ri.resume_point != kAwait && ri.resume_point != kChosen) {
            ri.resume_data.assign(kCountSlot + 1, 0);
            ri.resume_point = kPicking;
        }
        auto picked = [&]() {
            std::vector<GameObjectId> out;
            const auto n = static_cast<size_t>(ri.resume_data[kCountSlot]);
            for (size_t i = 0; i < n; ++i)
                out.push_back(GameObjectId::fromRaw(ri.resume_data[kCountSlot + 1 + i]));
            return out;
        };
        auto mightOf = [&](const std::vector<GameObjectId>& units) {
            int total = 0;
            for (auto id : units) total += std::max(0, ctx.state.getObject(id).current_might);
            return total;
        };
        auto orderUnits = [&]() {
            return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller &&
                       std::find(o.domains.begin(), o.domains.end(), Domain::Order) !=
                           o.domains.end();
            });
        };
        auto candidates = [&]() {
            const auto have = picked();
            const int budget = kTotalMight - mightOf(have);
            std::vector<GameObjectId> out;
            for (auto id : orderUnits())
                if (std::find(have.begin(), have.end(), id) == have.end() &&
                    ctx.state.getObject(id).current_might <= budget)
                    out.push_back(id);
            return offerableChoices(ctx, out);   // Deflect (CR 809)
        };
        if (ri.resume_point == kAwait) {
            const GameObjectId unit = takenObject(ctx);
            const auto legal = candidates();
            if (std::find(legal.begin(), legal.end(), unit) != legal.end()) {
                commitChosenObject(ctx, unit);   // Deflect, "chosen" bookkeeping
                ri.resume_data.push_back(static_cast<int32_t>(unit.raw()));
                ++ri.resume_data[kCountSlot];
                ri.resume_point = kPicking;
            } else {
                ri.resume_point = kChosen;   // "Done"
            }
        }
        if (ri.resume_point == kPicking) {
            const auto c = candidates();
            if (!c.empty()) {
                std::vector<Intent> opts;
                for (auto id : c) opts.push_back(objChoice(ctx.controller, id));
                opts.push_back(valChoice(ctx.controller, 0, "Done"));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                    "Decree of Discord: return an enemy Order unit (Might left: " +
                    std::to_string(kTotalMight - mightOf(picked())) + ")", PromptKind::ChooseObject);
                ri.resume_point = kAwait;
                return;
            }
            ri.resume_point = kChosen;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)

        const auto chosen = picked();
        std::vector<GameObjectId> originals;
        const auto now = orderUnits();
        for (auto id : chosen)
            if (std::find(now.begin(), now.end(), id) != now.end() &&
                ctx.state.stillTheChosenObject(ri, id))
                originals.push_back(id);
        const auto group = chooseFittingSubset(ctx,
            "Decree of Discord: the chosen units no longer have total Might 5 or less -"
            " choose which of them to return",
            originals, [&](const std::vector<GameObjectId>& set) {
                return mightOf(set) <= kTotalMight;
            }, kCountSlot + 1 + chosen.size());
        if (!group.has_value()) return;   // waiting for the agent
        ri.resume_point = kDone;
        for (auto id : *group) {
            if (!ctx.state.getObject(id).location.has_value()) continue;
            ctx.executor.bounceToHand(id);
        }
    }
private:
    static constexpr int kTotalMight = 5;
    enum : int { kPicking = 40, kAwait = 41, kChosen = 42, kDone = 43 };
    static constexpr size_t kCountSlot = 7;
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 894;
        d.def_id = R"RB(ven-107-166)RB";
        d.name = R"RB(Decree of Discord)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-107/166)RB";
        d.collector_number = 107;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Return any number of enemy Order ([Y]) units with total Might 5 or less to their owners' hands.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b0e82842acddb99c47af6793dab48164caf498c2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_894(CardRegistry& r) {
    r.registerCard(894, std::make_unique<DecreeOfDiscord>());
}

}  // namespace riftbound
