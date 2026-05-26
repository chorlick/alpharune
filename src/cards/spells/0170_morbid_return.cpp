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

class MorbidReturn : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Return a unit from your trash to your hand." The target lives in trash,
    // not on the board, so defer selection to resolve-time pickTarget over the
    // trash (the default board-only enumerator can't see it).
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> trash_units;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).isUnit()) trash_units.push_back(cid);
        }
        if (trash_units.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Morbid Return (unit from trash)",
                                          trash_units);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it == ps.trash.end()) return;
        ps.trash.erase(it);
        auto& obj = ctx.state.getObject(picked);
        obj.zone = ZoneType::Hand;
        obj.location = std::nullopt;
        ps.hand.push_back(picked);
        ctx.events.logTrace("MORBID RETURN: returned " + obj.name +
                             " from trash to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 170;
        d.def_id = R"RB(ogn-170-298)RB";
        d.name = R"RB(Morbid Return)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-170/298)RB";
        d.collector_number = 170;
        d.artist = R"RB(Rafael Zanchetin)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Return a unit from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/33b5ca4e580b94f2056bd884a4a35e8c630920a2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_170(CardRegistry& r) {
    r.registerCard(170, std::make_unique<MorbidReturn>());
}

} // namespace riftbound
