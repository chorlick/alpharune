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

class JannaSavior : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Reaction] (Play any time ...)" — the card itself has Reaction.
    bool hasReactionTiming() const override { return true; }

    // "When you play me, heal your units here, then move up to one enemy unit
    // from here to its base." "here" = the battlefield I was played to.
    // "here" is wherever I was played — a battlefield or my base (CR 464.2.b).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto here = ctx.state.getObject(ctx.source).location;
        const PlayerId me = ctx.controller;
        auto enemies_here = [&]() {
            if (!here.has_value()) return std::vector<GameObjectId>{};
            return chooseableUnits(ctx.state, me, [&](const GameObject& o) {
                return o.controller != me && *o.location == *here;
            });
        };
        // "up to one": the controller may move none. Not a leading "may":
        // declining it leaves the heal (Card::mayIsNotTheWholeAbility).
        mayIsNotTheWholeAbility(ctx);
        int conf = confirmOptional(ctx, "Janna: move an enemy unit from here to its base?",
                                   [&]() { return !enemies_here().empty(); });
        GameObjectId enemy = kInvalidId;
        if (conf == 1)
            enemy = pickTarget(ctx, "Janna: move up to one enemy unit from here to base",
                               enemies_here());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf == -1) return;               // waiting for the agent
        if (!here.has_value()) return;        // I left the board: no "here"

        // Heal ALL friendly units here.
        std::vector<GameObjectId> mine;
        for (auto& [id, obj] : ctx.state.objects.onBoard())
            if (obj.isUnit() && obj.controller == me && obj.location.has_value() &&
                *obj.location == *here)
                mine.push_back(id);
        for (auto id : mine) ctx.executor.healObject(id);
        // Then move up to one enemy unit from here to its base.
        if (conf == 1 && enemy != kInvalidId) ctx.executor.moveToBase(enemy);
        RIFT_TRACE(ctx.events, "JANNA SAVIOR: healed friendly units here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 376;
        d.def_id = R"RB(sfd-053-221)RB";
        d.name = R"RB(Janna, Savior)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-053/221)RB";
        d.collector_number = 53;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Janna)RB", R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve, including to a battlefield you control.)
When you play me, heal your units here, then move up to one enemy unit from here to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/191f04059bbfd69dfe63d36010586597bb3549b2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_376(CardRegistry& r) {
    r.registerCard(376, std::make_unique<JannaSavior>());
}

} // namespace riftbound
