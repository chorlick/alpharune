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

class XinZhaoVigilant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        int n = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (obj.card_def_id == cardDefId()) continue;   // "other" — skip self def
            if (!obj.isAtBase()) continue;
            ++n;
        }
        return n >= 2;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 496;
        d.def_id = R"RB(sfd-176-221)RB";
        d.name = R"RB(Xin Zhao, Vigilant)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-176/221)RB";
        d.collector_number = 176;
        d.artist = R"RB(JunHuan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Demacia)RB", R"RB(Xin Zhao)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
I enter ready if you have two or more other units in your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ec694ac53a49fd4419434cb144daaaf0f38da574-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_496(CardRegistry& r) {
    r.registerCard(496, std::make_unique<XinZhaoVigilant>());
}

} // namespace riftbound
