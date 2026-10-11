#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "[Vision] (When you play me, look at the top card of your Main Deck. You may
//  recycle it.) When you recycle one or more cards to your Main Deck, buff a
//  friendly unit."

class KarmaChanneler_548 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "[Vision]" is the engine's (TriggerManager::onCardPlayed, from the
    // keyword in the def). "When you recycle one or more cards to your Main
    // Deck, buff a friendly unit." — WhenYouRecycle, as the original (235).
    TriggerType triggerType() const override { return TriggerType::WhenYouRecycle; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> friendly;
        for (auto& [id, obj] : ctx.state.objects.onBoard())
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                friendly.push_back(id);
        const GameObjectId pick = pickTarget(ctx, "Karma, Channeler: buff a friendly unit",
                                             friendly);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (pick == kInvalidId) return;       // none / mistarget / waiting
        ctx.executor.buffUnit(pick);
        RIFT_TRACE(ctx.events, "KARMA CHANNELER: buffed a friendly unit on recycle");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 548;
        d.def_id = R"RB(sfd-237-221)RB";
        d.name = R"RB(Karma, Channeler)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-237/221)RB";
        d.collector_number = 237;
        d.artist = R"RB(Andres Blanco)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        // Tags as printed on SFD-237 (cards/raw/gallery_raw.json): the Spiritforged
        // reprint is not a copy of the Origins def — OGN-235 (card 235) also carries Ionia.
        d.tags = {R"RB(Karma)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
When you recycle one or more cards to your Main Deck, buff a friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff. Runes aren't cards.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ba0113d449813c94534ae0e74f3ef38f9b8010c2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_548(CardRegistry& r) {
    r.registerCard(548, std::make_unique<KarmaChanneler_548>());
}

} // namespace riftbound
