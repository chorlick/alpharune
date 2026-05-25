#pragma once
/// @file play_index_html.h
/// Builds the embedded HTML/CSS/JS payload served at GET / by
/// riftbound_play. Implemented in play_index_html.cpp.
///
/// The function templates a single-page UI with:
///   • CARD_IMAGES JS const populated from the card gallery JSON.
///   • Header banner showing engine version, build date, and game seed.
///   • Embedded WS bootstrap + state renderer + clickable legal actions
///     + god-mode side panel + clickable card-name → image panel.
///
/// Inputs are baked into the page once at server startup; subsequent
/// HTTP / GETs return the same precomputed string.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace riftbound {

std::string buildPlayIndexHtml(
    const std::map<std::string, std::string>& card_images,
    std::string_view version_tag,
    std::string_view build_date,
    uint64_t seed,
    bool god_mode_enabled);

} // namespace riftbound
