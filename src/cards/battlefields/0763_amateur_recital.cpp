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

class AmateurRecital : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you hold here, you may move a unit at a battlefield to its base."
    // Any unit at any battlefield, either player's; the holder decides and
    // picks as the ability is finalized (CR 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto movable = [&]() {
            auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                // Not from a battlefield units can't move to base from, and
                // not a unit enemy effects can't move.
                return u.isAtBattlefield() && !moveToBaseBlocked(ctx.state, u) &&
                       !(u.controller != ctx.controller && u.cant_be_moved_by_enemy);
            });
            return out;
        };
        const int go = confirmOptional(ctx, "Amateur Recital: move a unit at a battlefield "
                                            "to its base?",
                                       [&]() { return !movable().empty(); });
        GameObjectId t = kInvalidId;
        if (go == 1) t = pickTarget(ctx, "Amateur Recital: move which unit to its base", movable());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go < 1 || t == kInvalidId) return;   // declined / none / mistarget / waiting
        ctx.executor.moveToBase(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 763;
        d.def_id = R"RB(unl-207-219)RB";
        d.name = R"RB(Amateur Recital)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-207/219)RB";
        d.collector_number = 207;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, you may move a unit at a battlefield to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9372a1574aa077e8b02cc6f00a1add060ae3b520-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_763(CardRegistry& r) {
    r.registerCard(763, std::make_unique<AmateurRecital>());
}

} // namespace riftbound
