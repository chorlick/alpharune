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

class TheDreamingTree : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When a player chooses a friendly unit here with a spell for the first
    // time each turn, they draw 1." Handler wired on WhenAFriendlyUnitChosenHere
    // (once-per-turn-per-player via a turn-stamped card_counter -> draw 1).
    // APPROX/INERT: no engine site currently emits "a spell chose a friendly unit
    // here", so this handler does not yet fire in play. It is the faithful card
    // implementation, ready for that emit; this card is also tournament-BANNED.
    TriggerType triggerType() const override {
        return TriggerType::WhenAFriendlyUnitChosenHere;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        const int stamp = ctx.state.turn.turn_number + 1;  // +1 so default 0 = never
        const std::string key = "__dreaming_tree_drew_p" +
            std::to_string(static_cast<int>(ctx.controller));
        if (me.card_counters[key] == stamp) return;  // already drew this turn
        me.card_counters[key] = stamp;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("THE DREAMING TREE: first friendly-unit choice here -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 287;
        d.def_id = R"RB(ogn-292-298)RB";
        d.name = R"RB(The Dreaming Tree)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-292/298)RB";
        d.collector_number = 292;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a player chooses a friendly unit here with a spell for the first time each turn, they draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f382c2c2442959f2a8905de2be7d202458918a04-1040x744.png)RB";
        d.banned = true;  // tournament ban (formerly cards/ban-list.csv)
        return d;
    }();
};

}  // anonymous namespace

void register_card_287(CardRegistry& r) {
    r.registerCard(287, std::make_unique<TheDreamingTree>());
}

} // namespace riftbound
