#include "KLTinyBBSModule.h"
#include "MeshService.h"
#include "configuration.h"
#include "main.h"
#include "NodeDB.h"
#include "FSCommon.h"
#include "memGet.h"
#include "gps/RTC.h"
#include "sleep.h"
#include "PowerStatus.h"
#include "modules/KLTinyBBS/config.h"
#include "modules/KLTinyBBS/UserStore.h"
#include "modules/KLTinyBBS/PmStore.h"
#include "modules/KLTinyBBS/PrefStore.h"
#include "modules/KLTinyBBS/KLTinyBBSStrings.h"

#include <assert.h>
#include <cstring>
#include <cstdio>
#include <cctype>

#ifndef KLTINYBBS_MAX_RESPONSE
#define KLTINYBBS_MAX_RESPONSE 200
#endif
#ifndef KLTINYBBS_MAX_LINE
#define KLTINYBBS_MAX_LINE 35
#endif
#ifndef KLTINYBBS_USERNAME_MIN
#define KLTINYBBS_USERNAME_MIN 4
#endif
#ifndef KLTINYBBS_USERNAME_MAX
#define KLTINYBBS_USERNAME_MAX 8
#endif
#ifndef KLTINYBBS_PASSWORD_MIN
#define KLTINYBBS_PASSWORD_MIN 4
#endif
#ifndef KLTINYBBS_PASSWORD_MAX
#define KLTINYBBS_PASSWORD_MAX 8
#endif
#ifndef KLTINYBBS_LIST_MAX
/** Max list slots fetched for mail/news (e.g. 16). */
#define KLTINYBBS_LIST_MAX 16
#endif
#ifndef KLTINYBBS_LIST_PREVIEW_LINES
/** Number of list lines shown in preview (e.g. 5). */
#define KLTINYBBS_LIST_PREVIEW_LINES 5
#endif

const char* const KLTinyBBSModule::MODULE_VERSION = KLTINYBBS_MODULE_VERSION_STRING;

namespace {

constexpr size_t MAX_RESPONSE = KLTINYBBS_MAX_RESPONSE;
constexpr size_t MAX_LINE = KLTINYBBS_MAX_LINE;
constexpr unsigned USERNAME_MIN = KLTINYBBS_USERNAME_MIN;
constexpr unsigned USERNAME_MAX = KLTINYBBS_USERNAME_MAX;
constexpr unsigned PASSWORD_MIN = KLTINYBBS_PASSWORD_MIN;
constexpr unsigned PASSWORD_MAX = KLTINYBBS_PASSWORD_MAX;

kltinybbs::UserStore gUsers;
kltinybbs::PmStore gPm;
kltinybbs::PrefStore gPref;
bool storesInited = false;

static constexpr size_t kMaxSessionSlots = 8;
static uint32_t gAdminNodes[kMaxSessionSlots] = {0};
struct UnlockSlot { uint32_t node = 0; uint32_t expiry = 0; };
static UnlockSlot gUnlockSlots[kMaxSessionSlots];

struct DedupEntry {
    uint32_t from = 0;
    uint32_t id = 0;
};
static_assert(KLTINYBBS_DEDUP_CACHE_SIZE > 0, "KLTINYBBS_DEDUP_CACHE_SIZE must be > 0");
static DedupEntry gRecentPacketKeys[KLTINYBBS_DEDUP_CACHE_SIZE];
static size_t gRecentPacketWritePos = 0;

static bool isNodeInAdminMode(uint32_t node) {
    for (size_t i = 0; i < kMaxSessionSlots; i++)
        if (gAdminNodes[i] == node) return true;
    return false;
}
static void setAdminMode(uint32_t node) {
    for (size_t i = 0; i < kMaxSessionSlots; i++) {
        if (gAdminNodes[i] == 0) { gAdminNodes[i] = node; return; }
        if (gAdminNodes[i] == node) return;
    }
}
static void clearAdminMode(uint32_t node) {
    for (size_t i = 0; i < kMaxSessionSlots; i++)
        if (gAdminNodes[i] == node) gAdminNodes[i] = 0;
}

static void expireUnlockSlots(uint32_t now) {
    for (size_t i = 0; i < kMaxSessionSlots; i++)
        if (gUnlockSlots[i].node != 0 && now > gUnlockSlots[i].expiry)
            gUnlockSlots[i] = UnlockSlot{};
}
static bool isNodeUnlocked(uint32_t node, uint32_t now) {
    for (size_t i = 0; i < kMaxSessionSlots; i++)
        if (gUnlockSlots[i].node == node && now < gUnlockSlots[i].expiry) return true;
    return false;
}
static void setNodeUnlocked(uint32_t node, uint32_t expiry) {
    for (size_t i = 0; i < kMaxSessionSlots; i++) {
        if (gUnlockSlots[i].node == node) { gUnlockSlots[i].expiry = expiry; return; }
        if (gUnlockSlots[i].node == 0) { gUnlockSlots[i].node = node; gUnlockSlots[i].expiry = expiry; return; }
    }
}
static void refreshUnlockExpiry(uint32_t node, uint32_t expiry) {
    for (size_t i = 0; i < kMaxSessionSlots; i++)
        if (gUnlockSlots[i].node == node) { gUnlockSlots[i].expiry = expiry; return; }
}

// Return true if this packet key was seen recently; otherwise record it.
static bool isDuplicatePacket(uint32_t from, uint32_t id) {
    if (id == 0) return false;
    for (size_t i = 0; i < KLTINYBBS_DEDUP_CACHE_SIZE; i++) {
        if (gRecentPacketKeys[i].from == from && gRecentPacketKeys[i].id == id) {
            return true;
        }
    }
    gRecentPacketKeys[gRecentPacketWritePos].from = from;
    gRecentPacketKeys[gRecentPacketWritePos].id = id;
    gRecentPacketWritePos = (gRecentPacketWritePos + 1) % KLTINYBBS_DEDUP_CACHE_SIZE;
    return false;
}

static void ensureStoresInited_() {
    if (storesInited) return;
    if (gUsers.begin("/kltinybbs")) {
        LOG_INFO("KLTinyBBS UserStore ok, %d users", (int)gUsers.userCount());
    } else {
        LOG_ERROR("KLTinyBBS UserStore init failed");
    }
    if (gPm.begin("/kltinybbs", "pm.bin")) {
        LOG_INFO("KLTinyBBS PmStore ok");
    } else {
        LOG_ERROR("KLTinyBBS PmStore init failed");
    }
    if (gPref.begin("/kltinybbs", "pref.bin")) {
        LOG_INFO("KLTinyBBS PrefStore ok");
    } else {
        LOG_ERROR("KLTinyBBS PrefStore init failed");
    }
    storesInited = true;
}

// Session: for each userId, nodeNum that is logged in (0 = none). Lookup by nodeNum to get userId.
uint32_t nodeBoundToUserId[KLTINYBBS_MAX_USERS] = {0};

static int getUserIdByNode(uint32_t nodeNum) {
    for (int i = 0; i < KLTINYBBS_MAX_USERS; i++) {
        if (nodeBoundToUserId[i] == nodeNum) return i;
    }
    return -1;
}

static void bindNodeToUser(uint32_t nodeNum, int userId) {
    for (int i = 0; i < KLTINYBBS_MAX_USERS; i++) {
        if (nodeBoundToUserId[i] == nodeNum) nodeBoundToUserId[i] = 0;
    }
    if (userId >= 0 && userId < KLTINYBBS_MAX_USERS) nodeBoundToUserId[userId] = nodeNum;
}

static void unbindUser(int userId) {
    if (userId >= 0 && userId < KLTINYBBS_MAX_USERS) nodeBoundToUserId[userId] = 0;
}

// Return byte length to truncate UTF-8 so no multi-byte character is cut. At most maxBytes.
// Ref: KLARBot utf8TruncateLen (UTF-8 lead/continuation byte structure only).
static size_t utf8TruncateLen(const unsigned char* buf, size_t len, size_t maxBytes) {
    if (buf == nullptr || maxBytes == 0) return 0;
    if (len > maxBytes) len = maxBytes;
    size_t pos = 0;
    while (pos < len) {
        unsigned char c = buf[pos];
        size_t charLen = 1;
        if ((c & 0x80) != 0) {
            if ((c & 0xE0) == 0xC0) charLen = 2;
            else if ((c & 0xF0) == 0xE0) charLen = 3;
            else if ((c & 0xF8) == 0xF0) charLen = 4;
        }
        if (pos + charLen > maxBytes) break;
        if (pos + charLen > len) break;
        pos += charLen;
    }
    return pos;
}

// Check 4-8 bytes, printable ASCII only (no Unicode)
static bool isPrintableAscii4to8(const char* s, size_t len) {
    if (len < USERNAME_MIN || len > USERNAME_MAX) return false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c > 0x7e) return false;
    }
    return true;
}

// Format UTC epoch seconds as local time "MM-DD HH:MM" using node timezone (same idea as KLARBot).
// Compute TZ offset from RTC without using getTZOffset: localNow - utcNow (only within this module).
static void formatTs(uint32_t ts, char* out, size_t outSz) {
    if (outSz < 12) {
        if (outSz) { out[0] = '\0'; }
        return;
    }
    if (ts == 0) {
        snprintf(out, outSz, "%s", kltinybbs::str::InvalidTime);
        return;
    }
    uint32_t utcNow = getValidTime(RTCQuality::RTCQualityDevice, false);
    uint32_t localNow = getValidTime(RTCQuality::RTCQualityDevice, true);
    int32_t tzOffset = (utcNow != 0 && localNow != 0) ? (int32_t)(localNow - utcNow) : 0;
    int64_t localSec = (int64_t)ts + (int64_t)tzOffset;
    if (localSec <= 0) {
        snprintf(out, outSz, "%s", kltinybbs::str::InvalidTime);
        return;
    }
    uint32_t t = (uint32_t)localSec;
    t /= 60;
    int min = (int)(t % 60);
    t /= 60;
    int hour = (int)(t % 24);
    t /= 24;

    static const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year = 1970;
    while (true) {
        int daysInYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
        if (t >= (uint32_t)daysInYear) {
            t -= daysInYear;
            year++;
        } else
            break;
    }
    int month = 0;
    while (month < 12) {
        int dim = daysInMonth[month];
        if (month == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
            dim++;
        if (t >= (uint32_t)dim) {
            t -= dim;
            month++;
        } else
            break;
    }
    int day = (int)t + 1;
    snprintf(out, outSz, "%02d-%02d %02d:%02d", month + 1, day, hour, min);
}

// Trim leading/trailing spaces from a null-terminated string; returns new length.
static size_t trimStr(char* s) {
    size_t len = strlen(s);
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t')) { s[--len] = '\0'; }
    char* p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) {
        len = strlen(p);
        memmove(s, p, len + 1);
    }
    return len;
}

// Get next token: advance *start past whitespace, then fill token until space or end. Return length.
static size_t nextToken(const char* buf, size_t bufLen, size_t* start, char* token, size_t tokenSz) {
    if (!buf || !start || !token || tokenSz == 0) return 0;
    while (*start < bufLen && (buf[*start] == ' ' || buf[*start] == '\t')) (*start)++;
    size_t beg = *start;
    while (*start < bufLen && buf[*start] != ' ' && buf[*start] != '\t') (*start)++;
    size_t n = *start - beg;
    if (n >= tokenSz) n = tokenSz - 1;
    memcpy(token, buf + beg, n);
    token[n] = '\0';
    return n;
}

// Strip trailing \r \n from a null-terminated string (so "n\n" matches "n").
static void stripTrailingCrLf(char* s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n')) s[--len] = '\0';
}

// Build one list line (index + fromName + ts + summary) with UTF-8 safe truncation. Total line length <= MAX_LINE bytes including newline.
static int formatListLine(char* out, size_t outSz, unsigned idx, const char* fromName, const char* tsBuf,
                          const char* text, size_t textLen) {
    if (!out || outSz < 2) return 0;
    size_t maxLineBytes = (outSz > MAX_LINE + 1) ? (MAX_LINE + 1) : outSz;  // cap at 36 (35 + newline)
    int prefixLen = snprintf(out, maxLineBytes, "%u %s %s ", idx, fromName, tsBuf);
    if (prefixLen < 0 || (size_t)prefixLen >= maxLineBytes) return 0;
    size_t summaryMax = (maxLineBytes - 1) - (size_t)prefixLen;  // -1 for newline
    if (summaryMax == 0) {
        out[prefixLen] = '\n';
        return prefixLen + 1;
    }
    size_t safeLen = utf8TruncateLen((const unsigned char*)text, textLen, summaryMax);
    memcpy(out + prefixLen, text, safeLen);
    prefixLen += (int)safeLen;
    out[prefixLen] = '\n';
    return prefixLen + 1;
}

// Context for hasNewMailSince callback
struct NewMailSinceCtx { uint32_t sinceTs; bool found; };
static bool checkNewMailCb(const kltinybbs::PmStore::MsgView& msg, void* userCtx) {
    NewMailSinceCtx* ctx = (NewMailSinceCtx*)userCtx;
    // Only count private mail (MSG_PM); to==0 is news (MSG_ANN), must not trigger "new mail" icon
    if (msg.type != kltinybbs::PmStore::MSG_PM) return true;
    if (msg.ts > ctx->sinceTs) { ctx->found = true; return false; }
    return true;
}
// Returns true if userId has at least one PM (private mail) with ts > sinceTs.
static bool hasNewMailSince(uint8_t userId, uint32_t sinceTs, uint32_t nowTs) {
    NewMailSinceCtx ctx = { sinceTs, false };
    gPm.forEachToUser(userId, nowTs, false, false, checkNewMailCb, &ctx);
    return ctx.found;
}

// UTF-8 envelope icon (U+2709) for "new mail" indicator
static const char kNewMailSuffix[] = u8" ✉️";

} // namespace

KLTinyBBSModule::KLTinyBBSModule() : SinglePortModule("kltinybbs", meshtastic_PortNum_TEXT_MESSAGE_APP) {}

void KLTinyBBSModule::setup() {
    // Ensure persistent stores are ready and register for system lifecycle events.
    ensureStoresInited_();
    notifyDeepSleepObserver.observe(&notifyDeepSleep);
    notifyRebootObserver.observe(&notifyReboot);
}

int KLTinyBBSModule::onNotifyDeepSleep(void * /*unused*/) {
    // Best-effort: flush pending user and pref data before deep sleep.
    ensureStoresInited_();
    if (!gUsers.flush()) {
        LOG_ERROR("KLTinyBBS UserStore flush failed (deep sleep)");
    }
    if (!gPref.flush()) {
        LOG_ERROR("KLTinyBBS PrefStore flush failed (deep sleep)");
    }
    return 0;
}

int KLTinyBBSModule::onNotifyReboot(void * /*unused*/) {
    // Best-effort: flush pending user and pref data before reboot.
    ensureStoresInited_();
    if (!gUsers.flush()) {
        LOG_ERROR("KLTinyBBS UserStore flush failed (reboot)");
    }
    if (!gPref.flush()) {
        LOG_ERROR("KLTinyBBS PrefStore flush failed (reboot)");
    }
    return 0;
}

meshtastic_MeshPacket* KLTinyBBSModule::allocReply() {
    assert(currentRequest);
    const char* prefix = kltinybbs::str::Ok;
    size_t prefixLen = strlen(prefix);
    auto reply = allocDataPacket();
    reply->to = currentRequest->from;
    reply->decoded.payload.size = prefixLen;
    memcpy(reply->decoded.payload.bytes, prefix, prefixLen);
    return reply;
}

bool KLTinyBBSModule::wantPacket(const meshtastic_MeshPacket* p) {
    return p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP && p->to == nodeDB->getNodeNum();
}

void KLTinyBBSModule::sendReply(uint32_t toNode, const char* text, size_t len) {
    if (len > MAX_RESPONSE) len = MAX_RESPONSE;
    auto reply = allocDataPacket();
    reply->to = toNode;
    reply->decoded.payload.size = (uint32_t)len;
    reply->want_ack = true; // Request ACK so ReliableRouter retries until ack
    memcpy(reply->decoded.payload.bytes, text, len);
    service->sendToMesh(reply);
}

void KLTinyBBSModule::sendReplyStr(uint32_t toNode, const char* text) {
    size_t len = strlen(text);
    sendReply(toNode, text, len);
}

ProcessMessage KLTinyBBSModule::handleReceived(const meshtastic_MeshPacket& mp) {
    ensureStoresInited_();

    const uint32_t fromNode = getFrom(&mp);
    if (isDuplicatePacket(fromNode, mp.id)) {
        return ProcessMessage::CONTINUE;
    }

    const size_t payloadLen = mp.decoded.payload.size;
    const uint8_t* payload = mp.decoded.payload.bytes;

    char buf[256];
    size_t copyLen = payloadLen < sizeof(buf) - 1 ? payloadLen : sizeof(buf) - 1;
    memcpy(buf, payload, copyLen);
    buf[copyLen] = '\0';
    trimStr(buf);
    size_t bufLen = strlen(buf);

    // Lines for KLARBot "[AB]..." protocol: ignore to avoid cross-module handling.
    if (bufLen >= 4 && memcmp(buf, "[AB]", 4) == 0) {
        return ProcessMessage::CONTINUE;
    }

    if (bufLen == 0) {
        sendReplyStr(fromNode, kltinybbs::str::SendHelpHint);
        return ProcessMessage::CONTINUE;
    }

    uint32_t now = getTime();

    // --- Private mode gate: if on and node not unlocked, only accept /private <password> ---
    if (gPref.getPrivateMode() == kltinybbs::PrefStore::kPrivateModeOn) {
        expireUnlockSlots(now);
        if (!isNodeUnlocked(fromNode, now)) {
            if (buf[0] != '/') return ProcessMessage::CONTINUE;
            size_t pos = 1;
            char cmd[32] = {0};
            nextToken(buf, bufLen + 1, &pos, cmd, sizeof(cmd));
            stripTrailingCrLf(cmd);
            if (strcmp(cmd, "private") != 0) return ProcessMessage::CONTINUE;
            char pwd[32] = {0};
            nextToken(buf, bufLen + 1, &pos, pwd, sizeof(pwd));
            stripTrailingCrLf(pwd);
            if (pwd[0] == '\0') return ProcessMessage::CONTINUE;
            if (!gPref.verifyPrivatePassword(pwd)) return ProcessMessage::CONTINUE;
            setNodeUnlocked(fromNode, now + KLTINYBBS_PRIVATE_IDLE_SEC);
            char out[MAX_RESPONSE];
            int n = snprintf(out, sizeof(out), "%s%s", kltinybbs::str::PrivateUnlocked, kltinybbs::str::PrivateIdleWarning);
            sendReply(fromNode, out, (size_t)n);
            return ProcessMessage::CONTINUE;
        }
        refreshUnlockExpiry(fromNode, now + KLTINYBBS_PRIVATE_IDLE_SEC);
    }

    // --- @username message (send PM) ---
    if (buf[0] == '@') {
        int me = getUserIdByNode(fromNode);
        if (me < 0) {
            sendReplyStr(fromNode, kltinybbs::str::LoginFirst);
            return ProcessMessage::CONTINUE;
        }
        size_t pos = 1;
        char uname[16] = {0};
        nextToken(buf, bufLen + 1, &pos, uname, sizeof(uname));
        if (uname[0] == '\0') {
            sendReplyStr(fromNode, kltinybbs::str::UsageAtUserMsg);
            return ProcessMessage::CONTINUE;
        }
        int toId = gUsers.findUserIdByName(uname);
        if (toId < 0) {
            sendReplyStr(fromNode, kltinybbs::str::UserNotFound);
            return ProcessMessage::CONTINUE;
        }
        const char* body = buf + pos;
        while (*body == ' ' || *body == '\t') body++;
        if (*body == '\0') {
            sendReplyStr(fromNode, kltinybbs::str::EmptyMessage);
            return ProcessMessage::CONTINUE;
        }
        size_t bodyLen = strlen(body);
        size_t maxLen = kltinybbs::PmStore::maxTextLen();
        if (bodyLen > maxLen) {
            unsigned pct = (unsigned)((bodyLen - maxLen) * 100u / maxLen);
            char overMsg[80];
            int n = snprintf(overMsg, sizeof(overMsg), kltinybbs::str::OverLengthFmt, pct, (unsigned)maxLen);
            sendReply(fromNode, overMsg, (size_t)n);
            return ProcessMessage::CONTINUE;
        }
        if (!gPm.add((uint8_t)me, (uint8_t)toId, kltinybbs::PmStore::MSG_PM, body, now, 0, nullptr)) {
            sendReplyStr(fromNode, kltinybbs::str::SendFailed);
            return ProcessMessage::CONTINUE;
        }
        sendReplyStr(fromNode, kltinybbs::str::Ok);
        return ProcessMessage::CONTINUE;
    }

    // --- Commands (must start with /) ---
    if (buf[0] != '/') {
        sendReplyStr(fromNode, kltinybbs::str::SendHelpHint);
        return ProcessMessage::CONTINUE;
    }

    // /sysinfo (no login required; same format as KLARBot)
    if (bufLen == 8 && memcmp(buf, "/sysinfo", 8) == 0) {
        char buffer[MAX_RESPONSE];
        int len = 0;
#if defined(FSCom) && defined(ARCH_ESP32)
        float fsUsedKB = FSCom.usedBytes() / 1024.0f;
        float fsTotalKB = FSCom.totalBytes() / 1024.0f;
        float fsFreeKB = fsTotalKB - fsUsedKB;
        float fsPercent = fsFreeKB / fsTotalKB * 100.0f;
        len += snprintf(buffer + len, (size_t)(sizeof(buffer) - len), "FS: (%.2f/%.2f KB, %.2f%% free)\n",
                        fsFreeKB, fsTotalKB, fsPercent);
#endif
        float memFreeKB = memGet.getFreeHeap() / 1024.0f;
        float memTotalKB = memGet.getHeapSize() / 1024.0f;
        float memPercent = memFreeKB / memTotalKB * 100.0f;
        len += snprintf(buffer + len, (size_t)(sizeof(buffer) - len), "MEM: (%.2f/%.2f KB, %.2f%% free)",
                        memFreeKB, memTotalKB, memPercent);
#if defined(ARCH_NRF52)
#if defined(EXTERNAL_FLASH_USE_QSPI)
#if defined(EXTERNAL_FLASH_DEVICES)
#define KLTINYBBS_QSPI_DEV_IMPL(x) #x
#define KLTINYBBS_QSPI_DEV(x) KLTINYBBS_QSPI_DEV_IMPL(x)
        len += snprintf(buffer + len, (size_t)(sizeof(buffer) - len), "\nQSPI: Y, %s",
                         KLTINYBBS_QSPI_DEV(EXTERNAL_FLASH_DEVICES));
#undef KLTINYBBS_QSPI_DEV
#undef KLTINYBBS_QSPI_DEV_IMPL
#else
        len += snprintf(buffer + len, (size_t)(sizeof(buffer) - len), "\nQSPI: Y");
#endif
#else
        len += snprintf(buffer + len, (size_t)(sizeof(buffer) - len), "\nQSPI: N");
#endif
#endif
        if (powerStatus && powerStatus->getHasBattery() && powerStatus->getBatteryVoltageMv() > 0) {
            int batMv = powerStatus->getBatteryVoltageMv();
            int batPct = powerStatus->getBatteryChargePercent();
            len += snprintf(buffer + len, (size_t)(sizeof(buffer) - len), "\nBAT: %d%%, %.2fV",
                            batPct, batMv / 1000.0f);
        }
        if (len > (int)MAX_RESPONSE) len = (int)MAX_RESPONSE;
        sendReply(fromNode, buffer, (size_t)len);
        return ProcessMessage::CONTINUE;
    }

    size_t cmdStart = 1;
    char cmd[32] = {0};
    nextToken(buf, bufLen + 1, &cmdStart, cmd, sizeof(cmd));
    stripTrailingCrLf(cmd);

    /*
     * /clean messages | /clean users | /clean pref
     * Temporarily disabled by request. Keep the implementation commented out
     * so it can be restored later if needed.
     */
    // if (strcmp(cmd, "clean") == 0) {
    //     char sub[32] = {0};
    //     nextToken(buf, bufLen + 1, &cmdStart, sub, sizeof(sub));
    //     stripTrailingCrLf(sub);
    //     if (strcmp(sub, "messages") == 0) {
    //         if (gPm.clearStorage()) {
    //             sendReplyStr(fromNode, kltinybbs::str::Ok);
    //         } else {
    //             sendReplyStr(fromNode, "Clean failed.");
    //         }
    //         return ProcessMessage::CONTINUE;
    //     }
    //     if (strcmp(sub, "users") == 0) {
    //         for (int i = 0; i < KLTINYBBS_MAX_USERS; i++) nodeBoundToUserId[i] = 0;
    //         if (gUsers.clearStorage()) {
    //             sendReplyStr(fromNode, kltinybbs::str::Ok);
    //         } else {
    //             sendReplyStr(fromNode, "Clean failed.");
    //         }
    //         return ProcessMessage::CONTINUE;
    //     }
    //     if (strcmp(sub, "pref") == 0) {
    //         if (gPref.clearStorage()) {
    //             sendReplyStr(fromNode, kltinybbs::str::Ok);
    //         } else {
    //             sendReplyStr(fromNode, "Clean failed.");
    //         }
    //         return ProcessMessage::CONTINUE;
    //     }
    //     sendReplyStr(fromNode, "Usage: /clean messages|users|pref");
    //     return ProcessMessage::CONTINUE;
    // }

    // /sync (admin only, hidden): force UserStore and PrefStore dirty data to flash
    if (strcmp(cmd, "sync") == 0) {
        if (!isNodeInAdminMode(fromNode)) {
            sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
            return ProcessMessage::CONTINUE;
        }
        bool okU = gUsers.flush();
        bool okP = gPref.flush();
        if (okU && okP) {
            sendReplyStr(fromNode, "Synced.");
        } else {
            sendReplyStr(fromNode, "Sync failed.");
        }
        return ProcessMessage::CONTINUE;
    }

    // /reset <admin password> (admin only): wipe PmStore, UserStore, PrefStore and all in-RAM session state
    if (strcmp(cmd, "reset") == 0) {
        if (!isNodeInAdminMode(fromNode)) {
            sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
            return ProcessMessage::CONTINUE;
        }
        char adminPass[32] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, adminPass, sizeof(adminPass));
        stripTrailingCrLf(adminPass);
        if (adminPass[0] == '\0') {
            sendReplyStr(fromNode, kltinybbs::str::ResetUsage);
            return ProcessMessage::CONTINUE;
        }
        if (!gPref.verifyAdminPassword(adminPass)) {
            sendReplyStr(fromNode, kltinybbs::str::BadPassword);
            return ProcessMessage::CONTINUE;
        }
        bool okPm = gPm.clearStorage();
        bool okUsers = gUsers.clearStorage();
        bool okPref = gPref.clearStorage();
        if (!okPm || !okUsers || !okPref) {
            sendReplyStr(fromNode, kltinybbs::str::ResetFailed);
            return ProcessMessage::CONTINUE;
        }
        for (int i = 0; i < KLTINYBBS_MAX_USERS; i++) nodeBoundToUserId[i] = 0;
        for (size_t i = 0; i < kMaxSessionSlots; i++) gAdminNodes[i] = 0;
        for (size_t i = 0; i < kMaxSessionSlots; i++) gUnlockSlots[i] = UnlockSlot{};
        sendReplyStr(fromNode, kltinybbs::str::Ok);
        return ProcessMessage::CONTINUE;
    }

    // /admin (hidden): enter admin, exit admin, or change admin password
    if (strcmp(cmd, "admin") == 0) {
        char sub[32] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, sub, sizeof(sub));
        stripTrailingCrLf(sub);
        if (sub[0] == '\0') {
            sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
            return ProcessMessage::CONTINUE;
        }
        if (strcmp(sub, "bye") == 0) {
            if (isNodeInAdminMode(fromNode)) {
                clearAdminMode(fromNode);
                if (gPref.isDirty()) gPref.flush();
                sendReplyStr(fromNode, kltinybbs::str::Ok);
            } else {
                sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
            }
            return ProcessMessage::CONTINUE;
        }
        if (strcmp(sub, "pass") == 0) {
            if (!isNodeInAdminMode(fromNode)) {
                sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
                return ProcessMessage::CONTINUE;
            }
            char newPass[32] = {0};
            nextToken(buf, bufLen + 1, &cmdStart, newPass, sizeof(newPass));
            stripTrailingCrLf(newPass);
            size_t pl = strlen(newPass);
            if (pl < PASSWORD_MIN || pl > PASSWORD_MAX) {
                sendReplyStr(fromNode, kltinybbs::str::PassRule);
                return ProcessMessage::CONTINUE;
            }
            for (size_t i = 0; i < pl; i++) {
                unsigned char c = (unsigned char)newPass[i];
                if (c < 0x20 || c > 0x7e) {
                    sendReplyStr(fromNode, kltinybbs::str::PassRule);
                    return ProcessMessage::CONTINUE;
                }
            }
            gPref.setAdminPassword(newPass);
            sendReplyStr(fromNode, kltinybbs::str::Ok);
            return ProcessMessage::CONTINUE;
        }
        if (gPref.verifyAdminPassword(sub)) {
            setAdminMode(fromNode);
            sendReplyStr(fromNode, kltinybbs::str::Ok);
        } else {
            sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
        }
        return ProcessMessage::CONTINUE;
    }

    // /welcome (admin only, hidden): set welcome greeting
    if (strcmp(cmd, "welcome") == 0) {
        if (!isNodeInAdminMode(fromNode)) {
            sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
            return ProcessMessage::CONTINUE;
        }
        const char* rest = buf + cmdStart;
        while (*rest == ' ' || *rest == '\t' || *rest == '\r' || *rest == '\n') rest++;
        size_t len = strnlen(rest, (size_t)KLTINYBBS_PREF_WELCOME_MAX);
        if (len >= (size_t)KLTINYBBS_PREF_WELCOME_MAX) {
            char msg[64];
            snprintf(msg, sizeof(msg), kltinybbs::str::WelcomeTooLongFmt, (unsigned)(KLTINYBBS_PREF_WELCOME_MAX - 1));
            sendReplyStr(fromNode, msg);
            return ProcessMessage::CONTINUE;
        }
        gPref.setWelcome(rest);
        sendReplyStr(fromNode, kltinybbs::str::Ok);
        return ProcessMessage::CONTINUE;
    }

    // /private (admin only for pass/lock/unlock; hidden)
    if (strcmp(cmd, "private") == 0) {
        char sub[32] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, sub, sizeof(sub));
        stripTrailingCrLf(sub);
        if (!isNodeInAdminMode(fromNode)) {
            sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
            return ProcessMessage::CONTINUE;
        }
        if (strcmp(sub, "pass") == 0) {
            char newPass[32] = {0};
            nextToken(buf, bufLen + 1, &cmdStart, newPass, sizeof(newPass));
            stripTrailingCrLf(newPass);
            size_t pl = strlen(newPass);
            if (pl < PASSWORD_MIN || pl > PASSWORD_MAX) {
                sendReplyStr(fromNode, kltinybbs::str::PassRule);
                return ProcessMessage::CONTINUE;
            }
            for (size_t i = 0; i < pl; i++) {
                unsigned char c = (unsigned char)newPass[i];
                if (c < 0x20 || c > 0x7e) {
                    sendReplyStr(fromNode, kltinybbs::str::PassRule);
                    return ProcessMessage::CONTINUE;
                }
            }
            gPref.setPrivatePassword(newPass);
            sendReplyStr(fromNode, kltinybbs::str::Ok);
            return ProcessMessage::CONTINUE;
        }
        if (strcmp(sub, "lock") == 0) {
            gPref.setPrivateMode(kltinybbs::PrefStore::kPrivateModeOn);
            sendReplyStr(fromNode, kltinybbs::str::Ok);
            return ProcessMessage::CONTINUE;
        }
        if (strcmp(sub, "unlock") == 0) {
            gPref.setPrivateMode(kltinybbs::PrefStore::kPrivateModeOff);
            sendReplyStr(fromNode, kltinybbs::str::Ok);
            return ProcessMessage::CONTINUE;
        }
        if (strcmp(sub, "status") == 0) {
            if (gPref.getPrivateMode() == kltinybbs::PrefStore::kPrivateModeOn) {
                sendReplyStr(fromNode, kltinybbs::str::PrivateStatusLocked);
            } else {
                sendReplyStr(fromNode, kltinybbs::str::PrivateStatusUnlocked);
            }
            return ProcessMessage::CONTINUE;
        }
        sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
        return ProcessMessage::CONTINUE;
    }

    // /hi username password
    if (strcmp(cmd, "hi") == 0) {
        char uname[32] = {0}, pass[32] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, uname, sizeof(uname));
        nextToken(buf, bufLen + 1, &cmdStart, pass, sizeof(pass));
        size_t ul = strlen(uname), pl = strlen(pass);
        if (!isPrintableAscii4to8(uname, ul) || !isPrintableAscii4to8(pass, pl)) {
            sendReplyStr(fromNode, kltinybbs::str::UserPassRule);
            return ProcessMessage::CONTINUE;
        }
        int uid = gUsers.findUserIdByName(uname);
        if (uid >= 0) {
            uint8_t outId;
            if (!gUsers.verifyLogin(uname, pass, &outId)) {
                sendReplyStr(fromNode, kltinybbs::str::BadPassword);
                return ProcessMessage::CONTINUE;
            }
            kltinybbs::UserInfo ui;
            uint32_t lastLoginBefore = 0;
            if (gUsers.getUser((uint8_t)uid, ui)) lastLoginBefore = ui.last_login;
            gUsers.setLastLogin((uint8_t)uid, now);
            bindNodeToUser(fromNode, uid);
            char reply[MAX_RESPONSE + 1];
            int n = snprintf(reply, sizeof(reply), "KLTinyBBS v%s, Welcome, %s.", KLTinyBBSModule::MODULE_VERSION, uname);
            if (hasNewMailSince((uint8_t)uid, lastLoginBefore, now)) {
                size_t sufLen = sizeof(kNewMailSuffix) - 1;
                if (n + (int)sufLen <= (int)sizeof(reply)) {
                    memcpy(reply + n, kNewMailSuffix, sufLen);
                    n += (int)sufLen;
                }
            }
            char welcomeBuf[KLTINYBBS_PREF_WELCOME_MAX];
            gPref.getWelcome(welcomeBuf, sizeof(welcomeBuf));
            if (welcomeBuf[0] != '\0') {
                size_t rem = (size_t)((int)sizeof(reply) - 1 - n);
                if (rem > 1) {
                    reply[n++] = ' ';
                    rem--;
                    size_t wLen = strlen(welcomeBuf);
                    size_t copyLen = utf8TruncateLen((const unsigned char*)welcomeBuf, wLen, rem);
                    if (copyLen > 0) {
                        memcpy(reply + n, welcomeBuf, copyLen);
                        n += (int)copyLen;
                    }
                }
            }
            if (n > (int)MAX_RESPONSE) n = (int)MAX_RESPONSE;
            sendReply(fromNode, reply, (size_t)n);
            return ProcessMessage::CONTINUE;
        }
        // New user
        if (gUsers.userCount() >= KLTINYBBS_MAX_USERS) {
            int oldId = gUsers.findOldestInactiveUserId(now, KLTINYBBS_INACTIVE_SECS);
            if (oldId < 0) {
                sendReplyStr(fromNode, kltinybbs::str::BBSFullNoSlot);
                return ProcessMessage::CONTINUE;
            }
            gPm.markDeletedForEvictedUser((uint8_t)oldId);
            gUsers.deleteUser((uint8_t)oldId);
            unbindUser(oldId);
        }
        uint8_t newId;
        if (!gUsers.addUser(uname, pass, &newId)) {
            sendReplyStr(fromNode, kltinybbs::str::NameExists);
            return ProcessMessage::CONTINUE;
        }
        gUsers.setLastLogin(newId, now);
        bindNodeToUser(fromNode, (int)newId);
        char reply[MAX_RESPONSE + 1];
        int n = snprintf(reply, sizeof(reply), "KLTinyBBS v%s, Welcome, %s.", KLTinyBBSModule::MODULE_VERSION, uname);
        if (hasNewMailSince(newId, 0, now)) {
            size_t sufLen = sizeof(kNewMailSuffix) - 1;
            if (n + (int)sufLen <= (int)sizeof(reply)) {
                memcpy(reply + n, kNewMailSuffix, sufLen);
                n += (int)sufLen;
            }
        }
        char welcomeBuf[KLTINYBBS_PREF_WELCOME_MAX];
        gPref.getWelcome(welcomeBuf, sizeof(welcomeBuf));
        if (welcomeBuf[0] != '\0') {
            size_t rem = (size_t)((int)sizeof(reply) - 1 - n);
            if (rem > 1) {
                reply[n++] = ' ';
                rem--;
                size_t wLen = strlen(welcomeBuf);
                size_t copyLen = utf8TruncateLen((const unsigned char*)welcomeBuf, wLen, rem);
                if (copyLen > 0) {
                    memcpy(reply + n, welcomeBuf, copyLen);
                    n += (int)copyLen;
                }
            }
        }
        if (n > (int)MAX_RESPONSE) n = (int)MAX_RESPONSE;
        sendReply(fromNode, reply, (size_t)n);
        gUsers.flushIfNeeded(now, KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC);
        return ProcessMessage::CONTINUE;
    }

    // /bye
    if (strcmp(cmd, "bye") == 0) {
        int uid = getUserIdByNode(fromNode);
        if (uid >= 0) unbindUser(uid);
        sendReplyStr(fromNode, kltinybbs::str::Bye);
        return ProcessMessage::CONTINUE;
    }

    // /pass newpassword
    if (strcmp(cmd, "pass") == 0) {
        int uid = getUserIdByNode(fromNode);
        if (uid < 0) {
            sendReplyStr(fromNode, kltinybbs::str::LoginFirst);
            return ProcessMessage::CONTINUE;
        }
        char pass[32] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, pass, sizeof(pass));
        size_t pl = strlen(pass);
        if (!isPrintableAscii4to8(pass, pl)) {
            sendReplyStr(fromNode, kltinybbs::str::PassRule);
            return ProcessMessage::CONTINUE;
        }
        gUsers.setPassword((uint8_t)uid, pass);
        sendReplyStr(fromNode, kltinybbs::str::PasswordUpdated);
        gUsers.flushIfNeeded(now, KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC);
        return ProcessMessage::CONTINUE;
    }

    // /mail or /m [N] or [N-]
    if (strcmp(cmd, "mail") == 0 || strcmp(cmd, "m") == 0) {
        int uid = getUserIdByNode(fromNode);
        if (uid < 0) {
            sendReplyStr(fromNode, kltinybbs::str::LoginFirst);
            return ProcessMessage::CONTINUE;
        }
        uint32_t seqs[KLTINYBBS_LIST_MAX];
        uint16_t slots[KLTINYBBS_LIST_MAX];
        size_t count = 0, total = 0;
        gPm.listSlotsToUserDesc((uint8_t)uid, kltinybbs::PmStore::MSG_PM, now, false, false,
                                seqs, slots, KLTINYBBS_LIST_MAX, &count, &total);

        char arg[32] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, arg, sizeof(arg));
        if (arg[0] == '\0') {
            // List total + latest 5 (UTF-8 safe summary)
            char out[MAX_RESPONSE];
            int n = snprintf(out, sizeof(out), kltinybbs::str::MailCountFmt, (unsigned)total);
            size_t lines = count > (size_t)KLTINYBBS_LIST_PREVIEW_LINES ? (size_t)KLTINYBBS_LIST_PREVIEW_LINES : count;
            kltinybbs::UserInfo fromInfo;
            for (size_t i = 0; i < lines && n < (int)MAX_RESPONSE - (int)MAX_LINE - 2; i++) {
                kltinybbs::PmStore::MsgView mv;
                if (!gPm.readSlot(slots[i], mv)) continue;
                char tsBuf[16];
                formatTs(mv.ts, tsBuf, sizeof(tsBuf));
                const char* fromName = kltinybbs::str::UnknownUser;
                if (gUsers.getUser(mv.from, fromInfo)) fromName = fromInfo.name;
                char line[MAX_LINE + 4];
                int ln = formatListLine(line, sizeof(line), (unsigned)(i + 1), fromName, tsBuf, mv.text, (size_t)mv.len);
                if (n + ln <= (int)MAX_RESPONSE) {
                    memcpy(out + n, line, (size_t)ln);
                    n += ln;
                }
            }
            while (n > 0 && out[n - 1] == '\n') n--;
            sendReply(fromNode, out, (size_t)n);
            return ProcessMessage::CONTINUE;
        }
        bool fromN = false;
        unsigned readN = 0;
        if (strlen(arg) >= 2 && arg[strlen(arg) - 1] == '-') {
            fromN = true;
            arg[strlen(arg) - 1] = '\0';
            readN = (unsigned)atoi(arg);
        } else {
            readN = (unsigned)atoi(arg);
        }
        if (fromN && readN >= 1) {
            // List from index readN (1-based), UTF-8 safe summary
            char out[MAX_RESPONSE];
            int n = snprintf(out, sizeof(out), kltinybbs::str::MailCountFmt, (unsigned)total);
            size_t startIdx = (size_t)(readN - 1);
            if (startIdx >= count) {
                while (n > 0 && out[n - 1] == '\n') n--;
                sendReply(fromNode, out, (size_t)n);
                return ProcessMessage::CONTINUE;
            }
            kltinybbs::UserInfo fromInfo;
            for (size_t i = startIdx; i < count && n < (int)MAX_RESPONSE - (int)MAX_LINE - 2; i++) {
                kltinybbs::PmStore::MsgView mv;
                if (!gPm.readSlot(slots[i], mv)) continue;
                char tsBuf[16];
                formatTs(mv.ts, tsBuf, sizeof(tsBuf));
                const char* fromName = kltinybbs::str::UnknownUser;
                if (gUsers.getUser(mv.from, fromInfo)) fromName = fromInfo.name;
                char line[MAX_LINE + 4];
                int ln = formatListLine(line, sizeof(line), (unsigned)(i + 1), fromName, tsBuf, mv.text, (size_t)mv.len);
                if (n + ln <= (int)MAX_RESPONSE) {
                    memcpy(out + n, line, (size_t)ln);
                    n += ln;
                }
            }
            while (n > 0 && out[n - 1] == '\n') n--;
            sendReply(fromNode, out, (size_t)n);
            return ProcessMessage::CONTINUE;
        }
        if (readN >= 1 && readN <= count) {
            kltinybbs::PmStore::MsgView mv;
            if (gPm.readSlot(slots[readN - 1], mv)) {
                char out[MAX_RESPONSE];
                kltinybbs::UserInfo fromInfo;
                const char* fromName = kltinybbs::str::UnknownUser;
                if (gUsers.getUser(mv.from, fromInfo)) fromName = fromInfo.name;
                char tsBuf[16];
                formatTs(mv.ts, tsBuf, sizeof(tsBuf));
                int hdr = snprintf(out, sizeof(out), "[%u] %s %s\n", (unsigned)readN, fromName, tsBuf);
                size_t bodyMax = (hdr < (int)MAX_RESPONSE - 1) ? (MAX_RESPONSE - 1 - (size_t)hdr) : 0;
                if (bodyMax > 0 && mv.len > 0) {
                    size_t copyLen = utf8TruncateLen((const unsigned char*)mv.text, (size_t)mv.len, bodyMax);
                    memcpy(out + hdr, mv.text, copyLen);
                    hdr += (int)copyLen;
                }
                if (hdr > (int)MAX_RESPONSE) hdr = MAX_RESPONSE;
                sendReply(fromNode, out, (size_t)hdr);
            } else {
                sendReplyStr(fromNode, kltinybbs::str::NoSuchMail);
            }
            return ProcessMessage::CONTINUE;
        }
        sendReplyStr(fromNode, kltinybbs::str::NoSuchMail);
        return ProcessMessage::CONTINUE;
    }

    // /news or /n: list / read N / list from N- (login required to view)
    if (strcmp(cmd, "news") == 0 || strcmp(cmd, "n") == 0) {
        if (getUserIdByNode(fromNode) < 0) {
            sendReplyStr(fromNode, kltinybbs::str::LoginFirst);
            return ProcessMessage::CONTINUE;
        }
        uint32_t seqs[KLTINYBBS_LIST_MAX];
        uint16_t slots[KLTINYBBS_LIST_MAX];
        size_t count = 0, total = 0;
        gPm.listSlotsToUserDesc(0, kltinybbs::PmStore::MSG_ANN, now, false, false,
                                seqs, slots, KLTINYBBS_LIST_MAX, &count, &total);

        char arg[32] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, arg, sizeof(arg));
        if (arg[0] == '\0') {
            // List total + latest 5 (UTF-8 safe summary)
            char out[MAX_RESPONSE];
            int n = snprintf(out, sizeof(out), kltinybbs::str::NewsCountFmt, (unsigned)total);
            size_t lines = count > (size_t)KLTINYBBS_LIST_PREVIEW_LINES ? (size_t)KLTINYBBS_LIST_PREVIEW_LINES : count;
            kltinybbs::UserInfo fromInfo;
            for (size_t i = 0; i < lines && n < (int)MAX_RESPONSE - (int)MAX_LINE - 2; i++) {
                kltinybbs::PmStore::MsgView mv;
                if (!gPm.readSlot(slots[i], mv)) continue;
                char tsBuf[16];
                formatTs(mv.ts, tsBuf, sizeof(tsBuf));
                const char* fromName = kltinybbs::str::UnknownUser;
                if (gUsers.getUser(mv.from, fromInfo)) fromName = fromInfo.name;
                char line[MAX_LINE + 4];
                int ln = formatListLine(line, sizeof(line), (unsigned)(i + 1), fromName, tsBuf, mv.text, (size_t)mv.len);
                if (n + ln <= (int)MAX_RESPONSE) {
                    memcpy(out + n, line, (size_t)ln);
                    n += ln;
                }
            }
            while (n > 0 && out[n - 1] == '\n') n--;
            sendReply(fromNode, out, (size_t)n);
            return ProcessMessage::CONTINUE;
        }
        bool fromN = false;
        unsigned readN = 0;
        if (strlen(arg) >= 2 && arg[strlen(arg) - 1] == '-') {
            fromN = true;
            arg[strlen(arg) - 1] = '\0';
            readN = (unsigned)atoi(arg);
        } else {
            readN = (unsigned)atoi(arg);
        }
        if (fromN && readN >= 1) {
            char out[MAX_RESPONSE];
            int n = snprintf(out, sizeof(out), kltinybbs::str::NewsCountFmt, (unsigned)total);
            size_t startIdx = (size_t)(readN - 1);
            if (startIdx >= count) {
                while (n > 0 && out[n - 1] == '\n') n--;
                sendReply(fromNode, out, (size_t)n);
                return ProcessMessage::CONTINUE;
            }
            kltinybbs::UserInfo fromInfo;
            for (size_t i = startIdx; i < count && n < (int)MAX_RESPONSE - (int)MAX_LINE - 2; i++) {
                kltinybbs::PmStore::MsgView mv;
                if (!gPm.readSlot(slots[i], mv)) continue;
                char tsBuf[16];
                formatTs(mv.ts, tsBuf, sizeof(tsBuf));
                const char* fromName = kltinybbs::str::UnknownUser;
                if (gUsers.getUser(mv.from, fromInfo)) fromName = fromInfo.name;
                char line[MAX_LINE + 4];
                int ln = formatListLine(line, sizeof(line), (unsigned)(i + 1), fromName, tsBuf, mv.text, (size_t)mv.len);
                if (n + ln <= (int)MAX_RESPONSE) {
                    memcpy(out + n, line, (size_t)ln);
                    n += ln;
                }
            }
            while (n > 0 && out[n - 1] == '\n') n--;
            sendReply(fromNode, out, (size_t)n);
            return ProcessMessage::CONTINUE;
        }
        if (readN >= 1 && readN <= count) {
            kltinybbs::PmStore::MsgView mv;
            if (gPm.readSlot(slots[readN - 1], mv)) {
                char out[MAX_RESPONSE];
                kltinybbs::UserInfo fromInfo;
                const char* fromName = kltinybbs::str::UnknownUser;
                if (gUsers.getUser(mv.from, fromInfo)) fromName = fromInfo.name;
                char tsBuf[16];
                formatTs(mv.ts, tsBuf, sizeof(tsBuf));
                int hdr = snprintf(out, sizeof(out), "[%u] %s %s\n", (unsigned)readN, fromName, tsBuf);
                size_t bodyMax = (hdr < (int)MAX_RESPONSE - 1) ? (MAX_RESPONSE - 1 - (size_t)hdr) : 0;
                if (bodyMax > 0 && mv.len > 0) {
                    size_t copyLen = utf8TruncateLen((const unsigned char*)mv.text, (size_t)mv.len, bodyMax);
                    memcpy(out + hdr, mv.text, copyLen);
                    hdr += (int)copyLen;
                }
                if (hdr > (int)MAX_RESPONSE) hdr = MAX_RESPONSE;
                sendReply(fromNode, out, (size_t)hdr);
            } else {
                sendReplyStr(fromNode, kltinybbs::str::NoSuchNews);
            }
            return ProcessMessage::CONTINUE;
        }
        sendReplyStr(fromNode, kltinybbs::str::NoSuchNews);
        return ProcessMessage::CONTINUE;
    }

    // /post or /p txt: post new news (login required)
    if (strcmp(cmd, "post") == 0 || strcmp(cmd, "p") == 0) {
        int uid = getUserIdByNode(fromNode);
        if (uid < 0) {
            sendReplyStr(fromNode, kltinybbs::str::LoginFirstToPost);
            return ProcessMessage::CONTINUE;
        }
        const char* rest = buf + cmdStart;
        while (*rest == ' ' || *rest == '\t' || *rest == '\n' || *rest == '\r') rest++;
        if (*rest == '\0') {
            sendReplyStr(fromNode, kltinybbs::str::EmptyMessage);
            return ProcessMessage::CONTINUE;
        }
        size_t restLen = strlen(rest);
        size_t maxLen = kltinybbs::PmStore::maxTextLen();
        if (restLen > maxLen) {
            unsigned pct = (unsigned)((restLen - maxLen) * 100u / maxLen);
            char overMsg[80];
            int n = snprintf(overMsg, sizeof(overMsg), kltinybbs::str::OverLengthFmt, pct, (unsigned)maxLen);
            sendReply(fromNode, overMsg, (size_t)n);
            return ProcessMessage::CONTINUE;
        }
        if (!gPm.add((uint8_t)uid, 0, kltinybbs::PmStore::MSG_ANN, rest, now, 0, nullptr)) {
            sendReplyStr(fromNode, kltinybbs::str::PostFailed);
            return ProcessMessage::CONTINUE;
        }
        sendReplyStr(fromNode, kltinybbs::str::Ok);
        return ProcessMessage::CONTINUE;
    }

    // /delete post N: delete a news post by list index (N is 1-based, newest first)
    if (strcmp(cmd, "delete") == 0) {
        char target[16] = {0};
        char indexToken[16] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, target, sizeof(target));
        nextToken(buf, bufLen + 1, &cmdStart, indexToken, sizeof(indexToken));
        stripTrailingCrLf(target);
        stripTrailingCrLf(indexToken);

        if (strcmp(target, "post") != 0 || indexToken[0] == '\0') {
            sendReplyStr(fromNode, kltinybbs::str::DeleteUsage);
            return ProcessMessage::CONTINUE;
        }

        const unsigned readN = (unsigned)atoi(indexToken);
        if (readN < 1) {
            sendReplyStr(fromNode, kltinybbs::str::DeleteUsage);
            return ProcessMessage::CONTINUE;
        }

        const bool isAdmin = isNodeInAdminMode(fromNode);
        const int uid = getUserIdByNode(fromNode);
        if (!isAdmin && uid < 0) {
            sendReplyStr(fromNode, kltinybbs::str::LoginFirst);
            return ProcessMessage::CONTINUE;
        }

        uint32_t seqs[KLTINYBBS_LIST_MAX];
        uint16_t slots[KLTINYBBS_LIST_MAX];
        size_t count = 0, total = 0;
        gPm.listSlotsToUserDesc(0, kltinybbs::PmStore::MSG_ANN, now, false, false,
                                seqs, slots, KLTINYBBS_LIST_MAX, &count, &total);
        if (readN > count) {
            sendReplyStr(fromNode, kltinybbs::str::NoSuchNews);
            return ProcessMessage::CONTINUE;
        }

        kltinybbs::PmStore::MsgView mv;
        if (!gPm.readSlot(slots[readN - 1], mv)) {
            sendReplyStr(fromNode, kltinybbs::str::NoSuchNews);
            return ProcessMessage::CONTINUE;
        }

        if (!isAdmin && mv.from != (uint8_t)uid) {
            sendReplyStr(fromNode, kltinybbs::str::DeleteForbidden);
            return ProcessMessage::CONTINUE;
        }

        if (!gPm.markDeletedBySlot(slots[readN - 1])) {
            sendReplyStr(fromNode, kltinybbs::str::DeleteFailed);
            return ProcessMessage::CONTINUE;
        }
        sendReplyStr(fromNode, kltinybbs::str::Ok);
        return ProcessMessage::CONTINUE;
    }

    // /users [keyword]
    if (strcmp(cmd, "users") == 0) {
        int uid = getUserIdByNode(fromNode);
        if (uid < 0) {
            sendReplyStr(fromNode, kltinybbs::str::LoginFirst);
            return ProcessMessage::CONTINUE;
        }
        char keyword[32] = {0};
        nextToken(buf, bufLen + 1, &cmdStart, keyword, sizeof(keyword));
        // Count matching users first for "User(N)" header
        size_t userCount = 0;
        for (size_t i = 0; i < KLTINYBBS_MAX_USERS; i++) {
            kltinybbs::UserInfo ui;
            if (!gUsers.getUser((uint8_t)i, ui) || !ui.used || ui.deleted) continue;
            if (keyword[0] != '\0') {
                size_t ul = strlen(ui.name);
                size_t kl = strlen(keyword);
                bool match = false;
                for (size_t ki = 0; ki + kl <= ul; ki++) {
                    bool m = true;
                    for (size_t kk = 0; kk < kl; kk++) {
                        if (tolower((unsigned char)ui.name[ki + kk]) != tolower((unsigned char)keyword[kk])) {
                            m = false;
                            break;
                        }
                    }
                    if (m) { match = true; break; }
                }
                if (!match) continue;
            }
            userCount++;
        }
        char out[MAX_RESPONSE];
        int n = snprintf(out, sizeof(out), kltinybbs::str::UserCountFmt, (unsigned)userCount);
        for (size_t i = 0; i < KLTINYBBS_MAX_USERS && n < (int)MAX_RESPONSE - (int)MAX_LINE - 2; i++) {
            kltinybbs::UserInfo ui;
            if (!gUsers.getUser((uint8_t)i, ui) || !ui.used || ui.deleted) continue;
            if (keyword[0] != '\0') {
                size_t ul = strlen(ui.name);
                size_t kl = strlen(keyword);
                bool match = false;
                for (size_t ki = 0; ki + kl <= ul; ki++) {
                    bool m = true;
                    for (size_t kk = 0; kk < kl; kk++) {
                        if (tolower((unsigned char)ui.name[ki + kk]) != tolower((unsigned char)keyword[kk])) {
                            m = false;
                            break;
                        }
                    }
                    if (m) { match = true; break; }
                }
                if (!match) continue;
            }
            char tsBuf[16];
            formatTs(ui.last_login, tsBuf, sizeof(tsBuf));
            int ln = snprintf(out + n, sizeof(out) - (size_t)n, "%s %s\n", ui.name, tsBuf);
            if (ln > (int)MAX_LINE) ln = MAX_LINE;
            if (ln > 0) n += ln;
        }
        while (n > 0 && out[n - 1] == '\n') n--;
        sendReply(fromNode, out, (size_t)n);
        return ProcessMessage::CONTINUE;
    }

    // /help, /h, or /? (context-specific: guest / logged-in / admin)
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        const char* helpText = kltinybbs::str::HelpGuest;
        if (isNodeInAdminMode(fromNode)) {
            helpText = kltinybbs::str::HelpAdmin;
        } else if (getUserIdByNode(fromNode) >= 0) {
            helpText = kltinybbs::str::HelpLoggedIn;
        }
        size_t len = strlen(helpText);
        if (len > MAX_RESPONSE) len = MAX_RESPONSE;
        sendReply(fromNode, helpText, len);
        return ProcessMessage::CONTINUE;
    }

    sendReplyStr(fromNode, kltinybbs::str::UnknownCmd);
    return ProcessMessage::CONTINUE;
}

bool KLTinyBBSModule::handleCommand(const meshtastic_MeshPacket& mp) {
    (void)mp;
    return false;
}
