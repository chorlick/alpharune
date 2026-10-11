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

class ShadySpectacles : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "[Equip] [1][Y]"
    // "As this is attached to a unit, choose another friendly unit. The
    //  equipped unit becomes a copy of that unit for as long as this is
    //  attached to it."
    //
    // Ending the copy (CR 446 / 479.1: a copy effect lasts only as long as its
    // duration). What the bearer was is remembered ON THE BEARER, just before
    // EffectExecutor::copyUnit overwrites it:
    //   card_counters  __spectacles_copies  how many Spectacles copies are on it
    //                  __spectacles_def / _might / _kw / _super / _type /
    //                  _assault / _shield / _deflect
    //   string_state   __spectacles_name, __spectacles_tags (\x1f-separated),
    //                  __spectacles_domains (one char per Domain)
    // exactly the fields copyUnit writes. Keywords are saved without the
    // "this turn" grants (temp_keywords) so restoring can't make them lasting.
    // The gear remembers whom it turned into a copy in its own
    // card_counters["__spectacles_bearer"] (id + 1). onUnattached restores the
    // bearer when the last such copy comes off — which also covers plain
    // tokens, whose invalid card_def_id is put back (audit item 4).
    // With two Shady Spectacles on one unit, taking one off leaves the unit as
    // the copy most recently applied (copy timestamps are not modelled).
    //
    // Equip resolves outside the chain, so the unit to copy is chosen with a
    // direct agent decision (EffectExecutor::chooseObjectNow).
    static constexpr const char* kCopies = "__spectacles_copies";
    static constexpr const char* kBearer = "__spectacles_bearer";

    static void remember(GameObject& u) {
        u.card_counters["__spectacles_def"] = static_cast<int>(u.card_def_id);
        u.card_counters["__spectacles_might"] = u.base_might;
        u.card_counters["__spectacles_kw"] =
            static_cast<int>(u.keywords.bits & ~u.temp_keywords.bits);
        u.card_counters["__spectacles_super"] = static_cast<int>(u.super_type);
        u.card_counters["__spectacles_type"] = static_cast<int>(u.card_type);
        u.card_counters["__spectacles_assault"] = u.assault_value;
        u.card_counters["__spectacles_shield"] = u.shield_value;
        u.card_counters["__spectacles_deflect"] = u.deflect_value;
        u.string_state["__spectacles_name"] = u.name;
        std::string tags;
        for (size_t i = 0; i < u.tags.size(); ++i) {
            if (i) tags += '\x1f';
            tags += u.tags[i];
        }
        u.string_state["__spectacles_tags"] = tags;
        u.card_counters["__spectacles_ntags"] = static_cast<int>(u.tags.size());
        std::string doms;
        for (auto d : u.domains) doms += static_cast<char>('a' + static_cast<int>(d));
        u.string_state["__spectacles_domains"] = doms;
    }
    static void restore(GameObject& u) {
        u.card_def_id = static_cast<CardDefId>(u.card_counters["__spectacles_def"]);
        u.base_might = u.card_counters["__spectacles_might"];
        // Keep whatever is granted "this turn" right now; drop the copied rest.
        u.keywords.bits = static_cast<uint32_t>(u.card_counters["__spectacles_kw"]) |
                          u.temp_keywords.bits;
        u.super_type = static_cast<SuperType>(u.card_counters["__spectacles_super"]);
        u.card_type = static_cast<CardType>(u.card_counters["__spectacles_type"]);
        u.assault_value = u.card_counters["__spectacles_assault"];
        u.shield_value = u.card_counters["__spectacles_shield"];
        u.deflect_value = u.card_counters["__spectacles_deflect"];
        u.name = u.string_state["__spectacles_name"];
        u.tags.clear();
        if (u.card_counters["__spectacles_ntags"] > 0) {
            const std::string& tags = u.string_state["__spectacles_tags"];
            size_t start = 0;
            while (true) {
                size_t end = tags.find('\x1f', start);
                u.tags.push_back(tags.substr(start, end == std::string::npos
                                                        ? std::string::npos : end - start));
                if (end == std::string::npos) break;
                start = end + 1;
            }
        }
        u.domains.clear();
        for (char c : u.string_state["__spectacles_domains"])
            u.domains.push_back(static_cast<Domain>(c - 'a'));
        for (const char* k : {"__spectacles_def", "__spectacles_might", "__spectacles_kw",
                              "__spectacles_super", "__spectacles_type",
                              "__spectacles_assault", "__spectacles_shield",
                              "__spectacles_deflect", "__spectacles_ntags"})
            u.card_counters.erase(k);
        for (const char* k : {"__spectacles_name", "__spectacles_tags", "__spectacles_domains"})
            u.string_state.erase(k);
        u.recomputeMight();
    }
    /// End the copy this gear applied to `unit` (if it did).
    static void endCopy(CardContext& ctx, GameObjectId gear_id, GameObjectId unit) {
        if (!ctx.state.objectExists(gear_id)) return;
        auto& gear = ctx.state.getObject(gear_id);
        auto it = gear.card_counters.find(kBearer);
        if (it == gear.card_counters.end() || it->second != static_cast<int>(unit) + 1) return;
        gear.card_counters.erase(it);
        if (!ctx.state.objectExists(unit)) return;
        auto& u = ctx.state.getObject(unit);
        auto n = u.card_counters.find(kCopies);
        if (n == u.card_counters.end() || n->second <= 0) return;
        if (--n->second > 0) return;          // another Spectacles copy is still on it
        u.card_counters.erase(n);
        restore(u);
        RIFT_TRACE(ctx.events, "SHADY SPECTACLES: came off " + u.name +
                            " -> it is itself again");
    }
    void onUnattached(CardContext& ctx, GameObjectId unit) override {
        endCopy(ctx, ctx.source, unit);
    }

    bool hasEquipAbility() const override { return true; }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .energy = 1, .power = 1, .power_domain = Domain::Order};
    }
    void onEquipped(CardContext& ctx, GameObjectId unit) override {
        // A copy this gear left behind on an earlier bearer (it came off by a
        // path that skipped onUnattached) ends now at the latest.
        {
            auto& gear = ctx.state.getObject(ctx.source);
            auto it = gear.card_counters.find(kBearer);
            if (it != gear.card_counters.end() && it->second != static_cast<int>(unit) + 1)
                endCopy(ctx, ctx.source, static_cast<GameObjectId>(it->second - 1));
        }
        std::vector<GameObjectId> others;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (id == unit || !obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            others.push_back(id);
        }
        std::sort(others.begin(), others.end());
        GameObjectId chosen = ctx.state.chain.resuming.has_value()
            ? pickTarget(ctx, "Shady Spectacles: copy which friendly unit?", others)
            : ctx.executor.chooseObjectNow(ctx.controller, others);
        if (chosen != kInvalidId && ctx.state.objectExists(chosen)) {
            auto& u = ctx.state.getObject(unit);
            if (u.card_counters[kCopies]++ == 0) remember(u);
            ctx.state.getObject(ctx.source).card_counters[kBearer] =
                static_cast<int>(unit) + 1;
            ctx.executor.copyUnit(unit, chosen);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 924;
        d.def_id = R"RB(ven-137-166)RB";
        d.name = R"RB(Shady Spectacles)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-137/166)RB";
        d.collector_number = 137;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 4;
        d.might_bonus = 0;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][Y] ([1][Y]: Attach this to a unit you control.)
As this is attached to a unit, choose another friendly unit. The equipped unit becomes a copy of that unit for as long as this is attached to it.)RB";
        d.effect_text = R"RB((I am a copy of the chosen unit.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e49461109a4116c22af9206719f53fb73aee36d0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_924(CardRegistry& r) {
    r.registerCard(924, std::make_unique<ShadySpectacles>());
}

}  // namespace riftbound
