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

class DefenderOfTomorrow : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Empower] [2][A][A]" / "[1], [E]: Ready a gear." /
    // "[Empowered][>] [1], [E]: Ready 2 gear."
    // Both printed abilities cost [1], [E], so they are one activation; while
    // Empowered the controller says which of the two is being used ("Ready 2
    // gear" readies two if two can be chosen, so it is not always the better
    // one: the second may have to be an enemy's). Which ability and which gear
    // are chosen as the ability is played.
    EmpowerSpec empowerSpec() const override {
        return empowerCost(2, 2, Domain::Mind, /*any_domain=*/true);
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        c.energy = 1;
        return c;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "a gear": any gear on the board. Only an exhausted one is worth
        // readying, so only those are offered.
        auto exhausted_gear = [&](GameObjectId except) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isGear() || !obj.location.has_value() || !obj.is_exhausted) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
                if (id != except) out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        bool two = false;
        if (ctx.state.getObject(ctx.source).isEmpowered()) {
            const int which = pickMode(ctx, "Defender of Tomorrow: which ability", 2,
                                       {"Ready a gear", "Ready 2 gear"});
            if (which == -1) return;          // waiting on the controller
            two = which == 1;
        }
        GameObjectId first = kInvalidId, second = kInvalidId;
        if (two) {
            const auto both = pickTargetPair(
                ctx, "Defender of Tomorrow: ready 2 gear", exhausted_gear(kInvalidId),
                [&](GameObjectId picked) { return exhausted_gear(picked); });
            first = both.first;
            second = both.second;
        } else {
            first = pickTarget(ctx, "Defender of Tomorrow: ready a gear",
                               exhausted_gear(kInvalidId));
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        // Nothing chosen (yet): suspended, or no exhausted gear.
        if (first != kInvalidId) ctx.executor.readyObject(first);
        if (second != kInvalidId) ctx.executor.readyObject(second);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 936;
        d.def_id = R"RB(ven-149-166)RB";
        d.name = R"RB(Defender of Tomorrow)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-149/166)RB";
        d.collector_number = 149;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Body};
        d.tags = {R"RB(Jayce)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [2][A][A] ([2][A][A]: Empower me. Use only if not Empowered.)
[1], [E]: Ready a gear.
[Empowered][>] [1], [E]: Ready 2 gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/670b7efd367a42beba11c18fb59e240a7c574a57-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_936(CardRegistry& r) {
    r.registerCard(936, std::make_unique<DefenderOfTomorrow>());
}

}  // namespace riftbound
