#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class MaduliTheGatekeeper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "I can't be readied." — neither by the Awaken Phase nor by an effect.
    bool canBeReadied() const override { return false; }

    // "[P]: Move me to an occupied enemy battlefield if my Might is greater
    //  than the total Might of enemy units there." ([P] = one Chaos Power.)
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.power = 1, .power_domain = Domain::Chaos};
    }
    // Legality: there must be at least one qualifying enemy battlefield for
    // THIS Maduli (its own Might, its own location).
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!UnitCard::canActivateAbilityAt(state, controller, source, ability_index))
            return false;
        if (!state.objectExists(source)) return false;
        const auto& self = state.getObject(source);
        return self.location.has_value() &&
               !qualifyingBfs(state, controller, self.current_might, source).empty();
    }
    // The battlefield is the move's destination (CR 355.4), chosen as the
    // ability is activated, before its cost (CR 403.2 -> 405.1), among those
    // that qualify then. As it resolves, Maduli moves only if it still
    // qualifies ("if my Might is greater than the total Might of enemy units
    // there").
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& bfs = ctx.state.battlefields;
        const int n = static_cast<int>(std::min<size_t>(bfs.size(), 32));
        auto qualifying = [&]() {
            if (!ctx.state.objectExists(ctx.source) ||
                !ctx.state.getObject(ctx.source).location.has_value())
                return std::vector<BattlefieldId>{};
            return qualifyingBfs(ctx.state, ctx.controller,
                                 ctx.state.getObject(ctx.source).current_might, ctx.source);
        };
        // One mode per battlefield slot; the qualifying ones are legal.
        uint32_t mask = 0;
        std::vector<std::string> labels;
        const auto now = qualifying();
        for (int i = 0; i < n; ++i) {
            labels.push_back(destLabel(1 + i));
            if (std::find(now.begin(), now.end(), bfs[i].id) != now.end()) mask |= 1u << i;
        }
        const int chosen = pickMode(ctx, "Maduli: move to which enemy battlefield", n, labels, mask);
        if (chosen == -1) return;              // waiting on agent
        if (finalizingChoices(ctx)) return;    // choices end here (CR 403.2)
        if (chosen < 0 || chosen >= n) return; // nowhere to move
        const BattlefieldId dest = bfs[chosen].id;
        if (std::find(now.begin(), now.end(), dest) == now.end()) return;   // no longer qualifies
        ctx.executor.moveToBattlefield(ctx.source, dest);
        RIFT_TRACE(ctx.events, "MADULI: moved to occupied enemy battlefield");
    }

private:
    // "An occupied enemy battlefield" read as a battlefield with enemy units,
    // whoever controls it. "Enemy battlefield" is not defined (CR 170.11
    // defines "occupied"); Dauntless Vanguard's permission is read as one the
    // opponent controls: judge question 17 (testing phase R14), both pinned.
    // Battlefields that have enemy units AND where `my_might` > total enemy
    // Might there. Excludes the battlefield I'm already at.
    static std::vector<BattlefieldId> qualifyingBfs(const GameState& state,
                                                     PlayerId controller, int my_might,
                                                     GameObjectId self) {
        PlayerId opp = opponent(controller);
        std::optional<BattlefieldId> my_bf;
        if (state.objectExists(self)) my_bf = state.getObject(self).battlefieldId();
        std::vector<BattlefieldId> out;
        for (const auto& bf : state.battlefields) {
            if (my_bf && *my_bf == bf.id) continue;
            int enemy_might = 0;
            bool occupied = false;
            for (auto& [id, obj] : state.objects.onBoard()) {
                if (!obj.isUnit() || obj.controller != opp) continue;
                auto ubf = obj.battlefieldId();
                if (!ubf || *ubf != bf.id) continue;
                occupied = true;
                enemy_might += obj.current_might;
            }
            if (occupied && my_might > enemy_might) out.push_back(bf.id);
        }
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 706;
        d.def_id = R"RB(unl-144-219)RB";
        d.name = R"RB(Maduli the Gatekeeper)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-144/219)RB";
        d.collector_number = 144;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bandle City)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I can't be readied.
[P]: Move me to an occupied enemy battlefield if my Might is greater than the total Might of enemy units there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/20f1b46435845de99987e37125e0ef7bd61c00bb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_706(CardRegistry& r) {
    r.registerCard(706, std::make_unique<MaduliTheGatekeeper>());
}

} // namespace riftbound
