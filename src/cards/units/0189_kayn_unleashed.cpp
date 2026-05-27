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

class KaynUnleashed : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Ganking] is engine-handled.
    // "If I have moved twice this turn, I don't take damage." Wired via the
    // generic GameObject::immune_to_damage flag (reset each recalculateAuras,
    // checked at the top of EffectExecutor::dealDamage). moveUnit now increments
    // moves_this_turn; here we re-assert immunity each cleanup while the count
    // is >= 2.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller)
                continue;
            if (obj.moves_this_turn >= 2) obj.immune_to_damage = true;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 189;
        d.def_id = R"RB(ogn-189-298)RB";
        d.name = R"RB(Kayn, Unleashed)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-189/298)RB";
        d.collector_number = 189;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Kayn)RB", R"RB(Ionia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
If I have moved twice this turn, I don't take damage.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a1ce0e784264748f170bebb3b26a09a7f55efe0f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_189(CardRegistry& r) {
    r.registerCard(189, std::make_unique<KaynUnleashed>());
}

} // namespace riftbound
