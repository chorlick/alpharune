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

class Flash : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // A friendly unit that is not already in its base.
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object,
            [&](const GameObject& u) {
                return !u.isAtBase();
            });
    }
    // "Move up to 2 friendly units to base." The units are chosen one prompt
    // at a time as the spell is played; at each, the first included, the
    // controller may stop ("up to"). Progress lives on the item: resume_point kAwait while a
    // prompt is out, kChosen once the choices are made; resume_data = the
    // units chosen.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        auto picked_before = [&ri](GameObjectId id) {
            return std::find(ri.resume_data.begin(), ri.resume_data.end(),
                             static_cast<int32_t>(id)) != ri.resume_data.end();
        };
        auto movable = [&](GameObjectId id) {
            const auto& obj = ctx.state.getObject(id);
            return obj.isUnit() && obj.controller == ctx.controller &&
                   obj.location.has_value() && !obj.isAtBase();
        };
        auto candidates = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects.onBoard())
                if (movable(id) && !picked_before(id)) out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        if (ri.resume_point != kChosen) {
            bool stop = false;
            if (ri.resume_point == kAwait) {
                const auto units = candidates();
                const GameObjectId unit = takenObject(ctx);
                if (std::find(units.begin(), units.end(), unit) == units.end()) {
                    stop = true;   // "No more units"
                } else {
                    commitChosenObject(ctx, unit);
                    ri.resume_data.push_back(static_cast<int32_t>(unit));
                }
            } else {
                ri.resume_data.clear();
            }
            if (!stop && ri.resume_data.size() < 2) {
                const auto units = candidates();
                if (!units.empty()) {
                    std::vector<Intent> options;
                    // "up to 2" includes none (CR 355.13): offered on the
                    // first prompt too.
                    options.push_back(valChoice(ctx.controller, 0, "No more units"));
                    for (auto id : units) options.push_back(objChoice(ctx.controller, id));
                    ctx.executor.requestChoice(ctx.controller, std::move(options),
                        "Flash: move up to 2 friendly units to base");
                    ri.resume_point = kAwait;
                    return;
                }
            }
            ri.resume_point = kChosen;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        ri.resume_point = kDone;
        for (auto v : ri.resume_data) {
            const auto id = static_cast<GameObjectId>(v);
            if (!movable(id)) continue;   // no longer a legal target
            ctx.executor.moveToBase(id);
            RIFT_TRACE(ctx.events, "FLASH: moved " + ctx.state.getObject(id).name + " to base");
        }
    }
private:
    enum : int { kAwait = 21, kChosen = 22, kDone = 23 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 309;
        d.def_id = R"RB(ogs-011-024)RB";
        d.name = R"RB(Flash)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-011/024)RB";
        d.collector_number = 11;
        d.artist = R"RB(Sugar Free)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Move up to 2 friendly units to base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4d9cc1c13b75933e509e642213f13359350cd3f9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_309(CardRegistry& r) {
    r.registerCard(309, std::make_unique<Flash>());
}

} // namespace riftbound
