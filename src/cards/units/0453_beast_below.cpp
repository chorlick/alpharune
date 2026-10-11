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

class BeastBelow : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    // "When you play me, return another friendly unit and an enemy unit to
    // their owners' hands." Two targets; each is returned if it can be chosen,
    // whether or not the other exists.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        auto friends = [&]() {
            return boardObjects(ctx.state, [&](const GameObject& o) {
                return o.id != ctx.source && o.isUnit() && o.controller == me;
            });
        };
        auto enemies = [&]() {
            return chooseableUnits(ctx.state, me,
                                   [me](const GameObject& o) { return o.controller != me; });
        };
        // Which helper asks is fixed on the first entry, so later entries read
        // their answers back from the same one: resume_data[kShapeSlot] is
        // 1 = both (pair), 2 = only an enemy unit, 3 = only a friendly unit.
        auto& ri = ctx.state.chain.resuming.value();
        constexpr size_t kShapeSlot = Card::kChoiceStateSlot + 1;
        if (ri.resume_data.size() <= kShapeSlot) ri.resume_data.resize(kShapeSlot + 1, 0);
        if (ri.resume_data[kShapeSlot] == 0)
            ri.resume_data[kShapeSlot] = friends().empty() ? 2 : (enemies().empty() ? 3 : 1);
        const int shape = ri.resume_data[kShapeSlot];

        GameObjectId friendly = kInvalidId, enemy = kInvalidId;
        if (shape == 1) {
            auto pair = pickTargetPair(ctx, "Beast Below: another friendly unit, then an enemy unit",
                                       friends(), [&](GameObjectId) { return enemies(); });
            friendly = pair.first;
            enemy = pair.second;
        } else if (shape == 2) {
            enemy = pickTarget(ctx, "Beast Below: return an enemy unit to hand", enemies());
        } else {
            friendly = pickTarget(ctx, "Beast Below: return another friendly unit to hand", friends());
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        // Waiting, or a target that is no longer legal: kInvalidId; the other
        // one is still returned (CR 359.3.e.5).
        if (friendly != kInvalidId) ctx.executor.bounceToHand(friendly);
        if (enemy != kInvalidId) ctx.executor.bounceToHand(enemy);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 453;
        d.def_id = R"RB(sfd-132-221)RB";
        d.name = R"RB(Beast Below)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-132/221)RB";
        d.collector_number = 132;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, return another friendly unit and an enemy unit to their owners' hands.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d27a56111c68c651099d7b73b22a54a825458857-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_453(CardRegistry& r) {
    r.registerCard(453, std::make_unique<BeastBelow>());
}

} // namespace riftbound
