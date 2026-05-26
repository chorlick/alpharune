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

// "Play a 2 [M] Sand Soldier unit token for each Equipment you control.
//  Then do this: Ready up to two of them."

class Arise : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int equip_count = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.controller != ctx.controller) continue;
            if (!obj.isGear() || !obj.location.has_value()) continue;
            bool is_equip = false;
            for (auto& tag : obj.tags) if (tag == "Equipment") { is_equip = true; break; }
            if (is_equip) ++equip_count;
        }
        if (equip_count <= 0) return;
        LocationId loc{BaseLocation{ctx.controller}};
        std::vector<GameObjectId> made;
        for (int i = 0; i < equip_count; ++i) {
            auto tok = ctx.executor.createToken(ctx.controller, CardType::Unit,
                                                "Sand Soldier", 2, {"Sand Soldier"},
                                                KeywordSet{}, loc, /*enter_ready=*/false);
            if (tok != kInvalidId) made.push_back(tok);
        }
        ctx.events.logTrace("ARISE!: played " + std::to_string(made.size()) +
                            " Sand Soldier token(s)");
        // Ready up to two of them (no agent choice surfaced — ready the first two).
        int readied = 0;
        for (auto tok : made) {
            if (readied >= 2) break;
            if (ctx.state.objectExists(tok)) {
                ctx.executor.readyObject(tok);
                ++readied;
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 513;
        d.def_id = R"RB(sfd-198-221)RB";
        d.name = R"RB(Arise!)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-198/221)RB";
        d.collector_number = 198;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Azir)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Play a 2 [M] Sand Soldier unit token for each Equipment you control. Then do this: Ready up to two of them.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/346f4698fea2878dde88470c1793047140c981e3-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_513(CardRegistry& r) {
    r.registerCard(513, std::make_unique<Arise>());
}

} // namespace riftbound
