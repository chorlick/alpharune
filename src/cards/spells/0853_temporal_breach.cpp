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

class TemporalBreach : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Banish a unit, then its owner plays it to the same location, ignoring
    //  its cost."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // The unit locked in as the spell was played (from facedown: one at
        // that battlefield, CR 811.1.d.2), else asked now.
        const auto locked = lockedPlayTarget(ctx, targets);
        GameObjectId picked = locked ? *locked
            : pickTarget(ctx, "Temporal Breach", enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        const PlayerId owner = ctx.state.getObject(picked).owner;
        const auto where = ctx.state.getObject(picked).location;
        const bool token = ctx.state.getObject(picked).isToken();
        if (!where.has_value()) return;

        // Attached gear does not follow the unit out of play.
        const std::vector<GameObjectId> gear = ctx.state.getObject(picked).attachments;
        for (auto g : gear) ctx.executor.unattachGear(g);

        ctx.executor.banishObject(picked);
        // CR 183.1: a banished token ceases to exist — nothing to play back.
        if (token) return;

        // It comes back as a new object, with no damage, buffs, statuses or
        // roles: banishObject made it one as it left (CR 124.1,
        // EffectExecutor::leftBoard), so nothing is reset here.
        // "To the same location" does not lift a "can't": where its owner
        // cannot play a unit — Rockfall Path ("Units can't be played here."),
        // a base that is not the owner's, any battlefield under Mageseeker
        // Warden — the play is not possible and the unit stays banished
        // (Vendetta FAQ, Temporal Breach on a unit taken with Hostile
        // Takeover: "The unit will remain in Banishment"; CR 054.1). The
        // executor's play refuses such a location (unitMayBePlayedTo).
        if (!ctx.executor.playIgnoringCost(owner, picked, *where))
            RIFT_TRACE(ctx.events, "TEMPORAL BREACH: " + ctx.state.getObject(picked).name +
                                " can't be played to that location — it stays banished");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 853;
        d.def_id = R"RB(ven-066-166)RB";
        d.name = R"RB(Temporal Breach)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-066/166)RB";
        d.collector_number = 66;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
Banish a unit, then its owner plays it to the same location, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7c670dc3d997c52b90d67043989b53c67a72f4c4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_853(CardRegistry& r) {
    r.registerCard(853, std::make_unique<TemporalBreach>());
}

}  // namespace riftbound
