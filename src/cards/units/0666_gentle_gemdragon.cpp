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

class GentleGemdragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Which runes is this ability's choice: it is made as the ability is
    // finalized (CR 355.5, 383.3), so the opponent responds knowing them.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenYouPlayAUnit};
    }
    // "When you play me or another Dragon": WhenYouPlayAUnit fires for every
    // other unit its controller plays, with that unit as the subject.
    bool triggerCondition(const GameState& state, GameObjectId /*source*/,
                          PlayerId /*controller*/, TriggerType trigger,
                          GameObjectId subject) const override {
        if (trigger != TriggerType::WhenYouPlayAUnit) return true;
        return subject != kInvalidId && state.objectExists(subject) &&
               hasTag(state.getObject(subject), "Dragon");
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "ready up to 2 runes": which runes, and how many, is the
        // controller's choice — one prompt per rune, "None" on each (CR
        // 355.13). The rune matters: a rune that will be recycled for its
        // Power is better left exhausted and another one readied.
        //
        // "runes", with no "friendly" (Sona 73) or "your" (Ekko 110): the word
        // refers to any rune on the board (CR 133.3.a, 355.9.a.1) and only a
        // printed restriction narrows a choice (CR 355.9.b), so an opponent's
        // exhausted rune may be named too.
        auto readiable = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isRune() && obj.is_exhausted && obj.location.has_value();
        };
        const std::string label = "Gentle Gemdragon: ready a rune (up to 2)";
        // Asked as the ability is finalized. At resolution selectSeveral asks
        // nothing: it returns the runes named then, without any that has been
        // readied or has left the board since, and no other in its place.
        std::vector<GameObjectId> exhausted;
        for (const auto& [id, obj] : ctx.state.objects)
            if (readiable(id)) exhausted.push_back(id);
        const auto chosen = selectSeveral(ctx, label, exhausted, 2);
        if (!chosen.has_value()) return;   // waiting on the agent
        const std::vector<GameObjectId>& picks = *chosen;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        int readied = 0;
        // No second readiable() check: every id selectSeveral returns is in
        // the list built above in this entry, and nothing changes a rune
        // between there and here (readying one rune does not touch another).
        for (auto id : picks) {
            ctx.executor.readyObject(id);
            ++readied;
        }
        if (readied > 0) {
            ctx.events.logTrace("GENTLE GEMDRAGON: readied " +
                                std::to_string(readied) + " rune(s)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 666;
        d.def_id = R"RB(unl-104-219)RB";
        d.name = R"RB(Gentle Gemdragon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-104/219)RB";
        d.collector_number = 104;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 8;
        d.might = 8;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me or another Dragon, ready up to 2 runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65fcaff267b7f27cbda09ad23f2449188195d28e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_666(CardRegistry& r) {
    r.registerCard(666, std::make_unique<GentleGemdragon>());
}

} // namespace riftbound
