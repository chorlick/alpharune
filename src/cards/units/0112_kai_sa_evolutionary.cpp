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

class KaiSaEvolutionary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        const auto& db = ctx.executor.cardDB();
        int points = ps.score;

        auto find_spell = [&]() -> GameObjectId {
            for (int i = static_cast<int>(ps.trash.size()) - 1; i >= 0; --i) {
                auto cid = ps.trash[i];
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                if (obj.card_type != CardType::Spell) continue;
                if (obj.card_def_id == kInvalidId) continue;
                // "Energy cost less than your points."
                if (db.get(obj.card_def_id).energy_cost >= points) continue;
                return cid;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_spell() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Kai'Sa: play a spell (Energy cost < points) from trash without Energy cost?",
            still_legal);
        if (conf != 1) return;

        GameObjectId chosen = find_spell();
        if (chosen == kInvalidId) return;
        auto it = std::find(ps.trash.begin(), ps.trash.end(), chosen);
        if (it != ps.trash.end()) ps.trash.erase(it);

        std::string sname = ctx.state.getObject(chosen).name;
        // NOTE: playIgnoringCost ignores ALL costs (incl. Power); the printed
        // "you must still pay its Power cost" is approximated as free — same
        // convention as Fizz, Trickster (461).
        ctx.executor.playIgnoringCost(ctx.controller, chosen);
        ctx.executor.recycleCards(ctx.controller, {chosen});
        ctx.events.logTrace("KAI'SA: conquer -> played " + sname +
                             " from trash (ignoring Energy), then recycled it");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 112;
        d.def_id = R"RB(ogn-112-298)RB";
        d.name = R"RB(Kai'Sa, Evolutionary)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-112/298)RB";
        d.collector_number = 112;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Kai'Sa)RB", R"RB(The Void)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
When I conquer, you may play a spell from your trash with Energy cost less than your points without paying its Energy cost. Then recycle it. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc4ee60eaedd5c56a9222fd07482b6a86b11baa4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_112(CardRegistry& r) {
    r.registerCard(112, std::make_unique<KaiSaEvolutionary>());
}

} // namespace riftbound
