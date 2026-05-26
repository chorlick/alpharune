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

    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true,
                                   .must_be_at_battlefield = true};
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming.value();

        // The battlefield is fixed by the play-time target on case 0.
        auto build_choices = [&](BattlefieldId bf, std::vector<GameObjectId>& units) {
            units.clear();
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                auto ubf = obj.battlefieldId();
                if (ubf && *ubf == bf) units.push_back(id);
            }
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
            out.push_back(std::move(done));
            return out;
        };

        switch (ri.resume_point) {
        case 0: {
            if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
            auto bf_opt = ctx.state.getObject(targets[0]).battlefieldId();
            if (!bf_opt) return;
            BattlefieldId bf = *bf_opt;
            std::vector<GameObjectId> units;
            auto choices = build_choices(bf, units);
            if (choices.size() <= 1) return;  // no friendly units here
            // Stash the battlefield id for re-entry.
            ri.resume_data = {static_cast<int32_t>(bf)};
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Emperor's Divide: move a unit to base, or stop");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (ri.resume_data.empty()) return;
            BattlefieldId bf = static_cast<BattlefieldId>(ri.resume_data[0]);

            // "done" = empty chosen_objects.
            if (!choice || choice->chosen_objects.empty()) return;
            auto uid = choice->chosen_objects[0];
            if (ctx.state.objectExists(uid)) {
                ctx.executor.moveToBase(uid);
                ctx.events.logTrace("EMPEROR'S DIVIDE: moved a unit to base");
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
