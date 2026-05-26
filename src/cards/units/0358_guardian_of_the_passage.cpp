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

class GuardianOfThePassage : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I hold, you may return a unit or gear from your trash to your hand."
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto candidates = [&]() {
            std::vector<GameObjectId> out;
            for (auto id : ctx.state.player(ctx.controller).trash) {
                if (!ctx.state.objectExists(id)) continue;
                const auto& obj = ctx.state.getObject(id);
                if (obj.isUnit() || obj.isGear()) out.push_back(id);
            }
            return out;
        };
        int conf = confirmOptional(ctx,
            "Guardian of the Passage: return a unit/gear from trash to hand?",
            [&]() { return !candidates().empty(); });
        if (conf == -1) return;  // waiting for agent
        if (conf == 0) return;   // declined / none available

        GameObjectId pick = pickTarget(ctx,
            "Guardian: choose a unit or gear from trash", candidates());
        if (pick == kInvalidId || !ctx.state.objectExists(pick)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), pick);
        if (it == ps.trash.end()) return;
        ps.trash.erase(it);
        auto& obj = ctx.state.getObject(pick);
        obj.zone = ZoneType::Hand;
        obj.location = std::nullopt;
        ps.hand.push_back(pick);
        ctx.events.logTrace("GUARDIAN OF THE PASSAGE: returned " + obj.name +
                            " from trash to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 358;
        d.def_id = R"RB(sfd-035-221)RB";
        d.name = R"RB(Guardian of the Passage)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-035/221)RB";
        d.collector_number = 35;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.ability_text = R"RB(When I hold, you may return a unit or gear from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f6b932aa510c92d014422d31e27b1505e54abe20-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_358(CardRegistry& r) {
    r.registerCard(358, std::make_unique<GuardianOfThePassage>());
}

} // namespace riftbound
