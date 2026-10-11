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

// "You may exhaust your legend as an additional cost to play me. When you play
//  me, if you paid the additional cost, move any number of your units to an
//  open battlefield."
// ENGINE-GAP: OptionalAdditionalCost can only charge Energy / Power / a
// discard, so "exhaust your legend" cannot be paid during the play itself. It
// is offered and paid when the play trigger is finalized instead — before
// anyone can respond to the trigger, but after the unit has entered.
class BardMercurial : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    static GameObjectId readyLegend(const GameState& state, PlayerId controller) {
        const GameObjectId id = state.player(controller).legend_zone;
        if (id == kInvalidId || !state.objectExists(id)) return kInvalidId;
        return state.getObject(id).is_exhausted ? kInvalidId : id;
    }
    // CR 170.11.c: an open battlefield is unoccupied and uncontrolled.
    static std::vector<BattlefieldId> openBattlefields(const GameState& state) {
        std::vector<BattlefieldId> out;
        for (const auto& bf : state.battlefields) {
            if (bf.controller.has_value()) continue;
            bool occupied = false;
            for (const auto& [id, obj] : state.objects)
                if (obj.isUnit() && obj.battlefieldId() == bf.id) { occupied = true; break; }
            if (!occupied) out.push_back(bf.id);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The additional cost: offered while the legend is ready, paid as the
        // ability is finalized.
        auto still_legal = [&]() {
            return costAlreadyPaid(ctx) || readyLegend(ctx.state, ctx.controller) != kInvalidId;
        };
        int conf = confirmOptional(ctx,
            "Bard, Mercurial: exhaust your legend as an additional cost?", still_legal);
        bool paid = costAlreadyPaid(ctx);
        if (conf == 1 && costDueNow(ctx)) {
            const GameObjectId legend = readyLegend(ctx.state, ctx.controller);
            if (legend != kInvalidId) {
                ctx.executor.exhaustObject(legend);
                paid = true;
            }
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (conf != 1 || !paid) return;

        // "move any number of your units to an open battlefield": the
        // controller picks the battlefield, then the units, as this resolves.
        const auto open = openBattlefields(ctx.state);
        if (open.empty()) return;
        BattlefieldId dest = open.front();
        if (open.size() > 1) {
            std::vector<std::string> labels;
            for (auto bf : open) labels.push_back("Battlefield #" + std::to_string(bf));
            const int mode = pickMode(ctx, "Bard, Mercurial: which open battlefield",
                                      static_cast<int>(open.size()), labels);
            if (mode < 0) return;             // waiting for the agent
            if (static_cast<size_t>(mode) < open.size()) dest = open[static_cast<size_t>(mode)];
        }
        auto movable = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isUnit() && obj.controller == ctx.controller &&
                   obj.location.has_value() && obj.battlefieldId() != dest;
        };
        std::vector<GameObjectId> units;
        for (auto& [id, obj] : ctx.state.objects)
            if (movable(id)) units.push_back(id);
        auto picked = selectAnyNumber(ctx, "Bard, Mercurial: move a unit there", units);
        if (!picked.has_value()) return;      // waiting for the agent
        int moved = 0;
        for (auto id : *picked) {
            if (!movable(id)) continue;
            ctx.executor.moveToBattlefield(id, dest);
            ++moved;
        }
        ctx.events.logTrace("BARD, MERCURIAL: exhausted legend -> moved " +
                            std::to_string(moved) + " unit(s) to an open battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 540;
        d.def_id = R"RB(sfd-228-221)RB";
        d.name = R"RB(Bard, Mercurial)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-228/221)RB";
        d.collector_number = 228;
        d.artist = R"RB(Felicia Chen)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bard)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(You may exhaust your legend as an additional cost to play me.
When you play me, if you paid the additional cost, move any number of your units to an open battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9c33fbc2dc78bb6af3d31fab0c0ca58e1141141d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_540(CardRegistry& r) {
    r.registerCard(540, std::make_unique<BardMercurial>());
}

} // namespace riftbound
