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
        // "ready up to 2 runes". Which two is not asked: an exhausted rune
        // is worth the same whatever its Domain (ready it gives [1]; it can
        // be recycled for its Power ready or exhausted, CR 164.2.b), and
        // readying fewer is never better. Lowest ids first, so the result
        // does not depend on map order.
        std::vector<GameObjectId> exhausted;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted || !obj.location.has_value()) continue;
            exhausted.push_back(id);
        }
        std::sort(exhausted.begin(), exhausted.end());
        if (exhausted.size() > 2) exhausted.resize(2);
        const int readied = static_cast<int>(exhausted.size());
        for (auto id : exhausted) ctx.executor.readyObject(id);
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
