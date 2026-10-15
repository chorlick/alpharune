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

class TheZeroDrive : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // ── Equip [1][B] ──
    bool hasEquipAbility() const override { return true; }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .energy = 1, .power = 1, .power_domain = Domain::Mind};
    }

    // ── Equipped: "[Deathknell] — Banish me." ──
    // A Deathknell, not a replacement: the bearer dies like any unit (its own
    // Deathknells and "when a friendly unit dies" abilities trigger), and this
    // ability then banishes its card from the trash. A token bearer ceased to
    // exist as it died (CR 183.1): nothing is banished, nothing is recorded.
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIDie; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                           const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(unit) || !ctx.state.objectExists(ctx.source)) return;
        const auto& dead = ctx.state.getObject(unit);
        if (dead.isToken() || dead.zone != ZoneType::Trash) return;   // no longer where it died to
        ctx.executor.banishObject(unit);
        // "banished with this": only a Drive still on the board can play it back.
        auto& gear = ctx.state.getObject(ctx.source);
        if (gear.location.has_value()) gear.tracked_objects.push_back(unit);
        RIFT_TRACE(ctx.events, "THE ZERO DRIVE: Deathknell — banished " +
                            ctx.state.getObject(unit).name);
    }

    // Played again, this is a new object (CR 124.1): nothing is "banished
    // with this" yet (testing phase R7, as Cursed Sarcophagus).
    void onPlay(CardContext& ctx) override {
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).tracked_objects.clear();
    }

    // ── "[3][B], Banish this: Play all units banished with this, ignoring
    //     their costs. (Use only if unattached.)" ──
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        // All of it is the cost, paid as the ability is activated: the Drive
        // is banished before anyone can respond (ActivationCost::banish_self).
        return {.exhaust = false, .energy = 3, .power = 1, .power_domain = Domain::Mind,
                .banish_self = true};
    }
    // "(Use only if unattached.)" — asked of the copy that is activating. Not
    // offered with nothing to play back (the activation would only banish it).
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!GearCard::canActivateAbilityAt(state, controller, source, ability_index))
            return false;
        if (!state.objectExists(source)) return false;
        const auto& gear = state.getObject(source);
        return gear.location.has_value() && !gear.attached_to.has_value() &&
               !banishedWithThis(state, gear).empty();
    }
    // CR 355.2.a: each unit goes to its controller's base or to a battlefield
    // they control that units may be played to — asked one unit at a time.
    // The Drive is already banished when this resolves, so progress lives on
    // the resolving item: resume_point kPlaying once the list is taken
    // (kAwaitWhere while a location is being asked), resume_data = [units
    // played so far, the unit ids...].
    static constexpr int kPlaying = 31;
    static constexpr int kAwaitWhere = 32;
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ri = ctx.state.chain.resuming;
        const auto& bfs = ctx.state.battlefields;
        // Where `uid` may be played: a battlefield its controller controls or
        // one its own permissions allow (CR 355.2, 420.3.b), never one units
        // can't be played to (effectPlayAllowedAt).
        auto unitPlayableAt = [&](GameObjectId uid, const BattlefieldState& bf) {
            return effectPlayAllowedAt(ctx, ctx.controller, uid, bf);
        };
        auto play = [&](GameObjectId uid, std::optional<LocationId> where) {
            // playIgnoringCost takes the card out of the banishment zone itself.
            ctx.executor.playIgnoringCost(ctx.controller, uid, where);
            RIFT_TRACE(ctx.events, "THE ZERO DRIVE: replayed banished unit " +
                                ctx.state.getObject(uid).name);
        };

        std::vector<GameObjectId> to_play;
        size_t next = 0;
        const bool resumed = ri.has_value() && !ri->resume_data.empty() &&
                             (ri->resume_point == kPlaying || ri->resume_point == kAwaitWhere);
        if (resumed) {
            next = static_cast<size_t>(ri->resume_data[0]);
            for (size_t i = 1; i < ri->resume_data.size(); ++i)
                to_play.push_back(GameObjectId::fromRaw(ri->resume_data[i]));
            if (ri->resume_point == kAwaitWhere && next < to_play.size()) {
                // The answer: 0 = base, 1 + i = battlefield slot i.
                std::optional<LocationId> where;
                const int v = takenValue(ctx, 0);
                if (v >= 1 && static_cast<size_t>(v) <= bfs.size() &&
                    unitPlayableAt(to_play[next], bfs[v - 1]))
                    where = LocationId{BattlefieldLocation{bfs[v - 1].id}};
                ri->resume_point = kPlaying;
                play(to_play[next], where);
                ri->resume_data[0] = static_cast<int32_t>(++next);
            }
        } else {
            auto& gear = ctx.state.getObject(ctx.source);
            // "Banish this" was paid by the engine as the ability was activated;
            // the Drive is in banishment now and still knows what it banished.
            // Only the units that are still there are played.
            to_play = banishedWithThis(ctx.state, gear);
            gear.tracked_objects.clear();
            if (ri.has_value()) {
                ri->resume_data.assign(1, 0);
                for (auto uid : to_play) ri->resume_data.push_back(static_cast<int32_t>(uid.raw()));
                ri->resume_point = kPlaying;
            }
        }
        for (; next < to_play.size(); ++next) {
            const GameObjectId uid = to_play[next];
            if (ri.has_value() && ctx.state.objectExists(uid)) {
                std::vector<Intent> where;
                for (size_t i = 0; i < bfs.size(); ++i)
                    if (unitPlayableAt(uid, bfs[i]))
                        where.push_back(valChoice(ctx.controller, static_cast<int>(i) + 1,
                            "The Zero Drive: play to battlefield " + std::to_string(i)));
                if (!where.empty()) {
                    where.insert(where.begin(),
                                 valChoice(ctx.controller, 0, "The Zero Drive: play to base"));
                    ri->resume_data[0] = static_cast<int32_t>(next);
                    // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
                    ctx.executor.requestChoice(ctx.controller, std::move(where),
                        "The Zero Drive: where to play " + ctx.state.getObject(uid).name, PromptKind::Location);
                    ri->resume_point = kAwaitWhere;
                    return;
                }
            }
            play(uid, std::nullopt);
            if (ri.has_value()) ri->resume_data[0] = static_cast<int32_t>(next + 1);
        }
    }
private:
    /// The units this Drive banished that are still in banishment.
    static std::vector<GameObjectId> banishedWithThis(const GameState& state,
                                                      const GameObject& gear) {
        std::vector<GameObjectId> out;
        for (auto uid : gear.tracked_objects) {
            if (!state.objectExists(uid)) continue;
            const auto& u = state.getObject(uid);
            if (u.zone != ZoneType::Banishment || u.isToken()) continue;
            const auto& bz = state.player(u.owner).banishment;
            if (std::find(bz.begin(), bz.end(), uid) != bz.end()) out.push_back(uid);
        }
        return out;
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 412;
        d.def_id = R"RB(sfd-090-221)RB";
        d.name = R"RB(The Zero Drive)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-090/221)RB";
        d.collector_number = 90;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][B] ([1][B]: Attach this to a unit you control.)
[3][B], Banish this: Play all units banished with this, ignoring their costs. (Use only if unattached.))RB";
        d.effect_text = R"RB([Deathknell] — Banish me. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cc80cbfa1a64ad22dc5f7df870e9206f6938f7f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_412(CardRegistry& r) {
    r.registerCard(412, std::make_unique<TheZeroDrive>());
}

} // namespace riftbound
