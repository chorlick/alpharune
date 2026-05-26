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

class DarkChildStarter : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (readied >= 2) break;
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.readyObject(id);
            ++readied;
        }
        if (readied > 0) {
            ctx.events.logTrace("DARK CHILD: readied " + std::to_string(readied)
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
