#!/usr/bin/env python3
"""Validate that C++ binary features == Python features extracted from JSONL.

Runs the engine twice on the same seed: once with -o foo.jsonl, once with
-o foo.bin. For every decision, compares the binary record's state/action
features to what the Python extractor produces from the JSONL.

Any mismatch indicates Python and C++ feature extraction have drifted. This
is the silent-degradation risk flagged in CLAUDE.md — the trainer learns
on features the model never sees at inference.

Usage:
    python scripts/parity_check.py [--deck1 path] [--deck2 path]
                                    [--games N] [--seed S]
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np

# Make scripts/ importable so we can reuse the feature extractor.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from train_agent import (
    extract_state_features,
    featurize_single_action,
    _parse_game_file,
    ACTION_FEATURE_DIM,
    MAX_LEGAL_ACTIONS,
    RESERVED_STATE_DIM,
)

STATE_DIM_NATIVE = 4623
RECORD_SIZE = 4 + STATE_DIM_NATIVE * 4 + MAX_LEGAL_ACTIONS * ACTION_FEATURE_DIM * 4 + MAX_LEGAL_ACTIONS
HEADER_SIZE = 20

RECORD_DTYPE = np.dtype([
    ('perspective', 'u1'),
    ('num_legal',   'u1'),
    ('chosen_idx',  '<u2'),
    ('state',       '<f4', (STATE_DIM_NATIVE,)),
    ('actions',     '<f4', (MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM)),
    ('mask',        'u1',  (MAX_LEGAL_ACTIONS,)),
])
assert RECORD_DTYPE.itemsize == RECORD_SIZE, \
    f'record dtype size mismatch: {RECORD_DTYPE.itemsize} vs {RECORD_SIZE}'


def read_binary_header(path):
    with open(path, 'rb') as f:
        h = f.read(HEADER_SIZE)
    magic = h[0:4]
    if magic != b'RFBA':
        raise ValueError(f'bad magic {magic!r} in {path}')
    version, state_dim, action_dim, max_actions = struct.unpack('<HHHH', h[4:12])
    num_decisions, = struct.unpack('<I', h[12:16])
    winner = h[16]
    return {
        'version': version,
        'state_dim': state_dim,
        'action_dim': action_dim,
        'max_actions': max_actions,
        'num_decisions': num_decisions,
        'winner': winner,
    }


def load_binary_game(path):
    header = read_binary_header(path)
    if header['state_dim'] != STATE_DIM_NATIVE:
        raise ValueError(f"unexpected state_dim {header['state_dim']} in {path}")
    if header['action_dim'] != ACTION_FEATURE_DIM:
        raise ValueError(f"unexpected action_dim {header['action_dim']} in {path}")
    if header['max_actions'] != MAX_LEGAL_ACTIONS:
        raise ValueError(f"unexpected max_actions {header['max_actions']} in {path}")
    mm = np.memmap(path, dtype=RECORD_DTYPE, mode='r', offset=HEADER_SIZE,
                   shape=(header['num_decisions'],))
    return header, mm


def python_features_for_decision(record):
    perspective = record.get('turn_player', 'P1')
    state_vec = extract_state_features(record, perspective)  # padded to RESERVED_STATE_DIM
    # Trim back to native 354 for comparison with binary.
    state_native = state_vec[:STATE_DIM_NATIVE]

    legal = record.get('legal_actions', []) or []
    legal = legal[:MAX_LEGAL_ACTIONS]
    action_block = np.zeros((MAX_LEGAL_ACTIONS, ACTION_FEATURE_DIM), dtype=np.float32)
    mask = np.zeros(MAX_LEGAL_ACTIONS, dtype=np.uint8)
    for i, act in enumerate(legal):
        af = featurize_single_action(act)
        action_block[i, :len(af)] = af
        mask[i] = 1
    return state_native, action_block, mask, perspective, record.get('chosen_index', 0)


def compare_game(jsonl_path, bin_path, verbose=False):
    decisions, summary = _parse_game_file(jsonl_path)
    header, mm = load_binary_game(bin_path)

    json_decisions = [d for d in decisions
                      if isinstance(d.get('legal_actions', []), list)
                      and len(d.get('legal_actions', [])) > 0]

    if len(json_decisions) != header['num_decisions']:
        return {
            'path': bin_path,
            'ok': False,
            'reason': f"decision count mismatch: jsonl={len(json_decisions)} bin={header['num_decisions']}",
        }

    state_diffs = 0
    action_diffs = 0
    mask_diffs = 0
    chosen_diffs = 0
    perspective_diffs = 0
    first_mismatch = None

    for i, (rec, dec) in enumerate(zip(mm, json_decisions)):
        py_state, py_actions, py_mask, py_perspective, py_chosen = \
            python_features_for_decision(dec)

        cpp_state = rec['state']
        cpp_actions = rec['actions']
        cpp_mask = rec['mask']
        cpp_perspective = int(rec['perspective'])
        cpp_chosen = int(rec['chosen_idx'])

        # Perspective: Python uses 'P1'/'P2', binary uses 0/1
        py_perspective_byte = 0 if py_perspective == 'P1' else 1
        if py_perspective_byte != cpp_perspective:
            perspective_diffs += 1
            if first_mismatch is None:
                first_mismatch = ('perspective', i,
                                   py_perspective_byte, cpp_perspective)

        if not np.array_equal(py_state, cpp_state):
            state_diffs += 1
            if first_mismatch is None:
                idx = np.where(py_state != cpp_state)[0][:5]
                first_mismatch = ('state', i, idx.tolist(),
                                   [(py_state[j], cpp_state[j]) for j in idx])

        if not np.array_equal(py_actions, cpp_actions):
            action_diffs += 1
            if first_mismatch is None:
                pos = np.argwhere(py_actions != cpp_actions)[:5]
                first_mismatch = ('actions', i, pos.tolist(),
                                   [(py_actions[r, c], cpp_actions[r, c])
                                    for r, c in pos])

        if not np.array_equal(py_mask, cpp_mask):
            mask_diffs += 1
            if first_mismatch is None:
                first_mismatch = ('mask', i, py_mask.tolist(), cpp_mask.tolist())

        if py_chosen != cpp_chosen:
            chosen_diffs += 1
            if first_mismatch is None:
                first_mismatch = ('chosen_idx', i, py_chosen, cpp_chosen)

    return {
        'path': bin_path,
        'ok': (state_diffs == 0 and action_diffs == 0 and mask_diffs == 0
               and chosen_diffs == 0 and perspective_diffs == 0),
        'num_decisions': header['num_decisions'],
        'state_diffs': state_diffs,
        'action_diffs': action_diffs,
        'mask_diffs': mask_diffs,
        'chosen_diffs': chosen_diffs,
        'perspective_diffs': perspective_diffs,
        'first_mismatch': first_mismatch,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--deck1', default='decks/rengar_test.json')
    p.add_argument('--deck2', default='decks/rengar_test.json')
    p.add_argument('--games', type=int, default=3)
    p.add_argument('--seed', type=int, default=42)
    p.add_argument('--registry', default='cards/registry.json')
    p.add_argument('--build-dir', default='build')
    p.add_argument('--keep-tmp', action='store_true',
                   help='Do not delete the temp directory after running')
    args = p.parse_args()

    engine = os.path.join(args.build_dir, 'riftbound')
    if not os.path.exists(engine):
        print(f'ERROR: engine binary not found at {engine}. Build first:')
        print('  cmake --build build')
        sys.exit(1)

    tmpdir = tempfile.mkdtemp(prefix='riftbound_parity_')
    try:
        jsonl_out = os.path.join(tmpdir, 'parity.jsonl')
        bin_out = os.path.join(tmpdir, 'parity.bin')

        common = [args.deck1, args.deck2, '-r', args.registry,
                  '--games', str(args.games), '--seed', str(args.seed)]

        print('Running engine → JSONL ...')
        subprocess.run([engine, *common, '-o', jsonl_out], check=True,
                       stdout=subprocess.DEVNULL)
        print('Running engine → binary ...')
        subprocess.run([engine, *common, '-o', bin_out], check=True,
                       stdout=subprocess.DEVNULL)

        all_ok = True
        for game_idx in range(1, args.games + 1):
            jpath = (jsonl_out + f'.game{game_idx}'
                     if args.games > 1 else jsonl_out)
            bpath = (bin_out + f'.game{game_idx}'
                     if args.games > 1 else bin_out)

            r = compare_game(jpath, bpath)
            tag = 'OK ' if r['ok'] else 'FAIL'
            print(f'  [{tag}] game {game_idx}: {r["num_decisions"]} decisions  '
                  f'state_diffs={r["state_diffs"]} '
                  f'action_diffs={r["action_diffs"]} '
                  f'mask_diffs={r["mask_diffs"]} '
                  f'chosen_diffs={r["chosen_diffs"]} '
                  f'perspective_diffs={r["perspective_diffs"]}')
            if not r['ok']:
                all_ok = False
                print(f'    first mismatch: {r["first_mismatch"]}')

        if all_ok:
            print('PARITY OK — Python and C++ feature extractors agree byte-for-byte.')
            sys.exit(0)
        else:
            print('PARITY FAILED — see mismatches above. DO NOT train on binary yet.')
            sys.exit(2)
    finally:
        if args.keep_tmp:
            print(f'Temp files left at {tmpdir}')
        else:
            shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == '__main__':
    main()
