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

class TailCloakedMatriarch : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // "[Empower] [2][P]"
    EmpowerSpec empowerSpec() const override { return empowerCost(2, 1, Domain::Chaos); }
    // "When I become [Empowered], you may choose a unit in your trash with
    //  Energy cost no more than [3] and Power cost no more than [A]. Play it
    //  to your base, ignoring its cost."
    TriggerType triggerType() const override { return TriggerType::WhenIBecomeEmpowered; }
    std::vector<GameObjectId> candidates(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        for (auto cid : ctx.state.player(ctx.controller).trash) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit() || obj.card_def_id == kInvalidId) continue;
            const auto& def = ctx.executor.cardDB().get(obj.card_def_id);
            if (def.energy_cost <= 3 && def.power_cost <= 1) out.push_back(cid);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int conf = confirmOptional(ctx, "Tail-Cloaked Matriarch: play a unit from your trash?",
                                   [&]() { return !candidates(ctx).empty(); });
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Tail-Cloaked Matriarch: unit in trash",
                                         candidates(ctx));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (ctx.state.getObject(picked).zone != ZoneType::Trash) return;   // left the trash
        // "Play it to your base": playIgnoringCost takes it out of the trash.
        ctx.executor.playIgnoringCost(ctx.controller, picked,
                                      LocationId{BaseLocation{ctx.controller}});
        RIFT_TRACE(ctx.events, "TAIL-CLOAKED MATRIARCH: played " +
                            ctx.state.getObject(picked).name + " from trash to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 891;
        d.def_id = R"RB(ven-104-166)RB";
        d.name = R"RB(Tail-Cloaked Matriarch)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-104/166)RB";
        d.collector_number = 104;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [2][P] ([2][P]: Empower me. Use only if not Empowered.)
When I become [Empowered], you may choose a unit in your trash with Energy cost no more than [3] and Power cost no more than [A]. Play it to your base, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/91a04e2131245c81c2eac486e63925592b0e253a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_891(CardRegistry& r) {
    r.registerCard(891, std::make_unique<TailCloakedMatriarch>());
}

}  // namespace riftbound
