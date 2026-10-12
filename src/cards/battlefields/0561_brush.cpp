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

// "Bird, Cat, Dog, Poro, and Ivern units here have +1 [M]."
// "When you score here, you may replace this with the battlefield it
//  replaced." Reverse-replace (CR 438) — restore the banished original.

class Brush : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouScoreHere; }
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        static const char* const kTags[] = {"Bird", "Cat", "Dog", "Poro", "Ivern"};
        for (const auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;
            for (auto& [id, obj] : state.objects.onBoard()) {
                if (!obj.isUnit() || obj.battlefieldId() != bf.id) continue;
                bool tagged = false;
                for (const auto& tag : obj.tags)
                    for (const char* t : kTags) if (tag == t) tagged = true;
                if (!tagged) continue;
                // Called once per Brush in play: don't stack this one's bonus twice.
                bool already = false;
                for (const auto& ae : obj.aura_effects)
                    if (ae.source == bf.card_object_id) already = true;
                if (!already) auraMight(obj, bf.card_object_id, 1);
            }
        }
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Locate the BF slot this Brush card occupies.
        BattlefieldState* slot = nullptr;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { slot = &bf; break; }
        }
        // No return ahead of the "may" when there is nothing to put back:
        // confirmOptional then stores the "no" on the item.
        auto replaced = [&]() {
            return slot && slot->was_replaced && slot->replaced_card.has_value() &&
                   ctx.state.objectExists(*slot->replaced_card);
        };
        int conf = confirmOptional(ctx, "Brush: replace this with the battlefield it replaced?",
                                   replaced);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf != 1 || !replaced()) return;
        // The executor moves both battlefield cards (the token leaves play
        // as a new object, CR 124.1; the slot takes the original's rules).
        if (!ctx.executor.restoreReplacedBattlefield(slot->id)) return;
        RIFT_TRACE(ctx.events, "BRUSH: scored -> reverse-replaced back to original BF");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 561;
        d.def_id = R"RB(unl-t03)RB";
        d.name = R"RB(Brush)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-T03)RB";
        d.collector_number = 3;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::Token;
        d.ability_text = R"RB(Bird, Cat, Dog, Poro, and Ivern units here have +1 [M].
When you score here, you may replace this with the battlefield it replaced.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fad09d6bd9bf38e376f430ecb0b400762420d061-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_561(CardRegistry& r) {
    r.registerCard(561, std::make_unique<Brush>());
}

} // namespace riftbound
