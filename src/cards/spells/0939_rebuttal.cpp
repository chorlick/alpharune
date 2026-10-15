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

class Rebuttal : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Choose a spell with Energy cost no more than [4]. You may pay [A]. If
    //  you do, gain control of it and you may make new choices for it.
    //  Otherwise, counter it."
    // The spell is a target (CR 355.9.a.2), chosen as Rebuttal is played (CR
    // 355.5) among the finalized spells on the chain whose printed Energy cost
    // is [4] or less. The [A] is decided as it resolves.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    static bool qualifies(const ChainItem& ci, const CardDB& db) {
        return ci.is_spell &&
               (ci.card_def_id == kInvalidId || db.get(ci.card_def_id).energy_cost <= 4);
    }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/,
                         const CardDB& db) const override {
        return anyChainItem(state, [&](const ChainItem& ci) { return qualifies(ci, db); });
    }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return anyChainItem(state, [](const ChainItem& ci) { return ci.is_spell; });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const CardDB& db = ctx.executor.cardDB();
        const auto spell = pickChainItem(
            ctx, "Rebuttal: choose a spell with Energy cost no more than [4]",
            chainItemChoices(ctx, [&](const ChainItem& ci) { return qualifies(ci, db); }));
        if (!spell.has_value()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (*spell == kInvalidId) return;     // a mistarget: nothing is done
        auto still_there = [&]() { return chainItem(ctx.state, *spell) != nullptr; };
        const int pay = confirmOptional(ctx, "Rebuttal: pay [A] to gain control of the spell",
            [&]() { return still_there() && ctx.executor.canPayAnyPower(ctx.controller, 1); });
        if (pay == -1) return;   // waiting on the agent
        if (!still_there()) return;
        if (pay == 1 && ctx.executor.payAnyPower(ctx.controller, 1)) {
            std::string name = "spell";
            for (auto& ci : ctx.state.chain.items) {
                if (ci.id != *spell) continue;
                ci.controller = ctx.controller;
                if (ctx.state.objectExists(ci.source)) {
                    auto& obj = ctx.state.getObject(ci.source);
                    obj.controller = ctx.controller;
                    name = obj.name;
                }
                break;
            }
            RIFT_TRACE(ctx.events, "REBUTTAL: paid [A] - gained control of " + name);
            // "You may make new choices for it" (CR 750-755).
            ctx.executor.makeNewChoices(*spell);
            return;
        }
        // "Otherwise, counter it."
        counterChosenItem(ctx, *spell);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 939;
        d.def_id = R"RB(ven-152-166)RB";
        d.name = R"RB(Rebuttal)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-152/166)RB";
        d.collector_number = 152;
        d.artist = R"RB(小牛设计)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Mel)RB"};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a spell with Energy cost no more than [4]. You may pay [A]. If you do, gain control of it and you may make new choices for it. Otherwise, counter it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2df67fe2ba17f0796ff1f9c03b85d2db48fe954b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_939(CardRegistry& r) {
    r.registerCard(939, std::make_unique<Rebuttal>());
}

}  // namespace riftbound
