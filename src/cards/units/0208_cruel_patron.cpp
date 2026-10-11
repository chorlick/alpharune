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

class CruelPatron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "As an additional cost to play me, kill a friendly unit."
    // The engine has no hook for a "kill a unit" additional play cost, so:
    //  - canBePlayed gates the play on having a friendly unit to kill;
    //  - the kill itself is the cost of my WhenYouPlayMe ability, chosen and
    //    paid as that ability goes on the chain (CR 383.3), before anyone can
    //    respond — not as it resolves.
    // APPROX: the kill happens just after I enter the board, not before.
    bool canBePlayed(const GameState& state, PlayerId player) const override {
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == player && obj.location.has_value())
                return true;
        return false;
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!costAlreadyPaid(ctx)) {
            std::vector<GameObjectId> friendly;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                    friendly.push_back(id);
            }
            std::sort(friendly.begin(), friendly.end());
            GameObjectId picked = pickTarget(ctx, "Cruel Patron (kill a friendly unit)", friendly);
            if (picked == kInvalidId) return;   // waiting / no other unit
            if (costDueNow(ctx)) ctx.executor.killObject(picked);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 208;
        d.def_id = R"RB(ogn-208-298)RB";
        d.name = R"RB(Cruel Patron)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-208/298)RB";
        d.collector_number = 208;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.might = 6;
        d.ability_text = R"RB(As an additional cost to play me, kill a friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c1c2c08fa0032a245c1ffee6ba29fff2826bd468-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_208(CardRegistry& r) {
    r.registerCard(208, std::make_unique<CruelPatron>());
}

} // namespace riftbound
