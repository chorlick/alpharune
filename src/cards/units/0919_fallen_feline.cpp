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

class FallenFeline : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // "When you play me, name a spell." Any spell: one option per distinct
    // spell name in the card table (Intent::chosen_card_def). The name is kept
    // in string_state["named_spell"].
    static constexpr const char* kNamedKey = "named_spell";
    static constexpr int kAsked = 20, kNamed = 21;   // resume points
    static constexpr size_t kDefSlot = 6;             // resume_data slot
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    static std::vector<CardDefId> nameableSpells(const CardDB& db) {
        std::vector<std::pair<std::string, CardDefId>> by_name;
        for (const auto& [id, def] : db.all()) {
            if (def.card_type != CardType::Spell || def.name.empty()) continue;
            auto it = std::find_if(by_name.begin(), by_name.end(),
                                   [&](const auto& e) { return e.first == def.name; });
            if (it == by_name.end()) by_name.push_back({def.name, id});
            else if (id < it->second) it->second = id;      // reprints share a name
        }
        std::vector<CardDefId> out;
        for (const auto& e : by_name) out.push_back(e.second);
        std::sort(out.begin(), out.end());
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.chain.resuming.has_value()) return;   // only from the chain
        auto& ri = *ctx.state.chain.resuming;
        const auto& db = ctx.executor.cardDB();
        if (ri.resume_point == 0) {
            std::vector<Intent> options;
            for (auto def_id : nameableSpells(db)) {
                Intent i;
                i.type = IntentType::MakeChoice;
                i.player = ctx.controller;
                i.chosen_card_def = def_id;
                i.choice_label = db.get(def_id).name;
                options.push_back(std::move(i));
            }
            if (options.empty()) { ri.resume_point = kNamed; return; }
            ctx.executor.requestChoice(ctx.controller, std::move(options),
                                       "Fallen Feline: name a spell");
            ri.resume_point = kAsked;
            return;
        }
        if (ri.resume_point == kAsked) {
            auto c = ctx.executor.takeChoice();
            while (ri.resume_data.size() <= kDefSlot) ri.resume_data.push_back(0);
            ri.resume_data[kDefSlot] =
                c.has_value() ? static_cast<int32_t>(c->chosen_card_def.raw()) : -1;
            ri.resume_point = kNamed;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ri.resume_point != kNamed || ri.resume_data.size() <= kDefSlot) return;
        const auto def_id = CardDefId::fromRaw(ri.resume_data[kDefSlot]);
        if (def_id == kInvalidId || db.all().find(def_id) == db.all().end()) return;
        const std::string name = db.get(def_id).name;
        ctx.state.getObject(ctx.source).string_state[kNamedKey] = name;
        RIFT_TRACE(ctx.events, "FALLEN FELINE: named \"" + name + "\"");
    }
    // "While I'm at a battlefield, opponents can't play spells with that name."
    // locked_spell_names is cleared every aura pass and re-asserted here.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& self) {
            if (!self.isAtBattlefield()) return;
            auto it = self.string_state.find(kNamedKey);
            if (it == self.string_state.end() || it->second.empty()) return;
            auto& locked = state.player(opponent(controller)).locked_spell_names;
            if (std::find(locked.begin(), locked.end(), it->second) == locked.end())
                locked.push_back(it->second);
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 919;
        d.def_id = R"RB(ven-132-166)RB";
        d.name = R"RB(Fallen Feline)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-132/166)RB";
        d.collector_number = 132;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Cat)RB", R"RB(Piltover)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, name a spell.
While I'm at a battlefield, opponents can't play spells with that name.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e5d46c6d56dc14670b5e25547eeb1ee3ceca5a80-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_919(CardRegistry& r) {
    r.registerCard(919, std::make_unique<FallenFeline>());
}

}  // namespace riftbound
