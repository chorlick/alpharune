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

class EscapedGrayback : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // "[Empower] — Kill a friendly unit". The unit to kill is the ability's
    // target: the engine kills intent.targets[0] while paying the cost, so the
    // Empower descriptor must carry a target requirement for the action
    // generator to enumerate one activation per friendly unit.
    EmpowerSpec empowerSpec() const override {
        EmpowerSpec s;
        s.valid = true;
        s.cost.kill_friendly_unit = true;
        return s;
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab = empowerAbility();
        ab.targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                        .must_be_friendly = true};
        return {ab};
    }
    // Any friendly unit on the board may be killed (the generator does not
    // pass the source, so this cannot exclude the Grayback paying the cost;
    // killing itself is a legal but pointless payment).
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller, int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.isUnit() && obj.controller == controller && obj.location.has_value())
                out.push_back(id);
        return out;
    }
    // "[Empowered][>] I have +2 [M]."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachEmpoweredSelf(*this, state, controller,
            [](GameObjectId sid, GameObject& self) { auraMight(self, sid, 2); });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 911;
        d.def_id = R"RB(ven-124-166)RB";
        d.name = R"RB(Escaped Grayback)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-124/166)RB";
        d.collector_number = 124;
        d.artist = R"RB(Michal Ivan)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] — Kill a friendly unit (Pay the cost: Empower me. Use only if not Empowered.)
[Empowered][>] I have +2 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/253d782f8f6d4056eebecf9617e6858fdb2e712d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_911(CardRegistry& r) {
    r.registerCard(911, std::make_unique<EscapedGrayback>());
}

}  // namespace riftbound
