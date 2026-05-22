#pragma once
/// @file app_context.h
/// Application-scoped shared context (manual DI container).
///
/// Loaded once at startup, shared read-only across all game threads.
/// CardDB and CardRegistry are const after initialization.

#include "core/card_db.h"
#include "cards/card_registry.h"
#include "rules/deck_validator.h"

#include <string>

namespace riftbound {

struct AppContext {
    CardDB card_db;
    CardRegistry card_registry;

    void initialize(const std::string& registry_path) {
        card_db.loadFromRegistry(registry_path);
        card_registry.loadAll();
    }
};

} // namespace riftbound
