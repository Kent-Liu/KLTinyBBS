#pragma once

/**
 * KLTinyBBS central config: tunable constants in one place.
 * UserStore and PmStore limits are defined per ARCH_ESP32 / ARCH_NRF52 below.
 * Module, PrefStore, and language defaults follow; override with -D... at build time if needed.
 *
 * Chip detection: Meshtastic sets ARCH_ESP32 or ARCH_NRF52 via platform headers.
 */

/* =============================================================================
 * UserStore + PmStore — platform-specific (ESP32 vs nRF52)
 * ============================================================================= */

#if defined(ARCH_ESP32)

/* --- UserStore (ESP32): accounts, password hashing, flash persistence --- */

#ifndef KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC
/** Minimum seconds between automatic UserStore flush to flash (balances durability vs wear). */
#define KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC 1800
#endif

#ifndef KLTINYBBS_MAX_USERS
/** Maximum number of BBS user records kept in RAM / persisted. */
#define KLTINYBBS_MAX_USERS 32
#endif

#ifndef KLTINYBBS_SHA_ITER
/** Iteration count for password stretching (higher = slower login, harder offline guess). */
#define KLTINYBBS_SHA_ITER 2000
#endif

#ifndef KLTINYBBS_INACTIVE_SECS
/** Seconds without activity before a user may be evicted when the user table is full. */
#define KLTINYBBS_INACTIVE_SECS (30U * 24 * 3600)
#endif

/* --- PmStore (ESP32): private message / announcement backing store --- */

#ifndef KLTINYBBS_PM_SLOT_SIZE
/** Byte size of one PM file slot (must fit one serialized record including headers). */
#define KLTINYBBS_PM_SLOT_SIZE 128
#endif

#ifndef KLTINYBBS_PM_FILE_SIZE
/** Total PM LittleFS file size in bytes (typically slot_size * number_of_slots). */
#define KLTINYBBS_PM_FILE_SIZE 16384
#endif

#ifndef KLTINYBBS_PM_RECORD_TEXT_LEN
/** Max text payload bytes in one PM or announcement (must match on-disk record layout). */
#define KLTINYBBS_PM_RECORD_TEXT_LEN 104
#endif

#elif defined(ARCH_NRF52)

/* --- UserStore (nRF52): accounts, password hashing, flash persistence --- */

#ifndef KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC
/** Minimum seconds between automatic UserStore flush to flash (balances durability vs wear). */
#define KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC 3600
#endif

#ifndef KLTINYBBS_MAX_USERS
/** Maximum number of BBS user records kept in RAM / persisted. */
#define KLTINYBBS_MAX_USERS 8
#endif

#ifndef KLTINYBBS_SHA_ITER
/** Iteration count for password stretching (higher = slower login, harder offline guess). */
#define KLTINYBBS_SHA_ITER 2000
#endif

#ifndef KLTINYBBS_INACTIVE_SECS
/** Seconds without activity before a user may be evicted when the user table is full. */
#define KLTINYBBS_INACTIVE_SECS (30U * 24 * 3600)
#endif

/* --- PmStore (nRF52): private message / announcement backing store --- */

#ifndef KLTINYBBS_PM_SLOT_SIZE
/** Byte size of one PM file slot (must fit one serialized record including headers). */
#define KLTINYBBS_PM_SLOT_SIZE 128
#endif

#ifndef KLTINYBBS_PM_FILE_SIZE
/** Total PM LittleFS file size in bytes (typically slot_size * number_of_slots). */
#define KLTINYBBS_PM_FILE_SIZE 2048
#endif

#ifndef KLTINYBBS_PM_RECORD_TEXT_LEN
/** Max text payload bytes in one PM or announcement (must match on-disk record layout). */
#define KLTINYBBS_PM_RECORD_TEXT_LEN 104
#endif

#else

/* Fallback when neither ARCH is defined (e.g. host tools): same numeric defaults as before split. */

#ifndef KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC
#define KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC 3600
#endif
#ifndef KLTINYBBS_MAX_USERS
#define KLTINYBBS_MAX_USERS 8
#endif
#ifndef KLTINYBBS_SHA_ITER
#define KLTINYBBS_SHA_ITER 2000
#endif
#ifndef KLTINYBBS_INACTIVE_SECS
#define KLTINYBBS_INACTIVE_SECS (30U * 24 * 3600)
#endif
#ifndef KLTINYBBS_PM_SLOT_SIZE
#define KLTINYBBS_PM_SLOT_SIZE 128
#endif
#ifndef KLTINYBBS_PM_FILE_SIZE
#define KLTINYBBS_PM_FILE_SIZE 2048
#endif
#ifndef KLTINYBBS_PM_RECORD_TEXT_LEN
#define KLTINYBBS_PM_RECORD_TEXT_LEN 104
#endif

#endif /* ARCH_ESP32 / ARCH_NRF52 / else */

/* =============================================================================
 * KLTinyBBSModule (KLTinyBBSModule.cpp)
 * ============================================================================= */

#ifndef KLTINYBBS_MODULE_VERSION_STRING
/** Human-readable module version string sent in banners / diagnostics. */
#define KLTINYBBS_MODULE_VERSION_STRING "0.9.0"
#endif

#ifndef KLTINYBBS_MAX_RESPONSE
/** Max bytes for a single outbound text response buffer (UART / packet chunking). */
#define KLTINYBBS_MAX_RESPONSE 200
#endif

#ifndef KLTINYBBS_MAX_LINE
/** Max logical input line length for command parsing (chars). */
#define KLTINYBBS_MAX_LINE 35
#endif

#ifndef KLTINYBBS_USERNAME_MIN
/** Minimum allowed username length for registration / login. */
#define KLTINYBBS_USERNAME_MIN 4
#endif

#ifndef KLTINYBBS_USERNAME_MAX
/** Maximum allowed username length (fits UI and stored record fields). */
#define KLTINYBBS_USERNAME_MAX 8
#endif

#ifndef KLTINYBBS_PASSWORD_MIN
/** Minimum allowed password length. */
#define KLTINYBBS_PASSWORD_MIN 4
#endif

#ifndef KLTINYBBS_PASSWORD_MAX
/** Maximum allowed password length (matches hashing buffer sizing). */
#define KLTINYBBS_PASSWORD_MAX 8
#endif

#ifndef KLTINYBBS_LIST_MAX
/** Max items returned in one list command (boards, messages, etc.). */
#define KLTINYBBS_LIST_MAX 16
#endif

#ifndef KLTINYBBS_LIST_PREVIEW_LINES
/** Number of preview lines shown per list entry where applicable. */
#define KLTINYBBS_LIST_PREVIEW_LINES 5
#endif

/* =============================================================================
 * PrefStore / private mode
 * ============================================================================= */

#ifndef KLTINYBBS_PREF_FILE_SIZE
/** LittleFS file size reserved for user preference blob (private mode flag, etc.). */
#define KLTINYBBS_PREF_FILE_SIZE 256
#endif

#ifndef KLTINYBBS_PRIVATE_IDLE_SEC
/** Seconds of inactivity before private-mode unlock expires (default 15 minutes). */
#define KLTINYBBS_PRIVATE_IDLE_SEC 900
#endif

/* =============================================================================
 * UI language (KLTinyBBSStrings.cpp)
 * ============================================================================= */

/** Language id: English strings. */
#define KLTINYBBS_LANG_EN 0
/** Language id: Traditional Chinese (Taiwan) strings. */
#define KLTINYBBS_LANG_ZH_TW 1

#ifndef KLTINYBBS_LANG
/** Active UI language; pick KLTINYBBS_LANG_EN or KLTINYBBS_LANG_ZH_TW. */
#define KLTINYBBS_LANG KLTINYBBS_LANG_ZH_TW
#endif
