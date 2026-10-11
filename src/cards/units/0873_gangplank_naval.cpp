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

class GangplankNaval : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Empower] [O][O]"
    EmpowerSpec empowerSpec() const override { return empowerCost(0, 2, Domain::Body); }
    // "[Empowered][>] If a spell or ability that chooses me would stun me, give
    // me -[M], or return me to hand, give me +3 [M] instead."
    // A replacement effect: the executor asks this hook before stunning,
    // reducing the Might of, or bouncing a unit, and only for effects of the
    // resolving chain item that chose me (ChainItem::chosen_objects / targets).
    // The +3 [M] lasts this turn (Radiance patch-notes FAQ: "replaced with him
    // being given +3 [M] this turn"). The replacement is itself a positive
    // amount, so it cannot recurse into this hook.
    bool replaceHostileEffect(CardContext& ctx, GameObjectId self,
                              HostileEffect /*kind*/) override {
        if (!ctx.state.objectExists(self)) return false;
        const auto& me = ctx.state.getObject(self);
        if (!me.isEmpowered() || !me.location.has_value()) return false;
        ctx.executor.giveTemporaryMight(self, 3);
        RIFT_TRACE(ctx.events, "GANGPLANK, NAVAL: Empowered -> +3 [M] this turn instead");
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 873;
        d.def_id = R"RB(ven-086-166)RB";
        d.name = R"RB(Gangplank, Naval)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-086/166)RB";
        d.collector_number = 86;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bilgewater)RB", R"RB(Gangplank)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [O][O] ([O][O]: Empower me. Use only if not Empowered.)
[Empowered][>] If a spell or ability that chooses me would stun me, give me -[M], or return me to hand, give me +3 [M] instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f270a3ecc4850594513c45d145f2b855152e2cb7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_873(CardRegistry& r) {
    r.registerCard(873, std::make_unique<GangplankNaval>());
}

}  // namespace riftbound
