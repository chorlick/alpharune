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

class AmbessaRespectedAndFeared : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // "[Empower] [1][Y][Y]"
    EmpowerSpec empowerSpec() const override { return empowerCost(1, 2, Domain::Order); }
    // "[Empowered][>] I have [Assault 2]."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachEmpoweredSelf(*this, state, controller, [](GameObjectId sid, GameObject& self) {
            auraKeyword(self, sid, Keyword::Assault, 2);
        });
    }
    // "[Empowered][>] When I attack, kill an enemy unit here with less Might
    //  than me."
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // "[Empowered][>]": I have the ability only while I have the status (CR
    // 828.1.b.1), so it triggers only for an Empowered Ambessa — no ability
    // goes on the chain otherwise.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.objectExists(source) && state.getObject(source).isEmpowered();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        const int my_might = self.current_might;
        // Not Empowered, or off the board: nothing to choose. No return ahead
        // of pickTarget — it stores "no target" on the chain item, so the
        // resolution asks nothing if I have been Empowered since.
        std::vector<GameObjectId> legal;
        if (self.isEmpowered() && self.location.has_value()) {
            const auto here = *self.location;
            legal = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& obj) {
                return obj.controller != ctx.controller && *obj.location == here &&
                       obj.current_might < my_might;
            });
        }
        GameObjectId picked = pickTarget(ctx, "Ambessa: kill a weaker enemy here", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.killObject(picked);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 923;
        d.def_id = R"RB(ven-136-166)RB";
        d.name = R"RB(Ambessa, Respected and Feared)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-136/166)RB";
        d.collector_number = 136;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB", R"RB(Ambessa)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [1][Y][Y] ([1][Y][Y]: Empower me. Use only if not Empowered.)
[Empowered][>] I have [Assault 2]. (+2 [M] while I'm an attacker.)
[Empowered][>] When I attack, kill an enemy unit here with less Might than me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/687c273217cd94067be9625a7f85b61d108a6293-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_923(CardRegistry& r) {
    r.registerCard(923, std::make_unique<AmbessaRespectedAndFeared>());
}

}  // namespace riftbound
