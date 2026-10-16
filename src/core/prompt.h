#pragma once
/// @file prompt.h
/// What a decision asks (testing phase T1; trainer plan section 0, item 8).
///
/// Every decision the engine puts to a player goes through one place
/// (`GameEngine::askAgent`), and the code that asks knows what it is asking.
/// It says so in a `Prompt`, which travels with the decision: on
/// `StepResult::prompt`, `GameEngine::pendingPrompt()`, `AgentInterface::decide`
/// and `RiftboundState::decisionPrompt()`. Without it a `MakeChoice` is a bare
/// integer to an observer: "yes" to "pay [2] to draw" and "yes" to "kill your
/// own unit" look the same.
///
/// A `Prompt` is plain fields plus the words the asking code already builds for
/// its log line (moved, not built for this), so it costs nothing to carry. It
/// holds object ids and is NOT for a model: what a player may be shown of it is
/// `describePrompt` (engine/prompt_description.h), built only on request, which
/// names the asking card only if that player may see it and never an id.

#include "core/types.h"
#include "effects/effect_types.h"

#include <cstdint>
#include <source_location>
#include <string>

namespace riftbound {

/// The kind of question. Stable numbering: a consumer may use the value as a
/// category (new kinds are added at the end).
enum class PromptKind : uint8_t {
    Unspecified = 0,   // never offered by the engine (PromptCensus checks it)

    // ── The turn ──
    BattlefieldSetup,  // which battlefield of your list to bring (CR 480.5)
    Mulligan,          // which cards to set aside (CR 118)
    MainPhase,         // the turn player's open decision: play, move, activate, end turn
    ChainPriority,     // priority while a chain or a Closed State exists: react or pass
    ShowdownFocus,     // Focus in a showdown: act or pass
    NextStaged,        // where the next showdown / combat begins (CR 323.11-12)

    // ── Combat ──
    CombatDamage,      // how this side's combat damage is assigned (whole assignment)
    CombatDamageNext,  // which unit takes combat damage next

    // ── Paying ──
    RuneForEnergy,     // which rune is exhausted for this point of Energy
    RuneForPower,      // which rune (or floating Power) pays this point of Power
    AddForPayment,     // activate an [Add] ability to pay the cost being paid (CR 430.3)
    CostSplit,         // which half of a discount or cost: "[1] or [A] less", Hide's Energy or Power
    Accelerate,        // pay [Accelerate]'s extra cost? (yes / no)
    Repeat,            // pay [Repeat] once more? (yes / no)
    AdditionalCost,    // pay an optional additional cost? (yes / no), or which card pays it
    DiscardCost,       // which card to discard as a cost

    // ── Rules procedures ──
    TriggerOrder,      // which of simultaneous abilities goes on the chain next (CR 383.3.d)
    ReplacementOrder,  // which replacement applies first (CR 372)
    NewChoices,        // "make new choices" for a chain item (CR 750-755)
    MoveJoin,          // which other unit joins this Standard Move (CR 144.3), or none

    // ── A card's own question ──
    May,               // a "may" / "you may pay ... to ...": yes or no
    Mode,              // which mode / which of named options
    XAmount,           // the value of X
    Target,            // a target (one)
    TargetFirst,       // the first of two targets
    TargetSecond,      // the second of two targets
    ChooseObject,      // an object the effect has you choose (not a target), one per prompt
    SplitTargets,      // who is dealt part of a split damage (CR 355.14)
    SplitAmount,       // how much of a split damage this unit is dealt
    ChainItem,         // a spell or ability on the chain (a counter's target)
    Location,          // where: a base or a battlefield
    Discard,           // which card to discard (an effect's discard)
    LookTop,           // a card looked at: keep it or recycle / take it or leave it
    DeckOrder,         // which looked-at card goes on top
    Value,             // a number or a named option the effect asks for
    NameACard,         // name a card (Intent::chosen_card_def)

    Count
};

/// One word for the kind ("may", "mode", "target", ...). Never null.
const char* toString(PromptKind kind);

/// What one decision asks. See the file comment.
struct Prompt {
    PromptKind kind = PromptKind::Unspecified;
    /// Who decides (StepResult::perspective).
    PlayerId decider = PlayerId::None;
    /// The object whose rule asks: the card being played, the source of the
    /// ability being finalized or resolving, the unit whose death is being
    /// replaced. kInvalidId for the game's own questions (priority, the
    /// mulligan, which rune pays).
    GameObjectId source = kInvalidId;
    /// The chain item being finalized or resolved when the question is put,
    /// if any.
    ChainItemId chain_item = kInvalidId;
    /// For an ability: which activated ability (Intent::ability_index), or -1;
    /// the trigger that fired for a triggered ability (TriggerType::None
    /// otherwise); whether it is a keyword's ability ([Vision], ...).
    int ability_index = -1;
    TriggerType trigger = TriggerType::None;
    bool keyword_ability = false;
    bool is_spell = false;
    /// A triggered ability with no trigger of its own to name: a reflexive
    /// one (CR 388) or a delayed one.
    bool is_triggered = false;
    /// Asked as the chain item is finalized (CR 355, 383.3) rather than as it
    /// resolves.
    bool at_finalization = false;
    /// The asking code's own words (a card's prompt label, the engine's
    /// question). May name the source card; it never names a card the decider
    /// may not see (HiddenOrderLeak compares it).
    std::string question;
    /// The code that asked. Not part of what a player is shown: tests use it
    /// as an independent identity of the question's meaning (PromptCensus).
    std::source_location where{};
};

}  // namespace riftbound
