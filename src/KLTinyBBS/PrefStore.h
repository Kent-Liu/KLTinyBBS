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

#ifndef KLTINYBBS_PREF_FILE_SIZE
#define KLTINYBBS_PREF_FILE_SIZE 256
#endif

#ifndef KLTINYBBS_PREF_WELCOME_MAX
/** Max bytes for welcome greeting (C string, includes null). */
#define KLTINYBBS_PREF_WELCOME_MAX 104
#endif

namespace kltinybbs {

/**
 * PrefStore: 256-byte preference block.
 * - Version 1 byte (0x01)
 * - Admin password SHA256 hash 32 bytes (full hash, scheme A)
 * - Private password SHA256 hash 32 bytes
 * - Private mode 1 byte (0x00 off, 0x01 on)
 * - Welcome message KLTINYBBS_PREF_WELCOME_MAX bytes (C string, null-terminated)
 * - Rest reserved
 * Not written to flash until flush() (e.g. on /admin bye or /sync).
 */
class PrefStore {
public:
    static constexpr uint8_t kVersion = 0x01;
    static constexpr size_t kAdminHashLen = 32;
    static constexpr size_t kPrivateHashLen = 32;
    static constexpr uint8_t kPrivateModeOff = 0x00;
    static constexpr uint8_t kPrivateModeOn  = 0x01;

    bool begin(const char* dir = "/kltinybbs", const char* filename = "pref.bin");

    /** Clear file and reset in-memory to defaults. Returns true on success. */
    bool clearStorage();

    /** Write to flash only if dirty. Returns true on success. */
    bool flush();

    bool isDirty() const { return dirty_; }

    bool verifyAdminPassword(const char* password) const;
    bool verifyPrivatePassword(const char* password) const;
    void setAdminPassword(const char* password);
    void setPrivatePassword(const char* password);

    uint8_t getPrivateMode() const { return privateMode_; }
    void setPrivateMode(uint8_t v) { privateMode_ = v; dirty_ = true; }

    /** Get welcome string (always null-terminated, max PREF_WELCOME_MAX). */
    void getWelcome(char* out, size_t outSz) const;
    /** Set welcome (C string); truncates to PREF_WELCOME_MAX-1 chars. Returns false if input too long. */
    bool setWelcome(const char* text);

private:
    static constexpr uint16_t kMagic = 0x5052; // 'P''R'
    static constexpr size_t kReservedBytes = KLTINYBBS_PREF_FILE_SIZE - 2 - 1 - 1 - kAdminHashLen - kPrivateHashLen - 1 - 2
                                             - (size_t)KLTINYBBS_PREF_WELCOME_MAX;

    struct __attribute__((packed)) Record {
        uint16_t magic;
        uint8_t  version;
        uint8_t  reserved0;
        uint8_t  admin_hash[kAdminHashLen];
        uint8_t  private_hash[kPrivateHashLen];
        uint8_t  private_mode;
        uint8_t  reserved1[2];
        char     welcome[KLTINYBBS_PREF_WELCOME_MAX];
        uint8_t  reserved2[kReservedBytes];
    };
    static_assert(sizeof(Record) == KLTINYBBS_PREF_FILE_SIZE, "Record must equal PREF_FILE_SIZE");

    static void sha256_(const uint8_t* data, size_t len, uint8_t out32[32]);
    static void computePassHash_(const char* password, const char* domain, uint8_t out32[32]);

    void setDefaults_();
    bool load_();
    bool write_() const;

    char dir_[48] = {0};
    char filename_[24] = {0};

    uint8_t admin_hash_[kAdminHashLen] = {0};
    uint8_t private_hash_[kPrivateHashLen] = {0};
    uint8_t privateMode_ = kPrivateModeOff;
    char welcome_[KLTINYBBS_PREF_WELCOME_MAX] = {0};

    bool dirty_ = false;
};

} // namespace kltinybbs
