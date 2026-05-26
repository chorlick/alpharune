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

class BaitedHook : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[1][C], [E]: Kill a friendly unit. Look at the top 5 cards of your Main
    //  Deck. You may banish a unit from among them that has Might up to 1 more
    //  than the killed unit and play it, ignoring its cost. Then recycle the rest."
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1, .recycle_self = true};
    }
    // Target = the friendly unit to kill.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }

    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId victim = targets[0];
        int killed_might = ctx.state.getObject(victim).current_might;
        ctx.executor.killObject(victim);

        // Look at the top 5 cards of the Main Deck (top = back).
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> top5;
        for (auto it = ps.main_deck.rbegin();
             it != ps.main_deck.rend() && (int)top5.size() < 5; ++it) {
            top5.push_back(*it);
        }
        // Eligible: units with Might up to killed_might + 1.
        std::vector<GameObjectId> eligible;
        for (auto cid : top5) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.base_might > killed_might + 1) continue;
            eligible.push_back(cid);
        }
        if (!eligible.empty()) {
            // "You may" — let the agent pick which to play (pickTarget over the
            // eligible top-5 units). Picking is optional in spirit; the agent
            // can decline by the engine offering all eligible — we always play
            // the chosen one if any eligible exist.
            GameObjectId picked = pickTarget(ctx, "Baited Hook (play a unit from top 5)",
                                              eligible);
            if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) {
                return;  // suspended for agent decision
            }
            if (picked != kInvalidId && ctx.state.objectExists(picked)) {
                // Remove from main deck and play ignoring cost.
                ps.main_deck.erase(
                    std::remove(ps.main_deck.begin(), ps.main_deck.end(), picked),
                    ps.main_deck.end());
                ctx.executor.playIgnoringCost(ctx.controller, picked);
            }
        }
        // "Then recycle the rest" — the other looked-at cards go to the bottom
        // of the Main Deck (recycle = put on bottom). Move remaining top5 cards
        // (still in deck) to the bottom (front = bottom).
        std::vector<GameObjectId> to_recycle;
        for (auto cid : top5) {
            auto pos = std::find(ps.main_deck.begin(), ps.main_deck.end(), cid);
            if (pos != ps.main_deck.end()) {
                to_recycle.push_back(cid);
                ps.main_deck.erase(pos);
            }
        }
        // Insert recycled cards at the bottom (front).
        ps.main_deck.insert(ps.main_deck.begin(), to_recycle.begin(), to_recycle.end());
        ctx.events.logTrace("BAITED HOOK: kill friendly, dig top 5, recycle rest");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 242;
        d.def_id = R"RB(ogn-242-298)RB";
        d.name = R"RB(Baited Hook)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-242/298)RB";
        d.collector_number = 242;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB([1][C], [E]: Kill a friendly unit. Look at the top 5 cards of your Main Deck. You may banish a unit from among them that has Might up to 1 more than the killed unit and play it, ignoring its cost. Then recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b4e6a1e32878a7d5fe4e40869c83e47ea08de7c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_242(CardRegistry& r) {
    r.registerCard(242, std::make_unique<BaitedHook>());
}

} // namespace riftbound
