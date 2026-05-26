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

class FizzTrickster : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        const auto& db = ctx.executor.cardDB();

        auto find_spell = [&]() -> GameObjectId {
            // Most-recent first.
            for (int i = static_cast<int>(ps.trash.size()) - 1; i >= 0; --i) {
                auto cid = ps.trash[i];
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                if (obj.card_type != CardType::Spell) continue;
                if (obj.card_def_id == kInvalidId) continue;
                // Energy cost gate: no more than 3.
                if (db.get(obj.card_def_id).energy_cost > 3) continue;
                return cid;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_spell() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Fizz: play a spell (cost <= 3) from trash for free?", still_legal);
        if (conf != 1) return;

        GameObjectId chosen = find_spell();
        if (chosen == kInvalidId) return;
        // Remove from trash before playing.
        auto it = std::find(ps.trash.begin(), ps.trash.end(), chosen);
        if (it != ps.trash.end()) ps.trash.erase(it);

        std::string sname = ctx.state.getObject(chosen).name;
        ctx.executor.playIgnoringCost(ctx.controller, chosen);
        // "Recycle that spell after you play it" — bottom of owner's main deck.
        ctx.executor.recycleCards(ctx.controller, {chosen});
        ctx.events.logTrace("FIZZ: played " + sname +
                             " from trash (energy<=3, ignoring energy), then recycled it");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 461;
        d.def_id = R"RB(sfd-140-221)RB";
        d.name = R"RB(Fizz, Trickster)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-140/221)RB";
        d.collector_number = 140;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Fizz)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you may play a spell from your trash with Energy cost no more than [3], ignoring its Energy cost. Recycle that spell after you play it. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6d1c6615fb6ef5fb520a35b2ce76f8feee9167dc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_461(CardRegistry& r) {
    r.registerCard(461, std::make_unique<FizzTrickster>());
}

} // namespace riftbound
