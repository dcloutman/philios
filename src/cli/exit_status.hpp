#pragma once

/// @file
/// @brief Process exit statuses for interpreter failures (plan 0001 §3.8, D20, D30).
///
/// Values follow BSD `sysexits`. A successful run instead exits with `main`'s `Integer` result
/// modulo 256 (Integer::exitStatus). Collisions between these values and script statuses are
/// accepted; stderr distinguishes them (§3.8).

namespace philios {

/// @brief Usage error: any command-line argument was given (`EX_USAGE`).
inline constexpr int kExitUsage = 64;

/// @brief Lexical, syntax, or declaration error (`EX_DATAERR`).
inline constexpr int kExitDataError = 65;

/// @brief `main.phil` missing, unreadable, not a regular file, or too large (`EX_NOINPUT`).
inline constexpr int kExitNoInput = 66;

/// @brief Runtime error, internal error, out of memory, or any other unexpected exception
///        (`EX_SOFTWARE`, D30).
inline constexpr int kExitSoftware = 70;

} // namespace philios
