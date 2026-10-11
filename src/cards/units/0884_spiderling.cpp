#include "cards/card.h"
#include "cards/card_helpers.h"
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

class Spiderling : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // [Hidden] is engine-handled.
    // "Your deck can have any number of cards named Spiderling." is a
    // deck-construction rule: def_ sets CardDef::unlimited_copies, which the
    // deck validator's copy-count check honours. No in-game behaviour.
    //
    // "I have +1 [M] for each other unit you control here with my name."
    // Each Spiderling counts the others at its own location.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            if (!self.location.has_value()) return;
            int others = 0;
            for (const auto& [id, obj] : state.objects) {
                if (id == sid || !obj.isUnit() || obj.controller != controller) continue;
                if (!obj.location.has_value() || *obj.location != *self.location) continue;
                if (obj.name == self.name) ++others;
            }
            if (others > 0) auraMight(self, sid, others);
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 884;
        d.def_id = R"RB(ven-097-166)RB";
        d.name = R"RB(Spiderling)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-097/166)RB";
        d.collector_number = 97;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spider)RB"};
        d.energy_cost = 3;
        d.might = 1;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Hidden);
        d.unlimited_copies = true;
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
I have +1 [M] for each other unit you control here with my name.
Your deck can have any number of cards named Spiderling.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fadf12e6e52eb5c70c65b479438f999f49d825fc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_884(CardRegistry& r) {
    r.registerCard(884, std::make_unique<Spiderling>());
}

}  // namespace riftbound
