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

// "As an additional cost to play this, kill a friendly unit. Play a unit from
// your trash that costs no more Energy and no more Power than the killed unit,
// ignoring its cost."
//
// The unit to kill is chosen and killed as the spell is played (CR 355: an
// additional cost is paid then, so it stays paid if the spell is countered).
// The unit to play is chosen as the spell resolves, from the trash as it is
// then — the killed unit itself included.

class HeedlessResurrection : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It chooses no unit of its controller's (the play gate, CR 355 -> 356).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        (void)state; (void)controller; (void)object;
        return FinalizeChoice::No;
    }
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId me) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.isUnit() && obj.controller == me && obj.location.has_value())
                out.push_back(id);
        return out;
    }
    // Playable only if there is a friendly unit to kill.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !friendlyUnits(state, controller).empty();
    }
    // resume_data, clear of the Card::pick* helpers' slots; resume_point
    // kAwaitRevive while the unit to play is being asked.
    static constexpr size_t kKilledSlot = 7;    // killed unit's id + 1 (cost paid)
    static constexpr size_t kRevivedSlot = 8;   // unit to play: id + 1, or -1 for none
    static constexpr int kAwaitRevive = 30;

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;   // needs the chain's resume slot
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() <= kRevivedSlot) ri.resume_data.push_back(0);

        // The additional cost: kill a friendly unit.
        if (ri.resume_data[kKilledSlot] == 0) {
            GameObjectId victim = pickTarget(ctx, "Heedless Resurrection: kill a friendly unit",
                                             friendlyUnits(ctx.state, ctx.controller));
            if (victim == kInvalidId || !ctx.state.objectExists(victim)) return;  // waiting / none
            if (costDueNow(ctx)) {
                RIFT_TRACE(ctx.events, "HEEDLESS RESURRECTION: killing " +
                                     ctx.state.getObject(victim).name + " (additional cost)");
                ctx.executor.killObject(victim);
                while (ri.resume_data.size() <= kRevivedSlot) ri.resume_data.push_back(0);
                ri.resume_data[kKilledSlot] = static_cast<int32_t>(victim.raw()) + 1;
            }
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (ri.resume_data[kKilledSlot] == 0) return;   // the cost was not paid
        const auto killed = GameObjectId::fromRaw(ri.resume_data[kKilledSlot] - 1);

        // "Play a unit from your trash that costs no more Energy and no more
        //  Power than the killed unit."
        auto revivable = [&]() {
            std::vector<GameObjectId> out;
            const CardDefId killed_def = ctx.state.getObject(killed).card_def_id;
            if (killed_def == kInvalidId) return out;
            const auto& kdef = ctx.executor.cardDB().get(killed_def);
            for (auto cid : ctx.state.player(ctx.controller).trash) {
                if (!ctx.state.objectExists(cid)) continue;
                const auto& obj = ctx.state.getObject(cid);
                if (!obj.isUnit() || obj.card_def_id == kInvalidId) continue;
                const auto& cdef = ctx.executor.cardDB().get(obj.card_def_id);
                if (cdef.energy_cost <= kdef.energy_cost && cdef.power_cost <= kdef.power_cost)
                    out.push_back(cid);
            }
            return out;
        };
        if (ri.resume_data[kRevivedSlot] == 0) {
            const auto cands = revivable();
            if (ri.resume_point == kAwaitRevive) {
                GameObjectId pick = takenObject(ctx);
                if (std::find(cands.begin(), cands.end(), pick) == cands.end())
                    pick = cands.empty() ? kInvalidId : cands.front();
                ri.resume_data[kRevivedSlot] = pick == kInvalidId ? -1
                    : static_cast<int32_t>(pick.raw()) + 1;
                ri.resume_point = 0;
            } else if (cands.empty()) {
                ri.resume_data[kRevivedSlot] = -1;
            } else {
                std::vector<Intent> opts;
                for (auto cid : cands) opts.push_back(objChoice(ctx.controller, cid));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                    "Heedless Resurrection: play a unit from your trash", PromptKind::ChooseObject);
                ri.resume_point = kAwaitRevive;
                return;
            }
        }
        if (ri.resume_data[kRevivedSlot] <= 0) return;   // nothing to play
        const auto revived = GameObjectId::fromRaw(ri.resume_data[kRevivedSlot] - 1);
        // Its controller's base or a battlefield they control (CR 355.2.a).
        const auto where = playLocations(ctx.state, ctx.controller);
        // playLocations leaves out a battlefield units can't be played to
        // (Rockfall Path; a player who may only play units to base).
        const int mode = pickMode(ctx, "Heedless Resurrection: where to play it",
                                  where.count(), where.labels);
        if (mode < 0 || mode >= where.count()) return;   // waiting for the agent
        ri.resume_data[kRevivedSlot] = -1;               // played once
        ctx.executor.playIgnoringCost(ctx.controller, revived, where.locations[mode]);
        RIFT_TRACE(ctx.events, "HEEDLESS RESURRECTION: played a unit from trash, ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 704;
        d.def_id = R"RB(unl-142-219)RB";
        d.name = R"RB(Heedless Resurrection)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-142/219)RB";
        d.collector_number = 142;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
As an additional cost to play this, kill a friendly unit.
Play a unit from your trash that costs no more Energy and no more Power than the killed unit, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/92419d138824a7d46155b59f0e5903196d68d9f7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_704(CardRegistry& r) {
    r.registerCard(704, std::make_unique<HeedlessResurrection>());
}

} // namespace riftbound
