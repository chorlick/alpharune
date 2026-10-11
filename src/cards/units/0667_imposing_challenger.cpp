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

class ImposingChallenger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When I move, you may move an enemy unit here with less Might than me to
    //  a different battlefield."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }

    std::vector<GameObjectId> candidates(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        if (!ctx.state.objectExists(ctx.source)) return out;
        const auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return out;
        int my_might = self.current_might;
        PlayerId opp = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != opp || !obj.location.has_value()) continue;
            if (obj.untargetable_by_enemy) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != *my_bf) continue;   // "here" = my battlefield
            if (obj.current_might >= my_might) continue;  // "with less Might than me"
            out.push_back(id);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        int conf = confirmOptional(ctx, "Imposing Challenger: move an enemy unit here?",
                                   [&]() { return !candidates(ctx).empty(); });
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / none

        GameObjectId tgt = pickTarget(ctx, "Imposing Challenger: choose an enemy unit",
                                      candidates(ctx));
        // "to a different battlefield" — any battlefield other than mine; the
        // controller picks when there is more than one.
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        // Which battlefields those are is fixed on the first entry past the
        // "may" (as the ability is finalized, CR 383.3) and kept on the chain
        // item: resume_data[kDestsSlot] = how many + 1 (0 = not set yet), the
        // ids after it. A battlefield added before this resolves (Baron
        // Nashor's Pit) then neither makes it ask "which battlefield" as it
        // resolves nor changes what the answer given at finalization means.
        std::vector<BattlefieldId> dests;
        if (auto& ri = ctx.state.chain.resuming; ri.has_value()) {
            constexpr size_t kDestsSlot = Card::kChoiceStateSlot + 1;
            auto& rd = ri->resume_data;
            if (rd.size() <= kDestsSlot || rd[kDestsSlot] == 0) {
                rd.resize(kDestsSlot + 1, 0);
                int32_t n = 0;
                for (const auto& bf : ctx.state.battlefields) {
                    if (my_bf && bf.id == *my_bf) continue;
                    rd.push_back(static_cast<int32_t>(bf.id.raw()));
                    ++n;
                }
                rd[kDestsSlot] = n + 1;
            }
            const size_t n = static_cast<size_t>(rd[kDestsSlot] - 1);
            for (size_t i = 0; i < n && kDestsSlot + 1 + i < rd.size(); ++i)
                dests.push_back(BattlefieldId::fromRaw(rd[kDestsSlot + 1 + i]));
        } else {
            for (const auto& bf : ctx.state.battlefields)
                if (!my_bf || bf.id != *my_bf) dests.push_back(bf.id);
        }
        std::vector<std::string> labels;
        for (auto id : dests) labels.push_back("Battlefield #" + toString(id));
        int which = 0;
        if (tgt != kInvalidId && dests.size() > 1)
            which = pickMode(ctx, "Imposing Challenger: move it to which battlefield",
                             static_cast<int>(dests.size()), labels);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        if (!my_bf || dests.empty()) return;  // no other battlefield exists
        if (which < 0) return;                // waiting for the agent
        const BattlefieldId dest = dests[std::min(static_cast<size_t>(which), dests.size() - 1)];
        if (dest == *my_bf) return;           // I have moved there since: not "a different battlefield"
        ctx.executor.moveToBattlefield(tgt, dest);
        RIFT_TRACE(ctx.events, "IMPOSING CHALLENGER: moved an enemy unit here to a different BF");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 667;
        d.def_id = R"RB(unl-105-219)RB";
        d.name = R"RB(Imposing Challenger)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-105/219)RB";
        d.collector_number = 105;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I move, you may move an enemy unit here with less Might than me to a different battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/01596f6039ef5618fae686e1c7df291e1c570fc3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_667(CardRegistry& r) {
    r.registerCard(667, std::make_unique<ImposingChallenger>());
}

} // namespace riftbound
