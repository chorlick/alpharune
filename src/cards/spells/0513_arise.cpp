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
    // "Then do this: Ready up to two of them." is a reflexive trigger (CR
    // 388): with the tokens played it goes on the chain as an item of its
    // own, carrying them; which of them, and how many (0, 1 or 2), is chosen
    // as that item is finalized (CR 388.4, 355.13).
    static constexpr int kDone = 21;

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value() && ri_opt->resume_point == kDone) return;

        // The number of tokens is fixed when the first one is played: the
        // count is re-read on re-entry, and tokens are not Equipment.
        int equip_count = 0;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (obj.controller != ctx.controller) continue;
            if (isEquipment(obj) && obj.location.has_value()) ++equip_count;
        }
        if (equip_count <= 0) return;
        std::vector<GameObjectId> made;
        // Where each one goes is asked token by token; a battlefield units
        // can't be played to is not offered (playLocations).
        auto make = [&](LocationId where) {
            // CR 187.3: a Sand Soldier token has 2 Might and the Shurima tag.
            return ctx.executor.createToken(ctx.controller, CardType::Unit,
                                            "Sand Soldier", 2, {"Shurima"}, KeywordSet{},
                                            where, /*enter_ready=*/false);
        };
        // At resolution: a play location is a permission, not a target (CR 355.10.b; M5; batch M1).
        if (!playTokensChoosingLocations(ctx, "Arise!: play a Sand Soldier", equip_count,
                                         make, made))
            return;   // waiting for a location
        RIFT_TRACE(ctx.events, "ARISE!: played " + std::to_string(made.size()) +
                            " Sand Soldier token(s)");
        if (ri_opt.has_value()) ri_opt->resume_point = kDone;
        std::vector<int32_t> tokens;
        for (auto tok : made) tokens.push_back(static_cast<int32_t>(tok.raw()));
        ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0, std::move(tokens));
    }
    bool hasReflexiveTrigger() const override { return true; }
    // `data`: the tokens this spell played.
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        auto readyable = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& t = ctx.state.getObject(id);
            return t.location.has_value() && t.is_exhausted;
        };
        std::vector<GameObjectId> cands;
        for (auto v : data)
            if (readyable(GameObjectId::fromRaw(v))) cands.push_back(GameObjectId::fromRaw(v));
        const auto chosen = selectSeveral(ctx, "Arise!: ready a Sand Soldier (up to two)", cands,
                                          /*max_picks=*/2);
        if (!chosen.has_value()) return;          // waiting for the agent
        if (finalizingChoices(ctx)) return;       // choices end here (CR 388.4)
        for (auto id : *chosen)
            if (readyable(id)) ctx.executor.readyObject(id);
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
