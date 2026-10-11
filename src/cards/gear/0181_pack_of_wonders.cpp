#include "cards/card.h"
#include "cards/card_helpers.h"
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

class PackOfWonders : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost = ActivationCost{.exhaust = true};
        ab.targets = TargetRequirements{.count = 1};
        ab.needs_activation_time_target = true;  // pickTarget excludes self
        return {ab};
    }
    // "another friendly gear, unit, or facedown card": the controller's units
    // and gear on the board other than this Pack, and the cards they have
    // hidden at a battlefield (no board location; GameObject::hidden_at).
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/, GameObjectId source) const override {
        std::vector<GameObjectId> out;
        // (Every object: the board and facedown cards.)
        for (auto& [id, obj] : state.objects) {
            if (id == source || obj.controller != controller) continue;   // "another"
            const bool on_board = obj.location.has_value() && (obj.isUnit() || obj.isGear());
            const bool facedown = obj.is_hidden && obj.hidden_at != kInvalidId;
            if (on_board || facedown) out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        const auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0, ctx.source);
        GameObjectId picked = pickTarget(ctx, "Pack of Wonders: return a friendly gear, unit "
                                              "or facedown card to hand", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (std::find(legal.begin(), legal.end(), picked) == legal.end()) return;
        auto& obj = ctx.state.getObject(picked);
        if (!obj.is_hidden) {
            ctx.executor.bounceToHand(picked);
            RIFT_TRACE(ctx.events, "PACK OF WONDERS: returned a friendly gear/unit to hand");
            return;
        }
        // A facedown card: out of the battlefield's facedown zone, into its
        // owner's hand (it was never on the board, so nothing "leaves" it).
        for (auto& bf : ctx.state.battlefields)
            bf.facedown.erase(std::remove(bf.facedown.begin(), bf.facedown.end(), picked),
                              bf.facedown.end());
        obj.is_hidden = false;
        obj.hidden_at = kInvalidId;
        obj.zone = ZoneType::Hand;
        obj.location = std::nullopt;
        ctx.state.player(obj.owner).hand.push_back(picked);
        knowledge::moved(ctx.state, picked, deck::Place::Facedown, deck::Place::Hand,
                         knowledge::nobody());
        RIFT_TRACE(ctx.events, "PACK OF WONDERS: returned a facedown card to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 181;
        d.def_id = R"RB(ogn-181-298)RB";
        d.name = R"RB(Pack of Wonders)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-181/298)RB";
        d.collector_number = 181;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([E]: Return another friendly gear, unit, or facedown card to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a0830f326c49abe7fb1d3c9787e7ce5d7b776eec-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_181(CardRegistry& r) {
    r.registerCard(181, std::make_unique<PackOfWonders>());
}

} // namespace riftbound
