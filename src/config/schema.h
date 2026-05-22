#pragma once
/// @file schema.h
/// JSON Schema document for the unified `riftbound` config file.
///
/// Compiled into the binary as a string constant. The runtime validator
/// uses this as the contract for any --config file the user passes. The
/// binary is the source of truth for what config it accepts — there's
/// no separate schema file to ship or version against.
///
/// Spec: JSON Schema draft 7 (https://json-schema.org/draft-07/schema).
///
/// Two top-level modes:
///   - "play":  run N games of two agents against each other.
///   - "train": run an AlphaZero-style self-play training loop for one legend.
///
/// Path resolution:
///   - If top-level `root_dir` is set, relative paths inside the config
///     resolve against `root_dir`.
///   - If `root_dir` is unset, all paths must be absolute (the validator
///     rejects relative paths). This is enforced post-schema in the parser
///     because JSON Schema can't express "absolute path required when X
///     is unset" cleanly.

namespace riftbound::config {

/// The full JSON Schema document. Inline string literal so it ships with
/// the binary.
extern const char* const kSchemaJson;

}  // namespace riftbound::config
