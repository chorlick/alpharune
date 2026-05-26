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
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Validation closure: "you may banish it" is legal iff the
        // most-recently-played spell exists, is in trash, and its
        // play-time energy spend was ≥4. Re-checked before AND after
        // the agent's yes/no via confirmOptional.
        auto still_legal = [&]() -> bool {
            // Read the snapshot stored on this trigger's chain item
            // (Phase 6q+: TriggerManager captures the triggering
            // spell's spent at trigger-add time so we don't read a
            // clobbered PlayerState::last_spell_energy_spent at
            // resume time). Fall back to the PlayerState field for
            // legacy code paths that don't snapshot.
            int spent = ctx.state.chain.resuming.has_value()
                ? ctx.state.chain.resuming->triggering_spell_energy_spent
                : 0;
            if (spent == 0) {
                spent = ctx.state.player(ctx.controller).last_spell_energy_spent;
            }
            if (spent < 4) return false;
            GameObjectId sid = findMostRecentlyPlayedSpell(
                ctx.state, ctx.controller);
            if (sid == kInvalidId) return false;
            if (!ctx.state.objectExists(sid)) return false;
            return ctx.state.getObject(sid).zone == ZoneType::Trash;
        };

        int conf = confirmOptional(ctx, "Virtuoso: banish 4-cost spell?",
                                    still_legal);
        if (conf == -1) return;     // waiting for agent
        if (conf == 0) return;      // declined or invalidated
        // conf == 1: confirmed, proceed.

        GameObjectId spell_id = findMostRecentlyPlayedSpell(ctx.state, ctx.controller);
        if (spell_id == kInvalidId) return;  // should be live per validation
        auto& spell = ctx.state.getObject(spell_id);

        // The spell is already in trash (resolved before this trigger's
        // ability did). Reroute it from trash → banishment.
        auto& owner_ps = ctx.state.player(spell.owner);
        auto& tr = owner_ps.trash;
        auto it = std::find(tr.begin(), tr.end(), spell_id);
        if (it == tr.end()) return;  // shouldn't happen
        tr.erase(it);
        spell.zone = ZoneType::Banishment;
        owner_ps.banishment.push_back(spell_id);

        auto& legend = ctx.state.getObject(ctx.source);
        legend.tracked_objects.push_back(spell_id);  // "banished with me"
        ctx.events.logTrace("VIRTUOSO: banished " + spell.name +
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
                    moved++;
                }
            }
            // Remove the consumed entries from the tracked list.
            legend.tracked_objects.erase(legend.tracked_objects.begin(),
                                          legend.tracked_objects.begin() + take_first_n);

            ctx.events.logTrace("VIRTUOSO: 4 spells banished-with-me — "
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
