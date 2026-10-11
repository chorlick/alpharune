#include "engine/keyword_abilities.h"

#include "cards/card_helpers.h"
#include "cards/units/weaponmaster_base.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

namespace riftbound {

namespace {

CardDef keywordDef(const char* name) {
    CardDef d;
    d.id = kInvalidId;          // not a card: never in the registry or the CardDB
    d.name = name;
    return d;
}

/// [Vision] — CR 817.1.b: "When this is played, [Predict]." The controller
/// looks at the top card of their Main Deck and may recycle it
/// (EffectExecutor::predict asks).
class VisionAbility : public Card {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.predict(ctx.controller, 1);
    }
private:
    const CardDef def_ = keywordDef("Vision");
};

/// [Weaponmaster] — CR 821.1.c, for a unit that has the keyword without a
/// card class that implements it (a token, or a grant from a continuous
/// effect). The ability itself is WeaponmasterUnit's: the same "may", the
/// same choice of Equipment, the same payment.
class WeaponmasterAbility : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = keywordDef("Weaponmaster");
};

}  // namespace

Card* keywordAbilityCard(Keyword keyword) {
    static VisionAbility vision;
    static WeaponmasterAbility weaponmaster;
    switch (keyword) {
        case Keyword::Vision:       return &vision;
        case Keyword::Weaponmaster: return &weaponmaster;
        default:                    return nullptr;
    }
}

int keywordAbilityInstances(const GameObject& object, Keyword keyword, bool own_card_fires) {
    int n = 0;
    if (object.keywords.has(keyword) && !own_card_fires) ++n;
    for (const auto& ae : object.aura_effects)
        if (ae.keyword == keyword) ++n;
    return n;
}

} // namespace riftbound
