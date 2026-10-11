// Plan item 5.5: mixes of id kinds that must NOT compile.
//
// Compiled (syntax only) once per case by the `riftbound_strong_id_*` ctests
// (CMakeLists.txt): case 0 must compile (it proves the harness and the
// includes are sound, so a failing case fails for its own line), every other
// case must be rejected. tests/test_strong_ids.cpp asserts what a trait can
// show; these are the cases a trait cannot, plus the ones worth seeing spelled
// as the mistake they are.
#include "core/card_db.h"
#include "core/game_state.h"
#include "core/types.h"

#ifndef STRONG_ID_CASE
#error "define STRONG_ID_CASE"
#endif

namespace riftbound {

inline uint32_t runtimeNumber();
inline void takesCard(CardDefId);

inline void strongIdCase(GameState& state, const CardDB& db, GameObject& obj) {
    (void)state; (void)db; (void)obj;
#if STRONG_ID_CASE == 0
    // The control: what card code does with ids all day.
    CardDefId card = 235;                       // a card named by its number
    takesCard(235);
    card = obj.card_def_id;
    if (card == kInvalidId || card != obj.card_def_id) card = kInvalidId;
    const uint32_t n = card.raw();              // to a number, by name
    card = CardDefId::fromRaw(n);               // and back, by name
    (void)db.get(card);
    obj.location = BattlefieldLocation{1};      // slot 1, spelled with its type
    obj.hidden_at = BattlefieldId::fromRaw(n);
    (void)state.battlefields[obj.hidden_at];
    (void)state.battlefields[0];
#elif STRONG_ID_CASE == 1
    // A number only known at run time is not a card id.
    const uint32_t n = runtimeNumber();
    CardDefId card = n;
    (void)card;
#elif STRONG_ID_CASE == 2
    int slot = static_cast<int>(runtimeNumber());
    takesCard(slot);
#elif STRONG_ID_CASE == 3
    // 0 is not "no card": there is one none, kInvalidId.
    CardDefId card = 0;
    (void)card;
#elif STRONG_ID_CASE == 4
    if (obj.card_def_id == 0) return;
#elif STRONG_ID_CASE == 5
    // A card id is not a number by itself ...
    const uint32_t n = obj.card_def_id;
    (void)n;
#elif STRONG_ID_CASE == 6
    const int n = static_cast<int>(obj.card_def_id);
    (void)n;
#elif STRONG_ID_CASE == 7
    // ... nor a truth value ...
    if (obj.card_def_id) return;
#elif STRONG_ID_CASE == 8
    // ... nor an index.
    std::vector<int> table(2000);
    (void)table[obj.card_def_id];
#elif STRONG_ID_CASE == 9
    // A cast does not make a number an id either: only fromRaw does.
    const auto card = static_cast<CardDefId>(runtimeNumber());
    (void)card;
#elif STRONG_ID_CASE == 10
    // A battlefield id takes no number, literal or not.
    BattlefieldId bf = 0;
    (void)bf;
#elif STRONG_ID_CASE == 11
    obj.hidden_at = 1;
#elif STRONG_ID_CASE == 12
    // A slot number makes a location only as a literal.
    const int slot = static_cast<int>(runtimeNumber());
    obj.location = BattlefieldLocation{slot};
#elif STRONG_ID_CASE == 13
    // A card id is not a battlefield, nor the reverse.
    obj.location = BattlefieldLocation{obj.card_def_id};
#elif STRONG_ID_CASE == 14
    (void)db.get(obj.hidden_at);
#elif STRONG_ID_CASE == 15
    (void)state.battlefields[obj.card_def_id];
#else
#error "unknown STRONG_ID_CASE"
#endif
}

}  // namespace riftbound
