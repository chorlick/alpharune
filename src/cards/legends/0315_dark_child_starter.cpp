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

class DarkChildStarter : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // Which runes is this ability's choice: it is made as the ability is
    // finalized (CR 355.5, 383.3), so the opponent responds knowing them.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "ready up to 2 runes": which runes, and how many, is the
        // controller's choice — one prompt per rune, "None" on each (CR
        // 355.13). The rune matters: a rune can be recycled for its Power
        // ready or exhausted (CR 164.2.b), so one that will be recycled is
        // better left exhausted and another one readied for its [1].
        //
        // "runes", with no "friendly" (Sona 73) or "your" (Ekko 110): the word
        // refers to any rune on the board (CR 133.3.a, 355.9.a.1) and only a
        // printed restriction narrows a choice (CR 355.9.b), so an opponent's
        // exhausted rune may be named too.
        // (judge ruling 2026-10-11: confirmed — an opponent's exhausted rune may be chosen; plans/09 9.BB4 (a).)
        auto readiable = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isRune() && obj.is_exhausted && obj.location.has_value();
        };
        const std::string label = "Dark Child: ready a rune (up to 2)";
        // Asked as the ability is finalized. At resolution selectSeveral asks
        // nothing: it returns the runes named then, without any that has been
        // readied or has left the board since, and no other in its place.
        std::vector<GameObjectId> exhausted;
        for (const auto& [id, obj] : ctx.state.objects.onBoard())
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
            RIFT_TRACE(ctx.events, "DARK CHILD: readied " + std::to_string(readied)
                                + " rune(s) at end of turn");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 315;
        d.def_id = R"RB(ogs-017-024)RB";
        d.name = R"RB(Dark Child - Starter)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-017/024)RB";
        d.collector_number = 17;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Annie)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(At the end of your turn, ready up to 2 runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/89963e1d1cffd69c620fb5d6b037f50d5c334463-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_315(CardRegistry& r) {
    r.registerCard(315, std::make_unique<DarkChildStarter>());
}

} // namespace riftbound
