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

class BellowsBreath : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] [1][B]" (CR 820): the Repeat cost, as data the engine charges.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 1, .power = 1, .power_domain = Domain::Mind}};
    }
    // "Deal 1 to up to three units at the same location." The units are
    // chosen by this card, one prompt each, as the spell is played (CR 355):
    // the first may be any unit, each further one a unit where the first is,
    // and the controller may stop at every prompt, the first included ("up
    // to": none is a choice, CR 355.13). Chosen here and not as play-time
    // targets of the intent so that a [Repeat] execution makes its OWN
    // choices the same way (CR 820.2.a: "may make different choices") —
    // GameEngine::makeNewChoicesFor runs this pass again for it. With targets
    // locked into the play intent the Repeat was asked for "that many again",
    // one unit at a time from every unit on the board: wrong count, no "same
    // location", and nothing at all when the first execution chose none (D45).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It can choose any unit its controller has on the board (the play gate).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    // Progress on the item, clear of the slots the Card::pick* helpers use
    // (pickMode below keeps its answer in resume_data[1]):
    //   resume_data[kPhase]   0 new / kAsking a prompt is out / kChosen / kDealt
    //   resume_data[kCount]   units chosen so far
    //   resume_data[kUnits..] their ids, in chosen order
    static constexpr size_t kPhase = 7, kCount = 8, kUnits = 9;
    static constexpr int kAsking = 1, kChosen = 2, kDealt = 3;
    static constexpr int kMaxUnits = 3;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& given) override {
        std::vector<GameObjectId> targets;
        ChainItem* ri = ctx.state.chain.resuming.has_value() ? &*ctx.state.chain.resuming : nullptr;
        if (!ri) {
            targets = given;   // called directly (no chain item): the caller's units
        } else {
            auto& data = ri->resume_data;
            while (data.size() < kUnits + kMaxUnits) data.push_back(0);
            if (data[kPhase] == kDealt) return;
            auto chosen = [&]() {
                std::vector<GameObjectId> out;
                for (int k = 0; k < data[kCount]; ++k)
                    out.push_back(GameObjectId::fromRaw(data[kUnits + static_cast<size_t>(k)]));
                return out;
            };
            // Units that can be added to the choice: not chosen yet, where the
            // first chosen unit is, and with a Deflect its controller can pay.
            auto candidates = [&]() {
                const auto have = chosen();
                std::optional<LocationId> where;
                if (!have.empty() && ctx.state.objectExists(have.front()))
                    where = ctx.state.getObject(have.front()).location.get();
                return offerableChoices(ctx,
                    chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                        if (std::find(have.begin(), have.end(), o.id) != have.end()) return false;
                        return have.empty() || (where.has_value() && o.location.has_value() &&
                                                *o.location == *where);
                    }));
            };
            if (data[kPhase] != kChosen) {
                bool stop = false;
                if (data[kPhase] == kAsking) {
                    const auto units = candidates();
                    const GameObjectId unit = takenObject(ctx);
                    if (std::find(units.begin(), units.end(), unit) == units.end()) {
                        stop = true;   // "No more units"
                    } else {
                        commitChosenObject(ctx, unit);
                        data[kUnits + static_cast<size_t>(data[kCount])] =
                            static_cast<int32_t>(unit.raw());
                        ++data[kCount];
                    }
                }
                if (!stop && data[kCount] < kMaxUnits) {
                    const auto units = candidates();
                    if (!units.empty()) {
                        std::vector<Intent> options;
                        options.push_back(valChoice(ctx.controller, 0, "No more units"));
                        for (auto id : units) options.push_back(objChoice(ctx.controller, id));
                        ctx.executor.requestChoice(ctx.controller, std::move(options),
                            "Bellows Breath: deal 1 to up to three units at the same location", PromptKind::ChooseObject);
                        data[kPhase] = kAsking;
                        return;
                    }
                }
                data[kPhase] = kChosen;
            }
            if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
            targets = chosen();
        }
        auto dealt = [&]() { if (ri) ri->resume_data[kPhase] = kDealt; };
        if (targets.empty()) { dealt(); return; }
        // "at the same location" is a requirement on the targets as a group
        // (CR 355.11). The targets still legal are grouped by where they are
        // NOW: if they no longer share a location, the controller chooses
        // which legal subset is affected — all of the chosen units at one
        // location, never part of them (CR 355.11.b, 355.11.b.1, whose
        // example is this card). Units that moved together are still a group.
        std::vector<std::pair<LocationId, std::vector<GameObjectId>>> groups;
        for (auto t : targets) {
            if (!ctx.state.objectExists(t)) continue;
            const auto& obj = ctx.state.getObject(t);
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            const LocationId loc = *obj.location;
            auto it = std::find_if(groups.begin(), groups.end(),
                                   [&](const auto& g) { return g.first == loc; });
            if (it == groups.end()) { groups.push_back({loc, {t}}); continue; }
            // de-dup (action gen may repeat in degenerate cases)
            if (std::find(it->second.begin(), it->second.end(), t) == it->second.end())
                it->second.push_back(t);
        }
        if (groups.empty()) { dealt(); return; }
        size_t use = 0;
        if (groups.size() > 1) {
            std::vector<std::string> labels;
            for (const auto& g : groups) {
                std::string names;
                for (auto t : g.second) {
                    if (!names.empty()) names += ", ";
                    names += ctx.state.getObject(t).name;
                }
                labels.push_back("Deal 1 to " + names);
            }
            const int which = pickMode(ctx, "Bellows Breath: units at the same location",
                                       static_cast<int>(groups.size()), labels);
            if (which == -1) return;   // waiting on the controller
            if (which >= 0 && which < static_cast<int>(groups.size()))
                use = static_cast<size_t>(which);
        }
        // Collect first, kill lethal after (AoE iterator-safety per CLAUDE.md).
        const std::vector<GameObjectId> hit = groups[use].second;
        for (auto t : hit) {
            if (ctx.state.objectExists(t)) ctx.executor.dealDamage(t, 1, ctx.source);
        }
        {   // the batch is for these kills only
            ctx.executor.killLethallyDamagedTogether(hit);   // they die at the same time
        }
        dealt();
        RIFT_TRACE(ctx.events, "BELLOWS BREATH: dealt 1 to " +
                             std::to_string(hit.size()) + " units at same location");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 402;
        d.def_id = R"RB(sfd-080-221)RB";
        d.name = R"RB(Bellows Breath)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-080/221)RB";
        d.collector_number = 80;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Repeat] [1][B] (You may pay the additional cost to repeat this spell's effect.)
Deal 1 to up to three units at the same location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f0ae1c4bb788f6fadc251c9d9b36f3f92eef4c56-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_402(CardRegistry& r) {
    r.registerCard(402, std::make_unique<BellowsBreath>());
}

} // namespace riftbound
