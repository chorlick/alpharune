#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class BlindFury : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each opponent reveals the top card of their Main Deck. Choose one and
    //  banish it, then play it, ignoring its cost. Then recycle the rest."
    // 1v1: a single opponent reveals one card; you choose it, banish, then
    //  play it under your control. "the rest" is empty in 1v1.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        // Reveal once — not again when re-entered with the play location.
        const bool first_pass = !ctx.state.chain.resuming.has_value() ||
                                ctx.state.chain.resuming->resume_point == 0;
        // Void Hatchling: "If you would reveal cards from a deck, look at the
        // top card first." The OPPONENT reveals here ("Each opponent reveals
        // the top card of their Main Deck"), so it is the opponent's Hatchling
        // that looks, at the opponent's deck; the caster's does nothing.
        if (first_pass) ctx.executor.lookBeforeReveal(opp);
        const auto& opp_deck = ctx.state.player(opp).main_deck;
        if (opp_deck.empty()) {
            RIFT_TRACE(ctx.events, "BLIND FURY: opponent deck empty — no-op");
            return;
        }
        GameObjectId top = opp_deck.back();
        if (!ctx.state.objectExists(top)) return;
        const std::string name = ctx.state.getObject(top).name;
        // Reveal publicly.
        if (first_pass) {
            const auto& card = ctx.state.getObject(top);
            ctx.events.emit(CardRevealedEvent{
                top, card.card_def_id, card.owner,
                /*revealed_to_all=*/true, ctx.controller, ZoneType::MainDeck});
            RIFT_TRACE(ctx.events, "BLIND FURY: opponent reveals " + name);
        }
        // CR 355.2.a: no location is printed, so a unit goes to my base or to
        // a battlefield I control (and may be played to), my choice. Asked
        // while the card is still the revealed top card. Gear goes to base.
        std::optional<LocationId> where_to;
        if (ctx.state.getObject(top).isUnit()) {
            const int where = pickEffectPlayLocation(*this, ctx, "Blind Fury: play it to");
            if (where == -1) return;   // waiting for the agent
            where_to = effectPlayLocation(ctx.state, ctx.controller, where);
        }
        // Banish it (the executor takes it out of the deck and tells the
        // knowledge tracker), then play it under my control: a unit or gear
        // onto the board, a spell onto the chain. Either way the play takes
        // it back out of banishment.
        ctx.executor.banishObject(top);
        ctx.executor.playIgnoringCost(ctx.controller, top, where_to);
        RIFT_TRACE(ctx.events, "BLIND FURY: banished + played " + name + " ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 25;
        d.def_id = R"RB(ogn-025-298)RB";
        d.name = R"RB(Blind Fury)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-025/298)RB";
        d.collector_number = 25;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Each opponent reveals the top card of their Main Deck. Choose one and banish it, then play it, ignoring its cost. Then recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d74ba4998df9348408f1400090a0b0737c675fbd-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_25(CardRegistry& r) {
    r.registerCard(25, std::make_unique<BlindFury>());
}

} // namespace riftbound
