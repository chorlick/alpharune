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

    /// Card data — including ban status — is owned by the registered Card
    /// classes. registry.json and ban-list.csv are retired.
    void initialize() {
        card_registry.loadAll();
        card_db.buildFromClasses(card_registry);
    }
};

} // namespace riftbound
