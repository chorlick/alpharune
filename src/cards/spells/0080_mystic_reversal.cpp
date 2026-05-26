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

class MysticReversal : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Gain control of a spell. You may make new choices for it."
    // [Reaction] — only playable with a spell on the chain to take.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it)
            if (it->is_spell) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        auto& top = ctx.state.chain.items.back();
        if (!top.is_spell) return;
        // Gain control: re-point both the chain item and the underlying spell
        // object to me. The spell will now resolve under my control.
        // NOTE: "you may make new choices for it" (re-targeting) is not
        // supported by the executor — the original targets are retained
        // (documented engine gap).
        top.controller = ctx.controller;
        if (ctx.state.objectExists(top.source))
            ctx.state.getObject(top.source).controller = ctx.controller;
        ctx.events.logTrace("MYSTIC REVERSAL: gained control of the spell on the "
                            "chain (re-target unsupported)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 80;
        d.def_id = R"RB(ogn-080-298)RB";
        d.name = R"RB(Mystic Reversal)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-080/298)RB";
        d.collector_number = 80;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Gain control of a spell. You may make new choices for it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/298fe91f9d76086b7d77880e11016ed46389b61b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_80(CardRegistry& r) {
    r.registerCard(80, std::make_unique<MysticReversal>());
}

} // namespace riftbound
