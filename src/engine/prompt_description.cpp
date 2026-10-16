#include "engine/prompt_description.h"

#include "engine/knowledge_tracker.h"
#include "openspiel/action_vocab.h"

#include <algorithm>
#include <numeric>

namespace riftbound {

const char* toString(PromptKind kind) {
    switch (kind) {
        case PromptKind::Unspecified:      return "unspecified";
        case PromptKind::BattlefieldSetup: return "battlefield setup";
        case PromptKind::Mulligan:         return "mulligan";
        case PromptKind::MainPhase:        return "main phase action";
        case PromptKind::ChainPriority:    return "priority";
        case PromptKind::ShowdownFocus:    return "focus";
        case PromptKind::NextStaged:       return "next showdown or combat";
        case PromptKind::CombatDamage:     return "combat damage";
        case PromptKind::CombatDamageNext: return "combat damage, next unit";
        case PromptKind::RuneForEnergy:    return "rune for energy";
        case PromptKind::RuneForPower:     return "rune for power";
        case PromptKind::AddForPayment:    return "[Add] for a payment";
        case PromptKind::CostSplit:        return "which half of a cost";
        case PromptKind::Accelerate:       return "accelerate";
        case PromptKind::Repeat:           return "repeat";
        case PromptKind::AdditionalCost:   return "additional cost";
        case PromptKind::DiscardCost:      return "discard as a cost";
        case PromptKind::TriggerOrder:     return "trigger order";
        case PromptKind::ReplacementOrder: return "replacement order";
        case PromptKind::NewChoices:       return "new choices";
        case PromptKind::MoveJoin:         return "move another unit";
        case PromptKind::May:              return "may";
        case PromptKind::Mode:             return "mode";
        case PromptKind::XAmount:          return "X";
        case PromptKind::Target:           return "target";
        case PromptKind::TargetFirst:      return "first target";
        case PromptKind::TargetSecond:     return "second target";
        case PromptKind::ChooseObject:     return "choose";
        case PromptKind::SplitTargets:     return "split damage, targets";
        case PromptKind::SplitAmount:      return "split damage, amount";
        case PromptKind::ChainItem:        return "spell or ability on the chain";
        case PromptKind::Location:         return "location";
        case PromptKind::Discard:          return "discard";
        case PromptKind::LookTop:          return "look";
        case PromptKind::DeckOrder:        return "deck order";
        case PromptKind::Value:            return "value";
        case PromptKind::NameACard:        return "name a card";
        case PromptKind::Count:            break;
    }
    return "?";
}

const char* toString(OptionRole role) {
    switch (role) {
        case OptionRole::Action:    return "action";
        case OptionRole::Yes:       return "yes";
        case OptionRole::No:        return "no";
        case OptionRole::Value:     return "value";
        case OptionRole::Object:    return "object";
        case OptionRole::NamedCard: return "named card";
        case OptionRole::ChainItem: return "chain item";
    }
    return "?";
}

namespace {

int relative(PlayerId p, PlayerId observer) {
    if (p == PlayerId::None) return 0;
    return p == observer ? 1 : 2;
}

const char* zoneWords(ZoneType z) {
    switch (z) {
        case ZoneType::MainDeck:        return "in the Main Deck";
        case ZoneType::RuneDeck:        return "in the Rune Deck";
        case ZoneType::Hand:            return "in hand";
        case ZoneType::Base:            return "at base";
        case ZoneType::Trash:           return "in the trash";
        case ZoneType::Banishment:      return "in banishment";
        case ZoneType::ChampionZone:    return "in the Champion Zone";
        case ZoneType::LegendZone:      return "in the Legend Zone";
        case ZoneType::BattlefieldZone: return "at a battlefield";
        case ZoneType::Chain:           return "on the chain";
        case ZoneType::FacedownZone:    return "face down";
    }
    return "";
}

std::string whose(PlayerId p, PlayerId observer) {
    if (p == PlayerId::None) return "a";
    return p == observer ? "your" : "the opponent's";
}

std::string locationWords(const LocationId& loc, PlayerId observer) {
    if (const auto* b = std::get_if<BaseLocation>(&loc))
        return b->player == observer ? "your base" : "the opponent's base";
    return "battlefield " + toString(std::get<BattlefieldLocation>(loc).id);
}

int chainPosition(const GameState& state, ChainItemId id) {
    if (id == kInvalidId) return -1;
    for (size_t i = 0; i < state.chain.items.size(); ++i)
        if (state.chain.items[i].id == id) return static_cast<int>(i);
    return -1;
}

const ChainItem* findChainItem(const GameState& state, ChainItemId id) {
    for (const auto& ci : state.chain.items)
        if (ci.id == id) return &ci;
    if (state.chain.resuming.has_value() && state.chain.resuming->id == id)
        return &*state.chain.resuming;
    return nullptr;
}

std::string actionWords(const GameState& state, const Intent& i, PlayerId observer) {
    auto obj = [&](GameObjectId id) { return objectInWords(state, id, observer); };
    switch (i.type) {
        case IntentType::EndTurn:      return "end the turn";
        case IntentType::PassPriority: return "pass priority";
        case IntentType::PassFocus:    return "pass focus";
        case IntentType::Concede:      return "concede";
        case IntentType::PlayFirstDecision:
            return i.choose_first ? "go first" : "go second";
        case IntentType::ChooseBattlefield:
            return "bring battlefield " + std::to_string(i.battlefield_pool_index.raw() + 1) +
                   " of your list";
        case IntentType::MulliganDecision: {
            if (i.cards_to_mulligan.empty()) return "keep the hand";
            std::vector<std::string> names;
            for (auto c : i.cards_to_mulligan) names.push_back(obj(c));
            std::sort(names.begin(), names.end());
            std::string out = "set aside";
            for (size_t k = 0; k < names.size(); ++k) out += (k ? " and " : " ") + names[k];
            return out;
        }
        case IntentType::AssignCombatDamage: {
            std::vector<std::string> parts;
            for (const auto& da : i.damage_assignments)
                if (da.damage > 0) parts.push_back(std::to_string(da.damage) + " to " + obj(da.target_unit));
            std::sort(parts.begin(), parts.end());
            std::string out = "assign combat damage:";
            for (size_t k = 0; k < parts.size(); ++k) out += (k ? ", " : " ") + parts[k];
            return out;
        }
        case IntentType::StandardMove: {
            std::string out = "move " + (i.units_to_move.empty() ? std::string("?")
                                                                 : obj(i.units_to_move.front()));
            if (i.move_destination) out += " to " + locationWords(*i.move_destination, observer);
            return out;
        }
        case IntentType::HideCard:
            return "hide " + obj(i.card) + " at battlefield " + toString(i.chosen_battlefield);
        default: break;
    }
    std::string out = toString(i.type);
    if (i.card != kInvalidId) out += " " + obj(i.card);
    if (i.ability_source != kInvalidId) {
        out += " " + obj(i.ability_source);
        out += " ability " + std::to_string(i.ability_index + 1);
    }
    if (i.play_location) out += " to " + locationWords(*i.play_location, observer);
    if (i.use_alt_play_cost) out += " (alternative cost)";
    if (i.pay_location_cost) out += " (paying the battlefield's cost)";
    if (i.play_source != Intent::PlaySource::Hand) {
        static const char* kSource[] = {"hand", "trash", "banishment", "Champion Zone",
                                        "face down", "chain", "Main Deck"};
        out += std::string(" from ") + kSource[static_cast<int>(i.play_source)];
    }
    if (!i.targets.empty()) {
        out += " targeting";
        for (size_t k = 0; k < i.targets.size(); ++k) out += (k ? ", " : " ") + obj(i.targets[k]);
    }
    return out;
}

}  // namespace

namespace {
/// The battlefield slot whose card `object` is, if it is one.
const BattlefieldState* slotOfCard(const GameState& state, GameObjectId object) {
    for (const auto& bf : state.battlefields)
        if (bf.card_object_id == object) return &bf;
    return nullptr;
}
/// Whose an object is, as a player is told: a battlefield is its
/// controller's (CR 190.6; nobody's while uncontrolled), anything on the
/// board its controller's, anything else its owner's (testing phase U16: a
/// battlefield was named by the player who brought it).
PlayerId holderOf(const GameState& state, GameObjectId object) {
    const GameObject& o = state.getObject(object);
    if (const auto* bf = slotOfCard(state, object))
        return bf->controller.value_or(PlayerId::None);
    return o.location.has_value() ? o.controller : o.owner;
}
}  // namespace

std::string objectInWords(const GameState& state, GameObjectId object, PlayerId observer) {
    if (object == kInvalidId || !state.objectExists(object)) return "nothing";
    const GameObject& o = state.getObject(object);
    std::string out = whose(holderOf(state, object), observer) + " ";
    out += knowledge::identityVisible(state, object, observer) ? o.name.get()
                                                                : std::string("card");
    // What tells two objects of one name apart on the board (testing phase
    // U17: a [Temporary] token copy and the card it copies were described
    // alike): a token says so, and an exhausted permanent says so.
    if (o.location.has_value()) {
        if (o.isToken()) out += " token";
        if (o.is_exhausted) out += " (exhausted)";
    }
    if (o.location.has_value()) out += " at " + locationWords(*o.location, observer);
    else if (const auto* bf = slotOfCard(state, object))
        out += " (battlefield " + toString(bf->id) + ")";
    else out += std::string(" ") + zoneWords(o.zone.get());
    return out;
}

PromptDescription describePrompt(const GameState& state, const Prompt& prompt,
                                 const std::vector<Intent>& legal, PlayerId observer,
                                 const CardDB& card_db) {
    (void)card_db;
    PromptDescription d;
    d.kind = prompt.kind;
    d.decider = relative(prompt.decider, observer);
    const bool decider = prompt.decider == observer;

    if (prompt.source != kInvalidId && state.objectExists(prompt.source)) {
        const GameObject& src = state.getObject(prompt.source);
        d.has_source = true;
        d.asker = relative(holderOf(state, prompt.source), observer);
        if (knowledge::identityVisible(state, prompt.source, observer))
            d.source_card = src.card_def_id;
        d.source_zone = src.zone.get();
    }
    if (prompt.is_spell) d.ability = PromptDescription::Ability::Spell;
    else if (prompt.keyword_ability) d.ability = PromptDescription::Ability::Keyword;
    else if (prompt.ability_index >= 0) d.ability = PromptDescription::Ability::Activated;
    else if (prompt.trigger != TriggerType::None || prompt.is_triggered)
        d.ability = PromptDescription::Ability::Triggered;
    else if (prompt.chain_item != kInvalidId) d.ability = PromptDescription::Ability::Permanent;
    d.ability_index = prompt.ability_index;
    d.trigger = prompt.trigger;
    d.at_finalization = prompt.at_finalization;
    d.chain_item = prompt.chain_item != kInvalidId;
    d.chain_position = chainPosition(state, prompt.chain_item);

    // The sentence.
    std::string head = d.decider == 1 ? "You decide" : d.decider == 2 ? "The opponent decides"
                                                                      : "Decision";
    head += std::string(" (") + toString(prompt.kind) + ")";
    if (d.has_source) {
        head += " for " + objectInWords(state, prompt.source, observer);
        static const char* kAbility[] = {"", "spell", "activated ability", "triggered ability",
                                         "keyword ability", "play"};
        if (d.ability != PromptDescription::Ability::None) {
            head += std::string(", its ") + kAbility[static_cast<int>(d.ability)];
            if (d.ability == PromptDescription::Ability::Activated)
                head += " " + std::to_string(d.ability_index + 1);
        }
        if (d.chain_item)
            head += d.at_finalization ? ", as it is put on the chain" : ", as it resolves";
    }
    if (!decider) {
        d.text = head;
        return d;
    }
    d.question = prompt.question;
    if (!d.question.empty()) head += ": " + d.question;
    d.text = head;

    // The options, in action-id order.
    const std::vector<int> ids = openspiel::actionIds(legal, state);
    std::vector<size_t> order(legal.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return ids[a] < ids[b]; });
    const bool any_object = std::any_of(legal.begin(), legal.end(), [](const Intent& i) {
        return !i.chosen_objects.empty() || i.chosen_card_def != kInvalidId;
    });
    const bool any_decline = std::any_of(legal.begin(), legal.end(),
                                         [](const Intent& i) { return i.declines; });
    d.options.reserve(legal.size());
    for (size_t idx : order) {
        const Intent& i = legal[idx];
        OptionDescription o;
        o.index = idx;
        o.action_id = ids[idx];
        o.type = i.type;
        o.label = i.choice_label;
        if (i.type != IntentType::MakeChoice) {
            o.role = OptionRole::Action;
            for (auto id : {i.card, i.ability_source}) {
                if (id == kInvalidId) continue;
                o.cards.push_back(state.objectExists(id) &&
                                          knowledge::identityVisible(state, id, observer)
                                      ? state.getObject(id).card_def_id : CardDefId{kInvalidId});
                o.objects.push_back(openspiel::visibleObjectDescription(state, id, observer));
            }
            o.text = actionWords(state, i, observer);
            d.options.push_back(std::move(o));
            continue;
        }
        if (i.chosen_card_def != kInvalidId) {
            o.role = OptionRole::NamedCard;
            o.named_card = i.chosen_card_def;
            o.text = "name " + card_db.get(i.chosen_card_def).name;
        } else if (!i.chosen_objects.empty() && prompt.kind == PromptKind::ChainItem &&
                   i.chosen_value.has_value()) {
            o.role = OptionRole::ChainItem;
            const auto item_id = ChainItemId::fromRaw(*i.chosen_value);
            o.chain_position = chainPosition(state, item_id);
            const ChainItem* ci = findChainItem(state, item_id);
            o.text = std::string(ci && ci->is_spell ? "the spell " : "the ability of ") +
                     objectInWords(state, i.chosen_objects.front(), observer) +
                     " (chain position " + std::to_string(o.chain_position) + ")";
        } else if (!i.chosen_objects.empty()) {
            o.role = OptionRole::Object;
            o.value = i.chosen_value.value_or(-1);
        } else {
            o.value = i.chosen_value.value_or(-1);
            if (i.declines || (any_object && o.value == 0)) o.role = OptionRole::No;
            else if (any_decline && legal.size() == 2 && o.value == 1) o.role = OptionRole::Yes;
            else o.role = OptionRole::Value;
            o.text = !i.choice_label.empty() ? i.choice_label
                     : o.role == OptionRole::No  ? std::string("no")
                     : o.role == OptionRole::Yes ? std::string("yes")
                                                 : std::to_string(o.value);
        }
        // Several objects named by one answer are a set: listed in the order
        // of what the observer sees of them, not in the answer's (ids).
        std::vector<std::pair<std::vector<int64_t>, GameObjectId>> named;
        for (auto id : i.chosen_objects)
            named.push_back({openspiel::visibleObjectDescription(state, id, observer), id});
        std::stable_sort(named.begin(), named.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        for (auto& [seen, id] : named) {
            o.cards.push_back(state.objectExists(id) && knowledge::identityVisible(state, id, observer)
                                  ? state.getObject(id).card_def_id : CardDefId{kInvalidId});
            o.objects.push_back(std::move(seen));
            if (o.role == OptionRole::Object)
                o.text += (o.text.empty() ? "" : ", ") + objectInWords(state, id, observer);
        }
        if (o.role == OptionRole::ChainItem) o.value = -1;   // an item id is not shown
        d.options.push_back(std::move(o));
    }
    return d;
}

std::string promptKey(const PromptDescription& d) {
    std::string k = std::to_string(static_cast<int>(d.kind));
    k += '|';
    k += std::to_string(d.asker == 0 ? 0 : d.asker == d.decider ? 1 : 2);
    k += '|';
    k += d.has_source ? std::to_string(d.source_card.raw()) : std::string("-");
    k += '|';
    k += std::to_string(static_cast<int>(d.ability));
    k += '|';
    k += std::to_string(d.ability_index);
    k += '|';
    k += std::to_string(static_cast<int>(d.trigger));
    k += '|';
    k += d.question;
    return k;
}

}  // namespace riftbound
