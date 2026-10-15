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

class DazzlingAurora : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    // "... until you reveal a unit and banish it. Play it, ignoring its cost,
    // and recycle the rest." In that order: the revealed cards wait, taken
    // out of the deck, while the unit is banished and played, and are
    // recycled after (testing phase R6). They are kept on the resolving item
    // from resume_data[kRestSlot] (clear of the slots pickMode uses).
    static constexpr size_t kRestSlot = 10;
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto recycleRest = [&]() {
            std::vector<GameObjectId> rest;
            for (size_t i = kRestSlot; i < ri.resume_data.size(); ++i)
                rest.push_back(GameObjectId::fromRaw(ri.resume_data[i]));
            if (ri.resume_data.size() > kRestSlot)   // only ever shrinks
                ri.resume_data.erase(ri.resume_data.begin() + kRestSlot, ri.resume_data.end());
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
        };

        // First-entry-only steps: reveal from deck until a unit, banish the
        // unit. Stash the unit id in ri.targets so re-entries from pickMode
        // don't re-reveal. Empty ri.targets = "haven't entered yet"; a single
        // kInvalidId entry = "entered, but no unit was found".
        if (ri.targets.empty()) {
            auto [u, rest] = ctx.executor.revealUntil(ctx.controller, CardType::Unit);
            while (ri.resume_data.size() < kRestSlot) ri.resume_data.push_back(0);
            for (auto cid : rest) ri.resume_data.push_back(static_cast<int32_t>(cid.raw()));
            if (u == kInvalidId || !ctx.state.objectExists(u)) {
                ri.targets.push_back(kInvalidId);
                recycleRest();
                return;
            }
            // It stays banished while the location is picked; playIgnoringCost
            // takes it out of the banishment zone when it plays it.
            ctx.executor.banishObject(u);
            ri.targets.push_back(u);
        }
        GameObjectId unit_id = ri.targets.front();
        if (unit_id == kInvalidId) return;

        // Per CR 355.2, the controller picks the landing location: base, any
        // battlefield they control, or where the unit's own permissions allow
        // (playLocationsFor leaves out a battlefield units can't be played to).
        // pickMode publishes one option per location so the agent records the
        // decision (mandatory even when only base is legal — engine-wide rule).
        const auto where = playLocationsFor(ctx, ctx.controller, unit_id);
        const std::vector<LocationId>& locations = where.locations;
        const std::vector<std::string>& labels = where.labels;
        uint32_t legal_mask = (locations.size() >= 32)
            ? 0xFFFFFFFFu
            : ((1u << locations.size()) - 1);
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
        int chosen = pickMode(ctx, "Aurora: where to play unit",
                               static_cast<int>(locations.size()),
                               labels, legal_mask, std::source_location::current(),
                               PromptKind::Location);
        if (chosen == -1) return;  // yielded for agent input
        if (chosen < 0 || static_cast<size_t>(chosen) >= locations.size()) {
            chosen = 0;
        }
        ctx.executor.playIgnoringCost(ctx.controller, unit_id,
                                       locations[chosen]);
        recycleRest();
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 160;
        d.def_id = R"RB(ogn-160-298)RB";
        d.name = R"RB(Dazzling Aurora)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-160/298)RB";
        d.collector_number = 160;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 9;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(At the end of your turn, reveal cards from the top of your Main Deck until you reveal a unit and banish it. Play it, ignoring its cost, and recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c023856a50c75058465f283d181277ce265ba108-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_160(CardRegistry& r) {
    r.registerCard(160, std::make_unique<DazzlingAurora>());
}

} // namespace riftbound
