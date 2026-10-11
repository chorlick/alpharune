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

class DecreeOfDiscord : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Return any number of enemy Order ([Y]) units with total Might 5 or less
    //  to their owners' hands."
    // Resumable: repeated picks (each with a "Done" option) against a Might
    // budget of 5; all chosen units are returned together at the end.
    // resume_data = {remaining budget, picked ids...}. The picks are built by
    // hand, so each goes through offerableChoices / commitChosenObject.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        auto candidates = [&]() {
            auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                if (o.controller == ctx.controller) return false;
                if (std::find(o.domains.begin(), o.domains.end(), Domain::Order) ==
                    o.domains.end()) return false;
                if (o.current_might > ri.resume_data[0]) return false;
                for (size_t i = 1; i < ri.resume_data.size(); ++i)
                    if (GameObjectId::fromRaw(ri.resume_data[i]) == o.id) return false;
                return true;
            });
            std::sort(out.begin(), out.end());
            // Deflect (CR 809): only units whose cost the controller can pay.
            return offerableChoices(ctx, out);
        };
        auto prompt = [&]() {
            auto c = candidates();
            if (c.empty()) return false;
            std::vector<Intent> opts;
            for (auto id : c) opts.push_back(objChoice(ctx.controller, id));
            // "any number" includes zero (CR 355.13): "Done" is on every
            // prompt, and with no unit to choose the spell still resolves.
            opts.push_back(valChoice(ctx.controller, 0, "Done"));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                "Decree of Discord: return an enemy Order unit (Might left: " +
                std::to_string(ri.resume_data[0]) + ")");
            ri.resume_point = 1;
            return true;
        };
        auto finish = [&]() {
            ri.resume_point = 2;
            for (size_t i = 1; i < ri.resume_data.size(); ++i) {
                auto id = GameObjectId::fromRaw(ri.resume_data[i]);
                if (!ctx.state.objectExists(id)) continue;
                auto& unit = ctx.state.getObject(id);
                if (!unit.location.has_value()) continue;
                for (auto gid : std::vector<GameObjectId>(unit.attachments))
                    ctx.executor.unattachGear(gid);
                ctx.executor.bounceToHand(id);
            }
        };
        if (ri.resume_point == 0) {
            ri.resume_data = {5};
            if (!prompt()) finish();
            return;
        }
        if (ri.resume_point != 1 || ri.resume_data.empty()) return;
        GameObjectId picked = takenObject(ctx);
        auto legal = candidates();
        if (picked == kInvalidId ||
            std::find(legal.begin(), legal.end(), picked) == legal.end()) {
            finish();   // "Done"
            return;
        }
        // The unit is chosen: Deflect is paid and the choice recorded, as
        // Card::pickTarget would.
        commitChosenObject(ctx, picked);
        ri.resume_data[0] -= ctx.state.getObject(picked).current_might;
        ri.resume_data.push_back(static_cast<int32_t>(picked.raw()));
        if (!prompt()) finish();
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 894;
        d.def_id = R"RB(ven-107-166)RB";
        d.name = R"RB(Decree of Discord)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-107/166)RB";
        d.collector_number = 107;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Return any number of enemy Order ([Y]) units with total Might 5 or less to their owners' hands.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b0e82842acddb99c47af6793dab48164caf498c2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_894(CardRegistry& r) {
    r.registerCard(894, std::make_unique<DecreeOfDiscord>());
}

}  // namespace riftbound
