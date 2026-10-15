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

class EdgeOfNight : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    bool hasEquipAbility() const override { return true; }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .energy = 0, .power = 1, .power_domain = Domain::Chaos};
    }

    // "When you play this from face down, attach it to a unit you control
    // (here)." Played face down means played at the battlefield it was hidden
    // at; from hand it lands in base and nothing happens. The controller
    // chooses which of their units here (CR 383.3: at finalization).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override {
        return TriggerType::WhenYouPlayThis;
    }
    // "from face down" is part of the trigger condition: a play from hand
    // does not trigger at all. GameEngine::executePlayFromHidden marks the
    // card (and the turn) before the play is announced.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return playedFromFacedown(state, source);
    }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // "here" is the battlefield it was played from facedown at (the
        // engine's stamp, facedownPlayBattlefield) — not where the gear is by
        // now: the Cleanup that follows its entering recalls an unattached
        // gear from a battlefield to its base (CR 319.6, 323.6) before this
        // ability is finalized.
        std::vector<GameObjectId> here;
        if (ctx.state.objectExists(ctx.source)) {
            const auto& self = ctx.state.getObject(ctx.source);
            const auto bf = facedownPlayBattlefield(ctx.state, ctx.source);
            if (!self.attached_to.has_value() && self.location.has_value() && bf.has_value()) {
                for (const auto& [id, obj] : ctx.state.objects.onBoard()) {
                    if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                    if (obj.battlefieldId() != bf) continue;
                    here.push_back(id);
                }
            }
        }
        std::sort(here.begin(), here.end());
        const GameObjectId unit = pickTarget(ctx, "Edge of Night: attach to which unit here?", here);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (unit == kInvalidId) return;       // none / mistarget / waiting
        // Both ends must still be on the board (EffectExecutor::attachGear
        // checks it too; this is the early out before the attached_to read).
        if (!equipEndsOnBoard(ctx.state, ctx.source, unit)) return;
        if (ctx.state.getObject(ctx.source).attached_to.has_value()) return;
        attachFree(ctx, ctx.source, unit);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 460;
        d.def_id = R"RB(sfd-139-221)RB";
        d.name = R"RB(Edge of Night)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-139/221)RB";
        d.collector_number = 139;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play this from face down, attach it to a unit you control (here).
[Equip] [C] ([C]: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b3e385162bf3566618bb58b7a866eef846beefba-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_460(CardRegistry& r) {
    r.registerCard(460, std::make_unique<EdgeOfNight>());
}

} // namespace riftbound
