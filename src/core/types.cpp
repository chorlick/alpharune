#include "types.h"

namespace riftbound {

const char* toString(CardType t) {
    switch (t) {
        case CardType::Unit:        return "Unit";
        case CardType::Spell:       return "Spell";
        case CardType::Gear:        return "Gear";
        case CardType::Rune:        return "Rune";
        case CardType::Battlefield: return "Battlefield";
        case CardType::Legend:      return "Legend";
    }
    return "Unknown";
}

const char* toString(SuperType t) {
    switch (t) {
        case SuperType::None:      return "None";
        case SuperType::Champion:  return "Champion";
        case SuperType::Signature: return "Signature";
        case SuperType::Token:     return "Token";
    }
    return "Unknown";
}

const char* toString(Domain d) {
    switch (d) {
        case Domain::Fury:  return "Fury";
        case Domain::Calm:  return "Calm";
        case Domain::Mind:  return "Mind";
        case Domain::Body:  return "Body";
        case Domain::Chaos: return "Chaos";
        case Domain::Order: return "Order";
        default:            return "Unknown";
    }
}

const char* toString(TurnPhase p) {
    switch (p) {
        case TurnPhase::Setup:          return "Setup";
        case TurnPhase::Mulligan:       return "Mulligan";
        case TurnPhase::AwakenPhase:    return "AwakenPhase";
        case TurnPhase::BeginningStep:  return "BeginningStep";
        case TurnPhase::ScoringStep:    return "ScoringStep";
        case TurnPhase::ChannelPhase:   return "ChannelPhase";
        case TurnPhase::DrawPhase:      return "DrawPhase";
        case TurnPhase::MainPhase:      return "MainPhase";
        case TurnPhase::EndingStep:     return "EndingStep";
        case TurnPhase::ExpirationStep: return "ExpirationStep";
        case TurnPhase::GameOver:       return "GameOver";
    }
    return "Unknown";
}

const char* toString(PlayerId p) {
    switch (p) {
        case PlayerId::None:    return "None";
        case PlayerId::Player1: return "P1";
        case PlayerId::Player2: return "P2";
    }
    return "Unknown";
}

const char* toString(IntentType t) {
    switch (t) {
        case IntentType::PlayCard:                return "PlayCard";
        case IntentType::StandardMove:            return "StandardMove";
        case IntentType::ActivateAbility:         return "ActivateAbility";
        case IntentType::HideCard:                return "HideCard";
        case IntentType::EndTurn:                 return "EndTurn";
        case IntentType::PlayReaction:            return "PlayReaction";
        case IntentType::ActivateReactionAbility: return "ActivateReactionAbility";
        case IntentType::PassPriority:            return "PassPriority";
        case IntentType::PlayActionCard:          return "PlayActionCard";
        case IntentType::ActivateActionAbility:   return "ActivateActionAbility";
        case IntentType::PassFocus:               return "PassFocus";
        case IntentType::AssignCombatDamage:      return "AssignCombatDamage";
        case IntentType::MulliganDecision:        return "MulliganDecision";
        case IntentType::ChooseBattlefield:       return "ChooseBattlefield";
        case IntentType::MakeChoice:              return "MakeChoice";
        case IntentType::SideboardSwap:           return "SideboardSwap";
        case IntentType::PlayFirstDecision:       return "PlayFirstDecision";
        case IntentType::PlaceOptionalTrigger:    return "PlaceOptionalTrigger";
        case IntentType::DeclineOptionalTrigger:  return "DeclineOptionalTrigger";
        case IntentType::PayTriggeredCost:        return "PayTriggeredCost";
        case IntentType::DeclineTriggeredCost:    return "DeclineTriggeredCost";
        case IntentType::Concede:                 return "Concede";
    }
    return "Unknown";
}

const char* toString(CombatDesignation d) {
    switch (d) {
        case CombatDesignation::None:     return "None";
        case CombatDesignation::Attacker: return "Attacker";
        case CombatDesignation::Defender: return "Defender";
    }
    return "Unknown";
}

const char* toString(ScoreMethod m) {
    switch (m) {
        case ScoreMethod::Conquer: return "Conquer";
        case ScoreMethod::Hold:    return "Hold";
        case ScoreMethod::Direct:  return "Direct";
    }
    return "Unknown";
}

const char* toString(Keyword k) {
    switch (k) {
        case Keyword::Accelerate:   return "Accelerate";
        case Keyword::Action:       return "Action";
        case Keyword::Ambush:       return "Ambush";
        case Keyword::Assault:      return "Assault";
        case Keyword::Backline:     return "Backline";
        case Keyword::Deathknell:   return "Deathknell";
        case Keyword::Deflect:      return "Deflect";
        case Keyword::Equip:        return "Equip";
        case Keyword::Ganking:      return "Ganking";
        case Keyword::Hidden:       return "Hidden";
        case Keyword::Hunt:         return "Hunt";
        case Keyword::Legion:       return "Legion";
        case Keyword::Level:        return "Level";
        case Keyword::Predict:      return "Predict";
        case Keyword::QuickDraw:    return "Quick-Draw";
        case Keyword::Reaction:     return "Reaction";
        case Keyword::Repeat:       return "Repeat";
        case Keyword::Shield:       return "Shield";
        case Keyword::Tank:         return "Tank";
        case Keyword::Temporary:    return "Temporary";
        case Keyword::Unique:       return "Unique";
        case Keyword::Vision:       return "Vision";
        case Keyword::Weaponmaster: return "Weaponmaster";
        case Keyword::Count:        return "Count";
    }
    return "Unknown";
}

} // namespace riftbound
