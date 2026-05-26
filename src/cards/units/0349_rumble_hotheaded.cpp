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

class RumbleHotheaded : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // ── Part 1: "Your Mechs each have [Assault]." ──
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        GameObjectId self_id = kInvalidId;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            self_id = sid;
            break;
        }
        if (self_id == kInvalidId) return;
        for (auto& [uid, u] : state.objects) {
            if (!u.isUnit() || u.controller != controller) continue;
            bool is_mech = false;
            for (auto& tag : u.tags)
                if (tag == "Mech") { is_mech = true; break; }
            if (!is_mech) continue;
            GameObject::AuraEffect ae;
            ae.source = self_id;
            ae.keyword = Keyword::Assault;
            u.aura_effects.push_back(ae);
        }
    }

    // ── Part 2: conquer -> recur a Mech ──
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // Legal if there's ANOTHER friendly unit on board to recycle AND a Mech
        // unit in the controller's trash to play.
        auto find_recyclable = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                return id;
            }
            return kInvalidId;
        };
        auto find_trash_mech = [&]() -> GameObjectId {
            const auto& ps = ctx.state.player(ctx.controller);
            for (auto it = ps.trash.rbegin(); it != ps.trash.rend(); ++it) {
                if (!ctx.state.objectExists(*it)) continue;
                const auto& obj = ctx.state.getObject(*it);
                if (!obj.isUnit()) continue;
                for (auto& tag : obj.tags)
                    if (tag == "Mech") return *it;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() -> bool {
            return find_recyclable() != kInvalidId &&
                   find_trash_mech() != kInvalidId;
        };
        int conf = confirmOptional(ctx,
            "Rumble: recycle a friendly unit to play a Mech from trash?",
            still_legal);
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined / not legal

        GameObjectId recyclable = find_recyclable();
        GameObjectId trash_mech = find_trash_mech();
        if (recyclable == kInvalidId || trash_mech == kInvalidId) return;
        // Recycle the chosen friendly unit (to bottom of deck).
        ctx.executor.recycleCards(ctx.controller, {recyclable});
        // Play the Mech from trash (free — see header note on cost reduction).
        // Remove from trash first so playIgnoringCost finds a clean source.
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), trash_mech);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, trash_mech,
                                      LocationId{BaseLocation{ctx.controller}});
        ctx.events.logTrace("RUMBLE: conquer -> recycled a unit, played a Mech "
                            "from trash (cost reduction approximated as free)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 349;
        d.def_id = R"RB(sfd-026-221)RB";
        d.name = R"RB(Rumble, Hotheaded)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-026/221)RB";
        d.collector_number = 26;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mech)RB", R"RB(Yordle)RB", R"RB(Rumble)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB(Your Mechs each have [Assault]. (+1 [M] while we're attackers.)
When I conquer, you may recycle another friendly unit to play a Mech from your trash. Reduce its Energy cost by the Might of the unit you recycled.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e211311ecb1475e87d3d94eba4a6774b4e900091-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_349(CardRegistry& r) {
    r.registerCard(349, std::make_unique<RumbleHotheaded>());
}

} // namespace riftbound
