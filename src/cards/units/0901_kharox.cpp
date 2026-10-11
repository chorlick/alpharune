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

class Kharox : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // "[Empower] [6][P][P]"
    EmpowerSpec empowerSpec() const override { return empowerCost(6, 2, Domain::Chaos); }
    // "When I become [Empowered], choose an opponent. They [Burn 3]. Then you
    //  may do this: Choose a unit in their trash and play it, ignoring its
    //  cost." (1v1: the opponent is the only choice.) No location is printed,
    //  so the unit goes to its new controller's base or a battlefield they
    //  control, their choice (CR 355.2.a).
    TriggerType triggerType() const override { return TriggerType::WhenIBecomeEmpowered; }
    static std::vector<GameObjectId> unitsInTrash(const GameState& state, PlayerId p) {
        std::vector<GameObjectId> out;
        for (auto cid : state.player(p).trash)
            if (state.objectExists(cid) && state.getObject(cid).isUnit()) out.push_back(cid);
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId opp = opponent(ctx.controller);
        ctx.executor.burn(opp, 3);
        // "Then you may do this: ..." — a reflexive trigger (CR 388): after
        // the burn it goes on the chain as an item of its own; the "may", the
        // unit and where it goes are chosen as that item is finalized (CR
        // 388.4), with the burned cards already in the trash.
        ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0);
    }
    bool hasReflexiveTrigger() const override { return true; }
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& /*data*/) override {
        const PlayerId opp = opponent(ctx.controller);
        int conf = confirmOptional(ctx, "Kharox: play a unit from their trash?",
                                   [&]() { return !unitsInTrash(ctx.state, opp).empty(); });
        GameObjectId picked = kInvalidId;
        int where = -2;
        if (conf == 1) {
            picked = pickTarget(ctx, "Kharox: unit in their trash", unitsInTrash(ctx.state, opp));
            if (picked != kInvalidId)
                where = pickEffectPlayLocation(*this, ctx, "Kharox: play it to");
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 388.4)
        if (conf < 1 || picked == kInvalidId || where < 0) return;
        if (!ctx.state.objectExists(picked) ||
            ctx.state.getObject(picked).zone != ZoneType::Trash) return;   // left the trash
        // playIgnoringCost takes it out of the trash.
        ctx.executor.playIgnoringCost(ctx.controller, picked,
                                      effectPlayLocation(ctx.state, ctx.controller, where));
        RIFT_TRACE(ctx.events, "KHAROX: played " + ctx.state.getObject(picked).name +
                            " from the opponent's trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 901;
        d.def_id = R"RB(ven-114-166)RB";
        d.name = R"RB(Kharox)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-114/166)RB";
        d.collector_number = 114;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 6;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [6][P][P] ([6][P][P]: Empower me. Use only if not Empowered.)
When I become [Empowered], choose an opponent. They [Burn 3]. Then you may do this: Choose a unit in their trash and play it, ignoring its cost. (To Burn 3, they put the top 3 cards of their Main Deck into their trash.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2453848e7069e994498c828d72c33df6905f38e5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_901(CardRegistry& r) {
    r.registerCard(901, std::make_unique<Kharox>());
}

}  // namespace riftbound
