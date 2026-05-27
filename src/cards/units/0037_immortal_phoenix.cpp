#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class ImmortalPhoenix : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you kill a unit with a spell, you may pay [1][R] to play me from
    //  your trash." Wired via WhenYouKillAUnitWithASpell (dispatched by
    //  TriggerManager::onUnitDied to the resolving spell controller's cards incl.
    //  trash). We pay [1][R] (1 energy + 1 Fury power) and play self from trash.
    TriggerType triggerType() const override {
        return TriggerType::WhenYouKillAUnitWithASpell;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.state.getObject(ctx.source).zone != ZoneType::Trash) return;
        LocationId base{BaseLocation{ctx.controller}};
        auto readyRunes = [&](std::optional<Domain> dom) {
            int n = 0;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isRune() || obj.controller != ctx.controller) continue;
                if (obj.is_exhausted || !obj.location.has_value()) continue;
                if (*obj.location != base) continue;
                if (dom) { bool m = false; for (auto d : obj.domains) if (d == *dom) m = true;
                           if (!m) continue; }
                ++n;
            }
            return n;
        };
        // Affordable: 1 ready Fury rune (recycled for [R]) + >=2 ready runes
        // total (one of those exhausted for the [1] energy).
        auto still_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).zone == ZoneType::Trash &&
                   readyRunes(Domain::Fury) >= 1 && readyRunes(std::nullopt) >= 2;
        };
        int conf = confirmOptional(ctx, "Immortal Phoenix: pay [1][R] to play from trash?",
                                   still_legal);
        if (conf == -1) return;   // waiting for agent
        if (conf < 1) return;     // declined / unaffordable
        // Pay [R] (recycle a Fury rune), then [1] (exhaust a remaining ready rune).
        if (!payOnePower(ctx, ctx.controller, Domain::Fury)) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (obj.is_exhausted || !obj.location.has_value() || *obj.location != base) continue;
            obj.is_exhausted = true;  // [1] energy
            break;
        }
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), ctx.source);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, ctx.source, std::nullopt);
        ctx.events.logTrace("IMMORTAL PHOENIX: paid [1][R] -> played from trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 37;
        d.def_id = R"RB(ogn-037-298)RB";
        d.name = R"RB(Immortal Phoenix)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-037/298)RB";
        d.collector_number = 37;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Spirit)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.assault_value = 2;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault 2] (+2 [M] while I'm an attacker.)
When you kill a unit with a spell, you may pay [1][R] to play me from your trash.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b623ae985bf5f362b6d8d4a17e9b8146aeae3c3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_37(CardRegistry& r) {
    r.registerCard(37, std::make_unique<ImmortalPhoenix>());
}

} // namespace riftbound
