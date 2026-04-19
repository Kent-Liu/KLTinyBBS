#include "KLTinyBBSStrings.h"
#include "config.h"

namespace kltinybbs {
namespace str {

#if (KLTINYBBS_LANG == KLTINYBBS_LANG_ZH_TW)

/* --- zh-TW: short strings to save transmission (file must be UTF-8) --- */
const char* const SendHelpHint       = "輸入 /?";
const char* const LoginFirst         = "先登入: /hi 帳 密";
const char* const LoginFirstToPost   = "先登入再貼";
const char* const UnknownCmd         = "未知指令. /?";

const char* const UsageAtUserMsg    = "@用戶 內容";
const char* const UserNotFound      = "無此用戶";
const char* const EmptyMessage      = "內容為空";
const char* const SendFailed        = "送出失敗";
const char* const OverLengthFmt     = "超約%u%%，上限%ubytes";

const char* const UserPassRule      = "帳密4-8字ASCII";
const char* const PassRule          = "密碼4-8字ASCII";
const char* const BadPassword       = "密碼錯誤";
const char* const BBSFullNoSlot     = "滿人，稍後再試";
const char* const NameExists        = "名稱已存在或無效";
const char* const HiFmt             = "Hi, %s.";
const char* const WelcomeFmt        = "歡迎 %s.";

const char* const Bye               = "Bye.";
const char* const PasswordUpdated   = "密碼已更新";

const char* const MailCountFmt      = "信(%u)\n";
const char* const NoSuchMail        = "無此信";
const char* const UnknownUser       = "?";

const char* const NewsCountFmt      = "新聞(%u)\n";
const char* const NoSuchNews        = "無此則";
const char* const PostFailed        = "張貼失敗";
const char* const DeleteUsage       = "用法:/delete post 編號";
const char* const DeleteForbidden   = "不可刪他人公告";
const char* const DeleteFailed      = "刪除失敗";

const char* const UsersHeader       = "用戶\n";
const char* const UserCountFmt      = "用戶(%u)\n";

/* Avoid trigraph: ??- in source would become ~ */
const char* const InvalidTime       = "\x3F\x3F-\x3F\x3F \x3F\x3F:\x3F\x3F";

const char* const Ok                = "OK";

const char* const PrivateUnlocked     = "已解鎖";
const char* const PrivateIdleWarning  = " 15分閒置再鎖";
const char* const WelcomeTooLongFmt   = "問候語過長，上限%ubytes";
const char* const PrivateStatusLocked   = "Private: 鎖定";
const char* const PrivateStatusUnlocked = "Private: 解鎖";

const char* const ResetUsage   = "用法:/reset 管理密碼";
const char* const ResetFailed   = "重置失敗.";

const char* const HelpGuest =
    "/hi 帳 密:登入或註冊(4-8字ASCII)\n/?:說明";
const char* const HelpLoggedIn =
    "/bye:登出 /pass:改密\n"
    "/m信 /n新聞 /p貼 /delete post編號\n"
    "@用戶私訊 /users列表\n/?:說明";
const char* const HelpAdmin =
    "/admin bye:離開管理\n/admin pass 新密:改管理密\n"
    "/sync:寫入flash\n"
    "/reset 管理密:清檔如首次\n"
    "/delete post 編號:刪公告\n"
    "/welcome 文字:問候\n/private pass|lock|unlock|status\n/?:說明";

#else  /* KLTINYBBS_LANG_EN (default) */

const char* const SendHelpHint       = "Send /? for help.";
const char* const LoginFirst         = "Login first: /hi user pass";
const char* const LoginFirstToPost   = "Login first to post.";
const char* const UnknownCmd         = "Unknown cmd. /? for help.";

const char* const UsageAtUserMsg    = "Usage: @username message";
const char* const UserNotFound      = "User not found.";
const char* const EmptyMessage      = "Empty message.";
const char* const SendFailed        = "Send failed.";
const char* const OverLengthFmt     = "Over by ~%u%%. Max %u bytes.";

const char* const UserPassRule      = "User & pass: 4-8 chars, ASCII only.";
const char* const PassRule          = "Pass: 4-8 chars, ASCII only.";
const char* const BadPassword       = "Bad password.";
const char* const BBSFullNoSlot     = "BBS full. No inactive slot. Try later.";
const char* const NameExists        = "Name exists or invalid.";
const char* const HiFmt             = "Hi, %s.";
const char* const WelcomeFmt        = "Welcome, %s.";

const char* const Bye               = "Bye.";
const char* const PasswordUpdated   = "Password updated.";

const char* const MailCountFmt      = "Mail(%u)\n";
const char* const NoSuchMail        = "No such mail.";
const char* const UnknownUser       = "?";

const char* const NewsCountFmt      = "News(%u)\n";
const char* const NoSuchNews        = "No such news.";
const char* const PostFailed        = "Post failed.";
const char* const DeleteUsage       = "Usage: /delete post N";
const char* const DeleteForbidden   = "Cannot delete others' posts.";
const char* const DeleteFailed      = "Delete failed.";

const char* const UsersHeader       = "Users\n";
const char* const UserCountFmt      = "User(%u)\n";

const char* const InvalidTime       = "\x3F\x3F-\x3F\x3F \x3F\x3F:\x3F\x3F";

const char* const Ok                = "OK";

const char* const PrivateUnlocked     = "Unlocked.";
const char* const PrivateIdleWarning  = " Idle 15min will lock again.";
const char* const WelcomeTooLongFmt  = "Welcome too long. Max %u bytes.";
const char* const PrivateStatusLocked   = "Private: locked.";
const char* const PrivateStatusUnlocked = "Private: unlocked.";

const char* const ResetUsage   = "Usage: /reset admin_pw";
const char* const ResetFailed   = "Reset failed.";

const char* const HelpGuest =
    "/hi user pass: login or register (4-8 chars)\n/?: help";
const char* const HelpLoggedIn =
    "/bye: logout\n"
    "/pass new: change pw\n"
    "/m: list; /m N: read; /m N-: from N\n"
    "@user msg: send PM\n"
    "/n: like /m; /p txt: post news\n"
    "/delete post N: delete own post\n"
    "/users [kw]: list\n"
    "/?: help";
const char* const HelpAdmin =
    "/admin bye: exit admin\n"
    "/admin pass new: change admin pw\n"
    "/sync: flush UserStore+PrefStore to flash\n"
    "/reset admin_pw: wipe all\n"
    "/delete post N: delete any post\n"
    "/welcome txt: greeting\n"
    "/private pass|lock|unlock|status\n/?: help";

#endif  /* KLTINYBBS_LANG */

} // namespace str
} // namespace kltinybbs
