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

class TornadoWarrior : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // [Hidden] is engine-handled.
    // "When you play me from face down, you may empower something here.
    //  Disempower it at end of turn."
    // The "may" and the object are chosen as the ability is finalized, before
    // anyone can respond (CR 403.1-403.2; testing phase D54).
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "from face down" is part of the trigger condition (playedFromFacedown:
    // this play's stamp, testing phase B2): a play from hand puts nothing on
    // the chain.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return playedFromFacedown(state, source);
    }
    std::vector<GameObjectId> empowerableHere(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        if (!ctx.state.objectExists(ctx.source)) return out;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return out;
        for (const auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() && !obj.isGear()) continue;
            if (!obj.location.has_value() || *obj.location != *self.location) continue;
            if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
            int cap = 1;
            if (ctx.registry && obj.card_def_id != kInvalidId)
                if (const Card* c = ctx.registry->get(obj.card_def_id)) cap = c->maxEmpower();
            if (obj.empowered >= cap) continue;   // empowering it would do nothing
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::AtEndOfTurn) {
            // "Disempower it at end of turn": the delayed ability (finding G13).
            if (finalizingChoices(ctx)) return;   // nothing to choose
            const auto& ri = ctx.state.chain.resuming;
            const GameObjectId it = ri.has_value() ? ri->triggering_subject : kInvalidId;
            if (it == kInvalidId || !ctx.state.objectExists(it)) return;
            ctx.executor.disempower(it);
            RIFT_TRACE(ctx.events, "TORNADO WARRIOR: end of turn -> disempower " +
                                ctx.state.getObject(it).name);
            return;
        }
        const int conf = confirmOptional(ctx, "Tornado Warrior: empower something here?",
                                         [&]() { return !empowerableHere(ctx).empty(); });
        if (conf < 1) return;   // waiting, declined or nothing to empower
        const GameObjectId picked = pickTarget(ctx, "Tornado Warrior: empower", empowerableHere(ctx));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 403)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.empower(picked);
        atEndOfTurnAbout(ctx, cardDefId(), picked);   // "Disempower it at end of turn."
        RIFT_TRACE(ctx.events, "TORNADO WARRIOR: empowered " +
                            ctx.state.getObject(picked).name + " until end of turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 886;
        d.def_id = R"RB(ven-099-166)RB";
        d.name = R"RB(Tornado Warrior)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-099/166)RB";
        d.collector_number = 99;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me from face down, you may empower something here. Disempower it at end of turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f9b2ffecc997670713bffd3168e5578b5fb7a014-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_886(CardRegistry& r) {
    r.registerCard(886, std::make_unique<TornadoWarrior>());
}

}  // namespace riftbound
