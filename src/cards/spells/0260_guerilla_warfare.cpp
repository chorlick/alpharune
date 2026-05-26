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

class GuerillaWarfare : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        const CardDB& db = ctx.executor.cardDB();

        // Collect up to two [Hidden] cards from trash and return them. We
        // return the first matching cards (deterministic); modeled as "up to
        // two" by stopping at two.
        int returned = 0;
        for (int i = static_cast<int>(ps.trash.size()) - 1;
             i >= 0 && returned < 2; --i) {
            auto cid = ps.trash[i];
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (obj.card_def_id == kInvalidId) continue;
            if (!db.get(obj.card_def_id).keywords.has(Keyword::Hidden)) continue;
            // Move trash -> hand.
            ps.trash.erase(ps.trash.begin() + i);
            obj.zone = ZoneType::Hand;
            obj.location = std::nullopt;
            ps.hand.push_back(cid);
            ++returned;
            ctx.events.logTrace("GUERILLA WARFARE: returned " + obj.name +
                                 " from trash to hand");
        }
        // TODO: "You can hide cards ignoring costs this turn." — the engine
        // has no per-player "hide ignoring costs" flag, so this clause is not
        // implemented (engine gap).
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 260;
        d.def_id = R"RB(ogn-264-298)RB";
        d.name = R"RB(Guerilla Warfare)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-264/298)RB";
        d.collector_number = 264;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Teemo)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB(Return up to two cards with [Hidden] from your trash to your hand. You can hide cards ignoring costs this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3a1f70e49f1fdbac907a3395f517e96717e8afea-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_260(CardRegistry& r) {
    r.registerCard(260, std::make_unique<GuerillaWarfare>());
}

} // namespace riftbound
