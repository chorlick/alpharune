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

class RumbleHotheaded : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // ── Part 1: "Your Mechs each have [Assault]." ──
    // Each Rumble on board grants one instance (CR 807.2: they sum); a bare
    // aura [Assault] is Assault 1, and the engine's aura fold adds the +1 [M]
    // while the Mech is an attacker. Rumble is a Mech, so this is also where
    // his own Assault comes from (it is not a printed keyword of his).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            if (!self.location.has_value()) return;
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit() || u.controller != controller || !u.location.has_value()) continue;
                if (!hasTag(u, "Mech")) continue;
                auraKeyword(u, sid, Keyword::Assault, 1);
            }
        });
    }
    // ── Part 2: conquer -> recur a Mech ──
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // "you may recycle another friendly unit to play a Mech from your
        // trash": the controller chooses both. Recycling the unit is the cost
        // of the ability, paid as the trigger is finalized (CR 383.3).
        auto recyclable = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;   // "another"
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                out.push_back(id);
            }
            return out;
        };
        auto trash_mechs = [&]() {
            std::vector<GameObjectId> out;
            for (auto id : ctx.state.player(ctx.controller).trash) {
                if (!ctx.state.objectExists(id)) continue;
                const auto& obj = ctx.state.getObject(id);
                if (obj.isUnit() && obj.card_def_id != kInvalidId && hasTag(obj, "Mech"))
                    out.push_back(id);
            }
            return out;
        };
        auto still_legal = [&]() -> bool {
            return costAlreadyPaid(ctx) || (!recyclable().empty() && !trash_mechs().empty());
        };
        auto& ri = ctx.state.chain.resuming.value();
        // resume_data[kMightSlot]: 1 + the Might of the unit recycled as the
        // cost (0 = not paid yet). Past the choice helpers' own slots.
        constexpr size_t kMightSlot = Card::kChoiceStateSlot + 1;
        auto paid_might = [&]() -> int {
            return ri.resume_data.size() > kMightSlot ? ri.resume_data[kMightSlot] - 1 : -1;
        };

        int conf = confirmOptional(ctx,
            "Rumble: recycle a friendly unit to play a Mech from trash?", still_legal);
        std::pair<GameObjectId, GameObjectId> pick{kInvalidId, kInvalidId};
        if (conf == 1) {
            // (At resolution the recycled unit is gone, so the first pick comes
            // back as kInvalidId; only the Mech is still needed then.)
            pick = pickTargetPair(ctx, "Rumble: unit to recycle, then Mech to play", recyclable(),
                                  [&](GameObjectId) { return trash_mechs(); });
            if (pick.first != kInvalidId && pick.second != kInvalidId &&
                paid_might() < 0 && costDueNow(ctx)) {
                const int might = ctx.state.getObject(pick.first).current_might;
                ctx.executor.recycleCards(ctx.controller, {pick.first});
                if (ri.resume_data.size() <= kMightSlot) ri.resume_data.resize(kMightSlot + 1, 0);
                ri.resume_data[kMightSlot] = might + 1;
            }
        }
        if (finalizingChoices(ctx)) return;   // choices and the cost end here (CR 383.3)
        if (conf < 1 || paid_might() < 0) return;   // waiting / declined / cost not paid
        const GameObjectId trash_mech = pick.second;
        if (trash_mech == kInvalidId) return;       // the Mech left the trash in response

        // "Reduce its Energy cost by the Might of the unit you recycled." The
        // rest of the Mech's cost is still owed (CR 356.1.b.2).
        const EffectExecutor::EffectPlayCost reduced{.energy_reduction = paid_might()};
        if (!ctx.executor.canPlayPayingCost(ctx.controller, trash_mech, reduced)) return;

        // CR 355.2.a: the Mech goes to its controller's base or to a
        // battlefield they control (and may be played to) — chosen as it is
        // played, through the shared helper every effect play uses.
        const int where = pickEffectPlayLocation(*this, ctx, "Rumble: play the Mech to");
        if (where == -1) return;   // waiting for the agent
        // playPayingCost takes the card out of the trash only if it is played.
        if (ctx.executor.playPayingCost(ctx.controller, trash_mech,
                                        effectPlayLocation(ctx.state, ctx.controller, where), reduced))
            RIFT_TRACE(ctx.events, "RUMBLE: conquer -> recycled a unit, played a Mech from trash");
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
