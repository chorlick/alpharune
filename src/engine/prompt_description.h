#pragma once
/// @file prompt_description.h
/// What a player may be shown of a decision's question (testing phase T1).
///
/// `Prompt` (core/prompt.h) is what the asking code says about a decision; it
/// holds object ids. `describePrompt` turns it, with the decision's options,
/// into what one observer may be shown: no object id, the asking card only if
/// the observer may see it (knowledge::identityVisible), each option's object
/// as openspiel::visibleObjectDescription has it, and words. Built only when
/// called: nothing of it is computed while a game is played and nobody asks.
///
/// The decider is shown everything here. Another observer is shown that the
/// decision is being made and what kind it is (who decides, the kind, the
/// asking card if they may see it, which ability, where on the chain) but not
/// the question's words or the options: those are the decider's.
///
/// Options are listed in action-id order (openspiel::actionIds: what the
/// decider sees, never object ids), each with its index in the legal list
/// (batch 5b N3: a consumer never orders by list index).

#include "core/card_db.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "core/prompt.h"

#include <cstdint>
#include <string>
#include <vector>

namespace riftbound {

/// What one option is, as a role the observer can read without the words.
enum class OptionRole : uint8_t {
    Action = 0,   // not a prompt answer: a play, a move, an activation, a pass, ...
    Yes,          // the "yes" of a yes/no (value 1 beside a declining option)
    No,           // the declining option of a yes/no, or "None" / "No more" / "Keep it"
    Value,        // a value: a mode, an X, a named option ("[1] less"), an order position
    Object,       // an object (a target, a card to discard, a unit to move, ...)
    NamedCard,    // a card named, not pointed at (Intent::chosen_card_def)
    ChainItem,    // a spell or ability on the chain (a counter's choice)
};
const char* toString(OptionRole role);

/// One option as the observer sees it.
struct OptionDescription {
    /// Its position in the decision's legal list: how a consumer maps the
    /// option back to an Intent. NOT part of what is shown (the list's order
    /// may follow object ids, batch 5b N3), so not compared by operator==.
    size_t index = 0;
    int action_id = -1;        // openspiel::actionIds of it
    OptionRole role = OptionRole::Action;
    IntentType type = IntentType::MakeChoice;
    int value = -1;            // Intent::chosen_value, or -1
    /// The card of each object it names, kInvalidId where the observer may
    /// not see it; and what the observer sees of each
    /// (openspiel::visibleObjectDescription: whose, zone, place, card if
    /// visible, Might, damage, statuses, ...).
    std::vector<CardDefId> cards;
    std::vector<std::vector<int64_t>> objects;
    CardDefId named_card = kInvalidId;   // Intent::chosen_card_def
    int chain_position = -1;   // ChainItem role: the item's place, 0 = bottom
    std::string label;         // Intent::choice_label (the asking code's words for it)
    std::string text;          // the option in words
    /// Equal when they show the same thing (every field but `index`).
    bool operator==(const OptionDescription& o) const {
        return action_id == o.action_id && role == o.role && type == o.type &&
               value == o.value && cards == o.cards && objects == o.objects &&
               named_card == o.named_card && chain_position == o.chain_position &&
               label == o.label && text == o.text;
    }
};

/// A decision's question as one observer sees it.
struct PromptDescription {
    PromptKind kind = PromptKind::Unspecified;
    /// Players relative to the observer: 1 = the observer, 2 = the other
    /// player, 0 = nobody (the game's own question).
    int decider = 0;
    int asker = 0;             // the source's controller
    /// The asking object: whether there is one, its card (kInvalidId when the
    /// observer may not see it) and its zone.
    bool has_source = false;
    CardDefId source_card = kInvalidId;
    ZoneType source_zone = ZoneType::MainDeck;
    enum class Ability : uint8_t { None, Spell, Activated, Triggered, Keyword, Permanent };
    Ability ability = Ability::None;
    int ability_index = -1;
    TriggerType trigger = TriggerType::None;
    bool at_finalization = false;
    /// The chain item being finalized or resolved: its place on the chain (0 =
    /// bottom) while it is on it, -1 while it resolves or when there is none.
    bool chain_item = false;
    int chain_position = -1;
    /// The decider's: the asking code's words and the options (empty for any
    /// other observer).
    std::string question;
    std::vector<OptionDescription> options;
    /// The whole question in words.
    std::string text;
    bool operator==(const PromptDescription&) const = default;
};

/// What `observer` may be shown of the decision `prompt` with options `legal`
/// in `state` (the state the decision is pending in).
PromptDescription describePrompt(const GameState& state, const Prompt& prompt,
                                 const std::vector<Intent>& legal, PlayerId observer,
                                 const CardDB& card_db);

/// The decision's question without its options, as one comparable key: two
/// decisions with equal keys ask the same question (PromptCensus checks that
/// they mean the same).
std::string promptKey(const PromptDescription& d);

/// What `observer` sees of one object, in words ("your Yasuo, Remorseful at
/// battlefield 1", "an opponent's card in hand").
std::string objectInWords(const GameState& state, GameObjectId object, PlayerId observer);

}  // namespace riftbound
