#pragma once

/**
 * Centralized UI strings for KLTinyBBS.
 * Language at compile time: set KLTINYBBS_LANG in config.h to KLTINYBBS_LANG_EN or KLTINYBBS_LANG_ZH_TW.
 * All strings are UTF-8; reply size limit (e.g. 200 bytes) still applies.
 * Uses extern declarations for C++11 compatibility (no inline constexpr).
 */
namespace kltinybbs {
namespace str {

// --- Prompts / hints ---
extern const char* const SendHelpHint;
extern const char* const LoginFirst;
extern const char* const LoginFirstToPost;
extern const char* const UnknownCmd;

// --- @username (PM) ---
extern const char* const UsageAtUserMsg;
extern const char* const UserNotFound;
extern const char* const EmptyMessage;
extern const char* const SendFailed;
// Over length: "Over by ~%u%%. Max %u bytes." (no send/post)
extern const char* const OverLengthFmt;

// --- /hi (login/register) ---
extern const char* const UserPassRule;
extern const char* const PassRule;
extern const char* const BadPassword;
extern const char* const BBSFullNoSlot;
extern const char* const NameExists;
extern const char* const HiFmt;
extern const char* const WelcomeFmt;

// --- /bye ---
extern const char* const Bye;

// --- /pass ---
extern const char* const PasswordUpdated;

// --- /mail /m ---
extern const char* const MailCountFmt;
extern const char* const NoSuchMail;
extern const char* const UnknownUser;

// --- /news ---
extern const char* const NewsCountFmt;
extern const char* const NoSuchNews;
extern const char* const PostFailed;
extern const char* const DeleteUsage;
extern const char* const DeleteForbidden;
extern const char* const DeleteFailed;

// --- /users ---
extern const char* const UsersHeader;  // legacy, prefer UserCountFmt
extern const char* const UserCountFmt; // "User(%u)\n"

// --- Time (invalid); use \? to avoid trigraph ??- and ?: ---
extern const char* const InvalidTime;

// --- Generic ---
extern const char* const Ok;

// --- /help (context-specific, keep each under 200 bytes) ---
extern const char* const HelpGuest;     // not logged in: login only
extern const char* const HelpLoggedIn;  // logged in: user commands
extern const char* const HelpAdmin;     // admin mode: admin commands only

// --- Admin / private (not in public /help) ---
extern const char* const PrivateUnlocked;       // "Unlocked. Idle 15min will lock again."
extern const char* const PrivateIdleWarning;    // "Idle 15min will lock. Use /private pwd to unlock."
extern const char* const WelcomeTooLongFmt;     // "Welcome too long. Max %u bytes."
extern const char* const PrivateStatusLocked;   // "Private: locked."
extern const char* const PrivateStatusUnlocked; // "Private: unlocked."

// --- /reset (admin + password) ---
extern const char* const ResetUsage;   // missing password token
extern const char* const ResetFailed;  // filesystem clear failed

} // namespace str
} // namespace kltinybbs
