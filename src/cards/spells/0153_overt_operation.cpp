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

class OvertOperation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "For each friendly unit, you may spend its buff to ready it. Then buff
    //  all friendly units." One yes/no per friendly unit that has a buff, in
    //  id order. Progress lives on the resolving item: resume_point kAwait
    //  while a prompt is out, resume_data = [units asked so far, the units
    //  to ask...] (fixed when the spell starts resolving).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto friendly_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value()) {
                    out.push_back(id);
                }
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        auto has_buff = [&](GameObjectId id) {
            const auto& u = ctx.state.getObject(id);
            return u.isUnit() && u.controller == ctx.controller &&
                   u.location.has_value() && u.buff_count > 0;
        };
        // Spend the buff: the counter is the +1 [M] (recomputeMight).
        auto spend_to_ready = [&](GameObjectId id) {
            auto& u = ctx.state.getObject(id);
            ctx.executor.spendBuff(id, ctx.controller);   // CR 427.2; "When you spend a buff"
            ctx.executor.readyObject(id);
            ctx.events.logTrace("OVERT OPERATION: spent buff to ready " + u.name);
        };
        auto buff_all = [&]() {
            // "Then buff all friendly units." (Each one without a buff gets one.)
            for (auto id : friendly_units()) ctx.executor.buffUnit(id);
            ctx.events.logTrace("OVERT OPERATION: buffed all friendly units");
        };
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (no chain item to resume): nobody can be
            // asked, so every "may" is a yes, like confirmOptional's default.
            for (auto id : friendly_units()) if (has_buff(id)) spend_to_ready(id);
            buff_all();
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point == kAwait) {
            const GameObjectId id = static_cast<GameObjectId>(ri.resume_data[ri.resume_data[0]]);
            if (takenValue(ctx, 0) == 1 && has_buff(id)) spend_to_ready(id);
        } else if (ri.resume_point != kAsking) {
            ri.resume_data.assign(1, 0);
            for (auto id : friendly_units())
                if (has_buff(id) && ri.resume_data.size() <= kMaxAsked)
                    ri.resume_data.push_back(static_cast<int32_t>(id));
        }
        ri.resume_point = kAsking;
        while (ri.resume_data[0] + 1 < static_cast<int32_t>(ri.resume_data.size())) {
            const GameObjectId id =
                static_cast<GameObjectId>(ri.resume_data[++ri.resume_data[0]]);
            if (!has_buff(id)) continue;
            ctx.executor.requestChoice(ctx.controller,
                {valChoice(ctx.controller, 0, "Keep its buff"),
                 valChoice(ctx.controller, 1, "Spend its buff to ready it")},
                "Overt Operation: spend " + ctx.state.getObject(id).name +
                "'s buff to ready it?");
            ri.resume_point = kAwait;
            return;
        }
        ri.resume_point = kDone;
        buff_all();
    }
private:
    // One question per buffed friendly unit. ChainManager::runResumable allows
    // 1024 re-entries per resolution (kMaxResumeIterations) and throws beyond
    // that; no board comes near it, this only keeps the spell under the bound.
    static constexpr size_t kMaxAsked = 1000;
    enum : int { kAsking = 20, kAwait = 21, kDone = 22 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 153;
        d.def_id = R"RB(ogn-153-298)RB";
        d.name = R"RB(Overt Operation)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-153/298)RB";
        d.collector_number = 153;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
For each friendly unit, you may spend its buff to ready it. Then buff all friendly units. (Each one that doesn't have a buff gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/69dba13c930ba3962851346d2bc6cbeb4ca48455-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_153(CardRegistry& r) {
    r.registerCard(153, std::make_unique<OvertOperation>());
}

} // namespace riftbound
