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

class DivineJudgment : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each player chooses 2 units, 2 gear, 2 runes, and 2 cards in their hands.
    //  Recycle the rest."
    // Nothing is targeted (CR 355.10.e): each player chooses as the spell
    // resolves, the caster first. One prompt per category that holds more
    // than two (each option is a pair to keep). Progress lives on the
    // resolving item: resume_point kAwait while a prompt is out, resume_data
    // = [next step, kept ids...]; step = 4 * player + category.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        const PlayerId players[2] = {ctx.controller, opponent(ctx.controller)};
        auto category = [&](int step) {
            const PlayerId p = players[step / 4];
            std::vector<GameObjectId> out;
            if (step % 4 == 3) {
                out = ctx.state.player(p).hand;
            } else {
                for (auto& [id, obj] : ctx.state.objects.onBoard()) {
                    if (obj.controller != p || !obj.location.has_value()) continue;
                    if (step % 4 == 0 ? obj.isUnit() : step % 4 == 1 ? obj.isGear() : obj.isRune())
                        out.push_back(id);
                }
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        if (ri.resume_point == kAwait) {
            const auto in_category = category(ri.resume_data[0]);
            auto picked = ctx.executor.takeChoice();
            std::vector<GameObjectId> keep;
            if (picked.has_value())
                for (auto id : picked->chosen_objects)
                    if (std::find(in_category.begin(), in_category.end(), id) != in_category.end() &&
                        std::find(keep.begin(), keep.end(), id) == keep.end() && keep.size() < 2)
                        keep.push_back(id);
            // No answer: the first two.
            for (size_t i = 0; keep.size() < 2 && i < in_category.size(); ++i)
                if (std::find(keep.begin(), keep.end(), in_category[i]) == keep.end())
                    keep.push_back(in_category[i]);
            for (auto id : keep) ri.resume_data.push_back(static_cast<int32_t>(id.raw()));
            ++ri.resume_data[0];
        } else if (ri.resume_point != kChoosing) {
            ri.resume_data.assign(1, 0);
        }
        ri.resume_point = kChoosing;
        static const char* const kWhat[4] = {"units", "gear", "runes", "cards in hand"};
        for (; ri.resume_data[0] < 8; ++ri.resume_data[0]) {
            const int step = ri.resume_data[0];
            const auto in_category = category(step);
            if (in_category.size() <= 2) {
                for (auto id : in_category) ri.resume_data.push_back(static_cast<int32_t>(id.raw()));
                continue;
            }
            const PlayerId p = players[step / 4];
            std::vector<Intent> options;
            for (size_t a = 0; a < in_category.size(); ++a)
                for (size_t b = a + 1; b < in_category.size(); ++b) {
                    Intent keep_two = objChoice(p, in_category[a]);
                    keep_two.chosen_objects.push_back(in_category[b]);
                    options.push_back(std::move(keep_two));
                }
            ctx.executor.requestChoice(p, std::move(options),
                std::string("Divine Judgment: choose 2 ") + kWhat[step % 4] + " to keep", PromptKind::ChooseObject);
            ri.resume_point = kAwait;
            return;
        }
        ri.resume_point = kDone;

        auto kept = [&ri](GameObjectId id) {
            return std::find(ri.resume_data.begin() + 1, ri.resume_data.end(),
                             static_cast<int32_t>(id.raw())) != ri.resume_data.end();
        };
        for (const PlayerId p : players) {
            std::vector<GameObjectId> rest;
            for (int c = 0; c < 4; ++c)
                for (auto id : category(4 * (p == players[0] ? 0 : 1) + c))
                    if (!kept(id)) rest.push_back(id);
            recycleAll(ctx, p, rest);
        }
        RIFT_TRACE(ctx.events, "DIVINE JUDGMENT: each player keeps 2/category, recycle rest");
    }

private:
    enum : int { kChoosing = 20, kAwait = 21, kDone = 22 };

    // Recycle `cards` (on `p`'s board or in their hand). recycleCards takes a
    // permanent off the board properly: gear comes off first (CR 719.5), a
    // token ceases to exist instead of entering a deck (CR 183.1), and the
    // departure is announced.
    static void recycleAll(CardContext& ctx, PlayerId p, const std::vector<GameObjectId>& cards) {
        if (!cards.empty()) ctx.executor.recycleCards(p, cards);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 244;
        d.def_id = R"RB(ogn-244-298)RB";
        d.name = R"RB(Divine Judgment)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-244/298)RB";
        d.collector_number = 244;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Each player chooses 2 units, 2 gear, 2 runes, and 2 cards in their hands. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/995778eea2e24fdc62ade38c2baa25b9f1e6ab79-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_244(CardRegistry& r) {
    r.registerCard(244, std::make_unique<DivineJudgment>());
}

} // namespace riftbound
