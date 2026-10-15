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
    // The trash is a public zone, so the three cards are targets (CR
    // 355.10.a), chosen as the ability is finalized (CR 383.3, 403.2): one
    // prompt each (slots 7, 9, 11). With fewer than three cards there the
    // third has no legal choice and the ability is removed (CR 403.4, read
    // strictly: judge question 15). A chosen card that has left the trash as
    // it resolves is a mistarget; the others are still recycled.
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto inTrash = [&](const std::vector<GameObjectId>& except) {
            std::vector<GameObjectId> out;
            for (auto cid : ps.trash)
                if (ctx.state.objectExists(cid) &&
                    std::find(except.begin(), except.end(), cid) == except.end())
                    out.push_back(cid);
            return out;
        };
        static const char* const kLabels[3] = {
            "Dr. Mundo: recycle which card from your trash (3 left)",
            "Dr. Mundo: recycle which card from your trash (2 left)",
            "Dr. Mundo: recycle which card from your trash (1 left)"};
        std::vector<GameObjectId> chosen;
        for (size_t k = 0; k < 3; ++k) {
            const auto pick = selectTargetOrNone(*this, ctx, kLabels[k],
                                                 inTrash(chosen), 7 + 2 * k, /*allow_none=*/false);
            if (!pick.has_value()) return;   // waiting for the agent
            if (*pick == kInvalidId) {
                // None to choose (stored, CR 403.4 in the finalize pass), or
                // a mistarget as it resolves: the next ones are still asked
                // for what they stored.
                continue;
            }
            chosen.push_back(*pick);
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        std::vector<GameObjectId> out;
        for (auto cid : chosen) {
            auto it = std::find(ps.trash.begin(), ps.trash.end(), cid);
            if (it == ps.trash.end()) continue;   // left the trash meanwhile
            ps.trash.erase(it);
            out.push_back(cid);
        }
        if (out.empty()) return;
        ctx.executor.recycleCards(ctx.controller, out);
        RIFT_TRACE(ctx.events, "DR. MUNDO: recycled " + std::to_string(out.size()) +
                            " from trash");
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
