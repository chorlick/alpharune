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

class EmperorSDivide : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // The play-time target names the battlefield; which of the units there
    // move is then asked unit by unit, "done" on every prompt. "Any number"
    // includes zero (CR 355.13), so the requirement is optional: from hand the
    // spell is playable with no friendly unit at a battlefield (and then does
    // nothing). From facedown too: with no friendly unit at its battlefield it
    // is played choosing none (judge ruling 2026-10-11, plans/09 9.Z4: CR
    // 355.13 wins over 811.1.d's "no valid targets"); with units there the
    // hidden generator offers one play per target there (CR 811.1.d.2).
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true,
                                   .must_be_at_battlefield = true,
                                   .optional = true};
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming.value();

        // The battlefield is fixed by the play-time target on case 0.
        auto build_choices = [&](BattlefieldId bf, std::vector<GameObjectId>& units) {
            // Only a unit this spell can move to its base (not one that
            // "can't move to base", not at "Units can't move from here to
            // base": effectCanMoveToBase), and each unit once: resume_data
            // holds the battlefield and then the units already chosen, so a
            // move that did not happen is never offered again.
            units.clear();
            for (auto& [id, obj] : ctx.state.objects.onBoard()) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                auto ubf = obj.battlefieldId();
                if (!ubf || *ubf != bf) continue;
                if (!effectCanMoveToBase(ctx.state, ctx.controller, obj)) continue;
                if (std::find(ri.resume_data.begin() + (ri.resume_data.empty() ? 0 : 1),
                              ri.resume_data.end(),
                              static_cast<int32_t>(id.raw())) != ri.resume_data.end()) continue;
                units.push_back(id);
            }
            std::sort(units.begin(), units.end());
            std::vector<Intent> out;
            for (auto uid : units) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {uid};
                out.push_back(std::move(c));
            }
            // Trailing "done" choice (empty chosen_objects).
            Intent done;
            done.type = IntentType::MakeChoice;
            done.player = ctx.controller;
            done.chosen_value = 0;   // nameable by value, not only by position
            done.choice_label = "Emperor's Divide: move no more units";
            out.push_back(std::move(done));
            return out;
        };

        switch (ri.resume_point) {
        case 0: {
            if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
            auto bf_opt = ctx.state.getObject(targets[0]).battlefieldId();
            if (!bf_opt) return;
            BattlefieldId bf = *bf_opt;
            // Stash the battlefield id for re-entry.
            ri.resume_data = {static_cast<int32_t>(bf.raw())};
            std::vector<GameObjectId> units;
            auto choices = build_choices(bf, units);
            if (choices.size() <= 1) return;  // no friendly unit here that can move
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Emperor's Divide: move a unit to base, or stop");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (ri.resume_data.empty()) return;
            BattlefieldId bf = BattlefieldId::fromRaw(ri.resume_data[0]);

            // "done" = empty chosen_objects.
            if (!choice || choice->chosen_objects.empty()) return;
            auto uid = choice->chosen_objects[0];
            ri.resume_data.push_back(static_cast<int32_t>(uid.raw()));   // chosen: not offered again
            if (ctx.state.objectExists(uid)) {
                ctx.executor.moveToBase(uid);
                if (ctx.state.getObject(uid).isAtBase())
                    RIFT_TRACE(ctx.events, "EMPEROR'S DIVIDE: moved " +
                                        ctx.state.getObject(uid).name + " to base");
            }
            // Offer the remaining units (+ done) again.
            std::vector<GameObjectId> units;
            auto choices = build_choices(bf, units);
            if (choices.size() <= 1) return;  // none left
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Emperor's Divide: move a unit to base, or stop");
            // stay at resume_point = 1
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 366;
        d.def_id = R"RB(sfd-043-221)RB";
        d.name = R"RB(Emperor's Divide)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-043/221)RB";
        d.collector_number = 43;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Move any number of friendly units at a battlefield to their base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/80f367f06549d597c7b5a77be73eb647eab34f59-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_366(CardRegistry& r) {
    r.registerCard(366, std::make_unique<EmperorSDivide>());
}

} // namespace riftbound
