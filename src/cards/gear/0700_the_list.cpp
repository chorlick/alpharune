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

    // "As you play this, name a tag." Named as the card is played (CR 355.1):
    // no chain item, no priority window, before its cost. Any tag of the
    // game's list may be named (CR 763, 763.1), not only those on cards the
    // player can see (testing phase D46).
    void asYouPlayChoices(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& tags = allTags();
        std::vector<std::string> labels;
        labels.reserve(tags.size());
        for (const auto& t : tags) labels.push_back("Name the tag: " + t);
        const int picked = ctx.executor.chooseValueNow(ctx.controller, labels,
                                                       "The List: name a tag");
        const std::string& tag = tags[static_cast<size_t>(
            picked >= 0 && picked < static_cast<int>(tags.size()) ? picked : 0)];
        ctx.state.getObject(ctx.source).string_state["named_tag"] = tag;
        RIFT_TRACE(ctx.events, "THE LIST: named tag \"" + tag + "\"");
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
        RIFT_TRACE(ctx.events, "THE LIST: -2 [M] this turn to a \"" + tag + "\" unit");
    }
private:
    /// The tags that exist in Riftbound (CR 763.1), in that order.
    static const std::vector<std::string>& allTags() {
        static const std::vector<std::string> kTags = {
            "Ahri", "Akali", "Akshan", "Ambessa", "Anivia", "Annie", "Aphelios", "Ashe", "Azir",
            "Bandle City", "Bard", "Bilgewater", "Bird", "Blitzcrank", "Bomb", "Caitlyn", "Cat",
            "Darius", "Demacia", "Demon", "Diana", "Dog", "Dr. Mundo", "Dragon", "Draven", "Ekko",
            "Elite", "Equipment", "Evelynn", "Ezreal", "Fae", "Fiora", "Fizz", "Freljord", "Galio",
            "Gangplank", "Garen", "Graves", "Heimerdinger", "Hwei", "Icathia", "Illaoi", "Ionia",
            "Irelia", "Ivern", "Ixtal", "Janna", "Jarvan IV", "Jax", "Jayce", "Jhin", "Jinx",
            "K'Sante", "Kai'Sa", "Karma", "Karthus", "Katarina", "Kathkan", "Kayle", "Kayn",
            "Kennen", "Kha'Zix", "Kog'Maw", "LeBlanc", "Lee Sin", "Leona", "Lillia", "Lucian",
            "Lulu", "Lux", "Malzahar", "Master Yi", "Mech", "Mel", "Miss Fortune", "Mordekaiser",
            "Morgana", "Mount Targon", "Nami", "Nasus", "Neeko", "Nidalee", "Nilah", "Nocturne",
            "Noxus", "Orianna", "Ornn", "Piltover", "Pirate", "Poppy", "Poro", "Pyke", "Qiyana",
            "Recruit", "Rek'Sai", "Rell", "Renata Glasc", "Renekton", "Rengar", "Riven", "Rumble",
            "Sentinel", "Seraphine", "Sett", "Shadow Isles", "Shen", "Shurima", "Sivir", "Sona",
            "Soraka", "Spider", "Spirit", "Swain", "Syndra", "Taric", "Teemo", "The Void",
            "Trifarian", "Tryndamere", "Twisted Fate", "Udyr", "Vayne", "Vex", "Vi", "Viktor",
            "Volibear", "Warwick", "Xerath", "Xin Zhao", "Yasuo", "Yone", "Yordle", "Yuumi", "Zaun",
            "Zed", "Ziggs", "Zilean"};
        return kTags;
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
