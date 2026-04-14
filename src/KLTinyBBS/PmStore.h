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

#ifndef KLTINYBBS_PM_SLOT_SIZE
#define KLTINYBBS_PM_SLOT_SIZE 128
#endif

#ifndef KLTINYBBS_PM_FILE_SIZE
// 2KB default: 16 slots * 128 bytes
#define KLTINYBBS_PM_FILE_SIZE 2048
#endif

#ifndef KLTINYBBS_PM_RECORD_TEXT_LEN
/** Max payload bytes for message text in one PM/announcement slot. Must match Record layout (slot size minus header). */
#define KLTINYBBS_PM_RECORD_TEXT_LEN 104
#endif

namespace kltinybbs {

class PmStore {
public:
    // Message type you can use (PM / ANN etc.)
    enum MsgType : uint8_t {
        MSG_PM  = 1,
        MSG_ANN = 2,
    };

    struct MsgView {
        bool     valid = false;
        bool     deleted = false;

        uint32_t seq = 0;
        uint32_t ts = 0;
        uint16_t ttl = 0;      // optional, can be 0
        uint8_t  from = 0;
        uint8_t  to = 0;
        uint8_t  type = 0;

        uint8_t  len = 0;
        char     text[1 + KLTINYBBS_PM_RECORD_TEXT_LEN] = {0};
    };

    // Iterate messages for a given user in ascending seq order (oldest->newest)
    // Return false from callback to stop iteration early.
    using ForEachCb = bool (*)(const MsgView& msg, void* userCtx);

    bool begin(const char* dir = "/kltinybbs", const char* filename = "pm.bin");

    // Remove data file and recreate empty (for testing/reset). Clears in-memory state.
    bool clearStorage();

    // Capacity info
    static constexpr size_t slotSize() { return KLTINYBBS_PM_SLOT_SIZE; }
    static constexpr size_t fileSize() { return KLTINYBBS_PM_FILE_SIZE; }
    static constexpr size_t slotCount() { return KLTINYBBS_PM_FILE_SIZE / KLTINYBBS_PM_SLOT_SIZE; }

    // Max payload bytes available for text in one slot (override via KLTINYBBS_PM_RECORD_TEXT_LEN)
    static constexpr size_t maxTextLen() { return (size_t)KLTINYBBS_PM_RECORD_TEXT_LEN; }

    // Add a message (writes one slot to flash). Returns assigned seq in outSeq (optional).
    bool add(uint8_t from, uint8_t to, uint8_t type,
             const char* text, uint32_t ts, uint16_t ttl,
             uint32_t* outSeq = nullptr);

    // Read by slot index (0..slotCount-1)
    bool readSlot(uint16_t slotIndex, MsgView& out) const;

    // Delete by seq (marks deleted flag + rewrites the slot)
    bool markDeletedBySeq(uint32_t seq);

    // Delete by slot index
    bool markDeletedBySlot(uint16_t slotIndex);

    // Mark all messages for an evicted user: PM where to==userId, and ANN where from==userId.
    // Returns count of slots marked deleted.
    int markDeletedForEvictedUser(uint8_t userId);

    // Iterate messages for a user (match to==userId) in seq order.
    // includeDeleted=false: skip deleted; includeExpired=false: skip expired (ttl!=0 and ts+ttl < now)
    bool forEachToUser(uint8_t userId, uint32_t nowTs,
                       bool includeDeleted,
                       bool includeExpired,
                       ForEachCb cb, void* userCtx) const;

    // List slots for toUserId+type (newest first). Fills outSeq/outSlot up to maxOut; *outCount = filled; optional *outTotal = total.
    // Use toUserId=0, type=MSG_ANN for news. includeDeleted/includeExpired control filtering.
    bool listSlotsToUserDesc(uint8_t toUserId, uint8_t typeFilter, uint32_t nowTs,
                             bool includeDeleted, bool includeExpired,
                             uint32_t* outSeq, uint16_t* outSlot, size_t maxOut, size_t* outCount,
                             size_t* outTotal = nullptr) const;

    // Optional: remove old/expired messages (marks deleted)
    // Returns how many were marked deleted.
    int purge(uint32_t nowTs, uint32_t olderThanTs /*0 to ignore*/,
              bool purgeExpired /*true*/,
              bool purgeDeleted /*false: keep deleted, true: re-mark deleted slots (no-op)*/);

    // Debug: returns next seq and next slot to write
    uint32_t nextSeq() const { return nextSeq_; }
    uint16_t nextWriteSlot() const { return (uint16_t)((lastWrittenSlot_ + 1) % slotCount()); }

private:
    static constexpr uint16_t kMagic = 0x4D50; // 'P''M' in little-endian style
    static constexpr uint8_t  kVersion = 1;

    static constexpr uint8_t kFlagDeleted = 1u << 0;

    // On-disk record: fixed 128 bytes
    struct __attribute__((packed)) Record {
        uint16_t magic;     // kMagic
        uint8_t  version;   // kVersion
        uint8_t  flags;     // deleted etc.

        uint32_t seq;       // monotonically increasing
        uint32_t ts;        // timestamp seconds (or your chosen epoch)
        uint16_t ttl;       // seconds/minutes as you decide; treat as seconds here; 0=never expire

        uint8_t  from;
        uint8_t  to;
        uint8_t  type;
        uint8_t  len;       // bytes in text[]
        uint8_t  text[KLTINYBBS_PM_RECORD_TEXT_LEN];
        uint8_t  reserved[2]; // padding so sizeof(Record) == KLTINYBBS_PM_SLOT_SIZE (included in CRC)

        uint32_t crc32;     // crc32 of all bytes up to (but excluding) crc32
    };

    static_assert(sizeof(Record) == KLTINYBBS_PM_SLOT_SIZE, "Record must equal slot size");

private:
    void buildPath_(char* out, size_t outSz) const;

    bool ensureDir_() const;
    bool ensureFile_();

    bool writeSlot_(uint16_t slotIndex, const Record& rec);
    bool readSlotRaw_(uint16_t slotIndex, Record& rec) const;

    bool validate_(const Record& rec) const;
    static uint32_t crc32_update_(uint32_t crc, const uint8_t* data, size_t len);
    static uint32_t crc32_calc_(const uint8_t* data, size_t len);

    void scanForWritePtr_();

private:
    char dir_[48] = {0};
    char filename_[24] = {0};

    uint16_t lastWrittenSlot_ = 0;  // slot index containing max seq (or last valid). next write goes to +1.
    uint32_t nextSeq_ = 1;
};

} // namespace kltinybbs