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

/// [Temporary] — CR 816.1.b: "At the start of this permanent's controller's
/// Beginning Phase, before scoring, kill this." Queued by
/// TriggerManager::queueTemporaryAbilities; "before scoring" holds because the
/// Beginning Step's chain is resolved, with its Cleanups, before the Scoring
/// Step's Hold task (GameEngine::beginningStep). "Kill this": the permanent it
/// triggered on, whoever controls it by now, if it is still on the board — a
/// permanent that left in response is a new object wherever it is (CR 124.1)
/// and is not killed (ChainItem::source_left_board).
class TemporaryAbility : public Card {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;
        if (ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->source_left_board)
            return;   // it left the board since and is a new object
        ctx.executor.killObject(ctx.source);
    }
private:
    const CardDef def_ = keywordDef("Temporary");
};

/// [Quick-Draw] — CR 819.1.a "a Triggered Ability keyword"; 819.1.d
/// "functionally short for [Reaction] and 'When you play this, attach it to a
/// Unit you control.'" The unit is chosen as the ability is finalized (CR
/// 383.3, 403.2): the opponent may respond before the gear attaches, and a
/// unit gone by then is a mistarget (testing phase D14). With no unit to
/// choose the ability is removed (CR 403.4).
class QuickDrawAbility : public Card {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller;
        });
        const GameObjectId unit = pickTarget(ctx, "Quick-Draw: attach it to a unit you control",
                                             units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 403)
        if (unit == kInvalidId) return;       // a mistarget
        if (!ctx.state.objectExists(ctx.source) ||
            !ctx.state.getObject(ctx.source).location.has_value()) return;
        RIFT_TRACE(ctx.events, "QUICK_DRAW: attaching " + ctx.state.getObject(ctx.source).name +
                            " to " + ctx.state.getObject(unit).name);
        ctx.executor.attachGear(ctx.source, unit);
    }
private:
    const CardDef def_ = keywordDef("Quick-Draw");
};

}  // namespace

Card* keywordAbilityCard(Keyword keyword) {
    static VisionAbility vision;
    static WeaponmasterAbility weaponmaster;
    static TemporaryAbility temporary;
    static QuickDrawAbility quick_draw;
    switch (keyword) {
        case Keyword::Vision:       return &vision;
        case Keyword::Weaponmaster: return &weaponmaster;
        case Keyword::Temporary:    return &temporary;
        case Keyword::QuickDraw:    return &quick_draw;
        default:                    return nullptr;
    }
}

int keywordAbilityInstances(const GameObject& object, Keyword keyword, bool own_card_fires) {
    int n = 0;
    if (object.keywords.has(keyword) && !own_card_fires) ++n;
    for (const auto& ae : object.aura_effects)
        if (ae.keyword == keyword) ++n;
    // CR 819.2: "Multiple instances of Quick-Draw do not trigger separately".
    if (keyword == Keyword::QuickDraw) n = std::min(n, 1);
    return n;
}

} // namespace riftbound
