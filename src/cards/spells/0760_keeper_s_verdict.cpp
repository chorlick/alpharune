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

class KeeperSVerdict : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose an enemy unit at a battlefield. Its owner places it on the top or
    //  bottom of their Main Deck."
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
            // "Can't be chosen by enemy spells and abilities."
            if (obj.untargetable_by_enemy) continue;
            if (obj.isAtBattlefield()) out.push_back(id);
        }
        return out;
    }
    // resume_point while the unit's OWNER is being asked top or bottom (clear
    // of the Card::pick* helpers' values).
    static constexpr int kAwaitOwner = 30;
    static constexpr int kDone = 31;

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming;
        if (ri.has_value() && ri->resume_point == kDone) return;
        const auto locked = lockedPlayTarget(ctx, targets);
        const GameObjectId picked = locked ? *locked
            : pickTarget(ctx, "Keeper's Verdict: place enemy unit on deck",
                         enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        const PlayerId owner = ctx.state.getObject(picked).owner;

        // "Its owner places it": the owner decides, not this spell's controller.
        // A token is never placed anywhere — it ceases to exist as it leaves
        // the board (CR 183.1) — so its owner has nothing to decide.
        bool to_top = true;
        if (ri.has_value() && !ctx.state.getObject(picked).isToken()) {
            if (ri->resume_point != kAwaitOwner) {
                ctx.executor.requestChoice(owner,
                    {valChoice(owner, 0, "Keeper's Verdict: top of your Main Deck"),
                     valChoice(owner, 1, "Keeper's Verdict: bottom of your Main Deck")},
                    "Keeper's Verdict: top or bottom of your Main Deck");
                ri->resume_point = kAwaitOwner;
                return;
            }
            to_top = takenValue(ctx, 0) != 1;
            ri->resume_point = kDone;
        }

        auto& u = ctx.state.getObject(picked);
        if (!u.location.has_value()) return;   // it left the board meanwhile
        auto was_at = *u.location;
        const PlayerId controller = u.controller;
        // Detach any attached gear first (CR 719.5).
        for (auto gid : std::vector<GameObjectId>(u.attachments)) {
            if (ctx.state.objectExists(gid)) ctx.executor.unattachGear(gid);
        }
        u.attachments.clear();
        u.attachment_might_bonus = 0;
        u.damage_marked = 0;
        u.combat_designation = CombatDesignation::None;
        u.location = std::nullopt;
        const bool token = u.isToken();
        if (token) {
            // CR 183.1: a token that leaves the board ceases to exist.
            u.zone = ZoneType::Banishment;
        } else if (to_top) {
            // Through deck_ops, so the knowledge tracker sees a known card
            // land on top / bottom.
            deck::putOnTop(ctx.state, owner, picked, deck::Place::Public, knowledge::everyone());
        } else {
            deck::putOnBottom(ctx.state, owner, picked, deck::Place::Public, knowledge::everyone());
        }
        ctx.events.emit(LeftBoardEvent{picked, controller, CardType::Unit, was_at,
                                       token ? ZoneType::Banishment : ZoneType::MainDeck,
                                       /*was_killed=*/false});
        ctx.events.logTrace("KEEPER'S VERDICT: " + u.name +
                            (token ? " (a token) ceased to exist"
                                   : to_top ? " placed on TOP of deck"
                                            : " placed on BOTTOM of deck"));
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
