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

class TheList : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    static std::string namedTag(const GameState& state, GameObjectId gear) {
        if (!state.objectExists(gear)) return "";
        auto& g = state.getObject(gear);
        auto it = g.string_state.find("named_tag");
        return it == g.string_state.end() ? "" : it->second;
    }

    // "As you play this, name a tag." Heuristic: most common tag among enemy
    // units currently in play (ties broken by lexical order for determinism).
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        PlayerId opp = opponent(ctx.controller);
        std::map<std::string, int> counts;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            for (const auto& t : obj.tags) counts[t]++;
        }
        std::string best;
        int best_n = 0;
        for (const auto& [tag, n] : counts) {   // std::map → lexical iteration
            if (n > best_n) { best_n = n; best = tag; }
        }
        if (!best.empty()) {
            ctx.state.getObject(ctx.source).string_state["named_tag"] = best;
            ctx.events.logTrace("THE LIST: named tag \"" + best + "\"");
        } else {
            ctx.events.logTrace("THE LIST: no enemy units to name a tag from");
        }
    }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost = ActivationCost{.exhaust = true};
        ab.targets = TargetRequirements{.count = 1, .must_be_unit = true};
        ab.needs_activation_time_target = true;  // pickTarget reads named tag
        return {ab};
    }

    // Broad gate (instance unknown here): any unit carrying any tag is a
    // plausible target. The precise named-tag filter happens in onActivate.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/,
        int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (obj.isUnit() && obj.location.has_value() && !obj.tags.empty())
                out.push_back(id);
        }
        return out;
    }

    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        std::string tag = namedTag(ctx.state, ctx.source);
        // Legal targets: units that carry the named tag.
        std::vector<GameObjectId> legal;
        if (!tag.empty()) {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || !obj.location.has_value()) continue;
                for (const auto& t : obj.tags) {
                    if (t == tag) { legal.push_back(id); break; }
                }
            }
        }
        GameObjectId picked = pickTarget(ctx, "The List: -2 [M] (tag=" + tag + ")",
                                         legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryMight(picked, -2);
        ctx.events.logTrace("THE LIST: -2 [M] this turn to a \"" + tag + "\" unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 700;
        d.def_id = R"RB(unl-138-219)RB";
        d.name = R"RB(The List)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-138/219)RB";
        d.collector_number = 138;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(As you play this, name a tag. (For example, Miss Fortune, Demacia, and Poro are tags.)
[E]: Give a unit with the named tag -2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f35b49faeb444071445662c9af13710c00a4569c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_700(CardRegistry& r) {
    r.registerCard(700, std::make_unique<TheList>());
}

} // namespace riftbound
