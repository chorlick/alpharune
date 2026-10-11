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

class MirrorImage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        // Create a Reflection token at the controller's base. Full
        // 8-arg signature: (controller, type, name, might, tags,
        // keywords, location, enter_ready). copyUnit() below
        // overwrites might/tags/keywords from the source.
        GameObjectId token = ctx.executor.createToken(
            ctx.controller, CardType::Unit, "Reflection",
            /*might=*/0, /*tags=*/{}, /*keywords=*/KeywordSet{},
            // "to your base": the base must name its player — a default
            // BaseLocation{} is PlayerId::None's base, i.e. nobody's.
            BaseLocation{ctx.controller}, /*enter_ready=*/true);
        // "Then do this: It becomes a copy of that unit. Give it
        // [Temporary]." — a reflexive trigger (CR 388): with the token played
        // it goes on the chain as an item of its own.
        if (token != kInvalidId && ctx.state.objectExists(token))
            ctx.executor.queueReflexiveTrigger(
                ctx.source, ctx.controller, 0,
                {static_cast<int32_t>(token), static_cast<int32_t>(targets[0])});
    }
    bool hasReflexiveTrigger() const override { return true; }
    // `data` = {the Reflection token, the chosen unit}.
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        if (finalizingChoices(ctx)) return;   // nothing to choose
        if (data.size() < 2) return;
        const auto token = static_cast<GameObjectId>(data[0]);
        const auto model = static_cast<GameObjectId>(data[1]);
        auto in_play = [&](GameObjectId id) {
            return ctx.state.objectExists(id) && ctx.state.getObject(id).location.has_value();
        };
        if (!in_play(token)) return;          // the token left in response
        if (in_play(model)) {
            ctx.executor.copyUnit(token, model);
            RIFT_TRACE(ctx.events, "MIRROR IMAGE: Reflection became a copy of " +
                                 ctx.state.getObject(model).name);
        }
        // "Give it [Temporary]" has no duration, so it lasts (CR 816: the
        // token is killed at its controller's next Beginning Phase) — set on
        // the keywords, after the copy, which overwrites them.
        ctx.state.getObject(token).keywords.set(Keyword::Temporary);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 757;
        d.def_id = R"RB(unl-200-219)RB";
        d.name = R"RB(Mirror Image)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-200/219)RB";
        d.collector_number = 200;
        d.artist = R"RB(华锐)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(LeBlanc)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Choose a unit. Play a ready Reflection unit token to your base. Then do this: It becomes a copy of that unit. Give it [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a7ef8a09c0297cd796c4e2dee15922745ede7417-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_757(CardRegistry& r) {
    r.registerCard(757, std::make_unique<MirrorImage>());
}

} // namespace riftbound
