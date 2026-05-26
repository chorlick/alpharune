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

// "I must be assigned combat damage last." (= [Backline], CR 460.2.c)
// "If another unit you control here would die, if it has less Might than me,
//  instead heal it, exhaust it, and recall it."

class SorakaWanderer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.keyword = Keyword::Backline;
            obj.aura_effects.push_back(ae);
        }
    }

    bool hasReplacementEffect() const override { return true; }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        if (!ctx.state.objectExists(ctx.source)) return false;
        if (!ctx.state.objectExists(dying_unit)) return false;
        const auto& self = ctx.state.getObject(ctx.source);
        const auto& dying = ctx.state.getObject(dying_unit);
        if (dying_unit == ctx.source) return false;         // "another unit"
        if (!dying.isUnit() || dying.controller != ctx.controller) return false;
        // "here" — same battlefield as Soraka.
        auto my_bf = self.battlefieldId();
        auto dy_bf = dying.battlefieldId();
        if (!my_bf || !dy_bf || *my_bf != *dy_bf) return false;
        // "if it has less Might than me"
        if (dying.current_might >= self.current_might) return false;
        ctx.events.logTrace("SORAKA, WANDERER: heal/exhaust/recall a dying ally here");
        ctx.executor.healObject(dying_unit);
        ctx.executor.exhaustObject(dying_unit);
        ctx.executor.moveToBase(dying_unit);
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 550;
        d.def_id = R"RB(sfd-239-221)RB";
        d.name = R"RB(Soraka, Wanderer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-239/221)RB";
        d.collector_number = 239;
        d.artist = R"RB(Loiza Chen)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Soraka)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(I must be assigned combat damage last.
If another unit you control here would die, if it has less Might than me, instead heal it, exhaust it, and recall it. (Send it to base. This isn't a move.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc329097625a1f98a564134945950bd3bd3610a3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_550(CardRegistry& r) {
    r.registerCard(550, std::make_unique<SorakaWanderer>());
}

} // namespace riftbound
