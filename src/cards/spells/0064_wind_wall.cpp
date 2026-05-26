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

class WindWall : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        counterChainTop(ctx);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 64;
        d.def_id = R"RB(ogn-064-298)RB";
        d.name = R"RB(Wind Wall)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-064/298)RB";
        d.collector_number = 64;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter a spell.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/72c4dbe48d06916c847dab40340e5f05228fadfe-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_64(CardRegistry& r) {
    r.registerCard(64, std::make_unique<WindWall>());
}

} // namespace riftbound
