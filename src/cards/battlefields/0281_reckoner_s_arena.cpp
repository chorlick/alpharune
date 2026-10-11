#include "cards/card.h"
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

class ReckonerSArena : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you hold here, activate the conquer effects of units here."
    //
    // CR 383.4.g: to "activate" a named triggered ability the player checks its
    // condition as if the named part had been fulfilled, and if it holds the
    // ability "is placed on the chain as if it had just triggered"
    // (383.4.g.1); no conquer actually occurs (383.4.g.2), so nothing that
    // watches for a real conquer fires (Red Brambleback's "trigger an
    // additional time", ScoreEvent listeners).
    //
    // So each unit's conquer ability becomes its OWN chain item — with its own
    // resume slots for "you may" / target choices, open to responses — exactly
    // the item TriggerManager::fireTrigger would add for a real conquer
    // (ChainManager::addAbility + fired_trigger). The non-conquer parts of each
    // condition ("[Empowered][>]", "if you've played ...") are checked by the
    // unit's own onTrigger when its item resolves, as they are for a real one.
    //
    // The engine queues it (EffectExecutor::queueTriggeredAbility ->
    // TriggerManager::fireTrigger, with its suppression and trigger-condition
    // checks). A bare executor with no engine behind it (unit tests) has no
    // trigger manager, so queueConquerAbility writes the item itself there.
    // Conquer effects an Equipment gives its unit ("When I conquer, ..." in the
    // gear's effect text) are run inline through Card::onEquippedTrigger, which
    // is how TriggerManager::fireEquippedTriggers runs them for a real conquer.
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }

    static bool isConquerTrigger(TriggerType t) {
        return t == TriggerType::WhenIConquer || t == TriggerType::WhenIConquerOrHold;
    }
    // LeBlanc, Everywhere at Once: "your [Temporary] effects at my battlefield
    // don't trigger" — the same gate TriggerManager::fireTrigger applies.
    static bool suppressed(CardContext& ctx, GameObjectId uid, BattlefieldId bf) {
        const auto& unit = ctx.state.getObject(uid);
        if (!unit.hasKeyword(Keyword::Temporary)) return false;
        for (const auto& [oid, other] : ctx.state.objects) {
            if (oid == uid || other.controller != unit.controller) continue;
            auto obf = other.battlefieldId();
            if (!obf || *obf != bf) continue;
            const Card* oc = ctx.registry->get(other.card_def_id);
            if (oc && oc->suppressesTemporaryTriggersHere()) return true;
        }
        return false;
    }
    static void queueConquerAbility(CardContext& ctx, GameObjectId uid, TriggerType fired) {
        auto& chain = ctx.state.chain;
        const bool was_empty = !chain.exists();
        const auto& unit = ctx.state.getObject(uid);
        ChainItem item;
        item.id = chain.allocateId();
        item.status = ChainItemStatus::Pending;
        item.source = uid;
        item.card_def_id = unit.card_def_id;
        item.controller = unit.controller;
        item.is_ability = true;
        item.is_activated_ability = false;
        item.fired_trigger = fired;
        chain.items.push_back(item);
        ctx.state.turn.oc_state = OpenClosedState::Closed;   // as addAbility does
        if (was_empty) ctx.events.emit(ChainCreatedEvent{item.id, unit.controller});
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.registry) return;
        // Which battlefield does this BF-card object represent?
        std::optional<BattlefieldId> my_bf;
        for (const auto& b : ctx.state.battlefields)
            if (b.card_object_id == ctx.source) { my_bf = b.id; break; }
        if (!my_bf) return;
        // Snapshot the holder's units here first: queuing and gear effects can
        // touch the object map.
        std::vector<GameObjectId> units;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto ubf = obj.battlefieldId();
            if (ubf && *ubf == *my_bf) units.push_back(id);
        }
        std::sort(units.begin(), units.end());
        int activated = 0;
        for (auto uid : units) {
            if (!ctx.state.objectExists(uid)) continue;
            {
                const auto& unit = ctx.state.getObject(uid);
                if (!unit.battlefieldId() || *unit.battlefieldId() != *my_bf) continue;
            }
            // The unit's own conquer abilities, one chain item each.
            const CardDefId def_id = ctx.state.getObject(uid).card_def_id;
            if (Card* uc = (def_id != kInvalidId ? ctx.registry->get(def_id) : nullptr)) {
                const PlayerId uctrl = ctx.state.getObject(uid).controller;
                for (auto t : uc->triggerTypes()) {
                    if (!isConquerTrigger(t)) continue;
                    if (ctx.executor.canQueueTriggers())
                        ctx.executor.queueTriggeredAbility(uid, uctrl, t);
                    else if (!suppressed(ctx, uid, *my_bf))
                        queueConquerAbility(ctx, uid, t);
                    else
                        continue;
                    ++activated;
                }
            }
            // Conquer effects granted by attached Equipment.
            const auto attached = ctx.state.getObject(uid).attachments;   // copy
            for (auto gid : attached) {
                if (!ctx.state.objectExists(gid) || !ctx.state.objectExists(uid)) continue;
                const auto& gear = ctx.state.getObject(gid);
                if (gear.card_def_id == kInvalidId) continue;
                Card* gc = ctx.registry->get(gear.card_def_id);
                if (!gc || !isConquerTrigger(gc->equippedTriggerType())) continue;
                CardContext sub{ctx.state, ctx.events, ctx.executor, ctx.controller, gid};
                sub.firing_trigger = gc->equippedTriggerType();
                sub.registry = ctx.registry;
                gc->onEquippedTrigger(sub, uid, {});
                ++activated;
            }
        }
        RIFT_TRACE(ctx.events, "RECKONER'S ARENA: activated " + std::to_string(activated) +
                            " conquer effect(s) of units here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 281;
        d.def_id = R"RB(ogn-286-298)RB";
        d.name = R"RB(Reckoner's Arena)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-286/298)RB";
        d.collector_number = 286;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, activate the conquer effects of units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6b7d867487efcbefa8c3d67043a839497ec50388-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_281(CardRegistry& r) {
    r.registerCard(281, std::make_unique<ReckonerSArena>());
}

} // namespace riftbound
