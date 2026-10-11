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

class Deathgrip : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Kill a friendly unit. If you do, give +[M] equal to its Might to another
    //  friendly unit this turn. Draw 1."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    // {unit to kill, unit to pump} for every ordered pair of friendly units.
    // With a single friendly unit the spell is still playable on it alone
    // (kill it, no pump, draw 1).
    std::optional<std::vector<std::vector<GameObjectId>>> enumerateLegalTargetSets(
        const GameState& state, PlayerId controller) const override {
        const auto units = enumerateLegalTargets(state, controller);
        std::vector<std::vector<GameObjectId>> sets;
        for (auto kill : units)
            for (auto pump : units)
                if (pump != kill) sets.push_back({kill, pump});
        if (units.size() == 1) sets.push_back({units[0]});
        return sets;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Roles, not positions (Card::chosenTarget): the unit to kill is slot
        // 0, the unit to pump slot 1; either is kInvalidId once illegal.
        const GameObjectId kill = chosenTarget(ctx, targets, 0);
        const GameObjectId pump = chosenTarget(ctx, targets, 1);
        auto still_legal = [](GameObjectId id) { return id != kInvalidId; };
        if (still_legal(kill)) {
            const int might = ctx.state.getObject(kill).current_might;
            ctx.executor.killObject(kill);
            // "If you do": a replacement effect may have kept it on the board.
            const bool died = !ctx.state.getObject(kill).location.has_value();
            if (died && still_legal(pump)) ctx.executor.giveTemporaryMight(pump, might);
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 484;
        d.def_id = R"RB(sfd-163-221)RB";
        d.name = R"RB(Deathgrip)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-163/221)RB";
        d.collector_number = 163;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Kill a friendly unit. If you do, give +[M] equal to its Might to another friendly unit this turn.
Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/695304200c6fd3b287bd319ac564d31936b23d66-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_484(CardRegistry& r) {
    r.registerCard(484, std::make_unique<Deathgrip>());
}

} // namespace riftbound
