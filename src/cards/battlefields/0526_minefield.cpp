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

// "When you conquer here, put the top 2 cards of your Main Deck into your trash."

class Minefield : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        for (int i = 0; i < 2 && !ps.main_deck.empty(); ++i) {
            auto cid = ps.main_deck.back();
            ps.main_deck.pop_back();
            if (ctx.state.objectExists(cid)) {
                auto& obj = ctx.state.getObject(cid);
                obj.zone = ZoneType::Trash;
                obj.location = std::nullopt;
                ps.trash.push_back(cid);
            }
        }
        ctx.events.logTrace("MINEFIELD: conquer -> top 2 of Main Deck to trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 526;
        d.def_id = R"RB(sfd-212-221)RB";
        d.name = R"RB(Minefield)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-212/221)RB";
        d.collector_number = 212;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, put the top 2 cards of your Main Deck into your trash.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/46614658b08563f07cc81a4f4fa4ec8a067710b9-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_526(CardRegistry& r) {
    r.registerCard(526, std::make_unique<Minefield>());
}

} // namespace riftbound
