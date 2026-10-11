#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <map>
#include <set>
#include <utility>
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

    // "As you play this, name a tag." The controller names it: the ability
    // goes through the chain as the card is played (AsYouPlayMe) and offers
    // the tags that are public knowledge — on units on the board, on the
    // legends and Chosen Champions, and on unit cards in trash or banishment —
    // the opponent's first, most common first. At most kMaxTagChoices are
    // offered (the int-coded MakeChoice answers have 16 slots).
    TriggerType triggerType() const override { return TriggerType::AsYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto tags = nameableTags(ctx.state, ctx.controller);
        auto name_it = [&](const std::string& tag) {
            ctx.state.getObject(ctx.source).string_state["named_tag"] = tag;
            ctx.events.logTrace("THE LIST: named tag \"" + tag + "\"");
        };
        if (tags.empty()) {
            ctx.events.logTrace("THE LIST: no tag to name");
            return;
        }
        if (!ctx.state.chain.resuming.has_value()) {   // no chain item to suspend on
            name_it(tags.front());
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kNamed) return;
        if (ri.resume_point != kAsking) {
            std::vector<Intent> options;
            for (size_t i = 0; i < tags.size(); ++i)
                options.push_back(valChoice(ctx.controller, static_cast<int>(i),
                                            "Name the tag: " + tags[i]));
            ctx.executor.requestChoice(ctx.controller, std::move(options), "The List: name a tag");
            ri.resume_point = kAsking;
            return;
        }
        int picked = takenValue(ctx, 0);
        if (picked < 0 || picked >= static_cast<int>(tags.size())) picked = 0;
        ri.resume_point = kNamed;
        name_it(tags[static_cast<size_t>(picked)]);
    }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost = ActivationCost{.exhaust = true};
        ab.targets = TargetRequirements{.count = 1, .must_be_unit = true};
        ab.needs_activation_time_target = true;  // the engine asks; pickTarget returns it
        return {ab};
    }

    // Asked without saying which copy is activating: any unit that carries a
    // tag at all (the engine itself always uses the overload below).
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/) const override {
        auto out = chooseableUnits(state, controller,
                                   [](const GameObject& o) { return !o.tags.empty(); });
        std::sort(out.begin(), out.end());
        return out;
    }
    // "a unit with the named tag": the tag named by the copy that is
    // activating. With no tag named, or no unit carrying it, the ability is
    // not offered.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/, GameObjectId source) const override {
        const std::string tag = namedTag(state, source);
        if (tag.empty()) return {};
        auto out = chooseableUnits(state, controller,
                                   [&](const GameObject& o) { return hasTag(o, tag); });
        std::sort(out.begin(), out.end());
        return out;
    }

    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        const std::string tag = namedTag(ctx.state, ctx.source);
        GameObjectId picked = pickTarget(ctx, "The List: -2 [M] (tag=" + tag + ")",
                                         enumerateLegalTargets(ctx.state, ctx.controller, 0,
                                                               ctx.source));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryMight(picked, -2);
        ctx.events.logTrace("THE LIST: -2 [M] this turn to a \"" + tag + "\" unit");
    }
private:
    static constexpr int kAsking = 20;
    static constexpr int kNamed = 21;
    static constexpr size_t kMaxTagChoices = 16;

    /// Tags `player` can name, most useful first: the opponent's (by how many
    /// of their units on the board carry each, then alphabetically), then
    /// the player's own.
    static std::vector<std::string> nameableTags(const GameState& state, PlayerId player) {
        std::vector<std::string> out;
        auto add = [&out](const std::string& tag) {
            if (out.size() < kMaxTagChoices &&
                std::find(out.begin(), out.end(), tag) == out.end()) out.push_back(tag);
        };
        for (PlayerId side : {opponent(player), player}) {
            // On the board, most common first.
            std::map<std::string, int> on_board;
            for (const auto& [id, obj] : state.objects)
                if (obj.isUnit() && obj.controller == side && obj.location.has_value())
                    for (const auto& t : obj.tags) on_board[t]++;
            std::vector<std::pair<std::string, int>> ranked(on_board.begin(), on_board.end());
            std::stable_sort(ranked.begin(), ranked.end(),
                             [](const auto& a, const auto& b) { return a.second > b.second; });
            for (const auto& [tag, n] : ranked) add(tag);
            // Legend, Chosen Champion, and unit cards in the public piles.
            const auto& ps = state.player(side);
            std::set<std::string> rest;
            auto collect = [&](GameObjectId id, bool units_only) {
                if (id == kInvalidId || !state.objectExists(id)) return;
                const auto& obj = state.getObject(id);
                if (units_only && !obj.isUnit()) return;
                rest.insert(obj.tags.begin(), obj.tags.end());
            };
            collect(ps.legend_zone, false);
            collect(ps.champion_zone, false);
            for (auto id : ps.trash) collect(id, true);
            for (auto id : ps.banishment) collect(id, true);
            for (const auto& tag : rest) add(tag);
        }
        return out;
    }
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
