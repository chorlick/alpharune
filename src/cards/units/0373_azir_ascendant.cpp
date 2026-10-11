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

class AzirAscendant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }

    bool hasActivatedAbility() const override { return true; }
    bool isActionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.power = 1, .power_domain = Domain::Calm};
    }

    // "Choose a unit you control" — any other unit of mine on the board; the
    // activating Azir itself is excluded.
    static std::vector<GameObjectId> legalTargets(const GameState& state,
                                                  PlayerId controller,
                                                  GameObjectId azir) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            if (id == azir) continue;
            out.push_back(id);
        }
        return out;
    }

    // The engine asks with the Azir whose ability it is; the source-less
    // overloads (no particular Azir) offer nothing.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& /*state*/, PlayerId /*controller*/) const override {
        return {};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/, GameObjectId source) const override {
        return legalTargets(state, controller, source);
    }
    // Card-wide gate: some Azir of mine has a unit to swap with. Which Azir
    // may still activate this turn is canActivateAbilityAt.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            if (!legalTargets(state, controller, id).empty()) return true;
        }
        return false;
    }
    // "Use only once per turn." — per Azir, not per card name.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!state.objectExists(source)) return false;
        const auto& azir = state.getObject(source);
        if (!azir.location.has_value()) return false;
        auto it = azir.card_counters.find("azir_used_turn");
        if (it != azir.card_counters.end() && it->second == state.turn.turn_number + 1)
            return false;  // already used this turn
        if (legalTargets(state, controller, source).empty()) return false;
        return UnitCard::canActivateAbilityAt(state, controller, source, ability_index);
    }

    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        const GameObjectId source = ctx.source;
        // "Use only once per turn": activating is the use, whether or not the
        // ability then does anything (the chosen unit may leave in response).
        // Stamped as the ability goes on the chain, with its cost (store
        // turn_number+1 so 0 = never).
        if (ctx.state.objectExists(source))
            ctx.state.getObject(source).card_counters["azir_used_turn"] =
                ctx.state.turn.turn_number + 1;
        std::vector<GameObjectId> legal_a = legalTargets(ctx.state, ctx.controller, source);
        if (!targets.empty() &&
            std::find(legal_a.begin(), legal_a.end(), targets[0]) != legal_a.end())
            legal_a = {targets[0]};
        auto equipment_of = [&ctx](GameObjectId unit) {
            std::vector<GameObjectId> gear;
            if (unit == kInvalidId || !ctx.state.objectExists(unit)) return gear;
            for (auto gid : ctx.state.getObject(unit).attachments)
                if (ctx.state.objectExists(gid) && isEquipment(ctx.state.getObject(gid)))
                    gear.push_back(gid);
            return gear;
        };
        // "If it's equipped, you may attach one of its Equipment to me." The
        // "may" is asked first; a yes then picks the unit and one of its
        // Equipment, a no picks the unit alone.
        const int take = confirmOptional(ctx,
            "Azir: also attach one of the chosen unit's Equipment to me?", [&]() {
                for (auto u : legal_a) if (!equipment_of(u).empty()) return true;
                return false;
            });
        std::pair<GameObjectId, GameObjectId> pair{kInvalidId, kInvalidId};
        if (take == 1)
            pair = pickTargetPair(ctx, "Azir: choose a unit to swap with", legal_a, equipment_of);
        else if (take == 0)
            pair.first = pickTarget(ctx, "Azir: choose a unit to swap with", legal_a);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        const GameObjectId picked = pair.first;
        if (picked == kInvalidId || picked == source) return;   // waiting / none / mistarget

        auto& azir = ctx.state.getObject(source);
        auto& other = ctx.state.getObject(picked);
        // Both must still be on the board to trade places: a unit that left
        // is not moved back from wherever it went.
        if (!azir.location.has_value() || !other.location.has_value()) return;

        // Swap locations. Capture before moving.
        auto azir_loc = azir.location;
        auto other_loc = other.location;
        moveToLocation(ctx.executor, source, other_loc);
        moveToLocation(ctx.executor, picked, azir_loc);
        ctx.events.logTrace("AZIR ASCENDANT: swapped locations with " + other.name);

        // Optional Equipment transfer (pair.second): attachFree detaches it
        // from the chosen unit and announces the attach, so "when attached"
        // abilities see it.
        if (pair.second != kInvalidId && equipEndsOnBoard(ctx.state, pair.second, source))
            attachFree(ctx, pair.second, source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 373;
        d.def_id = R"RB(sfd-050-221)RB";
        d.name = R"RB(Azir, Ascendant)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-050/221)RB";
        d.collector_number = 50;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bird)RB", R"RB(Azir)RB", R"RB(Shurima)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([G]: [Action] — Choose a unit you control. Move me to its location and it to my original location. If it's equipped, you may attach one of its Equipment to me. Use only once per turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c42c02f6ebd2e4d4a640c2c6ac081231439888e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_373(CardRegistry& r) {
    r.registerCard(373, std::make_unique<AzirAscendant>());
}

} // namespace riftbound
