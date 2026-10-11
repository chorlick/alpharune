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

class DrMundoExpert : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "My Might is increased by the number of cards in your trash."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        int trash_count = static_cast<int>(state.player(controller).trash.size());
        if (trash_count <= 0) return;
        for (auto& [sid, self] : state.objects.onBoard()) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = trash_count;
            self.aura_effects.push_back(ae);
        }
    }

    // "At the start of your Beginning Phase, recycle 3 from your trash."
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto recycle = [&](const std::vector<GameObjectId>& cards) {
            std::vector<GameObjectId> out;
            for (auto cid : cards) {
                auto it = std::find(ps.trash.begin(), ps.trash.end(), cid);
                if (it == ps.trash.end()) continue;   // left the trash meanwhile
                ps.trash.erase(it);
                out.push_back(cid);
            }
            if (out.empty()) return;
            ctx.executor.recycleCards(ctx.controller, out);
            RIFT_TRACE(ctx.events, "DR. MUNDO: recycled " + std::to_string(out.size()) +
                                " from trash");
        };
        // Three or fewer cards: all of them, nothing to choose. Also when
        // driven without a chain item, where nobody can be asked.
        if (ps.trash.size() <= 3 || !ctx.state.chain.resuming.has_value()) {
            recycle(std::vector<GameObjectId>(
                ps.trash.begin(), ps.trash.begin() + std::min<size_t>(3, ps.trash.size())));
            return;
        }
        // More than three: my controller picks which three, one at a time.
        // resume_data = the cards picked so far; resume_point kAwaitPick while
        // a prompt is out, kDone once recycled.
        enum { kAwaitPick = 20, kDone = 21 };
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        auto picked_already = [&](GameObjectId id) {
            return std::find(ri.resume_data.begin(), ri.resume_data.end(),
                             static_cast<int32_t>(id.raw())) != ri.resume_data.end();
        };
        auto remaining = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : ps.trash) if (!picked_already(cid)) out.push_back(cid);
            return out;
        };
        if (ri.resume_point == kAwaitPick) {
            GameObjectId id = takenObject(ctx);
            auto rest = remaining();
            if (std::find(rest.begin(), rest.end(), id) == rest.end())
                id = rest.empty() ? kInvalidId : rest.front();
            if (id != kInvalidId) ri.resume_data.push_back(static_cast<int32_t>(id.raw()));
            ri.resume_point = 0;
        } else {
            ri.resume_data.clear();   // first entry: nothing picked yet
        }
        auto rest = remaining();
        if (ri.resume_data.size() < 3 && !rest.empty()) {
            std::vector<Intent> opts;
            for (auto cid : rest) opts.push_back(objChoice(ctx.controller, cid));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                "Dr. Mundo: recycle which card from your trash (" +
                std::to_string(3 - ri.resume_data.size()) + " left)");
            ri.resume_point = kAwaitPick;
            return;
        }
        std::vector<GameObjectId> chosen;
        for (auto v : ri.resume_data) chosen.push_back(GameObjectId::fromRaw(v));
        ri.resume_point = kDone;
        recycle(chosen);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 109;
        d.def_id = R"RB(ogn-109-298)RB";
        d.name = R"RB(Dr. Mundo, Expert)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-109/298)RB";
        d.collector_number = 109;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Dr. Mundo)RB", R"RB(Zaun)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(My Might is increased by the number of cards in your trash.
At the start of your Beginning Phase, recycle 3 from your trash.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cf51fe8bedf5139e8cc1fc062969949ea7b121f0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_109(CardRegistry& r) {
    r.registerCard(109, std::make_unique<DrMundoExpert>());
}

} // namespace riftbound
