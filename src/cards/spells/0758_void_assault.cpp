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

class VoidAssault : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move a friendly unit, then move an enemy unit. (If they both move to a
    //  battlefield you don't control, you're the attacker.)"
    // Each unit has its own destination (its controller's base or any other
    // battlefield); one prompt offers every combination. A unit that is no
    // longer a legal target is simply not moved — the other still is (CR
    // 359.3.e.5).
    // The parenthetical needs no code: at a battlefield the caster does not
    // control, cleanup applies Contested for the turn player first (CR
    // 190.3.a), and the player who applied it is the attacker — the caster,
    // who can only play this (no [Action] / [Reaction]) on their own turn.
    // The destinations are chosen as the spell is played (CR 355.4; testing
    // phase D3), with the units it names.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);   // its friendly unit
    }
    // resume_data slots (clear of the choice helpers'): the destinations the
    // pass decoded from the answer, +2 (0 = none decided, 1 = stays).
    static constexpr size_t kMyDestSlot = Card::kChoiceStateSlot + 1;
    static constexpr size_t kTheirDestSlot = Card::kChoiceStateSlot + 2;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        // Roles, not positions (Card::chosenTarget): my unit is slot 0, the
        // enemy unit slot 1; either is kInvalidId once it is no longer legal.
        const GameObjectId mine = chosenTarget(ctx, targets, 0);
        const GameObjectId theirs = chosenTarget(ctx, targets, 1);
        if (ri.resume_data.size() <= kTheirDestSlot ||
            ri.resume_data[kMyDestSlot] == 0) {
            // The choice, made as the spell is played: one prompt offers every
            // combination of the two units' destinations (each its
            // controller's base or any other battlefield). A unit with
            // nowhere to go contributes the single "stays" option.
            auto destsOf = [&](GameObjectId u) {
                return u == kInvalidId ? std::vector<int>{}
                                       : moveDests(ctx.state, ctx.state.getObject(u));
            };
            auto mine_dests = destsOf(mine);
            auto their_dests = destsOf(theirs);
            if (mine_dests.empty()) mine_dests.push_back(-1);
            if (their_dests.empty()) their_dests.push_back(-1);
            const int n = static_cast<int>(mine_dests.size() * their_dests.size());
            std::vector<std::string> labels;
            for (int a : mine_dests)
                for (int b : their_dests)
                    labels.push_back("yours: " + (a < 0 ? std::string("stays") : destLabel(a)) +
                                     " / theirs: " + (b < 0 ? std::string("stays") : destLabel(b)));
            // (Nothing to move — both targets gone, or none named: nothing asked.)
            const int picked = (n <= 32 && (mine != kInvalidId || theirs != kInvalidId))
                ? pickMode(ctx, "Void Assault: move the units where?", n, labels) : -2;
            if (picked == -1) return;               // waiting on the agent
            ri.resume_data.resize(std::max(ri.resume_data.size(), kTheirDestSlot + 1), 0);
            if (picked >= 0 && picked < n) {
                ri.resume_data[kMyDestSlot] = mine_dests[picked / their_dests.size()] + 2;
                ri.resume_data[kTheirDestSlot] = their_dests[picked % their_dests.size()] + 2;
            } else {
                ri.resume_data[kMyDestSlot] = 1;
                ri.resume_data[kTheirDestSlot] = 1;
            }
        }
        if (finalizingChoices(ctx)) return;          // choices end here (CR 355)
        const int my_dest = ri.resume_data[kMyDestSlot] - 2;
        const int their_dest = ri.resume_data[kTheirDestSlot] - 2;
        // A unit that is no longer a legal target is simply not moved — the
        // other still is (CR 359.3.e.5).
        if (mine != kInvalidId && my_dest >= 0) moveTo(ctx, mine, my_dest);             // friendly
        if (theirs != kInvalidId && their_dest >= 0) moveTo(ctx, theirs, their_dest);   // then enemy
        RIFT_TRACE(ctx.events, "VOID ASSAULT: moved the chosen units");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        // All units on the board; enumerateLegalTargetSets pairs them up.
        std::vector<GameObjectId> targets;
        for (auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            // "Can't be chosen by enemy spells and abilities."
            if (obj.controller != controller && obj.untargetable_by_enemy) continue;
            targets.push_back(id);
        }
        return targets;
    }
    // "A friendly unit, then an enemy unit": {friendly, enemy} pairs only.
    std::optional<std::vector<std::vector<GameObjectId>>> enumerateLegalTargetSets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> friendly, enemy;
        for (auto id : enumerateLegalTargets(state, controller)) {
            if (state.getObject(id).controller == controller) friendly.push_back(id);
            else enemy.push_back(id);
        }
        std::vector<std::vector<GameObjectId>> sets;
        for (auto f : friendly)
            for (auto e : enemy) sets.push_back({f, e});
        return sets;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 758;
        d.def_id = R"RB(unl-202-219)RB";
        d.name = R"RB(Void Assault)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-202/219)RB";
        d.collector_number = 202;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Kha'Zix)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Move a friendly unit, then move an enemy unit. (If they both move to a battlefield you don't control, you're the attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0b79108a9492fb34866cc63815b57bfab8a5aeae-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_758(CardRegistry& r) {
    r.registerCard(758, std::make_unique<VoidAssault>());
}

} // namespace riftbound
