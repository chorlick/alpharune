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

// "Play a 2 [M] Sand Soldier unit token for each Equipment you control.
//  Then do this: Ready up to two of them."

class Arise : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // Each token is played to the controller's base or a battlefield they
    // control, token by token (playTokensChoosingLocations).
    //
    // "Then do this: Ready up to two of them." — which of the new tokens, and
    // how many (0, 1 or 2), is the controller's choice. Progress lives on the
    // resolving chain item: resume_point kAwaitReady / kDone (clear of the
    // ranges the Card::pick* / confirmOptional helpers use) and
    // resume_data = [readied so far, token, token, ...].
    static constexpr int kAwaitReady = 20;
    static constexpr int kDone = 21;

    static std::vector<GameObjectId> readyable(const GameState& state,
                                               const std::vector<int32_t>& data) {
        std::vector<GameObjectId> out;
        for (size_t i = 1; i < data.size(); ++i) {
            const auto id = static_cast<GameObjectId>(data[i]);
            if (!state.objectExists(id)) continue;
            const auto& t = state.getObject(id);
            if (t.location.has_value() && t.is_exhausted) out.push_back(id);
        }
        return out;
    }
    // Offers the next "ready one of them" pick, or finishes.
    static void promptReady(CardContext& ctx) {
        auto& ri = *ctx.state.chain.resuming;
        auto cands = readyable(ctx.state, ri.resume_data);
        if (ri.resume_data.empty() || ri.resume_data[0] >= 2 || cands.empty()) {
            ri.resume_point = kDone;
            return;
        }
        std::vector<Intent> opts;
        for (auto id : cands) {
            Intent c;
            c.type = IntentType::MakeChoice;
            c.player = ctx.controller;
            c.chosen_objects = {id};
            opts.push_back(std::move(c));
        }
        Intent stop;                       // "up to two": may stop early
        stop.type = IntentType::MakeChoice;
        stop.player = ctx.controller;
        stop.chosen_value = 0;
        stop.choice_label = "Arise!: ready no more";
        opts.push_back(std::move(stop));
        ctx.executor.requestChoice(ctx.controller, std::move(opts),
            "Arise!: ready a Sand Soldier (" + std::to_string(ri.resume_data[0] + 1) +
            " of up to 2)");
        ri.resume_point = kAwaitReady;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value() && ri_opt->resume_point == kDone) return;
        if (ri_opt.has_value() && ri_opt->resume_point == kAwaitReady) {
            auto& ri = *ri_opt;
            auto choice = ctx.executor.takeChoice();
            auto cands = readyable(ctx.state, ri.resume_data);
            GameObjectId picked = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) picked = choice->chosen_objects[0];
            if (std::find(cands.begin(), cands.end(), picked) == cands.end()) {
                ri.resume_point = kDone;   // stopped (or nothing valid picked)
                return;
            }
            ctx.executor.readyObject(picked);
            ri.resume_data[0]++;
            promptReady(ctx);
            return;
        }

        // The number of tokens is fixed when the first one is played: the
        // count is re-read on re-entry, and tokens are not Equipment.
        int equip_count = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.controller != ctx.controller) continue;
            if (isEquipment(obj) && obj.location.has_value()) ++equip_count;
        }
        if (equip_count <= 0) return;
        std::vector<GameObjectId> made;
        auto make = [&](LocationId loc) {
            // CR 187.3: a Sand Soldier token has 2 Might and the Shurima tag.
            return ctx.executor.createToken(ctx.controller, CardType::Unit,
                                            "Sand Soldier", 2, {"Shurima"},
                                            KeywordSet{}, loc, /*enter_ready=*/false);
        };
        if (!playTokensChoosingLocations(ctx, "Arise!: play a Sand Soldier", equip_count,
                                         make, made))
            return;   // waiting for a location
        ctx.events.logTrace("ARISE!: played " + std::to_string(made.size()) +
                            " Sand Soldier token(s)");
        if (!ri_opt.has_value()) {
            // Direct invocation (no chain item to resume): no agent can be
            // asked, so ready the first two, as the choice helpers default to
            // their first option.
            int readied = 0;
            for (auto tok : made) {
                if (readied >= 2) break;
                if (!ctx.state.objectExists(tok)) continue;
                ctx.executor.readyObject(tok);
                ++readied;
            }
            return;
        }
        auto& ri = *ri_opt;
        ri.resume_data.clear();
        ri.resume_data.push_back(0);
        for (auto tok : made) ri.resume_data.push_back(static_cast<int32_t>(tok));
        promptReady(ctx);
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
