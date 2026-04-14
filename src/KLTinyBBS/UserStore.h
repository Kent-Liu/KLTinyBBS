#pragma once

#include "config.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "FSCommon.h"

#if defined(ARCH_NRF52)
/* Adafruit LittleFS uses uint8_t mode; FSCommon.h does not define these for NRF52. */
#define FILE_O_READ  Adafruit_LittleFS_Namespace::FILE_O_READ
#define FILE_O_WRITE Adafruit_LittleFS_Namespace::FILE_O_WRITE
#else
#ifndef FILE_O_WRITE
#define FILE_O_WRITE "w"
#endif
#ifndef FILE_O_READ
#define FILE_O_READ "r"
#endif
#endif

#ifndef KLTINYBBS_MAX_USERS
// nRF52 default small; you can override via build flags -DKLTINYBBS_MAX_USERS=...
#define KLTINYBBS_MAX_USERS 8
#endif

#ifndef KLTINYBBS_SHA_ITER
#define KLTINYBBS_SHA_ITER 2000
#endif

#ifndef KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC
/** Min interval (seconds) between automatic UserStore flush to flash. Override per chip/board. */
#define KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC 3600
#endif

#ifndef KLTINYBBS_INACTIVE_SECS
/** Seconds of inactivity before a user can be evicted when BBS is full (default 30 days). */
#define KLTINYBBS_INACTIVE_SECS (30U * 24 * 3600)
#endif

namespace kltinybbs {

struct UserInfo {
    uint8_t  userId = 0;
    uint8_t  flags = 0;
    char     name[9] = {0};     // null-terminated copy for UI
    uint32_t last_login = 0;
    bool     used = false;
    bool     deleted = false;
};

class UserStore {
public:
    // Uses global FSCom from FSCommon.h.
    // Assumes fsInit() was called elsewhere, but will call FSBegin() defensively.
    bool begin(const char* dir = "/kltinybbs");

    // Remove A/B snapshot files and reset in-memory state (for testing/reset).
    bool clearStorage();

    size_t userCount() const { return userCountCached_; }
    int    findUserIdByName(const char* name) const; // returns [0..MAX_USERS-1] or -1

    bool addUser(const char* name, const char* password, uint8_t* outUserId);
    bool deleteUser(uint8_t userId); // marks deleted
    bool setPassword(uint8_t userId, const char* newPassword);

    bool verifyLogin(const char* name, const char* password, uint8_t* outUserId);
    bool setLastLogin(uint8_t userId, uint32_t ts); // RAM only + dirty

    bool getUser(uint8_t userId, UserInfo& out) const;

    // Returns userId of a used, non-deleted user with last_login older than (nowTs - inactiveSecs).
    // If multiple qualify, returns the one with smallest last_login. Returns -1 if none.
    int findOldestInactiveUserId(uint32_t nowTs, uint32_t inactiveSecs) const;

    bool isDirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    bool flush(); // writes A/B snapshot if dirty
    bool flushIfNeeded(uint32_t now, uint32_t minIntervalSeconds);

    size_t snapshotSizeBytes() const;

private:
    // Snapshot files: <dir>/usersA.bin and <dir>/usersB.bin
    static constexpr uint16_t kMagic   = 0x4D55; // 'U''M' (keep stable)
    static constexpr uint8_t  kVersion = 1;

    // flags bits
    static constexpr uint8_t kFlagEnabled = 1u << 0;
    static constexpr uint8_t kFlagAdmin   = 1u << 1;
    static constexpr uint8_t kFlagDeleted = 1u << 2;

    struct __attribute__((packed)) SnapshotHeader {
        uint16_t magic;
        uint8_t  version;
        uint8_t  reserved0;
        uint32_t seq;
        uint8_t  user_count;      // non-deleted used users
        uint8_t  reserved1[3];
        uint32_t crc32;           // crc32(header with crc32=0 + records)
    };

    struct __attribute__((packed)) UserRecord {
        uint8_t  used;            // 0 empty, 1 used
        uint8_t  flags;           // includes deleted bit
        char     name[8];         // fixed 8 bytes
        uint32_t last_login;
        uint32_t salt;            // per-user
        uint8_t  pass_hash[16];   // truncated "SHA256"
        uint16_t reserved;
    };

private:
    // Helpers
    void buildPath_(char* out, size_t outSz, const char* leaf) const;

    static void normalizeName_(char out[8], const char* in);
    static bool nameEquals_(const char a[8], const char b[8]);
    static bool constTimeEq16_(const uint8_t a[16], const uint8_t b[16]);

    static uint32_t crc32_update_(uint32_t crc, const uint8_t* data, size_t len);
    static uint32_t crc32_calc_(const uint8_t* data, size_t len);

    // Crypto glue: replace sha256_stub_() with real SHA256 later.
    static void sha256_stub_(const uint8_t* data, size_t len, uint8_t out32[32]);
    static void computePassHash_(const char* password, uint32_t salt, uint8_t out16[16]);

    uint32_t genSalt_();

    bool loadSnapshot_();
    bool loadFile_(const char* path, SnapshotHeader& hdrOut, UserRecord recOut[KLTINYBBS_MAX_USERS]) const;
    bool writeFile_(const char* path, uint32_t seq, const UserRecord recIn[KLTINYBBS_MAX_USERS], uint8_t usedCount);

    void rebuildUserCount_();

private:
    char dir_[48] = {0};

    UserRecord users_[KLTINYBBS_MAX_USERS] = {};
    uint32_t activeSeq_ = 0;
    bool activeIsA_ = true;

    bool dirty_ = false;
    uint32_t lastFlush_ = 0;
    size_t userCountCached_ = 0;
};

} // namespace kltinybbs
