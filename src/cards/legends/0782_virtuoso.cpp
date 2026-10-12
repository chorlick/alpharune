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

class Virtuoso : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    // "if you spent [4] or more" is part of the trigger condition (CR
    // 383.2.a.1): asked when the spell has been played; a cheaper spell puts
    // nothing on the chain. What was spent on the spell is stamped on it as
    // it is played (TriggerManager::onCardPlayed);
    // PlayerState::last_spell_energy_spent is the fallback for a path that
    // names no spell.
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        int spent = state.player(controller).last_spell_energy_spent;
        if (subject != kInvalidId && state.objectExists(subject)) {
            const auto& c = state.getObject(subject).card_counters;
            if (const auto it = c.find("__energy_spent_on_play"); it != c.end()) spent = it->second;
        }
        return spent >= 4;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "it" is the spell that was played: the trigger's subject
        // (TriggerManager::onSpellResolved) — not whichever spell is newest in
        // the trash by the time this resolves.
        const GameObjectId spell_id = ctx.state.chain.resuming.has_value()
            ? ctx.state.chain.resuming->triggering_subject
            : findMostRecentlyPlayedSpell(ctx.state, ctx.controller);
        // Validation closure: "you may banish it" is legal iff that spell is
        // in a trash (what was spent on it was the trigger's condition).
        // Re-checked before AND after the agent's yes/no via confirmOptional.
        auto still_legal = [&]() -> bool {
            if (spell_id == kInvalidId || !ctx.state.objectExists(spell_id)) return false;
            return ctx.state.getObject(spell_id).zone == ZoneType::Trash;
        };

        int conf = confirmOptional(ctx, "Virtuoso: banish 4-cost spell?",
                                    still_legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf == -1) return;     // waiting for agent
        if (conf == 0) return;      // declined or invalidated
        // conf == 1: confirmed, proceed.
        if (!still_legal()) return;
        auto& spell = ctx.state.getObject(spell_id);

        // The spell is already in trash (resolved before this trigger's
        // ability did). Banish it through the executor, so it counts as a
        // card banished this turn and "when you banish a card" abilities see it.
        ctx.executor.banishObject(spell_id);

        auto& legend = ctx.state.getObject(ctx.source);
        legend.tracked_objects.push_back(spell_id);  // "banished with me"
        RIFT_TRACE(ctx.events, "VIRTUOSO: banished " + spell.name +
                             "; banished-with-me count=" +
                             std::to_string(legend.tracked_objects.size()));

        // Per CR triggered-ability rules, "Then, if there are four spells
        // banished with me, ..." is a CONTINUATION of the same triggered
        // ability — resolves atomically in the same chain item. NOT
        // deferrable to a later turn (verified against CR 359.3.f /
        // "Then,"-continuation examples 2026-05-17).
        if (legend.tracked_objects.size() >= 4) {
            // Payoff scope is STRICT: only the spells *Virtuoso* banished
            // ("banished with me") move back to trash. Other banished
            // spells in either player's banishment stay there.
            auto take_first_n = std::min<size_t>(4, legend.tracked_objects.size());
            std::vector<GameObjectId> to_trash(
                legend.tracked_objects.begin(),
                legend.tracked_objects.begin() + take_first_n);
            int moved = 0;
            for (auto id : to_trash) {
                if (!ctx.state.objectExists(id)) continue;
                auto& obj = ctx.state.getObject(id);
                auto& ownerPS = ctx.state.player(obj.owner);
                auto it = std::find(ownerPS.banishment.begin(),
                                     ownerPS.banishment.end(), id);
                if (it != ownerPS.banishment.end()) {
                    ownerPS.banishment.erase(it);
                    obj.zone = ZoneType::Trash;
                    ownerPS.trash.push_back(id);
                    settleInTrash(ctx.state, ctx.events, id);
                    moved++;
                }
            }
            // Remove the consumed entries from the tracked list.
            legend.tracked_objects.erase(legend.tracked_objects.begin(),
                                          legend.tracked_objects.begin() + take_first_n);

            RIFT_TRACE(ctx.events, "VIRTUOSO: 4 spells banished-with-me — "
                                 "routed " + std::to_string(moved) +
                                 " back to trash, channel 4 + draw 1");
            ctx.executor.channelRunes(ctx.controller, 4, /*enter_exhausted=*/false);
            ctx.executor.drawCards(ctx.controller, 1);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 782;
        d.def_id = R"RB(unl-226-219)RB";
        d.name = R"RB(Virtuoso)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-226/219)RB";
        d.collector_number = 226;
        d.artist = R"RB(Blaine Burgos)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Jhin)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you play a spell, if you spent [4] or more, you may banish it. Then, if there are four spells banished with me, put each in its trash, channel 4 runes, and draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/47f10693258104e9373396165e335014bf5783a2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_782(CardRegistry& r) {
    r.registerCard(782, std::make_unique<Virtuoso>());
}

} // namespace riftbound
