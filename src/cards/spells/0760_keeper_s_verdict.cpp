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

class KeeperSVerdict : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller == controller) continue;
            if (obj.isAtBattlefield()) out.push_back(id);
        }
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Keeper's Verdict: place enemy unit on deck",
                                 enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        int mode = pickMode(ctx, "Keeper's Verdict: top or bottom of deck", 2,
                            {"Top", "Bottom"});
        if (mode < 0) return;  // suspended

        auto& u = ctx.state.getObject(picked);
        PlayerId owner = u.owner;
        auto was_at = u.location.value_or(BaseLocation{owner});
        // Detach any attached gear first (CR 719.5).
        for (auto gid : std::vector<GameObjectId>(u.attachments)) {
            if (ctx.state.objectExists(gid)) ctx.executor.unattachGear(gid);
        }
        u.attachments.clear();
        u.attachment_might_bonus = 0;
        u.damage_marked = 0;
        u.combat_designation = CombatDesignation::None;
        u.location = std::nullopt;
        u.zone = ZoneType::MainDeck;
        auto& deck = ctx.state.player(owner).main_deck;
        if (mode == 0) deck.push_back(picked);            // top = back
        else           deck.insert(deck.begin(), picked); // bottom = front
        ctx.events.emit(LeftBoardEvent{picked, owner, CardType::Unit, was_at,
                                       ZoneType::MainDeck, /*was_killed=*/false});
        ctx.events.logTrace("KEEPER'S VERDICT: placed " + u.name +
                            (mode == 0 ? " on TOP" : " on BOTTOM") + " of deck");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 760;
        d.def_id = R"RB(unl-204-219)RB";
        d.name = R"RB(Keeper's Verdict)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-204/219)RB";
        d.collector_number = 204;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Poppy)RB"};
        d.energy_cost = 2;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose an enemy unit at a battlefield. Its owner places it on the top or bottom of their Main Deck.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/90dcafd8ca6cedd534416232ebd29c451f12107c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_760(CardRegistry& r) {
    r.registerCard(760, std::make_unique<KeeperSVerdict>());
}

} // namespace riftbound
