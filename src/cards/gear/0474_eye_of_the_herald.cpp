#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class EyeOfTheHerald : public SimpleEquipGear {
public:
    EyeOfTheHerald() : SimpleEquipGear(Domain::Order) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIMove; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto loc = ctx.state.getObject(unit).location.value_or(BaseLocation{ctx.controller});
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                  {"Recruit"}, {}, loc, true);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 474;
        d.def_id = R"RB(sfd-153-221)RB";
        d.name = R"RB(Eye of the Herald)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-153/221)RB";
        d.collector_number = 153;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [Y] ([Y]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I move, play a 1 [M] Recruit unit token here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc3e26e0e4ff94de360f1163c41641daf81c1900-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_474(CardRegistry& r) {
    r.registerCard(474, std::make_unique<EyeOfTheHerald>());
}

} // namespace riftbound
