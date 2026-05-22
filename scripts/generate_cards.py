#!/usr/bin/env python3
"""Generate C++ card implementations from registry.json.

Reads cards/registry.json, parses ability_text for each card, and generates:
  - src/cards/generated/unit_cards.cpp
  - src/cards/generated/spell_cards.cpp
  - src/cards/generated/gear_cards.cpp
  - src/cards/generated/legend_cards.cpp
  - src/cards/generated/battlefield_cards.cpp
  - src/cards/generated/rune_cards.cpp
  - src/cards/generated/registry_init.cpp

Each card becomes a C++ class that overrides the appropriate virtual methods.
Simple cards (~600) get fully generated implementations. Complex cards get
stub implementations with a TODO comment.
"""

import json
import os
import re
import sys
from dataclasses import dataclass, field
from typing import Optional


# ─── Data structures ─────────────────────────────────────────────────────────

@dataclass
class TargetSpec:
    unit: bool = False
    gear: bool = False
    spell: bool = False
    rune: bool = False
    any_card: bool = False
    friendly: bool = False
    enemy: bool = False
    at_battlefield: bool = False
    at_base: bool = False
    here: bool = False
    self_ref: bool = False
    it: bool = False
    the_rest: bool = False
    max_might: int = 0
    optional: bool = False
    count: int = 1
    all: bool = False

    def to_cpp(self) -> str:
        """Generate C++ TargetSpec initializer."""
        fields = []
        if self.unit: fields.append(".unit = true")
        if self.gear: fields.append(".gear = true")
        if self.spell: fields.append(".spell = true")
        if self.rune: fields.append(".rune = true")
        if self.any_card: fields.append(".any_card = true")
        if self.friendly: fields.append(".friendly = true")
        if self.enemy: fields.append(".enemy = true")
        if self.at_battlefield: fields.append(".at_battlefield = true")
        if self.at_base: fields.append(".at_base = true")
        if self.here: fields.append(".here = true")
        if self.self_ref: fields.append(".self = true")
        if self.it: fields.append(".it = true")
        if self.the_rest: fields.append(".the_rest = true")
        if self.max_might: fields.append(f".max_might = {self.max_might}")
        if self.optional: fields.append(".optional = true")
        if self.count != 1: fields.append(f".count = {self.count}")
        if self.all: fields.append(".all = true")
        return "{" + ", ".join(fields) + "}"


@dataclass
class EffectStep:
    effect_type: str  # C++ EffectType enum name
    value: int = 0
    target: Optional[TargetSpec] = None
    keyword: str = ""          # C++ Keyword enum name
    keyword_value: int = 0
    condition: str = "None"    # C++ EffectCondition enum name
    optional: bool = False
    reveal_condition: str = ""  # CardType name


@dataclass
class ActivationCost:
    exhaust: bool = False
    energy: int = 0

    def to_cpp(self) -> str:
        fields = []
        if self.exhaust: fields.append(".exhaust = true")
        if self.energy: fields.append(f".energy = {self.energy}")
        return "{" + ", ".join(fields) + "}"


@dataclass
class ParsedCard:
    card_id: int
    name: str
    card_type: str     # unit, spell, gear, legend, battlefield, rune
    ability_text: str
    trigger_type: str = "None"
    steps: list = field(default_factory=list)
    is_action: bool = False
    is_reaction: bool = False
    requires_legion: bool = False
    requires_level: bool = False
    level_threshold: int = 0
    has_activated: bool = False
    activation_cost: Optional[ActivationCost] = None
    is_complex: bool = False  # needs manual implementation
    class_name: str = ""


# ─── Text helpers ────────────────────────────────────────────────────────────

def strip_parenthetical(text: str) -> str:
    result = []
    depth = 0
    for c in text:
        if c == '(':
            depth += 1
        elif c == ')':
            if depth > 0:
                depth -= 1
        elif depth == 0:
            result.append(c)
    return ''.join(result)


def find_int(s: str, default: int = 0) -> int:
    m = re.search(r'\d+', s)
    return int(m.group()) if m else default


def make_class_name(name: str) -> str:
    """Convert card name to C++ class name."""
    # Remove special characters, convert to PascalCase
    cleaned = re.sub(r'[^a-zA-Z0-9\s]', '', name)
    parts = cleaned.split()
    return ''.join(w.capitalize() for w in parts)


# ─── Trigger extraction ─────────────────────────────────────────────────────

TRIGGER_PATTERNS = [
    ("When you play me,", "WhenYouPlayMe"),
    ("When you play me ", "WhenYouPlayMe"),
    ("When you play this,", "WhenYouPlayThis"),
    ("When I attack or defend,", "WhenIAttackOrDefend"),
    ("When I attack or defend ", "WhenIAttackOrDefend"),
    ("When I attack,", "WhenIAttack"),
    ("When I attack ", "WhenIAttack"),
    ("When I defend,", "WhenIDefend"),
    ("When I die,", "WhenIDie"),
    ("When I move to a battlefield,", "WhenIMoveToFB"),
    ("When I move,", "WhenIMove"),
    ("When I conquer or hold,", "WhenIConquerOrHold"),
    ("When I conquer,", "WhenIConquer"),
    ("When I hold,", "WhenIHold"),
    ("When you conquer here,", "WhenYouConquerHere"),
    ("When you hold here,", "WhenYouHoldHere"),
    ("When you score here,", "WhenYouScoreHere"),
    ("When you defend here,", "WhenYouDefendHere"),
    ("When you play a spell,", "WhenYouPlayASpell"),
    ("When you play a unit,", "WhenYouPlayAUnit"),
    ("When a friendly unit dies,", "WhenAFriendlyUnitDies"),
    ("When you discard,", "WhenYouDiscard"),
    ("At the end of your turn,", "AtEndOfTurn"),
    ("At the start of your Beginning Phase,", "AtStartOfBeginning"),
    ("At start of your Beginning Phase,", "AtStartOfBeginning"),
    ("At the start of each player's Beginning Phase,", "AtStartOfBeginning"),
    ("At the start of each player\u2019s Beginning Phase,", "AtStartOfBeginning"),
]


# ─── Target parsing ─────────────────────────────────────────────────────────

def parse_target(text: str) -> TargetSpec:
    spec = TargetSpec()
    lower = text.strip().lower()
    if not lower:
        return spec

    if lower in ("me", "i", "myself"):
        spec.self_ref = True
        return spec
    if lower in ("it", "them"):
        spec.it = True
        return spec
    if lower in ("the rest", "the others"):
        spec.the_rest = True
        return spec

    if "all " in lower:
        spec.all = True
    if "friendly" in lower or "your " in lower:
        spec.friendly = True
    if "enemy" in lower:
        spec.enemy = True
    if "unit" in lower:
        spec.unit = True
    if "gear" in lower:
        spec.gear = True
    if "spell" in lower:
        spec.spell = True
    if "rune" in lower:
        spec.rune = True
    if "card" in lower:
        spec.any_card = True
    if "at a battlefield" in lower:
        spec.at_battlefield = True
    if " here" in lower:
        spec.here = True
        spec.at_battlefield = True
    if "up to" in lower:
        spec.optional = True

    # Stat filter
    or_less = lower.find("or less")
    if or_less != -1:
        spec.max_might = find_int(lower[:or_less], 0)

    # Default to unit if nothing specific
    if not any([spec.unit, spec.gear, spec.spell, spec.rune,
                spec.any_card, spec.self_ref, spec.it, spec.the_rest]):
        spec.unit = True

    return spec


# ─── Effect clause parsing ───────────────────────────────────────────────────

def parse_effect_clause(clause: str) -> Optional[EffectStep]:
    text = clause.strip()
    if not text:
        return None

    # Strip leading em-dash
    if text.startswith('\u2014'):
        text = text[1:].strip()
    if text.startswith('-'):
        text = text[1:].strip()

    lower = text.lower()
    step = EffectStep(effect_type="None")

    # Check "you may"
    if lower.startswith("you may "):
        step.optional = True
        text = text[8:].strip()
        lower = text.lower()

    # Deal N to TARGET
    if lower.startswith("deal "):
        step.effect_type = "DealDamage"
        step.value = find_int(text[5:], 1)
        to_pos = lower.find(" to ")
        if to_pos != -1:
            step.target = parse_target(text[to_pos + 4:])
        return step

    # Draw N
    if lower.startswith("draw "):
        step.effect_type = "Draw"
        rest = text[5:]
        if rest.strip().lower().startswith("it"):
            step.value = 1
            step.target = TargetSpec(it=True)
        else:
            step.value = find_int(rest, 1)
        return step

    # Put N into your hand
    if lower.startswith("put ") and "into your hand" in lower:
        step.effect_type = "Draw"
        step.value = find_int(text[4:], 1)
        return step

    # Kill TARGET
    if lower.startswith("kill "):
        step.effect_type = "Kill"
        step.target = parse_target(text[5:])
        return step

    # Give TARGET ...
    if lower.startswith("give "):
        rest = text[5:]
        lower_rest = rest.lower()

        # +N [M]
        plus_pos = rest.find('+')
        minus_pos = rest.find('-')
        m_bracket = lower_rest.find("[m]")

        if m_bracket != -1 and (plus_pos != -1 or minus_pos != -1):
            step.effect_type = "GiveMight"
            sign_pos = plus_pos if plus_pos != -1 else minus_pos
            step.value = find_int(rest[sign_pos:], 1)
            if minus_pos != -1 and (plus_pos == -1 or minus_pos < plus_pos):
                step.value = -step.value
            step.target = parse_target(rest[:sign_pos])
            return step

        # [Keyword N]
        bracket_pos = rest.find('[')
        if bracket_pos != -1:
            bracket_end = rest.find(']', bracket_pos)
            if bracket_end != -1:
                kw_str = rest[bracket_pos + 1:bracket_end]
                step.effect_type = "GiveKeyword"
                step.target = parse_target(rest[:bracket_pos])
                step.keyword_value = find_int(kw_str, 0)

                kw_lower = kw_str.lower()
                if "assault" in kw_lower:
                    step.keyword = "Assault"
                elif "shield" in kw_lower:
                    step.keyword = "Shield"
                elif "ganking" in kw_lower:
                    step.keyword = "Ganking"
                elif "tank" in kw_lower:
                    step.keyword = "Tank"
                elif "temporary" in kw_lower:
                    step.keyword = "Temporary"
                elif "deflect" in kw_lower:
                    step.keyword = "Deflect"
                return step

        return None  # unrecognized give pattern

    # Buff TARGET
    if lower.startswith("buff "):
        step.effect_type = "Buff"
        step.target = parse_target(text[5:])
        step.value = 1
        return step

    # Ready TARGET
    if lower.startswith("ready "):
        step.effect_type = "Ready"
        step.target = parse_target(text[6:])
        return step

    # Return TARGET to hand
    if lower.startswith("return "):
        step.effect_type = "Bounce"
        to_pos = lower.find(" to ")
        if to_pos != -1:
            step.target = parse_target(text[7:to_pos])
        else:
            step.target = parse_target(text[7:])
        return step

    # Move TARGET
    if lower.startswith("move "):
        step.effect_type = "Move"
        step.target = parse_target(text[5:])
        return step

    # Stun TARGET
    if lower.startswith("stun "):
        step.effect_type = "Stun"
        step.target = parse_target(text[5:])
        return step

    # Discard N
    if lower.startswith("discard "):
        step.effect_type = "Discard"
        step.value = find_int(text[8:], 1)
        return step

    # Recycle TARGET
    if lower.startswith("recycle "):
        step.effect_type = "Recycle"
        step.target = parse_target(text[8:])
        return step

    # Counter a spell
    if lower.startswith("counter "):
        step.effect_type = "Counter"
        step.target = TargetSpec(spell=True)
        return step

    # Banish TARGET
    if lower.startswith("banish "):
        step.effect_type = "Banish"
        step.target = parse_target(text[7:])
        return step

    # Reveal cards...until
    if lower.startswith("reveal cards") and "until" in lower:
        step.effect_type = "RevealUntil"
        if "unit" in lower:
            step.reveal_condition = "Unit"
        elif "spell" in lower:
            step.reveal_condition = "Spell"
        elif "gear" in lower:
            step.reveal_condition = "Gear"
        else:
            step.reveal_condition = "Unit"
        return step

    # Look at top N
    if lower.startswith("look at the top "):
        step.effect_type = "LookAtTop"
        step.value = find_int(text[16:], 1)
        return step

    # Reveal top N
    if lower.startswith("reveal the top "):
        step.effect_type = "RevealFromTop"
        step.value = find_int(text[15:], 1)
        return step

    # Play it, ignoring cost
    if lower.startswith("play it"):
        step.effect_type = "PlayIgnoringCost"
        step.target = TargetSpec(it=True)
        return step

    # Channel N runes [exhausted]
    if lower.startswith("channel "):
        step.effect_type = "Channel"
        step.value = find_int(text[8:], 1)
        step.optional = "exhausted" in lower  # reuse optional flag for exhausted
        return step

    # Heal TARGET
    if lower.startswith("heal "):
        step.effect_type = "Heal"
        step.target = parse_target(text[5:])
        return step

    # Exhaust TARGET
    if lower.startswith("exhaust "):
        step.effect_type = "Exhaust"
        step.target = parse_target(text[8:])
        return step

    # "I enter ready"
    if "enter ready" in lower:
        step.effect_type = "Ready"
        step.target = TargetSpec(self_ref=True)
        return step

    # Gain N XP
    if lower.startswith("gain ") and "xp" in lower:
        step.effect_type = "GainXP"
        step.value = find_int(text[5:], 1)
        return step

    # Choose TARGET
    if lower.startswith("choose "):
        step.effect_type = "ChooseTarget"
        step.target = parse_target(text[7:])
        return step

    return None  # unrecognized


# ─── Main card parser ────────────────────────────────────────────────────────

# Patterns that indicate a card is too complex for auto-generation
COMPLEX_PATTERNS = [
    "would die",          # replacement effects
    "instead",            # replacement effects
    "cost is reduced",    # cost modification
    "costs",              # cost modification
    "cost by",            # cost modification
    "I cost",             # cost modification
    "other friendly",     # passive aura
    "other units",        # passive aura
    "each player",        # multi-player effect
    "each opponent",      # multi-player effect
    "copy",               # copy effects
    "become",             # transformation
    "create a",           # token creation
    "token",              # token creation
    "replace",            # battlefield replace
    "attach",             # attachment
    "search",             # search effects
    "rearrange",          # reorder effects
    "take a turn",        # extra turns
    "can't",              # restriction effects
    "can\u2019t",         # restriction effects
    "play me to",         # play-to-location
    "play me at",         # play-to-location
    "play me on",         # play-to-location
    "this enters",        # ETB replacement
    "play from",          # play-from alternate zone
    "while i'm",          # conditional passive
    "while i\u2019m",     # conditional passive
    "while you",          # conditional passive
    "the first time",     # non-standard trigger
    "the next time",      # delayed trigger
    "the next unit",      # delayed ability
    "the next spell",     # delayed ability
    "when you stun",      # non-standard trigger
    "when you kill",      # non-standard trigger
    "when you play your second", # non-standard trigger
    "when you discard me", # non-standard trigger
    "my might is",        # passive stat modification
    "damage equal to",    # computed damage
    "bonus damage",       # damage modification
    "increased by",       # passive stat modification
    "reduced by",         # passive stat modification
    "use this ability only", # usage restriction
    "alone",              # conditional (attacks/defends alone)
    "if you control",     # board-state conditional
    "if an opponent",     # opponent-state conditional
    "if you've discarded", # past-action conditional
    "if you\u2019ve",     # past-action conditional
    "if i'm",             # self-state conditional
    "if i\u2019m",        # self-state conditional
    "recycle 1 from",     # complex activation cost
    "recycle 2 from",     # complex activation cost
    "recycle 3 from",     # complex activation cost
    "play a spell from",  # play from trash
    "pay [c]",            # recycle-self cost
    "unless",             # conditional branch
    "as an additional cost", # alt cost (not Accelerate parens)
    "spend a buff",       # buff-spending mechanic
    "spend my buff",      # buff-spending mechanic
    "spend its buff",     # buff-spending mechanic
    "choose one",         # modal choice
    "do one of",          # modal choice
    "for each",           # iteration
    "when you play a gear", # non-standard trigger
    "when you play another", # non-standard trigger
    "when you ready",     # non-standard trigger
    "when you buff",      # non-standard trigger
    "when this is played", # non-standard trigger
    "when this leaves",   # non-standard trigger
    "when any unit",      # non-standard trigger
    "when a friendly unit moves", # non-standard trigger
    "when a unit",        # non-standard trigger
    "when you play a card from", # non-standard trigger
    "when i'm played",    # non-standard trigger
    "when i\u2019m played", # non-standard trigger
    "if it is stunned",   # state check
    "if i have",          # state check
    "if i\u2019ve",       # state check
    "the third time",     # count-based trigger
    "the second time",    # count-based trigger
    "reveal their hand",  # hand reveal
    "reveal your hand",   # hand reveal
    "as you look at",     # non-standard timing
    "as you reveal",      # non-standard timing
    "doesn't take damage", # damage prevention
    "doesn\u2019t take",  # damage prevention
    "i enter ready",      # self-ready on play (already handled by kw)
    "enters ready",       # passive aura
    "you score",          # direct scoring
    "leaves the board",   # zone-change trigger
    "takes damage",       # damage trigger
    "when you discard one", # non-standard discard trigger
    "if there is",        # board-state conditional
    "when you choose",    # non-standard trigger
    "when you win",       # non-standard trigger
    "when i win",         # non-standard trigger
    "when you defend at", # non-standard trigger
    "when you draw",      # non-standard trigger
    "when a showdown",    # non-standard trigger
    "when you play a unit during", # conditional play trigger
    "spend 1 xp",        # XP spending as activation cost
    "spend 2 xp",        # XP spending as activation cost
    "spend 3 xp",        # XP spending as activation cost
    "name a tag",         # naming mechanic
    "any amount of",      # variable amount
    "excess damage",      # damage tracking
    "at the start of your next", # delayed trigger
    "[buff]",             # [Buff] as verb in text
    "its controller",     # controller reference
    "their controller",   # controller reference
    "do the following",   # multi-option
    "based on",           # conditional branching
    "pay any amount",     # variable cost
    "equipped",           # equipment mechanic
    "your equipment",     # equipment reference
    "when you use",       # non-standard trigger
    "when you or",        # multi-player trigger
    "4+ units",           # count condition
    "4 or more",          # count condition
    "if you assigned",    # combat tracking
    "if you spent",       # cost tracking
    "when you play me or", # dual trigger
    "or when i",          # dual trigger
    "when i move from",   # directional move trigger (not standard WhenIMove)
    "when i attack, you may pay", # conditional combat effect
    "if you have moved",  # state tracking
    "[action][>]",        # leveled action ability
    "from your trash",    # play from trash
    "[quick-draw]",       # equipment system
    "[equip]",            # equipment system
    "[>]",                # leveled ability marker
    "they discard",       # opponent action
    "they reveal",        # opponent action
    "a player",           # multi-player targeting
    "you must",           # forced action
    "[mighty]",           # stat-checking condition
    "your legend",        # legend-specific targeting
    "when you move",      # non-standard trigger
    "when an enemy",      # non-standard trigger
    "when you resolve",   # non-standard trigger
    "when a friendly unit is played", # delayed trigger
    "[add]",              # resource generation
    "put a card from",    # zone manipulation
    "draw it",            # conditional draw
    "draw 1 or",          # OR choice
    "or draw",            # OR choice
    "ready or",           # OR choice
    "or buff",            # OR choice
    "or a gear",          # multi-type targeting
    "kill me to",         # self-sacrifice
    "kill this:",         # self-kill activation
    "kill a friendly",    # sacrifice
    "when i defend,",     # WhenIDefend effects need checking
    "reveal the top",     # reveal mechanics
    "when you play a [",  # conditional-type play trigger
    "when you play a card with", # conditional play trigger
    "or more,",           # threshold condition
    "[c],",               # recycle-self in activation
    "you may move",       # optional movement
    "you may pay [",      # conditional payment
    "buffed",             # buff-state conditional
    "[ambush]",           # ambush mechanic
    "its owner",          # owner reference for other player's card
    "[hidden]",           # Hidden keyword as mechanic
    "hidden ",            # Hidden without brackets
    "[temporary]",        # Temporary keyword in effect
    "your next spell",    # delayed spell modification
    "move here from",     # location-based movement rule
    "may return",         # optional return
    "give all",           # AoE give
    "may kill a unit you control here", # sacrifice-to-draw
    "when an opponent",   # opponent-action trigger
    "when you recycle",   # non-standard trigger
    "don't deal",         # damage prevention
    "don\u2019t deal",    # damage prevention
]


def parse_card(card: dict) -> ParsedCard:
    """Parse a card's ability_text into a ParsedCard with structured effects."""
    pc = ParsedCard(
        card_id=card['card_id'],
        name=card['name'],
        card_type=card['card_type'],
        ability_text=card.get('ability_text', '') or '',
    )
    pc.class_name = make_class_name(card['name'])

    text = pc.ability_text
    if not text.strip():
        return pc

    text = strip_parenthetical(text)

    # Check for complexity markers (AFTER stripping parenthetical reminder text)
    lower_text = text.lower()
    for pattern in COMPLEX_PATTERNS:
        if pattern in lower_text:
            pc.is_complex = True
            break

    # Extract timing keywords
    if "[Action]" in text:
        pc.is_action = True
        text = text.replace("[Action]", "")
    if "[Reaction]" in text:
        pc.is_reaction = True
        pc.is_action = True
        text = text.replace("[Reaction]", "")

    # Handle [Stun] as effect verb
    text = text.replace("[Stun]", "Stun")

    # Handle [Hunt N]
    hunt_match = re.search(r'\[Hunt\s*(\d*)\]', text)
    if hunt_match:
        xp_amount = int(hunt_match.group(1)) if hunt_match.group(1) else 1
        if pc.trigger_type == "None":
            pc.trigger_type = "WhenIConquerOrHold"
        pc.steps.append(EffectStep(
            effect_type="GainXP",
            value=xp_amount,
        ))
        text = text[:hunt_match.start()] + text[hunt_match.end():]

    # Handle [Level N][>]
    level_match = re.search(r'\[Level\s*(\d+)\]', text)
    if level_match:
        pc.requires_level = True
        pc.level_threshold = int(level_match.group(1))
        end = level_match.end()
        # Skip [>] if present
        gt_match = re.match(r'\s*\[>\]', text[end:])
        if gt_match:
            end += gt_match.end()
        # Find dash after level
        dash_match = re.match(r'\s*[\u2014\-]+\s*', text[end:])
        if dash_match:
            end += dash_match.end()
        text = text[:level_match.start()] + text[end:]

    # Handle [Legion]
    legion_match = re.search(r'\[Legion\]', text)
    if legion_match:
        pc.requires_legion = True
        end = legion_match.end()
        dash_match = re.match(r'\s*[\u2014\-]+\s*', text[end:])
        if dash_match:
            end += dash_match.end()
        text = text[:legion_match.start()] + text[end:]

    # Handle [Deathknell]
    dk_match = re.search(r'\[Deathknell\]', text)
    if dk_match:
        pc.trigger_type = "WhenIDie"
        end = dk_match.end()
        dash_match = re.match(r'\s*[\u2014\-]+\s*', text[end:])
        if dash_match:
            end += dash_match.end()
        text = text[end:]
        text = text.replace("[Deathknell]", "")

    # Strip standalone keyword declarations (not inside effect clauses)
    kw_names = [
        "Accelerate", "Ambush", "Assault", "Backline", "Deathknell", "Deflect",
        "Equip", "Ganking", "Hidden", "Hunt", "Legion", "Level", "Predict",
        "QuickDraw", "Repeat", "Shield", "Tank", "Temporary", "Unique",
        "Vision", "Weaponmaster",
    ]
    for kw in kw_names:
        pattern = f'[{kw}'
        pos = text.find(pattern)
        while pos != -1:
            # Only remove if standalone (not after give/have/gets/gain)
            before = text[max(0, pos - 20):pos].lower()
            is_after_verb = any(v in before for v in ['give', 'have', 'gets', 'gain'])
            close = text.find(']', pos)
            if close != -1 and not is_after_verb:
                text = text[:pos] + text[close + 1:]
            else:
                pos = close + 1 if close != -1 else -1
            pos = text.find(pattern, pos if pos != -1 else 0)

    # Extract activation cost
    text = text.strip()
    if text.startswith("[E]:"):
        pc.trigger_type = "Activated"
        pc.has_activated = True
        pc.activation_cost = ActivationCost(exhaust=True)
        text = text[4:].strip()
    elif len(text) > 3 and text[0] == '[' and text[1].isdigit():
        close = text.find(']')
        if close != -1:
            e_pos = text.find("[E]:", close)
            if e_pos != -1 and e_pos < close + 5:
                pc.trigger_type = "Activated"
                pc.has_activated = True
                pc.activation_cost = ActivationCost(
                    exhaust=True,
                    energy=int(text[1]),
                )
                text = text[e_pos + 4:].strip()

    # Extract trigger prefix (if not already set)
    if pc.trigger_type == "None":
        trimmed = text.strip()
        for prefix, trigger in TRIGGER_PATTERNS:
            if trimmed.lower().startswith(prefix.lower()):
                pc.trigger_type = trigger
                text = trimmed[len(prefix):].strip()
                break

    # Clean up dashes and whitespace
    text = text.strip()
    text = text.lstrip('\u2014-').strip()

    if not text:
        return pc

    # Split into sentences
    sentences = []
    current = []
    for i, c in enumerate(text):
        if c == '.' and (i + 1 >= len(text) or text[i + 1] in (' ', '\n')):
            s = ''.join(current).strip()
            if s:
                sentences.append(s)
            current = []
        elif c == '\n':
            s = ''.join(current).strip()
            if s:
                sentences.append(s)
            current = []
        else:
            current.append(c)
    s = ''.join(current).strip()
    if s:
        sentences.append(s)

    # Split compound sentences
    expanded = []
    verbs = ['draw', 'deal', 'recycle', 'kill', 'move', 'give', 'buff',
             'ready', 'stun', 'banish', 'heal', 'exhaust', 'play', 'discard',
             'return', 'put']
    for sent in sentences:
        then_pos = sent.find(", then ")
        and_pos = sent.find(" and ")
        if then_pos != -1:
            expanded.append(sent[:then_pos].strip())
            expanded.append(sent[then_pos + 7:].strip())
        elif and_pos != -1:
            after = sent[and_pos + 5:].strip().lower()
            if any(after.startswith(v) for v in verbs):
                expanded.append(sent[:and_pos].strip())
                expanded.append(sent[and_pos + 5:].strip())
            else:
                expanded.append(sent)
        else:
            expanded.append(sent)

    # Parse each clause
    pending_condition = "None"
    for sent in expanded:
        sent = sent.strip()
        if not sent:
            continue

        lower_sent = sent.lower()

        # Check conditional prefixes
        if lower_sent.startswith("if this kills it") or lower_sent.startswith("if this kill it"):
            pending_condition = "IfKills"
            sep = sent.find(':')
            if sep == -1:
                sep = sent.find(',')
            if sep != -1:
                sent = sent[sep + 1:].strip()
            else:
                continue
        elif lower_sent.startswith("if you do"):
            pending_condition = "IfYouDo"
            sep = sent.find(',')
            if sep != -1:
                sent = sent[sep + 1:].strip()
            else:
                continue
        elif lower_sent.startswith("if you can't") or lower_sent.startswith("if you can\u2019t") or \
             lower_sent.startswith("if you couldn't") or lower_sent.startswith("if you couldn\u2019t"):
            pending_condition = "IfCantDo"
            sep = sent.find(',')
            if sep != -1:
                sent = sent[sep + 1:].strip()
            else:
                continue

        step = parse_effect_clause(sent)
        if step and step.effect_type != "None":
            if pending_condition != "None":
                step.condition = pending_condition
                pending_condition = "None"
            pc.steps.append(step)

    return pc


# ─── C++ code generation ────────────────────────────────────────────────────

def gen_target_requirements(steps: list) -> Optional[str]:
    """Generate TargetRequirements from steps that need targeting."""
    for step in steps:
        if step.effect_type == "ChooseTarget" and step.target:
            t = step.target
            return gen_target_req_from_spec(t)
        if step.target and not step.target.self_ref and not step.target.it and \
           not step.target.the_rest and not step.target.all:
            t = step.target
            if t.unit or t.gear or t.enemy or t.friendly or t.at_battlefield:
                return gen_target_req_from_spec(t)
    return None


def gen_target_req_from_spec(t: TargetSpec) -> str:
    fields = [".count = 1"]
    if t.unit:
        fields.append(".must_be_unit = true")
    if t.gear:
        fields.append(".must_be_gear = true")
    if t.enemy:
        fields.append(".must_be_enemy = true")
    if t.friendly:
        fields.append(".must_be_friendly = true")
    if t.at_battlefield:
        fields.append(".must_be_at_battlefield = true")
    if t.max_might:
        fields.append(f".max_might = {t.max_might}")
    if t.optional:
        fields.append(".optional = true")
    return "TargetRequirements{" + ", ".join(fields) + "}"


def gen_effect_call(step: EffectStep, indent: str = "        ") -> list:
    """Generate C++ code lines for an effect step."""
    lines = []
    t = step.target

    def target_expr() -> str:
        if not t:
            return "ctx.source"
        if t.self_ref:
            return "ctx.source"
        if t.it:
            return "targets[0]"
        if t.the_rest:
            return "/* the_rest — TODO */"
        if t.all:
            return "/* all targets — resolved at runtime */"
        return "targets[0]"

    cond_open = ""
    cond_close = ""
    if step.condition != "None":
        # For conditionals, we'd need runtime tracking; skip for now in simple cards
        lines.append(f"{indent}// Condition: {step.condition}")

    # For non-AoE effects that ended up with an `all` target, treat as single-target
    def gen_single_target(method: str, *args):
        """Generate code for a single-target effect, handling all/self/it/default."""
        extra = ", ".join(str(a) for a in args)
        extra_str = f", {extra}" if extra else ""
        if t and t.all:
            # AoE variant: loop over matching objects
            filt = []
            if t.unit: filt.append(f"{indent}    if (!obj.isUnit()) continue;")
            if t.gear: filt.append(f"{indent}    if (!obj.isGear()) continue;")
            if t.enemy: filt.append(f"{indent}    if (obj.controller == ctx.controller) continue;")
            if t.friendly: filt.append(f"{indent}    if (obj.controller != ctx.controller) continue;")
            if t.at_battlefield: filt.append(f"{indent}    if (!obj.isAtBattlefield()) continue;")
            result = [f"{indent}for (auto& [id, obj] : ctx.state.objects) {{",
                      f"{indent}    if (!obj.location.has_value()) continue;"]
            result.extend(filt)
            result.append(f"{indent}    ctx.executor.{method}(id{extra_str});")
            result.append(f"{indent}}}")
            return result
        elif t and t.self_ref:
            return [f"{indent}ctx.executor.{method}(ctx.source{extra_str});"]
        else:
            return [f"{indent}if (!targets.empty())",
                    f"{indent}    ctx.executor.{method}({target_expr()}{extra_str});"]

    if step.effect_type == "DealDamage":
        if t and t.all:
            lines.append(f"{indent}// AoE: deal {step.value} to all matching")
            lines.append(f"{indent}{{")
            lines.append(f"{indent}    std::vector<GameObjectId> to_damage;")
            lines.append(f"{indent}    for (auto& [id, obj] : ctx.state.objects) {{")
            lines.append(f"{indent}        if (!obj.location.has_value()) continue;")
            if t.unit:
                lines.append(f"{indent}        if (!obj.isUnit()) continue;")
            if t.gear:
                lines.append(f"{indent}        if (!obj.isGear()) continue;")
            if t.enemy:
                lines.append(f"{indent}        if (obj.controller == ctx.controller) continue;")
            if t.friendly:
                lines.append(f"{indent}        if (obj.controller != ctx.controller) continue;")
            if t.at_battlefield:
                lines.append(f"{indent}        if (!obj.isAtBattlefield()) continue;")
            lines.append(f"{indent}        to_damage.push_back(id);")
            lines.append(f"{indent}    }}")
            lines.append(f"{indent}    for (auto id : to_damage)")
            lines.append(f"{indent}        ctx.executor.dealDamage(id, {step.value}, ctx.source);")
            # Kill-after-damage. Mirrors what cleanup does in real
            # games (lethal damage → Trash), but invoking it inline
            # ensures test fixtures + chain-resolver paths that bypass
            # cleanup still see correct semantics.
            lines.append(f"{indent}    for (auto id : to_damage) {{")
            lines.append(f"{indent}        if (ctx.state.objectExists(id) &&")
            lines.append(f"{indent}            ctx.state.getObject(id).hasLethalDamage()) {{")
            lines.append(f"{indent}            ctx.executor.killObject(id);")
            lines.append(f"{indent}        }}")
            lines.append(f"{indent}    }}")
            lines.append(f"{indent}}}")
        else:
            lines.append(f"{indent}if (!targets.empty()) {{")
            lines.append(f"{indent}    ctx.executor.dealDamage({target_expr()}, {step.value}, ctx.source);")
            # Same kill-after-damage. Also gates the IfKills
            # conditional rider — subsequent steps with
            # step.condition=="IfKills" check the same hasLethalDamage
            # state.
            lines.append(f"{indent}    if (ctx.state.objectExists({target_expr()}) &&")
            lines.append(f"{indent}        ctx.state.getObject({target_expr()}).hasLethalDamage()) {{")
            lines.append(f"{indent}        ctx.executor.killObject({target_expr()});")
            lines.append(f"{indent}    }}")
            lines.append(f"{indent}}}")

    elif step.effect_type == "Draw":
        lines.append(f"{indent}ctx.executor.drawCards(ctx.controller, {step.value});")

    elif step.effect_type == "Kill":
        if t and t.all:
            lines.append(f"{indent}// AoE kill")
            lines.append(f"{indent}{{")
            lines.append(f"{indent}    std::vector<GameObjectId> to_kill;")
            lines.append(f"{indent}    for (auto& [id, obj] : ctx.state.objects) {{")
            lines.append(f"{indent}        if (!obj.location.has_value()) continue;")
            if t.unit:
                lines.append(f"{indent}        if (!obj.isUnit()) continue;")
            if t.gear:
                lines.append(f"{indent}        if (!obj.isGear()) continue;")
            if t.enemy:
                lines.append(f"{indent}        if (obj.controller == ctx.controller) continue;")
            if t.friendly:
                lines.append(f"{indent}        if (obj.controller != ctx.controller) continue;")
            if t.at_battlefield:
                lines.append(f"{indent}        if (!obj.isAtBattlefield()) continue;")
            lines.append(f"{indent}        to_kill.push_back(id);")
            lines.append(f"{indent}    }}")
            lines.append(f"{indent}    for (auto id : to_kill) ctx.executor.killObject(id);")
            lines.append(f"{indent}}}")
        elif t and t.self_ref:
            lines.append(f"{indent}ctx.executor.killObject(ctx.source);")
        else:
            lines.append(f"{indent}if (!targets.empty())")
            lines.append(f"{indent}    ctx.executor.killObject({target_expr()});")

    elif step.effect_type == "GiveMight":
        lines.extend(gen_single_target("giveTemporaryMight", step.value))

    elif step.effect_type == "GiveKeyword":
        kw = step.keyword or "Assault"
        lines.extend(gen_single_target("giveTemporaryKeyword", f"Keyword::{kw}", step.keyword_value))

    elif step.effect_type == "Buff":
        lines.extend(gen_single_target("buffUnit"))

    elif step.effect_type == "Ready":
        lines.extend(gen_single_target("readyObject"))

    elif step.effect_type == "Bounce":
        lines.extend(gen_single_target("bounceToHand"))

    elif step.effect_type == "Move":
        lines.extend(gen_single_target("moveToBase"))

    elif step.effect_type == "Stun":
        lines.extend(gen_single_target("stunUnit"))

    elif step.effect_type == "Discard":
        lines.append(f"{indent}ctx.executor.discardCards(ctx.controller, {step.value});")

    elif step.effect_type == "Recycle":
        if t and t.the_rest:
            lines.append(f"{indent}// Recycle the rest -- handled by composite flow")
        else:
            lines.append(f"{indent}// Recycle effect")

    elif step.effect_type == "Counter":
        lines.append(f"{indent}// Counter spell on chain")

    elif step.effect_type == "Banish":
        lines.extend(gen_single_target("banishObject"))

    elif step.effect_type == "RevealUntil":
        cond = step.reveal_condition or "Unit"
        lines.append(f"{indent}// Reveal until {cond} found — composite effect")

    elif step.effect_type == "LookAtTop":
        lines.append(f"{indent}// Look at top {step.value} cards")

    elif step.effect_type == "RevealFromTop":
        lines.append(f"{indent}// Reveal top {step.value} cards")

    elif step.effect_type == "PlayIgnoringCost":
        lines.append(f"{indent}// Play it ignoring cost — composite effect")

    elif step.effect_type == "Channel":
        exhausted_str = "true" if step.optional else "false"  # optional flag reused for exhausted
        lines.append(f"{indent}ctx.executor.channelRunes(ctx.controller, {step.value}, {exhausted_str});")

    elif step.effect_type == "Heal":
        lines.extend(gen_single_target("healObject"))

    elif step.effect_type == "Exhaust":
        lines.extend(gen_single_target("exhaustObject"))

    elif step.effect_type == "GainXP":
        lines.append(f"{indent}ctx.state.player(ctx.controller).xp += {step.value};")

    elif step.effect_type == "ChooseTarget":
        lines.append(f"{indent}// Choose target — targeting handled by getTargetRequirements()")

    return lines


def get_base_class(card_type: str) -> str:
    return {
        'unit': 'UnitCard',
        'spell': 'SpellCard',
        'gear': 'GearCard',
        'legend': 'LegendCard',
        'battlefield': 'BattlefieldCard',
        'rune': 'RuneCard',
    }[card_type]


def sanitize_comment(text: str) -> str:
    """Sanitize text for use in a C++ comment."""
    return text.replace('\n', ' ').replace('\r', '').replace('"', "'")[:80]


def gen_card_class(pc: ParsedCard) -> str:
    """Generate a complete C++ class for a parsed card."""
    base = get_base_class(pc.card_type)
    lines = []

    safe_name = sanitize_comment(pc.name)
    lines.append(f"/// card_id={pc.card_id}, {safe_name}")

    if pc.is_complex:
        lines.append(f"/// NOTE: Complex card -- partial implementation (parseable effects only)")
        lines.append(f"/// ability: {sanitize_comment(pc.ability_text)}")

    lines.append(f"class {pc.class_name} : public {base} {{")
    lines.append(f"public:")
    lines.append(f"    {pc.class_name}() : {base}({pc.card_id}) {{}}")

    # Determine which method to override — generate for ALL cards with steps
    has_steps = bool(pc.steps)
    needs_on_resolve = pc.card_type == 'spell' and has_steps
    needs_on_play = pc.card_type in ('unit', 'gear') and \
                    pc.trigger_type in ('WhenYouPlayMe', 'WhenYouPlayThis', 'AsYouPlayMe') and has_steps
    needs_on_trigger = pc.trigger_type not in ('None', 'Activated', 'WhenYouPlayMe',
                                                 'WhenYouPlayThis', 'AsYouPlayMe') and has_steps
    needs_on_activate = pc.has_activated and has_steps

    # onResolve for spells
    if needs_on_resolve:
        lines.append(f"    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {{")
        for step in pc.steps:
            step_lines = gen_effect_call(step)
            lines.extend(step_lines)
        lines.append(f"    }}")

    # onTrigger for triggered abilities (includes WhenYouPlayMe)
    if needs_on_trigger or needs_on_play:
        lines.append(f"    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {{")
        # Condition gates
        if pc.requires_legion:
            lines.append(f"        if (ctx.state.player(ctx.controller).cards_played_this_turn < 2) return;")
        if pc.requires_level:
            lines.append(f"        if (ctx.state.player(ctx.controller).xp < {pc.level_threshold}) return;")
        for step in pc.steps:
            step_lines = gen_effect_call(step)
            lines.extend(step_lines)
        lines.append(f"    }}")

    # onActivate for activated abilities
    if needs_on_activate:
        lines.append(f"    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {{")
        for step in pc.steps:
            step_lines = gen_effect_call(step)
            lines.extend(step_lines)
        lines.append(f"    }}")

    # triggerType
    if pc.trigger_type != "None":
        lines.append(f"    TriggerType triggerType() const override {{ return TriggerType::{pc.trigger_type}; }}")

    # getTargetRequirements
    target_reqs = gen_target_requirements(pc.steps)
    if target_reqs:
        lines.append(f"    TargetRequirements getTargetRequirements() const override {{")
        lines.append(f"        return {target_reqs};")
        lines.append(f"    }}")

    # hasActivatedAbility / getActivationCost / isActionAbility
    if pc.has_activated:
        lines.append(f"    bool hasActivatedAbility() const override {{ return true; }}")
        if pc.activation_cost:
            lines.append(f"    ActivationCost getActivationCost() const override {{ return {pc.activation_cost.to_cpp()}; }}")
        if pc.is_action:
            lines.append(f"    bool isActionAbility() const override {{ return true; }}")

    # Condition gates
    if pc.requires_legion:
        lines.append(f"    bool requiresLegion() const override {{ return true; }}")
    if pc.requires_level:
        lines.append(f"    bool requiresLevel() const override {{ return true; }}")
        lines.append(f"    int levelThreshold() const override {{ return {pc.level_threshold}; }}")

    lines.append(f"}};")
    lines.append(f"")

    return "\n".join(lines)


# ─── File generation ─────────────────────────────────────────────────────────

FILE_HEADER = """// Auto-generated by scripts/generate_cards.py — DO NOT EDIT
// Regenerate with: python3 scripts/generate_cards.py

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <memory>
#include <vector>

namespace riftbound {

"""

FILE_FOOTER = """
} // namespace riftbound
"""


def generate_files(cards: list, output_dir: str):
    """Generate all C++ card files."""
    parsed = [parse_card(c) for c in cards]

    # Group by card type
    groups = {
        'unit': [], 'spell': [], 'gear': [],
        'legend': [], 'battlefield': [], 'rune': [],
    }
    for pc in parsed:
        groups[pc.card_type].append(pc)

    # Prefix all class names with card type to avoid collisions between types
    # and with C++ reserved words
    type_prefix = {
        'unit': 'U', 'spell': 'S', 'gear': 'G',
        'legend': 'L', 'battlefield': 'BF', 'rune': 'R',
    }
    seen_names = {}
    for pc in parsed:
        pc.class_name = type_prefix[pc.card_type] + pc.class_name
        if pc.class_name in seen_names:
            pc.class_name = f"{pc.class_name}_{pc.card_id}"
        seen_names[pc.class_name] = pc.card_id

    # Generate per-type files
    type_files = {
        'unit': 'unit_cards.cpp',
        'spell': 'spell_cards.cpp',
        'gear': 'gear_cards.cpp',
        'legend': 'legend_cards.cpp',
        'battlefield': 'battlefield_cards.cpp',
        'rune': 'rune_cards.cpp',
    }

    for card_type, filename in type_files.items():
        filepath = os.path.join(output_dir, filename)
        with open(filepath, 'w') as f:
            f.write(FILE_HEADER)
            for pc in groups[card_type]:
                f.write(gen_card_class(pc))
                f.write("\n")
            f.write(FILE_FOOTER)
        count = len(groups[card_type])
        print(f"  {filename}: {count} cards")

    # Generate registry_init.cpp
    init_path = os.path.join(output_dir, 'registry_init.cpp')
    with open(init_path, 'w') as f:
        f.write("// Auto-generated by scripts/generate_cards.py — DO NOT EDIT\n\n")
        f.write('#include "cards/card_registry.h"\n')
        f.write('#include "cards/card.h"\n\n')

        # Forward declare all card classes — they're defined in the type files
        # but since they're in the same namespace we just include the type files' headers
        # Actually, the classes are defined in .cpp files, so we need to either:
        # 1. Declare them in headers, or
        # 2. Put all registrations inline in each type file
        # Option 2 is cleaner — each type file contributes a register function

        f.write("namespace riftbound {\n\n")
        f.write("// Forward declarations of per-type registration functions\n")
        for card_type in type_files:
            f.write(f"void register_{card_type}_cards(CardRegistry& registry);\n")
        f.write("\n")
        f.write("void registerGeneratedCards(CardRegistry& registry) {\n")
        for card_type in type_files:
            f.write(f"    register_{card_type}_cards(registry);\n")
        f.write("}\n\n")
        f.write("} // namespace riftbound\n")

    # Now re-generate type files with registration functions
    for card_type, filename in type_files.items():
        filepath = os.path.join(output_dir, filename)
        with open(filepath, 'w') as f:
            f.write(FILE_HEADER)

            # Card classes
            for pc in groups[card_type]:
                f.write(gen_card_class(pc))
                f.write("\n")

            # Registration function
            f.write(f"void register_{card_type}_cards(CardRegistry& registry) {{\n")
            for pc in groups[card_type]:
                f.write(f"    registry.registerCard({pc.card_id}, std::make_unique<{pc.class_name}>());\n")
            f.write("}\n")

            f.write(FILE_FOOTER)

    # Print stats
    total = len(parsed)
    complex_count = sum(1 for pc in parsed if pc.is_complex)
    with_effects = sum(1 for pc in parsed if pc.steps and not pc.is_complex)
    no_effects = sum(1 for pc in parsed if not pc.steps and not pc.is_complex)
    print(f"\nStats:")
    print(f"  Total cards: {total}")
    print(f"  Fully generated: {with_effects} ({with_effects*100//total}%)")
    print(f"  No effects (keyword/vanilla): {no_effects}")
    print(f"  Complex (need manual): {complex_count} ({complex_count*100//total}%)")


def main():
    # Find project root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)

    registry_path = os.path.join(project_root, 'cards', 'registry.json')
    output_dir = os.path.join(project_root, 'src', 'cards', 'generated')

    if not os.path.exists(registry_path):
        print(f"Error: {registry_path} not found", file=sys.stderr)
        sys.exit(1)

    os.makedirs(output_dir, exist_ok=True)

    print(f"Loading {registry_path}...")
    with open(registry_path) as f:
        data = json.load(f)

    cards = data['cards']
    print(f"Loaded {len(cards)} cards")
    print(f"Generating card classes in {output_dir}/...")

    generate_files(cards, output_dir)
    print("\nDone!")


if __name__ == '__main__':
    main()
