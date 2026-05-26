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

class ImperialDecree : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Collect first (avoid iterator invalidation while killing).
        std::vector<GameObjectId> to_kill;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (obj.damage_marked > 0) to_kill.push_back(id);
        }
        for (auto id : to_kill) {
            if (ctx.state.objectExists(id)) ctx.executor.killObject(id);
        }
        ctx.events.logTrace("IMPERIAL DECREE: killed " +
                            std::to_string(to_kill.size()) +
                            " already-damaged units (future-damage-this-turn "
                            "kill unmodeled — no engine turn flag)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 221;
        d.def_id = R"RB(ogn-221-298)RB";
        d.name = R"RB(Imperial Decree)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-221/298)RB";
        d.collector_number = 221;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
When any unit takes damage this turn, kill it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc6ab617f1ce20c82ed76dc6446d42d416c08b34-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_221(CardRegistry& r) {
    r.registerCard(221, std::make_unique<ImperialDecree>());
}

} // namespace riftbound
