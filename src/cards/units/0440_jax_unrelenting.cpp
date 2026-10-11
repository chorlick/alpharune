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

class JaxUnrelenting : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Two triggered abilities:
    //  - [Weaponmaster] "When you play me, you may [Equip] one of your
    //    Equipment to me for [A] less, even if it's already attached."
    //  - "When you attach an Equipment to me, you may pay [1] to draw 1." —
    //    its own ability: it triggers on EVERY attach (Weaponmaster, a later
    //    Equip activation, an effect), not only on the play.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenEquipmentAttachedToMe};
    }

    // Resume points of the Weaponmaster choice (clear of the helper ranges).
    static constexpr int kAwaitGear = 20;
    static constexpr int kDone = 21;

    // What Weaponmaster's "[Equip] ... for [A] less" costs for `gear`.
    // ENGINE-GAP: an Equip cost lives inside each gear's onEquip — no Card
    // hook exposes it or accepts a discount (needed: e.g.
    // `virtual ActivationCost Card::equipCost() const` and an engine equip
    // path that applies a Power reduction). The printed "[Equip] [n][X]" cost
    // is read here instead: its Energy is charged and one Power is taken off.
    // Equipment whose Equip cost has non-rune parts ("[Equip] — Spend 1 XP",
    // "..., Recycle 2 cards", "..., Kill a friendly unit") or would still have
    // Power to pay after the discount is not offered.
    struct ReducedEquipCost {
        bool payable_here = false;
        int energy = 0;
    };
    static ReducedEquipCost reducedEquipCost(CardContext& ctx, const GameObject& gear) {
        ReducedEquipCost out;
        if (gear.card_def_id == kInvalidId) {   // no printed cost at all
            out.payable_here = true;
            return out;
        }
        const std::string& text = ctx.executor.cardDB().get(gear.card_def_id).ability_text;
        static const std::string kKey = "[Equip]";
        size_t p = text.find(kKey);
        if (p == std::string::npos) return out;
        p += kKey.size();
        while (p < text.size() && text[p] == ' ') ++p;
        int energy = 0, power = 0;
        bool any = false;
        while (p < text.size() && text[p] == '[') {
            const size_t q = text.find(']', p);
            if (q == std::string::npos) return out;
            const std::string tok = text.substr(p + 1, q - p - 1);
            if (tok.empty()) return out;
            if (std::all_of(tok.begin(), tok.end(),
                            [](unsigned char c) { return c >= '0' && c <= '9'; })) {
                energy += std::stoi(tok);
            } else if (tok.size() == 1 && std::string("RGBOPYCA").find(tok[0]) != std::string::npos) {
                ++power;
            } else {
                return out;
            }
            any = true;
            p = q + 1;
        }
        if (!any) return out;                                   // "[Equip] — ..." form
        while (p < text.size() && text[p] == ' ') ++p;
        if (p < text.size() && text[p] == ',') return out;     // further costs follow
        if (power > 1) return out;                              // Power left after "[A] less"
        out.payable_here = true;
        out.energy = energy;
        return out;
    }
    /// Equipment I may Weaponmaster-equip right now (cost payable, CR 445.2).
    std::vector<GameObjectId> equippable(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        const int energy = payableEnergy(ctx.state, ctx.controller);
        for (const auto& [id, obj] : ctx.state.objects) {
            if (obj.controller != ctx.controller || !obj.location.has_value()) continue;
            if (!isEquipment(obj)) continue;
            // "even if it's already attached" — to another unit; one already
            // on me has nothing to gain.
            if (obj.attached_to.has_value() && *obj.attached_to == ctx.source) continue;
            const auto cost = reducedEquipCost(ctx, obj);
            if (!cost.payable_here || cost.energy > energy) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    void equip(CardContext& ctx, GameObjectId gear_id) {
        const auto cost = reducedEquipCost(ctx, ctx.state.getObject(gear_id));
        if (!payEnergy(ctx, ctx.controller, cost.energy)) return;
        // Through the executor so the gear's onUnattached hook runs.
        if (ctx.state.getObject(gear_id).attached_to.has_value())
            ctx.executor.unattachGear(gear_id);
        attachFree(ctx, gear_id, ctx.source);   // emits "equipped" -> my attach trigger
        ctx.events.logTrace("JAX, UNRELENTING: [Weaponmaster] equipped " +
                            ctx.state.getObject(gear_id).name);
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.firing_trigger == TriggerType::WhenEquipmentAttachedToMe) {
            // "you may pay [1] to draw 1". CR 445.2 / 164.2.a: the [1] may come
            // from floating Energy or from exhausting a ready rune.
            auto still_legal = [&ctx]() {
                return payableEnergy(ctx.state, ctx.controller) >= 1;
            };
            int conf = confirmOptional(ctx, "Jax, Unrelenting: pay [1] to draw 1?",
                                       still_legal);
            if (conf != 1) return;       // waiting, declined, or can't pay
            if (!payEnergy(ctx, ctx.controller, 1)) return;
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("JAX, UNRELENTING: paid [1] -> draw 1");
            return;
        }

        // ── [Weaponmaster] (when you play me) ──
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        auto& ri_opt = ctx.state.chain.resuming;
        if (!ri_opt.has_value()) {
            // Direct invocation (no chain item to resume): no agent can be
            // asked, so take the first Equipment, as the choice helpers do.
            auto cands = equippable(ctx);
            if (!cands.empty()) equip(ctx, cands.front());
            return;
        }
        auto& ri = *ri_opt;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point == kAwaitGear) {
            ri.resume_point = kDone;
            auto choice = ctx.executor.takeChoice();
            auto cands = equippable(ctx);
            GameObjectId picked = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) picked = choice->chosen_objects[0];
            if (std::find(cands.begin(), cands.end(), picked) == cands.end()) {
                ctx.events.logTrace("JAX, UNRELENTING: [Weaponmaster] declined");
                return;
            }
            equip(ctx, picked);
            return;
        }
        // Which Equipment — or none ("you may") — is the controller's choice.
        auto cands = equippable(ctx);
        if (cands.empty()) { ri.resume_point = kDone; return; }
        std::vector<Intent> opts;
        for (auto id : cands) {
            Intent c;
            c.type = IntentType::MakeChoice;
            c.player = ctx.controller;
            c.chosen_objects = {id};
            opts.push_back(std::move(c));
        }
        Intent none;
        none.type = IntentType::MakeChoice;
        none.player = ctx.controller;
        none.chosen_value = 0;
        none.choice_label = "No — Jax, Unrelenting: [Weaponmaster]";
        opts.push_back(std::move(none));
        ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                   "Jax, Unrelenting: [Weaponmaster] equip which Equipment?");
        ri.resume_point = kAwaitGear;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 440;
        d.def_id = R"RB(sfd-119-221)RB";
        d.name = R"RB(Jax, Unrelenting)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-119/221)RB";
        d.collector_number = 119;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Jax)RB", R"RB(Icathia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)
When you attach an Equipment to me, you may pay [1] to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e63ab3370a6e4ce4baddfba12ab92468f6cc9541-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_440(CardRegistry& r) {
    r.registerCard(440, std::make_unique<JaxUnrelenting>());
}

} // namespace riftbound
