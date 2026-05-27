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

// "If you would reveal cards from a deck, look at the top card first. You may
//  recycle it. Then reveal those cards." Wired via PlayerState::has_reveal_peek
//  (set in applyPassiveAura): EffectExecutor::revealUntil / revealAndChoose peek
//  the top card and offer to recycle it (agent yes/no) before revealing.
//  APPROX: applies to the reveal helpers (revealUntil/revealAndChoose), the
//  engine's reveal-from-deck paths.
class VoidHatchling : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects) {
            if (obj.card_def_id == cardDefId() && obj.controller == controller &&
                obj.location.has_value()) {
                state.player(controller).has_reveal_peek = true;
                return;
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 341;
        d.def_id = R"RB(sfd-018-221)RB";
        d.name = R"RB(Void Hatchling)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-018/221)RB";
        d.collector_number = 18;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(The Void)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(If you would reveal cards from a deck, look at the top card first. You may recycle it. Then reveal those cards.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a909782d0a30f31f9414c99ba143fa570a3cb456-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_341(CardRegistry& r) {
    r.registerCard(341, std::make_unique<VoidHatchling>());
}

} // namespace riftbound
