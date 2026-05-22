#!/usr/bin/env python3
"""Train a supervised baseline agent from JSONL game data.

Loads decision records from engine-generated JSONL files, extracts state features,
and trains a PyTorch MLP to predict winning actions. The model learns a policy
(which action to take) and a value function (who is winning).

Usage:
    python3 scripts/train_agent.py training_data/bounty_hunter_vs_bounty_hunter/ \
        --epochs 20 --batch-size 256 --output models/miss_fortune/v001.pt

Requirements:
    pip install torch numpy
"""

import argparse
import glob
import json
import logging
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from tqdm import tqdm
import threading

log = logging.getLogger('riftbound')


def get_gpu_stats(gpu_index=0):
    """Get GPU temperature (C) and power draw (W). Returns (temp, power) or (None, None)."""
    try:
        result = subprocess.run(
            ['nvidia-smi', '--query-gpu=temperature.gpu,power.draw',
             '--format=csv,noheader,nounits', f'--id={gpu_index}'],
            capture_output=True, text=True, timeout=5)
        parts = result.stdout.strip().split(',')
        temp = int(parts[0].strip())
        power = float(parts[1].strip())
        return temp, power
    except Exception:
        return None, None


def thermal_backoff(gpu_index=0, max_temp=80, cooldown_temp=70):
    """If GPU temp exceeds max_temp, pause until it drops below cooldown_temp."""
    temp, power = get_gpu_stats(gpu_index)
    if temp is None or temp < max_temp:
        return
    log.warning("GPU %d at %dC/%.0fW (limit %dC) — cooling down...",
                gpu_index, temp, power, max_temp)
    while True:
        time.sleep(10)
        temp, power = get_gpu_stats(gpu_index)
        if temp is None or temp <= cooldown_temp:
            log.info("  Resumed at %sC/%.0fW", temp, power)
            return
        log.debug("  Still cooling: %dC...", temp)


# ─── Feature extraction ─────────────────────────────────────────────────────

# Load card feature vectors from registry.json
def load_card_features(registry_path):
    """Load 153-dim feature vectors for all cards."""
    with open(registry_path) as f:
        data = json.load(f)
    features = {}
    for card in data['cards']:
        card_id = card['card_id']
        feat = card.get('features', {})
        # Flatten features dict to vector
        vec = []
        for key in ['card_type', 'super_type', 'energy_cost', 'power_cost', 'might',
                     'might_bonus']:
            vec.append(feat.get(key, 0) if feat.get(key) is not None else -1)
        vec.extend(feat.get('domains', [0]*6))
        vec.extend(feat.get('keywords', [0]*23))
        vec.append(feat.get('assault_value', 0))
        vec.append(feat.get('shield_value', 0))
        vec.append(feat.get('deflect_value', 0))
        vec.append(feat.get('level_value', 0))
        vec.extend(feat.get('tags', [0]*114))
        features[card_id] = np.array(vec, dtype=np.float32)
    return features


# Keyword bit positions (must match types.h)
KW_ASSAULT = 3
KW_BACKLINE = 4
KW_DEFLECT = 6
KW_GANKING = 8
KW_SHIELD = 17
KW_TANK = 18
COMBAT_KEYWORDS = [KW_TANK, KW_BACKLINE, KW_GANKING, KW_ASSAULT, KW_SHIELD, KW_DEFLECT]

# Domain names (must match types.cpp toString)
DOMAIN_NAMES = ['Fury', 'Calm', 'Mind', 'Body', 'Chaos', 'Order']


def _has_keyword(bits, bit_pos):
    return (bits >> bit_pos) & 1


# Tokens (Mech, Sprite, etc.) have card_def_id = UINT32_MAX = 4294967295.
# Feeding that to a Linear layer as a raw scalar produces astronomical
# activations and blows up training. Treat tokens / out-of-range as 0.
_CARD_VOCAB_SIZE = 787
def _safe_card_id(v):
    if v is None:
        return 0
    iv = int(v)
    if iv < 1 or iv > _CARD_VOCAB_SIZE:
        return 0
    return iv

# BattlefieldIds are typically 0..3. Cap to a sane bound to defend against
# corruption (kInvalidId = 4294967295 would otherwise pollute features).
def _safe_bf_id(v):
    if v is None:
        return -1
    iv = int(v)
    if iv < 0 or iv > 15:
        return -1
    return iv


def extract_player_features(player_data, card_features, is_self=True,
                             perspective='P1', battlefields=None):
    """Extract feature vector from a player's state. (70 features)

    Layout:
      - scores/resources (8)
      - champion/legend (4)
      - power by domain (7)
      - hand_card_ids x10 (10)
      - hand_card_costs x10 (10)
      - ready_runes, exhausted_runes (2)
      - rune_domain_ready x6 (6)
      - rune_domain_exhausted x6 (6)
      - base units: count, total_might, total_damage, num_exhausted (4)
      - base_unit_ids top 3 (3)
      - base_gear_count (1)
      - base_gear_ids top 2 (2)
      - trash_size, banishment_size (2)
      - has_discarded_this_turn (1)
      - bfs_scored_this_turn (1)
      - num_bfs_controlled (1)
      - legion_active (1)
      - cost_modifier_count (1)
    Total: 8+4+7+10+10+2+6+6+4+3+1+2+2+1+1+1+1+1 = 70
    """
    features = []

    # Determine this player's identity for BF control counting
    player_id = perspective if is_self else ('P2' if perspective == 'P1' else 'P1')

    # Scores and resources (8)
    features.append(player_data.get('score', 0))
    features.append(player_data.get('xp', 0))
    features.append(player_data.get('hand_size', 0))
    features.append(player_data.get('deck_size', 0))
    features.append(player_data.get('rune_deck_size', 0))
    features.append(player_data.get('energy', 0))
    features.append(1 if player_data.get('burned_out', False) else 0)
    cards_played = player_data.get('cards_played_this_turn', 0)
    features.append(cards_played)

    # Champion and legend (4)
    features.append(_safe_card_id(player_data.get('champion_card_id', 0)))
    features.append(1 if player_data.get('champion_in_zone', False) else 0)
    features.append(_safe_card_id(player_data.get('legend_card_id', 0)))
    features.append(1 if player_data.get('legend_exhausted', False) else 0)

    # Power breakdown by domain (7)
    power = player_data.get('power') or {}
    for domain in DOMAIN_NAMES:
        features.append(power.get(domain, 0))
    features.append(power.get('universal', 0))

    # Hand card IDs (self only — opponent's hand is secret) (10)
    hand_ids = player_data.get('hand_card_ids', []) if is_self else []
    for i in range(10):
        features.append(_safe_card_id(hand_ids[i]) if i < len(hand_ids) else 0)

    # Hand card costs (self only) (10)
    hand_costs = player_data.get('hand_card_costs', []) if is_self else []
    for i in range(10):
        features.append(hand_costs[i] if i < len(hand_costs) else 0)

    # Rune count (ready vs exhausted) (2)
    runes = player_data.get('runes', [])
    ready_runes = sum(1 for r in runes if not r.get('exhausted', False))
    exhausted_runes = sum(1 for r in runes if r.get('exhausted', False))
    features.append(ready_runes)
    features.append(exhausted_runes)

    # Rune domain breakdown: ready (6)
    ready_rune_list = [r for r in runes if not r.get('exhausted', False)]
    for domain in DOMAIN_NAMES:
        features.append(sum(1 for r in ready_rune_list
                            if domain in r.get('domains', [])))

    # Rune domain breakdown: exhausted (6)
    exhausted_rune_list = [r for r in runes if r.get('exhausted', False)]
    for domain in DOMAIN_NAMES:
        features.append(sum(1 for r in exhausted_rune_list
                            if domain in r.get('domains', [])))

    # Base units: count, total might, total damage, num exhausted (4)
    base_units = player_data.get('base_units', [])
    features.append(len(base_units))
    features.append(sum(u.get('might', 0) for u in base_units))
    features.append(sum(u.get('damage', 0) for u in base_units))
    features.append(sum(1 for u in base_units if u.get('exhausted', False)))

    # Base unit IDs top 3 sorted by might desc (3)
    sorted_base = sorted(base_units, key=lambda u: u.get('might', 0), reverse=True)
    for i in range(3):
        features.append(_safe_card_id(sorted_base[i].get('card_def_id', 0)) if i < len(sorted_base) else 0)

    # Base gear count (1)
    base_gear = player_data.get('base_gear', [])
    features.append(len(base_gear))

    # Base gear IDs top 2 (2)
    for i in range(2):
        features.append(_safe_card_id(base_gear[i].get('card_def_id', 0)) if i < len(base_gear) else 0)

    # Trash and banishment zone sizes (2)
    features.append(len(player_data.get('trash_card_ids', [])))
    features.append(len(player_data.get('banishment_card_ids', [])))

    # has_discarded_this_turn (1)
    features.append(1 if player_data.get('has_discarded_this_turn', False) else 0)

    # bfs_scored_this_turn (1)
    features.append(player_data.get('bfs_scored_this_turn', 0))

    # num_bfs_controlled (1) — count BFs where controller matches this player
    num_controlled = 0
    if battlefields:
        for bf in battlefields:
            if bf.get('controller', 'none') == player_id:
                num_controlled += 1
    features.append(num_controlled)

    # legion_active (1) — cards_played_this_turn >= 2
    features.append(1 if cards_played >= 2 else 0)

    # cost_modifier_count (1)
    features.append(player_data.get('cost_modifier_count', 0))

    return features  # 70


MAX_CHAIN_ITEMS = 4
MAX_BF_UNITS_PER_SIDE = 3


def _extract_side_features(units):
    """Extract unit summary features for one side at a battlefield. (20 features)

    Layout:
      - unit_count (1)
      - total_might (1)
      - total_damage (1)
      - num_exhausted (1)
      - num_stunned (1)
      - num_tank, num_backline, num_ganking, num_assault, num_shield, num_deflect (6)
      - Top 3 unit card_def_ids sorted by might desc (3)
      - Top 3 unit current_might (3)
      - Top 3 unit damage_marked (3)
    Total: 1+1+1+1+1+6+3+3+3 = 20
    """
    features = []
    features.append(len(units))
    features.append(sum(u.get('might', 0) for u in units))
    features.append(sum(u.get('damage', 0) for u in units))
    features.append(sum(1 for u in units if u.get('exhausted', False)))
    features.append(sum(1 for u in units if u.get('stunned', False)))
    for kw_bit in COMBAT_KEYWORDS:
        features.append(sum(1 for u in units
                            if _has_keyword(u.get('keywords', 0), kw_bit)))

    # Top 3 units by might descending
    sorted_units = sorted(units, key=lambda u: u.get('might', 0), reverse=True)
    for i in range(MAX_BF_UNITS_PER_SIDE):
        features.append(_safe_card_id(sorted_units[i].get('card_def_id', 0))
                        if i < len(sorted_units) else 0)
    for i in range(MAX_BF_UNITS_PER_SIDE):
        features.append(sorted_units[i].get('might', 0)
                        if i < len(sorted_units) else 0)
    for i in range(MAX_BF_UNITS_PER_SIDE):
        features.append(sorted_units[i].get('damage', 0)
                        if i < len(sorted_units) else 0)

    return features  # 5 + 6 + 3 + 3 + 3 = 20


def extract_battlefield_features(battlefields, perspective='P1'):
    """Extract features from battlefield state. (49 per BF x 4 = 196)

    Per BF layout:
      - card_def_id (1)
      - controller: 0=none, 1=P1, 2=P2 (1)
      - contested (1)
      - combat (1)
      - showdown (1)
      - facedown_count (1)
      - attacker_is_self: 1 if self attacking, 0 if opp attacking, -1 if no combat (1)
      - combat_phase (0-3) (1)
      - is_token (1)
      - P1 side features (20)
      - P2 side features (20)
    Total per BF: 9 + 20 + 20 = 49
    """
    BF_FEATURE_DIM = 49
    features = []
    for i in range(4):
        if i < len(battlefields):
            bf = battlefields[i]
            features.append(_safe_card_id(bf.get('card_def_id', 0)))
            ctrl = bf.get('controller', 'none')
            features.append(0 if ctrl == 'none' else (1 if ctrl == 'P1' else 2))
            features.append(1 if bf.get('contested', False) else 0)
            in_combat = bf.get('combat', False)
            features.append(1 if in_combat else 0)
            features.append(1 if bf.get('showdown', False) else 0)
            features.append(bf.get('facedown_count', 0))

            # attacker_is_self: perspective-relative
            if in_combat:
                attacker = bf.get('attacker', '')
                if attacker == perspective:
                    features.append(1)
                else:
                    features.append(0)
            else:
                features.append(-1)

            features.append(bf.get('combat_phase', 0))
            features.append(1 if bf.get('is_token', False) else 0)

            units = bf.get('units', [])
            p1_units = [u for u in units if u.get('controller') == 'P1']
            p2_units = [u for u in units if u.get('controller') == 'P2']
            features.extend(_extract_side_features(p1_units))
            features.extend(_extract_side_features(p2_units))
        else:
            features.extend([0] * BF_FEATURE_DIM)
    return features


PER_UNIT_EXT_FIELDS = 4  # combat_designation, attachment_def_id, might_delta, temp_might_bonus
EXT_PER_SIDE = MAX_BF_UNITS_PER_SIDE * PER_UNIT_EXT_FIELDS  # 12


def _extract_side_ext(units):
    """Tier 1 per-unit extended features: top-3 × (cd, gear, delta, temp_might).

    Order must match feature_extractor.cpp extractSideFeatures(ext_out=...).
    """
    cd_map = {'Attacker': 1, 'Defender': 2}
    sorted_units = sorted(units, key=lambda u: u.get('might', 0), reverse=True)
    out = []
    for i in range(MAX_BF_UNITS_PER_SIDE):
        if i < len(sorted_units):
            u = sorted_units[i]
            out.append(cd_map.get(u.get('combat', ''), 0))
            out.append(_safe_card_id(u.get('attachment_def_id', 0)))
            out.append(u.get('might', 0) - u.get('base_might', 0))
            out.append(u.get('temp_might_bonus', 0))
        else:
            out.extend([0, 0, 0, 0])
    return out


def extract_state_features(record, perspective='P1'):
    """Extract full state feature vector from a decision record.

    Layout (470 features):
      0..353 : 10 global + 8 chain + 70 self + 70 opp + 49x4 BFs (base layout)
      354..361 : chain item targets (4 slots × 2 def_ids)
      362..365 : cost-modifier types (2 players × {next_spell, next_unit})
      366..373 : per-BF scored flags (4 BFs × 2 players)
      374..469 : per-unit extended (4 BFs × 2 sides × top-3 × 4 fields)
    """
    state = record['state']

    if perspective == 'P1':
        self_data = state['player1']
        opp_data = state['player2']
    else:
        self_data = state['player2']
        opp_data = state['player1']

    battlefields = state.get('battlefields', [])
    features = []

    # Global features (10)
    features.append(record.get('turn', 0))
    phase_map = {'Mulligan': 0, 'AwakenPhase': 1, 'BeginningStep': 2,
                 'ScoringStep': 3, 'ChannelPhase': 4, 'DrawPhase': 5,
                 'MainPhase': 6, 'EndingStep': 7, 'ExpirationStep': 8}
    features.append(phase_map.get(record.get('phase', ''), 0))
    features.append(1 if record.get('turn_player', '') == perspective else 0)
    starting = state.get('starting_player', 'P1')
    features.append(1 if perspective == starting else 0)  # went_first
    features.append(self_data.get('score', 0) - opp_data.get('score', 0))
    chain = state.get('chain', [])
    chain_list = chain if isinstance(chain, list) else []
    features.append(len(chain_list))
    # ns_state: 0=neutral, 1=showdown (JSONL writer emits lowercase strings)
    features.append(1 if state.get('ns_state', 'neutral') == 'showdown' else 0)
    # oc_state: 0=open, 1=closed
    features.append(1 if state.get('oc_state', 'open') == 'closed' else 0)
    features.append(1 if state.get('is_additional_turn', False) else 0)
    features.append(state.get('delayed_ability_count', 0))

    # Chain items (4 x 2 = 8, padded)
    for i in range(MAX_CHAIN_ITEMS):
        if i < len(chain_list):
            item = chain_list[i]
            features.append(_safe_card_id(item.get('source_card_def_id', 0)))
            ctrl = item.get('controller', '')
            features.append(1 if ctrl == perspective else 0)
        else:
            features.extend([0, 0])

    # Self player features (70)
    features.extend(extract_player_features(self_data, None, is_self=True,
                                            perspective=perspective,
                                            battlefields=battlefields))

    # Opponent features (70, no hand contents)
    features.extend(extract_player_features(opp_data, None, is_self=False,
                                            perspective=perspective,
                                            battlefields=battlefields))

    # Battlefield features (196)
    features.extend(extract_battlefield_features(battlefields, perspective))

    # ── Tier 1 expansion (positions 354..469) ────────────────────────────────

    # Chain item targets: 4 slots × 2 def_ids = 8
    for i in range(MAX_CHAIN_ITEMS):
        if i < len(chain_list):
            targets = chain_list[i].get('target_def_ids', []) or []
            for t in range(2):
                features.append(_safe_card_id(targets[t]) if t < len(targets) else 0)
        else:
            features.extend([0, 0])

    # Cost-modifier types: 2 per player × 2 players = 4
    for d in (self_data, opp_data):
        features.append(1 if d.get('cost_mod_next_spell_only', False) else 0)
        features.append(1 if d.get('cost_mod_next_unit_only', False) else 0)

    # Per-BF scored flags: 2 per BF × 4 BFs = 8
    self_scored = set(self_data.get('bfs_scored_ids', []))
    opp_scored = set(opp_data.get('bfs_scored_ids', []))
    for i in range(4):
        if i < len(battlefields):
            bf_id = battlefields[i].get('id', i)
            features.append(1 if bf_id in self_scored else 0)
            features.append(1 if bf_id in opp_scored else 0)
        else:
            features.extend([0, 0])

    # Per-unit extended features: 12 per side × 2 sides × 4 BFs = 96
    for i in range(4):
        if i < len(battlefields):
            units = battlefields[i].get('units', [])
            p1_units = [u for u in units if u.get('controller') == 'P1']
            p2_units = [u for u in units if u.get('controller') == 'P2']
            features.extend(_extract_side_ext(p1_units))
            features.extend(_extract_side_ext(p2_units))
        else:
            features.extend([0] * (EXT_PER_SIDE * 2))

    # ── Zone multi-hot encoding (positions 470..4404, 5 × 787 = 3935 floats) ─
    # Perfect-information per CR: self knows own deck/trash/banishment and
    # opponent's public zones (trash, banishment). Opponent's deck stays hidden.
    def _emit_zone_multihot(card_ids):
        vec = [0.0] * CARD_VOCAB_SIZE
        for cid in card_ids:
            if cid is None:
                continue
            cid = int(cid)
            if 1 <= cid <= CARD_VOCAB_SIZE:
                vec[cid - 1] += 1.0
        return vec

    features.extend(_emit_zone_multihot(self_data.get('deck_card_ids', [])))
    features.extend(_emit_zone_multihot(self_data.get('trash_card_ids', [])))
    features.extend(_emit_zone_multihot(self_data.get('banishment_card_ids', [])))
    features.extend(_emit_zone_multihot(opp_data.get('trash_card_ids', [])))
    features.extend(_emit_zone_multihot(opp_data.get('banishment_card_ids', [])))

    # Per-player turn flags (positions 4405..4406)
    features.append(1 if self_data.get('cant_play_cards_this_turn', False) else 0)
    features.append(1 if opp_data.get('cant_play_cards_this_turn', False) else 0)

    # ── Tier 2/3 expansion (positions 4407..4622) ────────────────────────────
    # Per-unit Tier 2/3 features: 8 per unit × 3 units × 2 sides × 4 BFs = 192.
    # Order must match feature_extractor.cpp emit_unit_t2: assault, shield,
    # deflect, buff_count, temp_buff_count, tank, backline, ganking.
    def _extract_side_t2(units):
        sorted_units = sorted(units, key=lambda u: u.get('might', 0), reverse=True)
        out = []
        for i in range(MAX_BF_UNITS_PER_SIDE):
            if i < len(sorted_units):
                u = sorted_units[i]
                kw_bits = u.get('keywords', 0)
                out.append(u.get('assault_value', 0))
                out.append(u.get('shield_value', 0))
                out.append(u.get('deflect_value', 0))
                out.append(u.get('buff_count', 0))
                out.append(u.get('temp_buff_count', 0))
                out.append(1 if _has_keyword(kw_bits, KW_TANK) else 0)
                out.append(1 if _has_keyword(kw_bits, KW_BACKLINE) else 0)
                out.append(1 if _has_keyword(kw_bits, KW_GANKING) else 0)
            else:
                out.extend([0] * 8)
        return out

    # Map "P1"/"P2" to self/opp ordering for the C++ side
    perspective_id_str = perspective
    opp_id_str = 'P2' if perspective == 'P1' else 'P1'

    for i in range(4):
        if i < len(battlefields):
            units = battlefields[i].get('units', [])
            self_units = [u for u in units if u.get('controller') == perspective_id_str]
            opp_units  = [u for u in units if u.get('controller') == opp_id_str]
            features.extend(_extract_side_t2(self_units))
            features.extend(_extract_side_t2(opp_units))
        else:
            features.extend([0] * (2 * MAX_BF_UNITS_PER_SIDE * 8))

    # Extended globals (positions 4599..4622) — 24 floats. Must match
    # feature_extractor.cpp layout exactly.
    # 4599..4600: ready base units (self, opp)
    features.append(self_data.get('ready_base_units', 0))
    features.append(opp_data.get('ready_base_units', 0))
    # 4601..4602: cost modifier magnitude sum (self, opp)
    features.append(self_data.get('cost_modifier_magnitude', 0))
    features.append(opp_data.get('cost_modifier_magnitude', 0))
    # 4603..4604: additional turn queue depth (self, opp)
    features.append(self_data.get('additional_turns_queued', 0))
    features.append(opp_data.get('additional_turns_queued', 0))
    # 4605..4608: contested-by-self per BF (×4)
    for i in range(4):
        if i < len(battlefields):
            bf = battlefields[i]
            features.append(1 if (bf.get('contested', False)
                                   and bf.get('contested_by', 'none') == perspective_id_str)
                            else 0)
        else:
            features.append(0)
    # 4609..4612: combat_staged per BF (×4)
    for i in range(4):
        features.append(1 if (i < len(battlefields)
                               and battlefields[i].get('combat_staged', False))
                        else 0)
    # 4613..4616: showdown_staged per BF (×4)
    for i in range(4):
        features.append(1 if (i < len(battlefields)
                               and battlefields[i].get('showdown_staged', False))
                        else 0)
    # 4617..4620: facedown age per BF (×4)
    for i in range(4):
        features.append(battlefields[i].get('oldest_facedown_age', 0)
                        if i < len(battlefields) else 0)
    # 4621: focus_holder (1=self, 0=opp, -1=no showdown / unassigned)
    fh = state.get('focus_holder', 'none')
    if fh == perspective_id_str:
        features.append(1)
    elif fh == 'none':
        features.append(-1)
    else:
        features.append(0)
    # 4622: total battlefield count
    features.append(len(battlefields))

    # Pad to reserved width for forward-compatible --resume
    if len(features) < RESERVED_STATE_DIM:
        features.extend([0.0] * (RESERVED_STATE_DIM - len(features)))

    return np.array(features, dtype=np.float32)


ACTION_TYPE_MAP = {
    'EndTurn': 0, 'PassPriority': 1, 'PassFocus': 2,
    'PlayCard': 3, 'PlayActionCard': 4, 'PlayReaction': 5,
    'StandardMove': 6, 'ActivateAbility': 7, 'ActivateActionAbility': 8,
    'MulliganDecision': 9, 'AssignCombatDamage': 10,
    'HideCard': 11, 'MakeChoice': 12, 'ChooseBattlefield': 13,
}
NUM_ACTION_TYPES = 14


def extract_action_features(record):
    """Extract action type index from chosen action."""
    action = record.get('chosen_action', {})
    action_type = action.get('type', 'EndTurn')
    return ACTION_TYPE_MAP.get(action_type, 0)


def featurize_single_action(action):
    """Extract features for a single legal action.

    Returns a fixed-size feature vector representing this specific action.

    Layout (25 features):
      - action type one-hot (14)
      - card_def_id (1)
      - target_def_ids x2 (2)
      - ability_source_def_id (1)
      - dest_bf (1)
      - play_bf (1)
      - mulligan_count (1)
      - unit_def_ids x2 (2) — for move actions
      - chosen_object_def_id (1) — first of chosen_def_ids if present
      - chosen_bf_id (1) — from chosen_bf if present
    """
    features = []

    # Action type (one-hot, 14 dims)
    action_type = ACTION_TYPE_MAP.get(action.get('type', 'EndTurn'), 0)
    one_hot = [0.0] * NUM_ACTION_TYPES
    one_hot[action_type] = 1.0
    features.extend(one_hot)

    # Card involved (card_def_id, 0 if none)
    features.append(float(_safe_card_id(action.get('card_def_id', 0))))

    # Target card_def_ids (pad to 2)
    target_ids = action.get('target_def_ids', [])
    for i in range(2):
        features.append(float(_safe_card_id(target_ids[i])) if i < len(target_ids) else 0.0)

    # Ability source
    features.append(float(_safe_card_id(action.get('ability_source_def_id', 0))))

    # Destination battlefield
    features.append(float(_safe_bf_id(action.get('dest_bf', -1))))
    features.append(float(_safe_bf_id(action.get('play_bf', -1))))

    # Mulligan count
    features.append(float(action.get('mulligan_count', 0)))

    # Unit move def IDs (pad to 2) — for StandardMove actions
    unit_def_ids = action.get('unit_def_ids', [])
    for i in range(2):
        features.append(float(_safe_card_id(unit_def_ids[i])) if i < len(unit_def_ids) else 0.0)

    # Chosen object def ID — first item of chosen_def_ids if present
    chosen_def_ids = action.get('chosen_def_ids', [])
    features.append(float(_safe_card_id(chosen_def_ids[0])) if chosen_def_ids else 0.0)

    # Chosen battlefield ID
    features.append(float(_safe_bf_id(action.get('chosen_bf', -1))))

    return features  # 14 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 1 = 25 floats


ACTION_FEATURE_DIM = 25
MAX_LEGAL_ACTIONS = 64  # pad/truncate to this
RESERVED_STATE_DIM = 4864  # pad state features for forward-compatible --resume
CARD_VOCAB_SIZE = 787       # card_def_id 1..787 → multi-hot position id-1
CARD_EMBEDDING_DIM = 32     # learned card identity vector dimension
CARD_PAD_INDEX = 0          # reserved index for "no card" / pad / invalid token


# ─── Dataset ─────────────────────────────────────────────────────────────────

import tempfile
import struct


# Binary record format (must match src/io/binary_data_serializer.cpp)
BIN_MAGIC = b'RFBA'
BIN_HEADER_SIZE = 20
BIN_STATE_DIM_NATIVE = 4623
BIN_RECORD_DTYPE = np.dtype([
    ('perspective', 'u1'),
    ('num_legal',   'u1'),
    ('chosen_idx',  '<u2'),
    ('state',       '<f4', (BIN_STATE_DIM_NATIVE,)),
    ('actions',     '<f4', (MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM)),
    ('mask',        'u1',  (MAX_LEGAL_ACTIONS,)),
])


def _find_game_files(data_dir, max_games=None):
    game_files = sorted(glob.glob(os.path.join(data_dir, 'game.jsonl.game*')))
    if not game_files:
        single = os.path.join(data_dir, 'game.jsonl')
        if os.path.exists(single):
            game_files = [single]
    if max_games:
        game_files = game_files[:max_games]
    return game_files


def _find_binary_files(data_dir, max_games=None):
    """Find .bin training data files emitted by BinaryDataSerializer."""
    bin_files = sorted(glob.glob(os.path.join(data_dir, '*.bin.game*')))
    if not bin_files:
        bin_files = sorted(glob.glob(os.path.join(data_dir, '*.bin')))
    if max_games:
        bin_files = bin_files[:max_games]
    return bin_files


def _read_binary_header(path):
    """Read and validate the 20-byte header of a .bin file.

    Header layout: magic(4) + version(2) + state_dim(2) + action_dim(2)
    + max_actions(2) + num_decisions(4) + winner(1) + score_p1(1) + score_p2(1)
    + pad(1) = 20 bytes. Score fields are int8 0..8; pre-change binaries have
    them as 0 and reward computation falls back to ±1 win/loss.
    """
    with open(path, 'rb') as f:
        h = f.read(BIN_HEADER_SIZE)
    if len(h) < BIN_HEADER_SIZE or h[0:4] != BIN_MAGIC:
        return None
    version, state_dim, action_dim, max_actions = struct.unpack('<HHHH', h[4:12])
    num_decisions, = struct.unpack('<I', h[12:16])
    winner = h[16]
    score_p1, score_p2 = struct.unpack('<bb', h[17:19])
    if (state_dim != BIN_STATE_DIM_NATIVE or action_dim != ACTION_FEATURE_DIM
            or max_actions != MAX_LEGAL_ACTIONS):
        log.warning("binary file %s has unexpected dims (state=%d action=%d max=%d)",
                    path, state_dim, action_dim, max_actions)
        return None
    return {
        'path': path,
        'version': version,
        'num_decisions': num_decisions,
        'winner': winner,
        'score_p1': int(score_p1),
        'score_p2': int(score_p2),
    }


def _parse_game_file(gf):
    """Parse a single JSONL game file. Returns (decisions, summary) or ([], None)."""
    decisions = []
    summary = None
    try:
        with open(gf) as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                record = json.loads(line)
                if record.get('type') == 'decision':
                    decisions.append(record)
                elif record.get('type') == 'game_summary':
                    summary = record
    except (json.JSONDecodeError, IOError):
        return [], None
    return decisions, summary


def _count_file_decisions(gf):
    """Count valid decisions in a single game file. Returns (path, count)."""
    decisions, summary = _parse_game_file(gf)
    if not summary or not decisions:
        return gf, 0
    count = sum(1 for dec in decisions
                if isinstance(dec.get('legal_actions', []), list)
                and len(dec.get('legal_actions', [])) > 0)
    return gf, count


def _count_decisions(game_files, num_workers=None):
    """Fast first pass: count total decisions and detect state_dim."""
    import multiprocessing
    if num_workers is None:
        num_workers = min(8, multiprocessing.cpu_count())

    log.info("Pass 1/2: counting decisions in %d files (%d workers)...",
             len(game_files), num_workers)

    per_file_counts = {}
    scan_start = time.time()
    if num_workers <= 1:
        pbar = tqdm(game_files, desc="  Scanning", unit="file",
                    dynamic_ncols=True, mininterval=2.0)
        for i, gf in enumerate(pbar):
            _, count = _count_file_decisions(gf)
            per_file_counts[gf] = count
            if (i + 1) % 1000 == 0:
                running_total = sum(per_file_counts.values())
                log.debug("  Scan progress: %d/%d files, %d decisions so far (%.0fs)",
                          i + 1, len(game_files), running_total,
                          time.time() - scan_start)
        pbar.close()
    else:
        from concurrent.futures import ProcessPoolExecutor, as_completed
        import signal as _sig
        def _worker_init():
            _sig.signal(_sig.SIGINT, _sig.SIG_IGN)
        with ProcessPoolExecutor(max_workers=num_workers, initializer=_worker_init) as pool:
            futures = {pool.submit(_count_file_decisions, gf): gf for gf in game_files}
            pbar = tqdm(total=len(game_files), desc="  Scanning", unit="file",
                        dynamic_ncols=True, mininterval=2.0)
            completed = 0
            try:
                for future in as_completed(futures):
                    gf, count = future.result()
                    per_file_counts[gf] = count
                    completed += 1
                    pbar.update(1)
                    if completed % 1000 == 0:
                        running_total = sum(per_file_counts.values())
                        log.debug("  Scan progress: %d/%d files, %d decisions so far (%.0fs)",
                                  completed, len(game_files), running_total,
                                  time.time() - scan_start)
            except KeyboardInterrupt:
                log.info("Interrupted during scan — shutting down workers...")
                pool.shutdown(wait=False, cancel_futures=True)
                pbar.close()
                raise
            pbar.close()

    total = sum(per_file_counts.values())
    log.debug("  Scan complete in %.1fs", time.time() - scan_start)

    # Detect state_dim from first file with decisions
    state_dim = RESERVED_STATE_DIM
    for gf in game_files:
        if per_file_counts.get(gf, 0) > 0:
            decisions, _ = _parse_game_file(gf)
            for dec in decisions:
                if isinstance(dec.get('legal_actions', []), list) and len(dec.get('legal_actions', [])) > 0:
                    feats = extract_state_features(dec, dec.get('turn_player', 'P1'))
                    state_dim = len(feats)
                    break
            break

    log.info("  Found %d decisions, state_dim=%s", total, state_dim)
    return total, state_dim, per_file_counts


def _extract_file_to_memmap(args_tuple):
    """Worker: extract features from one game file into memmap at a given offset.

    Each worker opens its own memmap handles — writes to non-overlapping slices
    so no synchronization needed. Safe for ProcessPoolExecutor.
    """
    gf, start_idx, tmpdir, state_dim, total_decisions = args_tuple

    decisions, summary = _parse_game_file(gf)
    if not summary or not decisions:
        return 0

    feat_arr = np.memmap(os.path.join(tmpdir, 'features.npy'),
                         dtype=np.float32, mode='r+', shape=(total_decisions, state_dim))
    act_arr = np.memmap(os.path.join(tmpdir, 'actions.npy'),
                        dtype=np.int64, mode='r+', shape=(total_decisions,))
    out_arr = np.memmap(os.path.join(tmpdir, 'outcomes.npy'),
                        dtype=np.float32, mode='r+', shape=(total_decisions,))
    act_feat_arr = np.memmap(os.path.join(tmpdir, 'act_feats.npy'),
                             dtype=np.float32, mode='r+',
                             shape=(total_decisions, MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM))
    act_mask_arr = np.memmap(os.path.join(tmpdir, 'act_masks.npy'),
                             dtype=np.float32, mode='r+',
                             shape=(total_decisions, MAX_LEGAL_ACTIONS))

    winner = summary.get('winner', 'None')
    idx = start_idx
    for dec in decisions:
        turn_player = dec.get('turn_player', 'P1')
        perspective = turn_player
        if winner == 'None':
            outcome = 0.0
        elif winner == perspective:
            outcome = 1.0
        else:
            outcome = -1.0

        chosen_idx = dec.get('chosen_index', 0)
        legal_actions = dec.get('legal_actions', [])
        if not isinstance(legal_actions, list) or len(legal_actions) == 0:
            continue

        features = extract_state_features(dec, perspective)
        feat_arr[idx, :len(features)] = features
        act_arr[idx] = min(chosen_idx, MAX_LEGAL_ACTIONS - 1)
        out_arr[idx] = outcome

        for j, act in enumerate(legal_actions[:MAX_LEGAL_ACTIONS]):
            if isinstance(act, dict):
                af = featurize_single_action(act)
                act_feat_arr[idx, j, :len(af)] = af
                act_mask_arr[idx, j] = 1.0
        idx += 1

    # Flush this worker's writes
    feat_arr.flush()
    act_arr.flush()
    out_arr.flush()
    act_feat_arr.flush()
    act_mask_arr.flush()
    return idx - start_idx


class RiftboundBinaryDataset(Dataset):
    """Dataset of decision points read directly from .bin training files.

    The C++ engine writes per-decision feature records (see
    src/io/binary_data_serializer.cpp). This dataset mmaps those files
    directly — no JSONL Pass 1 (counting) or Pass 2 (parse+featurize)
    needed. Disk I/O is the only data path.

    A small LRU cache keeps at most `mmap_cache_size` memory maps open at
    once so we stay under typical file-descriptor limits even with many
    DataLoader workers.
    """

    def __init__(self, data_dir, max_games=None, num_workers=None,
                 mmap_cache_size=128):
        bin_files = _find_binary_files(data_dir, max_games)
        if not bin_files:
            raise FileNotFoundError(f"No .bin training files found in {data_dir}")

        log.info("Scanning %d binary game files...", len(bin_files))
        scan_start = time.time()
        self._files = []   # parallel list of {path, num_decisions, winner}

        # Use parallel header reads — each is a 20-byte open/read/close.
        if num_workers and num_workers > 1:
            from concurrent.futures import ThreadPoolExecutor
            with ThreadPoolExecutor(max_workers=num_workers) as pool:
                headers = list(pool.map(_read_binary_header, bin_files))
        else:
            headers = [_read_binary_header(p) for p in bin_files]

        # Build flat (file_idx, decision_idx) index
        total = 0
        for h in headers:
            if h is None or h['num_decisions'] == 0:
                continue
            self._files.append(h)
            total += h['num_decisions']

        if total == 0:
            raise RuntimeError(f"No decisions in any binary file under {data_dir}")

        # Flat index — int32 each side handles up to 2B files / decisions.
        self._index = np.empty((total, 2), dtype=np.int32)
        cursor = 0
        for fi, h in enumerate(self._files):
            n = h['num_decisions']
            self._index[cursor:cursor + n, 0] = fi
            self._index[cursor:cursor + n, 1] = np.arange(n, dtype=np.int32)
            cursor += n
        assert cursor == total

        self._len = total
        self._state_dim = RESERVED_STATE_DIM
        self._mmap_cache = {}     # file_idx → np.memmap
        self._mmap_cache_size = mmap_cache_size

        scan_time = time.time() - scan_start
        log.info("  %d decisions across %d files (%.1fs scan)",
                 total, len(self._files), scan_time)

    def _get_mmap(self, file_idx):
        mm = self._mmap_cache.get(file_idx)
        if mm is not None:
            return mm
        if len(self._mmap_cache) >= self._mmap_cache_size:
            # Evict oldest insertion (Python dict preserves insertion order).
            oldest = next(iter(self._mmap_cache))
            del self._mmap_cache[oldest]
        h = self._files[file_idx]
        mm = np.memmap(h['path'], dtype=BIN_RECORD_DTYPE, mode='r',
                       offset=BIN_HEADER_SIZE, shape=(h['num_decisions'],))
        self._mmap_cache[file_idx] = mm
        return mm

    def __len__(self):
        return self._len

    def __getitem__(self, idx):
        file_idx = int(self._index[idx, 0])
        dec_idx = int(self._index[idx, 1])
        mm = self._get_mmap(file_idx)
        rec = mm[dec_idx]

        winner = self._files[file_idx]['winner']
        perspective = int(rec['perspective'])
        score_p1 = self._files[file_idx].get('score_p1', 0)
        score_p2 = self._files[file_idx].get('score_p2', 0)

        # Reward: score-differential when scores are available (newer .bin
        # files), else fall back to ±1 win/loss / 0 draw. Score-diff densifies
        # the gradient signal: a candidate that wins 8-7 gets +0.125, a
        # blowout 8-0 gets +1.0. Same for losses. Reduces variance.
        if score_p1 > 0 or score_p2 > 0:
            self_score = score_p1 if perspective == 0 else score_p2
            opp_score  = score_p2 if perspective == 0 else score_p1
            outcome = float(self_score - opp_score) / 8.0
        elif winner == 2:
            outcome = 0.0
        elif winner == perspective:
            outcome = 1.0
        else:
            outcome = -1.0

        # Pad state from native 354 → RESERVED_STATE_DIM (1024) at fetch time
        # so trained models keep their forward-compatible input width.
        state = np.zeros(self._state_dim, dtype=np.float32)
        state[:BIN_STATE_DIM_NATIVE] = rec['state']

        # Copy actions + mask out of the mmap so a later cache eviction
        # cannot pull the rug from under in-flight batches.
        actions = np.asarray(rec['actions'], dtype=np.float32).copy()
        mask = rec['mask'].astype(np.float32, copy=True)

        return (
            torch.from_numpy(state),
            torch.tensor(int(rec['chosen_idx']), dtype=torch.long),
            torch.tensor(outcome, dtype=torch.float32),
            torch.from_numpy(actions),
            torch.from_numpy(mask),
        )

    def cleanup(self):
        """No temp files to clean — kept for interface compat."""
        self._mmap_cache.clear()


def make_dataset(data_dir, max_games=None, num_workers=None):
    """Pick the right dataset based on what files are in `data_dir`.

    .bin files → RiftboundBinaryDataset (mmap, no Pass 2)
    .jsonl files → RiftboundDataset (legacy JSONL pipeline with memmap Pass 2)
    """
    if _find_binary_files(data_dir):
        return RiftboundBinaryDataset(data_dir, max_games=max_games,
                                       num_workers=num_workers)
    return RiftboundDataset(data_dir, max_games=max_games,
                             num_workers=num_workers)


class RiftboundDataset(Dataset):
    """Dataset of decision points from JSONL game files.

    Uses numpy memmap for disk-backed storage — datasets larger than RAM
    are handled via OS paging with no OOM risk. Two-pass loading: count
    decisions first, then fill pre-allocated memmap arrays on disk.
    """

    def __init__(self, data_dir, max_games=None, num_workers=None):
        import multiprocessing
        if num_workers is None:
            num_workers = min(8, multiprocessing.cpu_count())

        game_files = _find_game_files(data_dir, max_games)
        total_files = len(game_files)

        # Pass 1: count decisions and detect state_dim
        total_decisions, state_dim, per_file_counts = _count_decisions(game_files, num_workers=num_workers)
        self._state_dim = state_dim
        if total_decisions == 0:
            self._len = 0
            self.features = torch.zeros(0, state_dim, dtype=torch.float32)
            self.actions = torch.zeros(0, dtype=torch.long)
            self.outcomes = torch.zeros(0, dtype=torch.float32)
            self.action_feats = torch.zeros(0, MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM, dtype=torch.float32)
            self.action_masks = torch.zeros(0, MAX_LEGAL_ACTIONS, dtype=torch.float32)
            return

        # Create memmap files in a temp directory (auto-cleaned on process exit)
        mem_gb = (total_decisions * (state_dim + MAX_LEGAL_ACTIONS * ACTION_FEATURE_DIM
                  + MAX_LEGAL_ACTIONS + 2) * 4) / (1024**3)
        log.info("Pass 2/2: extracting features into disk-backed arrays (%.1f GB)...", mem_gb)

        self._tmpdir = tempfile.mkdtemp(prefix='riftbound_')
        import atexit
        atexit.register(self.cleanup)
        feat_arr = np.memmap(os.path.join(self._tmpdir, 'features.npy'),
                             dtype=np.float32, mode='w+', shape=(total_decisions, state_dim))
        act_arr = np.memmap(os.path.join(self._tmpdir, 'actions.npy'),
                            dtype=np.int64, mode='w+', shape=(total_decisions,))
        out_arr = np.memmap(os.path.join(self._tmpdir, 'outcomes.npy'),
                            dtype=np.float32, mode='w+', shape=(total_decisions,))
        act_feat_arr = np.memmap(os.path.join(self._tmpdir, 'act_feats.npy'),
                                 dtype=np.float32, mode='w+',
                                 shape=(total_decisions, MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM))
        act_mask_arr = np.memmap(os.path.join(self._tmpdir, 'act_masks.npy'),
                                 dtype=np.float32, mode='w+',
                                 shape=(total_decisions, MAX_LEGAL_ACTIONS))

        # Compute per-file offsets into the memmap arrays
        file_offsets = {}
        offset = 0
        for gf in game_files:
            file_offsets[gf] = offset
            offset += per_file_counts.get(gf, 0)

        # Pass 2: fill arrays (parallel)
        load_start = time.time()
        from concurrent.futures import ProcessPoolExecutor, as_completed
        num_load_workers = num_workers

        log.info("  Using %d workers for feature extraction", num_load_workers)

        files_with_decisions = [(gf, file_offsets[gf]) for gf in game_files
                                if per_file_counts.get(gf, 0) > 0]

        # Each worker opens its own memmap handles and writes to non-overlapping slices
        work_items = [(gf, start_idx, self._tmpdir, state_dim, total_decisions)
                      for gf, start_idx in files_with_decisions]

        if num_load_workers <= 1:
            pbar = tqdm(work_items, desc="  Loading", unit="file",
                        dynamic_ncols=True, mininterval=2.0)
            for i, item in enumerate(pbar):
                _extract_file_to_memmap(item)
                if (i + 1) % 1000 == 0:
                    log.debug("  Load progress: %d/%d files (%.0fs)",
                              i + 1, len(work_items), time.time() - load_start)
            pbar.close()
        else:
            import signal as _sig
            def _worker_init():
                _sig.signal(_sig.SIGINT, _sig.SIG_IGN)
            with ProcessPoolExecutor(max_workers=num_load_workers, initializer=_worker_init) as pool:
                futures = {pool.submit(_extract_file_to_memmap, item): item
                           for item in work_items}
                pbar = tqdm(total=len(work_items), desc="  Loading", unit="file",
                            dynamic_ncols=True, mininterval=2.0)
                completed = 0
                try:
                    for future in as_completed(futures):
                        future.result()
                        completed += 1
                        pbar.update(1)
                        if completed % 1000 == 0:
                            log.debug("  Load progress: %d/%d files (%.0fs)",
                                      completed, len(work_items), time.time() - load_start)
                except KeyboardInterrupt:
                    log.info("Interrupted during loading — shutting down workers...")
                    pool.shutdown(wait=False, cancel_futures=True)
                    pbar.close()
                    raise
                pbar.close()

        # Flush parent's handles
        feat_arr.flush()
        act_arr.flush()
        out_arr.flush()
        act_feat_arr.flush()
        act_mask_arr.flush()

        self._len = total_decisions

        # Re-open as read-only memmap
        idx = total_decisions
        self._feat = np.memmap(os.path.join(self._tmpdir, 'features.npy'),
                               dtype=np.float32, mode='r', shape=(idx, state_dim))
        self._act = np.memmap(os.path.join(self._tmpdir, 'actions.npy'),
                              dtype=np.int64, mode='r', shape=(idx,))
        self._out = np.memmap(os.path.join(self._tmpdir, 'outcomes.npy'),
                              dtype=np.float32, mode='r', shape=(idx,))
        self._act_feat = np.memmap(os.path.join(self._tmpdir, 'act_feats.npy'),
                                    dtype=np.float32, mode='r',
                                    shape=(idx, MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM))
        self._act_mask = np.memmap(os.path.join(self._tmpdir, 'act_masks.npy'),
                                    dtype=np.float32, mode='r',
                                    shape=(idx, MAX_LEGAL_ACTIONS))

        games_loaded = sum(1 for c in per_file_counts.values() if c > 0)
        load_time = time.time() - load_start
        log.info("  Loaded %d decisions from %d games in %.1fs", idx, games_loaded, load_time)
        log.info("  Temp storage: %s", self._tmpdir)

    def cleanup(self):
        """Remove temp memmap directory. Safe to call multiple times."""
        tmpdir = getattr(self, '_tmpdir', None)
        if tmpdir and os.path.exists(tmpdir):
            import shutil
            files = os.listdir(tmpdir)
            total_bytes = sum(os.path.getsize(os.path.join(tmpdir, f)) for f in files)
            log.info("Cleaning up temp storage: %s (%.1f GB)",
                     tmpdir, total_bytes / (1024**3))
            for f in files:
                log.debug("  Removing: %s (%.1f MB)",
                          f, os.path.getsize(os.path.join(tmpdir, f)) / (1024**2))
            shutil.rmtree(tmpdir, ignore_errors=True)
            log.info("  Cleanup complete.")
            self._tmpdir = None

    def __del__(self):
        self.cleanup()

    def __len__(self):
        return self._len

    def __getitem__(self, idx):
        return (torch.from_numpy(self._feat[idx].copy()),
                torch.tensor(self._act[idx], dtype=torch.long),
                torch.tensor(self._out[idx], dtype=torch.float32),
                torch.from_numpy(self._act_feat[idx].copy()),
                torch.from_numpy(self._act_mask[idx].copy()))


# ─── Model ───────────────────────────────────────────────────────────────────

def _state_card_id_positions():
    """Return a sorted list of state-feature positions that hold card_def_id
    values. These positions get embedded via the shared card embedding table;
    they're zeroed out of the normalized state vector to avoid double-counting.

    Layout must match feature_extractor.cpp exactly. Constants are duplicated
    here from the C++ side rather than imported because the model definition
    needs them at construction time before the dataset loads.
    """
    positions = []
    # 0..9: global features (no card IDs).
    # 10..17: 4 chain items × (source_def_id, is_self). Card ID at +0 of each.
    for slot in range(MAX_CHAIN_ITEMS):
        positions.append(10 + slot * 2)

    # Self player block starts at offset 18, 70 features long.
    # Within player block: champion at +8, legend at +10,
    #                      hand_ids at +19..28, base_unit_ids at +57..59,
    #                      base_gear_ids at +61..62.
    self_off = 18
    positions.append(self_off + 8)        # champion
    positions.append(self_off + 10)       # legend
    for k in range(10):                   # hand
        positions.append(self_off + 19 + k)
    for k in range(3):                    # base units
        positions.append(self_off + 57 + k)
    for k in range(2):                    # base gear
        positions.append(self_off + 61 + k)

    # Opp player block starts at offset 88 (same internal layout).
    opp_off = 88
    positions.append(opp_off + 8)
    positions.append(opp_off + 10)
    for k in range(10):                   # always 0 (opp hand hidden) but kept
        positions.append(opp_off + 19 + k)
    for k in range(3):
        positions.append(opp_off + 57 + k)
    for k in range(2):
        positions.append(opp_off + 61 + k)

    # BF block starts at offset 158, 49 features per BF, 4 BFs.
    # Per BF: bf_card_id at +0, then 8 status fields, then side1 (20),
    # then side2 (20). Within each side: 11 counts/keywords, then 3 unit IDs,
    # then 3 might, then 3 damage.
    for i in range(4):
        bf_base = 158 + i * 49
        positions.append(bf_base)             # bf card_def_id
        side1_base = bf_base + 9              # 1 (card_def_id) + 8 status fields
        for k in range(3):                    # side1 top-3 unit IDs at +11..13
            positions.append(side1_base + 11 + k)
        side2_base = bf_base + 29             # 9 + 20
        for k in range(3):                    # side2 top-3 unit IDs
            positions.append(side2_base + 11 + k)

    # Tier 1 chain item targets: 4 slots × 2 def_ids at positions 354..361.
    for k in range(8):
        positions.append(354 + k)

    # Tier 1 ext per-unit attachments at offset 374.
    # Layout: 8 side slots × 3 units × 4 fields. Attachment_def_id is field 1.
    for s in range(8):
        for u in range(3):
            positions.append(374 + s * 12 + u * 4 + 1)

    return sorted(set(positions))


def _action_card_id_positions():
    """Return positions in the 25-dim action feature vector that hold
    card_def_id values.

    Layout (must match featurize_single_action above):
      0..13  action type one-hot (no IDs)
      14     card_def_id        ← embedded
      15..16 target_def_ids x2  ← embedded
      17     ability_source_id  ← embedded
      18..19 dest_bf, play_bf   (BF IDs, not card IDs — skipped)
      20     mulligan_count      (count, not ID — skipped)
      21..22 unit_def_ids x2     ← embedded
      23     chosen_object_def   ← embedded
      24     chosen_bf            (BF ID — skipped)
    """
    return [14, 15, 16, 17, 21, 22, 23]


# Cached at module load so the model class can reference them as defaults.
_STATE_CARD_POSITIONS = _state_card_id_positions()
_ACTION_CARD_POSITIONS = _action_card_id_positions()


class RiftboundAgent(nn.Module):
    """Per-action scoring model with card embeddings, dropout, and attention.

    Architectural improvements over the previous 2-layer MLP:
      1. Card embedding table (788 × 32) shared between state and action
         features. Replaces raw card_def_id floats — gives the model a
         learnable identity vector per card so similar cards (equipment,
         tempo units, control spells, archetype synergies) cluster.
      2. Dropout in encoder + action scorer for regularization across
         REINFORCE iters where the data is on-policy-stale.
      3. Self-attention over the action set so each action's score is
         informed by all other available actions. The previous independent
         scoring couldn't reason "play X *instead of* Y".

    ONNX-exportable: uses standard ops (Embedding, MultiheadAttention,
    Linear, LayerNorm, Dropout, scatter via masked operations).
    """

    def __init__(self, state_dim, action_dim=ACTION_FEATURE_DIM, hidden_dim=512,
                 card_vocab=CARD_VOCAB_SIZE, card_embed_dim=CARD_EMBEDDING_DIM,
                 dropout=0.15, attn_heads=4):
        super().__init__()
        self.state_dim = state_dim
        self.action_dim = action_dim
        self.hidden_dim = hidden_dim
        self.card_embed_dim = card_embed_dim

        # ── Card embedding (shared between state and action features) ──────
        # Index 0 = "no card" / pad: padding_idx makes its embedding fixed at
        # zeros (no gradient updates) so empty slots don't waste capacity.
        # card_def_ids in [1, 787] → indices [1, 787]. Total table size: 788.
        self.card_embedding = nn.Embedding(card_vocab + 1, card_embed_dim,
                                           padding_idx=CARD_PAD_INDEX)

        # Position lists are registered as non-trainable buffers so they
        # travel with the model (state_dict, .to(device), ONNX export).
        self.register_buffer(
            "state_card_positions",
            torch.tensor(_STATE_CARD_POSITIONS, dtype=torch.long),
            persistent=False,
        )
        self.register_buffer(
            "action_card_positions",
            torch.tensor(_ACTION_CARD_POSITIONS, dtype=torch.long),
            persistent=False,
        )
        # Mask vector: 1 for non-card positions, 0 for card-ID positions.
        # Multiplied into state_features before LayerNorm to zero out the
        # raw card IDs (their content is now carried by the embedding).
        state_mask = torch.ones(state_dim, dtype=torch.float32)
        for p in _STATE_CARD_POSITIONS:
            if p < state_dim:
                state_mask[p] = 0.0
        self.register_buffer("state_card_zero_mask", state_mask, persistent=False)
        action_mask = torch.ones(action_dim, dtype=torch.float32)
        for p in _ACTION_CARD_POSITIONS:
            if p < action_dim:
                action_mask[p] = 0.0
        self.register_buffer("action_card_zero_mask", action_mask, persistent=False)

        # Number of card embeddings concatenated per sample.
        num_state_cards = len(_STATE_CARD_POSITIONS)
        num_action_cards = len(_ACTION_CARD_POSITIONS)
        self._num_state_cards = num_state_cards
        self._num_action_cards = num_action_cards

        enriched_state_dim = state_dim + num_state_cards * card_embed_dim
        enriched_action_dim = action_dim + num_action_cards * card_embed_dim

        # ── Input normalization ─────────────────────────────────────────────
        # LayerNorm on the state input AFTER zeroing card-ID slots. Card
        # embeddings are concatenated post-norm (they're already on a sane
        # scale by definition).
        self.state_norm = nn.LayerNorm(state_dim)
        self.action_norm = nn.LayerNorm(action_dim)

        # ── State encoder ───────────────────────────────────────────────────
        # Deeper than the old model (3 hidden layers vs 2) with dropout
        # between them. Card embeddings concatenated as input.
        self.state_encoder = nn.Sequential(
            nn.Linear(enriched_state_dim, hidden_dim),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(hidden_dim, hidden_dim // 2),
            nn.ReLU(),
        )

        # ── Per-action projection ───────────────────────────────────────────
        # Brings each action into a common embedding space sized to the
        # attention input. Uses dropout on the projection output.
        action_hidden = hidden_dim // 2
        self.action_proj = nn.Sequential(
            nn.Linear(enriched_action_dim + hidden_dim // 2, action_hidden),
            nn.ReLU(),
            nn.Dropout(dropout),
        )

        # ── Self-attention over the action set ──────────────────────────────
        # 4-head attention over the (up-to) 64 actions. Each action's
        # representation is informed by all others so the scorer can pick
        # the *best* action given the alternatives, not score each in
        # isolation.
        self.action_attn = nn.MultiheadAttention(
            embed_dim=action_hidden,
            num_heads=attn_heads,
            dropout=dropout,
            batch_first=True,
        )
        self.attn_norm = nn.LayerNorm(action_hidden)

        # ── Action scorer ───────────────────────────────────────────────────
        self.action_scorer = nn.Sequential(
            nn.Linear(action_hidden, action_hidden),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(action_hidden, 1),
        )

        # ── Value head (from state alone) ───────────────────────────────────
        self.value_head = nn.Sequential(
            nn.Linear(hidden_dim // 2, 128),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(128, 1),
            nn.Tanh(),
        )

    # ── Helpers ─────────────────────────────────────────────────────────────

    def _embed_state_cards(self, state_features):
        """Look up the card embedding at each state card_def_id position.

        Returns:
            embeddings: (batch, num_state_cards * card_embed_dim)
        """
        # Gather card_def_ids at the known positions, clamp to vocab range.
        # state_features is float; cast to long for embedding lookup. Clamp
        # negative values (from BF ID guards etc.) to padding index 0.
        ids = state_features.index_select(1, self.state_card_positions)
        ids = ids.long().clamp(min=0, max=CARD_VOCAB_SIZE)
        embs = self.card_embedding(ids)              # (B, N_cards, embed_dim)
        return embs.flatten(start_dim=1)             # (B, N_cards * embed_dim)

    def _embed_action_cards(self, action_features):
        """Look up the card embedding at each action card_def_id position.

        Returns:
            embeddings: (batch, max_actions, num_action_cards * card_embed_dim)
        """
        ids = action_features.index_select(2, self.action_card_positions)
        ids = ids.long().clamp(min=0, max=CARD_VOCAB_SIZE)
        embs = self.card_embedding(ids)              # (B, A, N_cards, embed_dim)
        return embs.flatten(start_dim=2)             # (B, A, N_cards * embed_dim)

    # ── Forward ─────────────────────────────────────────────────────────────

    def forward(self, state_features, action_features, action_mask=None):
        """
        Args:
            state_features: (batch, state_dim)
            action_features: (batch, max_actions, action_dim)
            action_mask: (batch, max_actions) — 1 for valid, 0 for padding
        Returns:
            action_scores: (batch, max_actions)
            value: (batch,)
        """
        # ── Embed card IDs ──────────────────────────────────────────────────
        state_card_embs = self._embed_state_cards(state_features)
        action_card_embs = self._embed_action_cards(action_features)

        # ── Zero out card_ID positions then normalize continuous features ───
        # Multiplying by the mask is ONNX-friendly (avoids in-place scatter).
        state_clean = state_features * self.state_card_zero_mask
        action_clean = action_features * self.action_card_zero_mask
        state_normed = self.state_norm(state_clean)
        action_normed = self.action_norm(action_clean)

        # ── Concatenate card embeddings to enrich the inputs ────────────────
        state_input = torch.cat([state_normed, state_card_embs], dim=1)
        action_input = torch.cat([action_normed, action_card_embs], dim=2)

        # ── Encode state ────────────────────────────────────────────────────
        state_enc = self.state_encoder(state_input)  # (B, hidden//2)

        # ── Per-action: combine with state encoding, project to attn space ──
        max_actions = action_input.shape[1]
        state_for_actions = state_enc.unsqueeze(1).expand(-1, max_actions, -1)
        combined = torch.cat([state_for_actions, action_input], dim=2)
        # (B, A, hidden//2 + enriched_action_dim)
        action_rep = self.action_proj(combined)      # (B, A, action_hidden)

        # ── Self-attention over the action set ──────────────────────────────
        # key_padding_mask: True at positions to IGNORE (padded actions).
        attn_padding = None
        if action_mask is not None:
            attn_padding = (action_mask == 0)        # (B, A)
            # If ALL actions are masked for some batch row (shouldn't happen
            # but defensive), drop the mask for that row to avoid NaN softmax.
            # MultiheadAttention NaN-guards internally if any row is all-True.
        attn_out, _ = self.action_attn(
            action_rep, action_rep, action_rep,
            key_padding_mask=attn_padding,
            need_weights=False,
        )
        # Residual connection + layer norm — standard transformer pattern.
        action_rep = self.attn_norm(action_rep + attn_out)

        # ── Score each action ───────────────────────────────────────────────
        scores = self.action_scorer(action_rep).squeeze(-1)  # (B, A)
        if action_mask is not None:
            scores = scores.masked_fill(action_mask == 0, float('-inf'))

        # ── Value head ──────────────────────────────────────────────────────
        value = self.value_head(state_enc).squeeze(-1)        # (B,)

        return scores, value


# ─── Training ────────────────────────────────────────────────────────────────

def train(args):
    # Load data
    dl_workers = args.dataloader_workers
    dataset = make_dataset(args.data_dir, max_games=args.max_games,
                           num_workers=dl_workers)
    if len(dataset) == 0:
        print("ERROR: No data loaded. Check the data directory path.")
        sys.exit(1)

    # Split train/val (90/10)
    n = len(dataset)
    n_val = max(1, n // 10)
    n_train = n - n_val
    train_ds, val_ds = torch.utils.data.random_split(dataset, [n_train, n_val])
    train_loader = DataLoader(train_ds, batch_size=args.batch_size, shuffle=True,
                              num_workers=dl_workers, pin_memory=True,
                              prefetch_factor=2 if dl_workers > 0 else None,
                              persistent_workers=dl_workers > 0)
    val_loader = DataLoader(val_ds, batch_size=args.batch_size, shuffle=False,
                            num_workers=dl_workers, pin_memory=True,
                            prefetch_factor=2 if dl_workers > 0 else None,
                            persistent_workers=dl_workers > 0)

    input_dim = dataset._state_dim
    log.info("Input dimension: %d", input_dim)
    log.info("Training samples: %d, Validation samples: %d", n_train, n_val)

    # Create model
    device = torch.device(args.device)
    hidden_dim = args.hidden_dim

    # Resume from checkpoint if specified
    if args.resume:
        log.info("Resuming from %s", args.resume)
        checkpoint = torch.load(args.resume, map_location='cpu', weights_only=False)
        hidden_dim = checkpoint.get('hidden_dim', args.hidden_dim)
        model = RiftboundAgent(input_dim, hidden_dim=hidden_dim).to(device)
        model.load_state_dict(checkpoint['model_state_dict'])
        log.info("  Loaded weights from %s (trained %s epochs, %s samples)",
                 args.resume, checkpoint.get('epochs_trained', '?'),
                 checkpoint.get('train_samples', '?'))
    else:
        model = RiftboundAgent(input_dim, hidden_dim=hidden_dim).to(device)
        log.info("Per-action scoring model (state_dim=%d, action_dim=%d)",
                 input_dim, ACTION_FEATURE_DIM)

    log.info("Model parameters: %s", f"{sum(p.numel() for p in model.parameters()):,}")
    log.info("DataLoader workers: %d, prefetch: %s, device: %s",
             dl_workers, 2 if dl_workers > 0 else 'N/A', device)

    optimizer = optim.Adam(model.parameters(), lr=args.lr, weight_decay=1e-5)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)
    policy_loss_fn = nn.CrossEntropyLoss()
    value_loss_fn = nn.MSELoss()

    # Training loop
    best_val_loss = float('inf')
    log.info("Training: %d batches/epoch, Validation: %d batches/epoch",
             len(train_loader), len(val_loader))
    log.info("Starting training...\n")

    for epoch in range(args.epochs):
        # Train
        model.train()
        train_policy_loss = 0
        train_value_loss = 0
        train_correct = 0
        train_total = 0
        epoch_start = time.time()

        log.debug("Epoch %d/%d starting train phase (%d batches)...",
                  epoch+1, args.epochs, len(train_loader))
        train_pbar = tqdm(train_loader,
                          desc=f"  Epoch {epoch+1}/{args.epochs} [train]",
                          leave=False, unit="batch",
                          dynamic_ncols=True, mininterval=2.0)
        batch_count = 0
        for batch in train_pbar:
            features, actions, outcomes, act_feats, act_masks = batch
            features = features.to(device)
            actions = actions.to(device)
            outcomes = outcomes.to(device)
            act_feats = act_feats.to(device)
            act_masks = act_masks.to(device)

            scores, values = model(features, act_feats, act_masks)
            p_loss = policy_loss_fn(scores, actions)
            v_loss = value_loss_fn(values, outcomes)
            _, predicted = scores.max(1)

            loss = p_loss + args.value_weight * v_loss

            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()

            train_policy_loss += p_loss.item() * features.size(0)
            train_value_loss += v_loss.item() * features.size(0)
            train_correct += predicted.eq(actions).sum().item()
            train_total += features.size(0)

            batch_count += 1
            if batch_count % 2000 == 0:
                log.debug("  Train batch %d/%d — p_loss=%.4f acc=%.1f%% (%.0fs)",
                          batch_count, len(train_loader), p_loss.item(),
                          train_correct/train_total*100,
                          time.time() - epoch_start)

            train_pbar.set_postfix(
                loss=f"{p_loss.item():.3f}",
                acc=f"{train_correct/train_total*100:.1f}%")

        train_pbar.close()
        log.debug("Epoch %d/%d train phase complete (%.0fs), starting validation...",
                  epoch+1, args.epochs, time.time() - epoch_start)
        scheduler.step()

        # Validate
        model.eval()
        val_policy_loss = 0
        val_value_loss = 0
        val_correct = 0
        val_total = 0

        val_pbar = tqdm(val_loader,
                        desc=f"  Epoch {epoch+1}/{args.epochs} [val]  ",
                        leave=False, unit="batch",
                        dynamic_ncols=True, mininterval=2.0)
        with torch.no_grad():
            for batch in val_pbar:
                features, actions, outcomes, act_feats, act_masks = batch
                features = features.to(device)
                actions = actions.to(device)
                outcomes = outcomes.to(device)
                act_feats = act_feats.to(device)
                act_masks = act_masks.to(device)

                scores, values = model(features, act_feats, act_masks)
                p_loss = policy_loss_fn(scores, actions)
                v_loss = value_loss_fn(values, outcomes)
                _, predicted = scores.max(1)

                val_policy_loss += p_loss.item() * features.size(0)
                val_value_loss += v_loss.item() * features.size(0)
                val_correct += predicted.eq(actions).sum().item()
                val_total += features.size(0)

        val_pbar.close()

        # Metrics
        avg_train_p = train_policy_loss / train_total
        avg_train_v = train_value_loss / train_total
        train_acc = train_correct / train_total * 100
        avg_val_p = val_policy_loss / val_total
        avg_val_v = val_value_loss / val_total
        val_acc = val_correct / val_total * 100
        total_val = avg_val_p + args.value_weight * avg_val_v

        improved = total_val < best_val_loss

        gpu_idx = int(args.device.split(':')[-1]) if ':' in args.device else 0
        temp, power = get_gpu_stats(gpu_idx)
        temp_str = f" GPU:{temp}C/{power:.0f}W" if temp else ""
        epoch_elapsed = time.time() - epoch_start

        log.info("Epoch %3d/%d (%3.0fs) | "
                 "Train: p_loss=%.4f v_loss=%.4f acc=%.1f%% | "
                 "Val: p_loss=%.4f v_loss=%.4f acc=%.1f%%%s%s",
                 epoch+1, args.epochs, epoch_elapsed,
                 avg_train_p, avg_train_v, train_acc,
                 avg_val_p, avg_val_v, val_acc,
                 " *" if improved else "", temp_str)

        # Save checkpoint on improvement or every 5 epochs
        if improved or (epoch + 1) % 5 == 0:
            if improved:
                best_val_loss = total_val
            ckpt_data = {
                'model_state_dict': model.state_dict(),
                'model_version': 'per_action',
                'input_dim': input_dim,
                'hidden_dim': hidden_dim,
                'num_actions': MAX_LEGAL_ACTIONS,
                'action_feature_dim': ACTION_FEATURE_DIM,
                'epochs_trained': epoch + 1,
                'train_samples': n_train,
                'val_samples': n_val,
                'best_val_loss': best_val_loss,
                'data_dir': args.data_dir,
            }
            os.makedirs(os.path.dirname(args.output) if os.path.dirname(args.output) else '.', exist_ok=True)
            torch.save(ckpt_data, args.output)
            log.debug("  Checkpoint saved to %s (epoch %d, val_loss=%.4f)",
                      args.output, epoch+1, total_val)

        # Thermal protection
        thermal_backoff(
            gpu_index=int(args.device.split(':')[-1]) if ':' in args.device else 0,
            max_temp=82, cooldown_temp=70)

    # Save final model
    os.makedirs(os.path.dirname(args.output) if os.path.dirname(args.output) else '.', exist_ok=True)
    save_data = {
        'model_state_dict': model.state_dict(),
        'model_version': 'per_action',
        'input_dim': input_dim,
        'hidden_dim': hidden_dim,
        'num_actions': MAX_LEGAL_ACTIONS,
        'action_feature_dim': ACTION_FEATURE_DIM,
        'epochs_trained': args.epochs,
        'train_samples': n_train,
        'val_samples': n_val,
        'best_val_loss': best_val_loss,
        'data_dir': args.data_dir,
    }
    torch.save(save_data, args.output)
    log.info("Model saved to %s", args.output)
    log.info("  Model version: per-action scoring")
    log.info("  Input dim: %d", input_dim)
    log.info("  Parameters: %s", f"{sum(p.numel() for p in model.parameters()):,}")
    log.info("  Best val loss: %.4f", best_val_loss)

    # Export to ONNX for C++ inference
    onnx_path = args.output.replace('.pt', '.onnx')
    model.eval()
    model.cpu()
    dummy_state = torch.randn(1, input_dim)
    dummy_actions = torch.randn(1, MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM)
    dummy_mask = torch.ones(1, MAX_LEGAL_ACTIONS)
    try:
        torch.onnx.export(
            model, (dummy_state, dummy_actions, dummy_mask), onnx_path,
            input_names=['state_features', 'action_features', 'action_mask'],
            output_names=['action_scores', 'value'],
            dynamic_axes={
                'state_features': {0: 'batch'},
                'action_features': {0: 'batch'},
                'action_mask': {0: 'batch'},
            },
            opset_version=17,
        )
        log.info("  ONNX exported to %s", onnx_path)
    except Exception as e:
        log.error("  ONNX export failed: %s", e)


# ─── RL training (REINFORCE) ─────────────────────────────────────────────────

def train_rl(args):
    """REINFORCE fine-tuning on self-play game data.

    Differs from supervised `train()` only in the loss:

      policy_loss = -(log_prob(chosen_action) * advantage).mean()
      advantage   = outcome - V(state).detach()
      value_loss  = MSE(V(state), outcome)        # same as supervised
      entropy     = -sum(p * log p) over valid actions
      total       = policy_loss + value_weight * value_loss
                                - entropy_coef * entropy

    The model's forward() applies masked_fill(-inf) at invalid action slots, so
    log_softmax yields -inf there and exp() yields 0 there. We use torch.where
    to dodge the 0 * -inf NaN trap in the entropy term.

    Requires --resume from a supervised checkpoint — RL fine-tunes; it does not
    train from scratch. Default LR (1e-4) is 10x lower than supervised to
    preserve learned weights.
    """
    if not args.resume:
        log.error("train-rl requires --resume <supervised_checkpoint.pt>")
        sys.exit(1)

    dl_workers = args.dataloader_workers
    dataset = make_dataset(args.data_dir, max_games=args.max_games,
                           num_workers=dl_workers)
    if len(dataset) == 0:
        log.error("No data loaded from %s", args.data_dir)
        sys.exit(1)

    n = len(dataset)
    n_val = max(1, n // 10)
    n_train = n - n_val
    train_ds, val_ds = torch.utils.data.random_split(dataset, [n_train, n_val])
    train_loader = DataLoader(train_ds, batch_size=args.batch_size, shuffle=True,
                              num_workers=dl_workers, pin_memory=True,
                              prefetch_factor=2 if dl_workers > 0 else None,
                              persistent_workers=dl_workers > 0)
    val_loader = DataLoader(val_ds, batch_size=args.batch_size, shuffle=False,
                            num_workers=dl_workers, pin_memory=True,
                            prefetch_factor=2 if dl_workers > 0 else None,
                            persistent_workers=dl_workers > 0)

    input_dim = dataset._state_dim
    device = torch.device(args.device)

    log.info("Loading checkpoint: %s", args.resume)
    checkpoint = torch.load(args.resume, map_location='cpu', weights_only=False)
    hidden_dim = checkpoint.get('hidden_dim', 512)
    model = RiftboundAgent(input_dim, hidden_dim=hidden_dim).to(device)
    model.load_state_dict(checkpoint['model_state_dict'])
    log.info("  Resumed from %s (trained %s epochs)",
             args.resume, checkpoint.get('epochs_trained', '?'))

    log.info("RL fine-tune (REINFORCE): state_dim=%d, hidden_dim=%d",
             input_dim, hidden_dim)
    log.info("Training samples: %d, Validation samples: %d", n_train, n_val)
    log.info("LR=%.0e, entropy_coef=%.4f, value_weight=%.2f",
             args.lr, args.entropy_coef, args.value_weight)

    optimizer = optim.Adam(model.parameters(), lr=args.lr, weight_decay=1e-5)
    value_loss_fn = nn.MSELoss()

    for epoch in range(args.epochs):
        model.train()
        train_p_loss = 0.0
        train_v_loss = 0.0
        train_entropy = 0.0
        train_correct = 0
        train_total = 0
        epoch_start = time.time()

        train_pbar = tqdm(train_loader,
                          desc=f"  Epoch {epoch+1}/{args.epochs} [rl-train]",
                          leave=False, unit="batch",
                          dynamic_ncols=True, mininterval=2.0)
        batch_count = 0
        for batch in train_pbar:
            features, actions, outcomes, act_feats, act_masks = batch
            features = features.to(device)
            actions = actions.to(device)
            outcomes = outcomes.to(device)
            act_feats = act_feats.to(device)
            act_masks = act_masks.to(device)

            scores, values = model(features, act_feats, act_masks)
            # scores already has -inf at masked positions (from model.forward).

            log_probs = F.log_softmax(scores, dim=-1)             # (B, A)
            probs = log_probs.exp()                                # (B, A); 0 at invalid

            # REINFORCE policy gradient
            chosen_log_prob = log_probs.gather(
                1, actions.unsqueeze(1)).squeeze(1)                # (B,)
            advantage = (outcomes - values.detach())               # (B,)
            policy_loss = -(chosen_log_prob * advantage).mean()

            # Value head MSE (unchanged from supervised)
            v_loss = value_loss_fn(values, outcomes)

            # Entropy bonus — only over valid (mask=1) positions.
            # Avoid 0 * -inf NaN by zeroing log_probs at invalid slots before mul.
            safe_log_probs = torch.where(
                act_masks == 1, log_probs, torch.zeros_like(log_probs))
            entropy_per_sample = -(probs * safe_log_probs).sum(dim=-1)  # (B,)
            entropy = entropy_per_sample.mean()

            loss = (policy_loss
                    + args.value_weight * v_loss
                    - args.entropy_coef * entropy)

            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()

            with torch.no_grad():
                _, predicted = scores.max(1)
                train_correct += predicted.eq(actions).sum().item()
            train_total += features.size(0)
            train_p_loss += policy_loss.item() * features.size(0)
            train_v_loss += v_loss.item() * features.size(0)
            train_entropy += entropy.item() * features.size(0)

            batch_count += 1
            if batch_count % 200 == 0:
                train_pbar.set_postfix(
                    p_loss=f"{policy_loss.item():.3f}",
                    H=f"{entropy.item():.2f}",
                    acc=f"{train_correct/train_total*100:.1f}%")

        train_pbar.close()

        # Validation
        model.eval()
        val_p_loss = 0.0
        val_v_loss = 0.0
        val_entropy = 0.0
        val_correct = 0
        val_total = 0
        with torch.no_grad():
            for batch in val_loader:
                features, actions, outcomes, act_feats, act_masks = batch
                features = features.to(device)
                actions = actions.to(device)
                outcomes = outcomes.to(device)
                act_feats = act_feats.to(device)
                act_masks = act_masks.to(device)

                scores, values = model(features, act_feats, act_masks)
                log_probs = F.log_softmax(scores, dim=-1)
                probs = log_probs.exp()
                chosen_log_prob = log_probs.gather(
                    1, actions.unsqueeze(1)).squeeze(1)
                advantage = (outcomes - values)
                policy_loss = -(chosen_log_prob * advantage).mean()
                v_loss = value_loss_fn(values, outcomes)
                safe_log_probs = torch.where(
                    act_masks == 1, log_probs, torch.zeros_like(log_probs))
                entropy = -(probs * safe_log_probs).sum(dim=-1).mean()

                bs = features.size(0)
                val_p_loss += policy_loss.item() * bs
                val_v_loss += v_loss.item() * bs
                val_entropy += entropy.item() * bs
                _, predicted = scores.max(1)
                val_correct += predicted.eq(actions).sum().item()
                val_total += bs

        gpu_idx = int(args.device.split(':')[-1]) if ':' in args.device else 0
        temp, power = get_gpu_stats(gpu_idx)
        temp_str = f" GPU:{temp}C/{power:.0f}W" if temp else ""
        elapsed = time.time() - epoch_start

        log.info("Epoch %3d/%d (%3.0fs) | "
                 "Train: p=%.4f v=%.4f H=%.3f acc=%.1f%% | "
                 "Val: p=%.4f v=%.4f H=%.3f acc=%.1f%%%s",
                 epoch+1, args.epochs, elapsed,
                 train_p_loss/train_total, train_v_loss/train_total,
                 train_entropy/train_total, train_correct/train_total*100,
                 val_p_loss/val_total, val_v_loss/val_total,
                 val_entropy/val_total, val_correct/val_total*100,
                 temp_str)

        thermal_backoff(gpu_index=gpu_idx, max_temp=82, cooldown_temp=70)

    # Save candidate checkpoint + ONNX
    os.makedirs(os.path.dirname(args.output) if os.path.dirname(args.output) else '.',
                exist_ok=True)
    save_data = {
        'model_state_dict': model.state_dict(),
        'model_version': 'per_action_rl',
        'input_dim': input_dim,
        'hidden_dim': hidden_dim,
        'num_actions': MAX_LEGAL_ACTIONS,
        'action_feature_dim': ACTION_FEATURE_DIM,
        'epochs_trained': checkpoint.get('epochs_trained', 0) + args.epochs,
        'train_samples': n_train,
        'val_samples': n_val,
        'data_dir': args.data_dir,
        'resumed_from': args.resume,
        'lr': args.lr,
        'entropy_coef': args.entropy_coef,
    }
    torch.save(save_data, args.output)
    log.info("Candidate saved to %s", args.output)

    # ONNX export — same shape as supervised model, plug-compatible with ModelAgent
    onnx_path = args.output.replace('.pt', '.onnx')
    model.eval()
    model.cpu()
    dummy_state = torch.randn(1, input_dim)
    dummy_actions = torch.randn(1, MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM)
    dummy_mask = torch.ones(1, MAX_LEGAL_ACTIONS)
    try:
        torch.onnx.export(
            model, (dummy_state, dummy_actions, dummy_mask), onnx_path,
            input_names=['state_features', 'action_features', 'action_mask'],
            output_names=['action_scores', 'value'],
            dynamic_axes={
                'state_features': {0: 'batch'},
                'action_features': {0: 'batch'},
                'action_mask': {0: 'batch'},
            },
            opset_version=17,
        )
        log.info("  ONNX exported to %s", onnx_path)
    except Exception as e:
        log.error("  ONNX export failed: %s", e)


# ─── Evaluation ──────────────────────────────────────────────────────────────

def evaluate(args):
    """Evaluate a trained model on held-out game data."""
    checkpoint = torch.load(args.model_path, map_location='cpu', weights_only=False)
    input_dim = checkpoint['input_dim']
    hidden_dim = checkpoint['hidden_dim']

    model = RiftboundAgent(input_dim, hidden_dim=hidden_dim)
    model.load_state_dict(checkpoint['model_state_dict'])
    model.eval()

    dataset = make_dataset(args.data_dir, max_games=args.max_games, num_workers=4)
    if len(dataset) == 0:
        log.error("No data loaded.")
        sys.exit(1)

    loader = DataLoader(dataset, batch_size=256, shuffle=False,
                        num_workers=4, pin_memory=True,
                        prefetch_factor=2, persistent_workers=True)

    correct = 0
    total = 0
    value_errors = []

    with torch.no_grad():
        for batch in tqdm(loader, desc="Evaluating", unit="batch",
                             dynamic_ncols=True, mininterval=2.0):
            features, actions, outcomes, act_feats, act_masks = batch
            scores, values = model(features, act_feats, act_masks)
            _, predicted = scores.max(1)
            correct += predicted.eq(actions).sum().item()
            total += features.size(0)
            value_errors.extend((values - outcomes).abs().tolist())

    log.info("Evaluation on %d decisions:", total)
    log.info("  Policy accuracy: %.1f%%", correct/total*100)
    log.info("  Value MAE: %.4f", np.mean(value_errors))


# ─── CLI ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description='Train Riftbound ML agent')
    subparsers = parser.add_subparsers(dest='command')

    # Train command
    train_parser = subparsers.add_parser('train', help='Train a model')
    train_parser.add_argument('data_dir', help='Directory with JSONL game files')
    train_parser.add_argument('--output', '-o', default='models/agent_v001.pt',
                              help='Output model path')
    train_parser.add_argument('--epochs', type=int, default=20)
    train_parser.add_argument('--batch-size', type=int, default=256)
    train_parser.add_argument('--hidden-dim', type=int, default=256)
    train_parser.add_argument('--lr', type=float, default=1e-3)
    train_parser.add_argument('--value-weight', type=float, default=0.5,
                              help='Weight of value loss relative to policy loss')
    train_parser.add_argument('--max-games', type=int, default=None,
                              help='Max number of game files to load')
    train_parser.add_argument('--device', default='cuda' if torch.cuda.is_available() else 'cpu')
    train_parser.add_argument('--gpu', type=int, default=None,
                              help='GPU index (0 or 1)')
    train_parser.add_argument('--resume', type=str, default=None,
                              help='Resume training from a checkpoint .pt file')
    train_parser.add_argument('--dataloader-workers', type=int, default=4,
                              help='DataLoader worker processes for parallel data prefetch (0=main thread only)')

    # RL fine-tuning command (REINFORCE on self-play data)
    rl_parser = subparsers.add_parser('train-rl',
        help='REINFORCE fine-tune on self-play game data')
    rl_parser.add_argument('data_dir', help='Directory with self-play JSONL game files')
    rl_parser.add_argument('--output', '-o', required=True,
                           help='Output candidate checkpoint path')
    rl_parser.add_argument('--resume', type=str, required=True,
                           help='Required: supervised checkpoint to warm-start from')
    rl_parser.add_argument('--epochs', type=int, default=3,
                           help='RL epochs (default 3 — small to avoid drift)')
    rl_parser.add_argument('--batch-size', type=int, default=256)
    rl_parser.add_argument('--lr', type=float, default=1e-4,
                           help='Learning rate (default 1e-4, 10x lower than supervised)')
    rl_parser.add_argument('--value-weight', type=float, default=0.5)
    rl_parser.add_argument('--entropy-coef', type=float, default=0.01,
                           help='Entropy bonus weight (higher = more exploration preserved)')
    rl_parser.add_argument('--max-games', type=int, default=None)
    rl_parser.add_argument('--device', default='cuda' if torch.cuda.is_available() else 'cpu')
    rl_parser.add_argument('--gpu', type=int, default=None)
    rl_parser.add_argument('--dataloader-workers', type=int, default=4)

    # Evaluate command
    eval_parser = subparsers.add_parser('eval', help='Evaluate a trained model')
    eval_parser.add_argument('model_path', help='Path to trained model .pt file')
    eval_parser.add_argument('data_dir', help='Directory with JSONL game files')
    eval_parser.add_argument('--max-games', type=int, default=None)

    # Global flags
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Enable debug-level logging')

    args = parser.parse_args()

    # Configure logging
    console_level = logging.DEBUG if getattr(args, 'verbose', False) else logging.INFO
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(console_level)
    handlers = [console_handler]

    # Add file handler alongside model output for train/eval
    log_path = None
    output_path = getattr(args, 'output', None) or getattr(args, 'model_path', None)
    if output_path:
        log_path = output_path.rsplit('.', 1)[0] + '.log'
        os.makedirs(os.path.dirname(log_path) if os.path.dirname(log_path) else '.', exist_ok=True)
        file_handler = logging.FileHandler(log_path, mode='w')
        file_handler.setLevel(logging.DEBUG)
        file_handler.setFormatter(logging.Formatter('%(asctime)s %(message)s',
                                                     datefmt='%Y-%m-%d %H:%M:%S'))
        handlers.append(file_handler)

    logging.basicConfig(
        level=logging.DEBUG,
        format='%(message)s',
        handlers=handlers
    )

    if log_path:
        log.info("=== Training log: %s ===", log_path)

    # atexit cleanup registered in RiftboundDataset.__init__

    if args.command == 'train':
        if args.gpu is not None:
            args.device = f'cuda:{args.gpu}'
        train(args)
    elif args.command == 'train-rl':
        if args.gpu is not None:
            args.device = f'cuda:{args.gpu}'
        train_rl(args)
    elif args.command == 'eval':
        evaluate(args)
    else:
        parser.print_help()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        log.info("Interrupted — exiting.")
        threading.excepthook = lambda _: None
        sys.exit(1)
