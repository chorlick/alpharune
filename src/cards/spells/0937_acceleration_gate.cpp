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

class AccelerationGate : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Ready up to 4 units, gear, and/or runes."
    // Resumable: up to four sequential picks (each with a "Done" option) over
    // the exhausted units, gear and runes on the board. The text is not
    // limited to friendly objects; enemy objects that can't be chosen by
    // enemy spells are excluded, an enemy [Deflect] unit is offered only if
    // its Deflect can be paid, and each pick is committed like a pickTarget
    // choice (Deflect paid, recorded as chosen).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        auto candidates = [&]() {
            std::vector<GameObjectId> out;
            for (const auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value() || !obj.is_exhausted) continue;
                if (!obj.isUnit() && !obj.isGear() && !obj.isRune()) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        auto prompt = [&]() {
            auto c = offerableChoices(ctx, candidates());
            if (c.empty()) return false;
            std::vector<Intent> opts;
            for (auto id : c) opts.push_back(objChoice(ctx.controller, id));
            opts.push_back(valChoice(ctx.controller, 0, "Done"));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Acceleration Gate: ready a unit, gear or rune");
            ri.resume_point = 1;
            return true;
        };
        if (ri.resume_point == 0) {
            ri.resume_data = {0};
            if (!prompt()) ri.resume_point = 2;
            return;
        }
        if (ri.resume_point != 1) return;
        GameObjectId picked = takenObject(ctx);
        auto legal = offerableChoices(ctx, candidates());
        if (picked == kInvalidId ||
            std::find(legal.begin(), legal.end(), picked) == legal.end()) {
            ri.resume_point = 2;   // "Done"
            return;
        }
        commitChosenObject(ctx, picked);
        ctx.executor.readyObject(picked);
        if (ri.resume_data.empty()) ri.resume_data = {0};
        if (++ri.resume_data[0] >= 4 || !prompt()) ri.resume_point = 2;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 937;
        d.def_id = R"RB(ven-150-166)RB";
        d.name = R"RB(Acceleration Gate)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-150/166)RB";
        d.collector_number = 150;
        d.artist = R"RB(小牛设计/Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Body};
        d.tags = {R"RB(Jayce)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Ready up to 4 units, gear, and/or runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/31b71a16ee85cf30124fdaa2c575e709a4c83771-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_937(CardRegistry& r) {
    r.registerCard(937, std::make_unique<AccelerationGate>());
}

}  // namespace riftbound
