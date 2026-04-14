#include "PmStore.h"

namespace kltinybbs {

static inline void safe_strncpy_(char* dst, const char* src, size_t n) {
    if (!dst || n == 0) return;
    if (!src) { dst[0] = 0; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = 0;
}

// CRC32 (IEEE 802.3, reflected)
uint32_t PmStore::crc32_update_(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}
uint32_t PmStore::crc32_calc_(const uint8_t* data, size_t len) {
    return crc32_update_(0, data, len);
}

void PmStore::buildPath_(char* out, size_t outSz) const {
    if (!out || outSz == 0) return;
    snprintf(out, outSz, "%s/%s", dir_, filename_);
}

bool PmStore::ensureDir_() const {
    if (FSCom.exists(dir_)) return true;
    return FSCom.mkdir(dir_);
}

bool PmStore::ensureFile_() {
    char path[96];
    buildPath_(path, sizeof(path));

    // If file doesn't exist, create and initialize to fixed size with zero bytes.
    if (!FSCom.exists(path)) {
        File f = FSCom.open(path, FILE_O_WRITE);
        if (!f) return false;

        // initialize file to fileSize() bytes
        uint8_t zeros[64];
        memset(zeros, 0, sizeof(zeros));
        size_t remaining = fileSize();
        while (remaining > 0) {
            size_t chunk = remaining > sizeof(zeros) ? sizeof(zeros) : remaining;
            if ((int)f.write(zeros, chunk) != (int)chunk) { f.close(); return false; }
            remaining -= chunk;
        }
        f.flush();
        f.close();
        return true;
    }

    // If exists, ensure size at least fileSize(). (Some FS provide size(); if not, skip)
    // We'll do a pragmatic approach: try open read and check available bytes by seeking end.
    File rf = FSCom.open(path, FILE_O_READ);
    if (!rf) return false;
#if defined(ARCH_NRF52)
    // Adafruit LittleFS File has size() and seek(pos) only (no SeekSet/SeekEnd)
    size_t sz = (size_t)rf.size();
#else
    rf.seek(0, SeekEnd);
    size_t sz = (size_t)rf.position();
#endif
    rf.close();

    if (sz == fileSize()) return true;

    // Recreate if wrong size
    // (Simplest & safest for fixed ring file)
    (void)FSCom.remove(path);
    File f = FSCom.open(path, FILE_O_WRITE);
    if (!f) return false;

    uint8_t zeros[64];
    memset(zeros, 0, sizeof(zeros));
    size_t remaining = fileSize();
    while (remaining > 0) {
        size_t chunk = remaining > sizeof(zeros) ? sizeof(zeros) : remaining;
        if ((int)f.write(zeros, chunk) != (int)chunk) { f.close(); return false; }
        remaining -= chunk;
    }
    f.flush();
    f.close();
    return true;
}

bool PmStore::begin(const char* dir, const char* filename) {
    safe_strncpy_(dir_, dir ? dir : "/kltinybbs", sizeof(dir_));
    safe_strncpy_(filename_, filename ? filename : "pm.bin", sizeof(filename_));

    (void)FSBegin();

    if (!ensureDir_()) return false;
    if (!ensureFile_()) return false;

    scanForWritePtr_();
    return true;
}

bool PmStore::clearStorage() {
    char path[96];
    buildPath_(path, sizeof(path));
    if (FSCom.exists(path)) {
        if (!FSCom.remove(path)) return false;
    }
    if (!ensureFile_()) return false;
    scanForWritePtr_();
    return true;
}

bool PmStore::validate_(const Record& rec) const {
    if (rec.magic != kMagic) return false;
    if (rec.version != kVersion) return false;
    if (rec.len > maxTextLen()) return false;

    // CRC32 validation
    const uint32_t expect = rec.crc32;
    const uint32_t got = crc32_calc_((const uint8_t*)&rec, sizeof(Record) - sizeof(uint32_t));
    return expect == got;
}

bool PmStore::readSlotRaw_(uint16_t slotIndex, Record& rec) const {
    if (slotIndex >= slotCount()) return false;

    char path[96];
    buildPath_(path, sizeof(path));

    File f = FSCom.open(path, FILE_O_READ);
    if (!f) return false;

    const size_t off = (size_t)slotIndex * slotSize();
#if defined(ARCH_NRF52)
    if (!f.seek(off)) { f.close(); return false; }
#else
    if (!f.seek(off, SeekSet)) { f.close(); return false; }
#endif

    int n = f.read((uint8_t*)&rec, sizeof(rec));
    f.close();
    return n == (int)sizeof(rec);
}

bool PmStore::writeSlot_(uint16_t slotIndex, const Record& rec) {
    if (slotIndex >= slotCount()) return false;

    char path[96];
    buildPath_(path, sizeof(path));

#if defined(ARCH_NRF52)
    // Adafruit LittleFS: FILE_O_WRITE opens RDWR and does not truncate existing file
    File f = FSCom.open(path, FILE_O_WRITE);
#else
    File f = FSCom.open(path, "r+"); // read/write without truncation
#endif
    if (!f) {
        // Some FS might not support "r+"; fallback: open read, then write not possible.
        // On ESP32 LittleFS, "r+" is usually supported. If not, you can re-open with FILE_O_WRITE
        // but that would truncate - not acceptable. So return false.
        return false;
    }

    const size_t off = (size_t)slotIndex * slotSize();
#if defined(ARCH_NRF52)
    if (!f.seek(off)) { f.close(); return false; }
#else
    if (!f.seek(off, SeekSet)) { f.close(); return false; }
#endif

    int n = f.write((const uint8_t*)&rec, sizeof(rec));
    f.flush();
    f.close();
    return n == (int)sizeof(rec);
}

void PmStore::scanForWritePtr_() {
    // Find the slot with maximum seq among valid records.
    uint32_t maxSeq = 0;
    uint16_t maxIdx = 0;
    bool found = false;

    Record rec;
    for (uint16_t i = 0; i < slotCount(); i++) {
        if (!readSlotRaw_(i, rec)) continue;
        if (!validate_(rec)) continue;
        if (!found || rec.seq > maxSeq) {
            found = true;
            maxSeq = rec.seq;
            maxIdx = i;
        }
    }

    if (!found) {
        lastWrittenSlot_ = 0;
        nextSeq_ = 1;
    } else {
        lastWrittenSlot_ = maxIdx;
        nextSeq_ = maxSeq + 1;
    }
}

bool PmStore::add(uint8_t from, uint8_t to, uint8_t type,
                  const char* text, uint32_t ts, uint16_t ttl,
                  uint32_t* outSeq) {
    // Next slot to write
    uint16_t slot = (uint16_t)((lastWrittenSlot_ + 1) % slotCount());

    Record rec;
    memset(&rec, 0, sizeof(rec));

    rec.magic = kMagic;
    rec.version = kVersion;
    rec.flags = 0;
    rec.seq = nextSeq_;
    rec.ts = ts;
    rec.ttl = ttl;
    rec.from = from;
    rec.to = to;
    rec.type = type;

    size_t len = text ? strnlen(text, maxTextLen()) : 0;
    rec.len = (uint8_t)len;
    if (len) memcpy(rec.text, text, len);

    rec.crc32 = crc32_calc_((const uint8_t*)&rec, sizeof(Record) - sizeof(uint32_t));

    if (!writeSlot_(slot, rec)) return false;

    lastWrittenSlot_ = slot;
    nextSeq_++;

    if (outSeq) *outSeq = rec.seq;
    return true;
}

bool PmStore::readSlot(uint16_t slotIndex, MsgView& out) const {
    Record rec;
    memset(&rec, 0, sizeof(rec));
    if (!readSlotRaw_(slotIndex, rec)) {
        out = MsgView{};
        return false;
    }
    if (!validate_(rec)) {
        out = MsgView{};
        return false;
    }

    out.valid = true;
    out.deleted = (rec.flags & kFlagDeleted) != 0;
    out.seq = rec.seq;
    out.ts = rec.ts;
    out.ttl = rec.ttl;
    out.from = rec.from;
    out.to = rec.to;
    out.type = rec.type;
    out.len = rec.len;

    size_t n = rec.len;
    if (n > maxTextLen()) n = maxTextLen();
    memcpy(out.text, rec.text, n);
    out.text[n] = 0;
    return true;
}

bool PmStore::markDeletedBySlot(uint16_t slotIndex) {
    Record rec;
    if (!readSlotRaw_(slotIndex, rec)) return false;
    if (!validate_(rec)) return false;
    if (rec.flags & kFlagDeleted) return true;

    rec.flags |= kFlagDeleted;
    rec.crc32 = crc32_calc_((const uint8_t*)&rec, sizeof(Record) - sizeof(uint32_t));
    return writeSlot_(slotIndex, rec);
}

bool PmStore::markDeletedBySeq(uint32_t seq) {
    Record rec;
    for (uint16_t i = 0; i < slotCount(); i++) {
        if (!readSlotRaw_(i, rec)) continue;
        if (!validate_(rec)) continue;
        if (rec.seq == seq) {
            if (rec.flags & kFlagDeleted) return true;
            rec.flags |= kFlagDeleted;
            rec.crc32 = crc32_calc_((const uint8_t*)&rec, sizeof(Record) - sizeof(uint32_t));
            return writeSlot_(i, rec);
        }
    }
    return false;
}

int PmStore::markDeletedForEvictedUser(uint8_t userId) {
    int marked = 0;
    Record rec;
    for (uint16_t i = 0; i < slotCount(); i++) {
        if (!readSlotRaw_(i, rec)) continue;
        if (!validate_(rec)) continue;
        if (rec.flags & kFlagDeleted) continue;
        bool match = (rec.to == userId && rec.type == MSG_PM) ||
                     (rec.from == userId && rec.type == MSG_ANN);
        if (!match) continue;
        rec.flags |= kFlagDeleted;
        rec.crc32 = crc32_calc_((const uint8_t*)&rec, sizeof(Record) - sizeof(uint32_t));
        if (writeSlot_(i, rec)) marked++;
    }
    return marked;
}

bool PmStore::forEachToUser(uint8_t userId, uint32_t nowTs,
                            bool includeDeleted,
                            bool includeExpired,
                            ForEachCb cb, void* userCtx) const {
    if (!cb) return false;

    struct SlotRef { uint32_t seq; uint16_t idx; };
    SlotRef refs[slotCount()];
    int count = 0;

    Record rec;
    for (uint16_t i = 0; i < slotCount(); i++) {
        if (!readSlotRaw_(i, rec)) continue;
        if (!validate_(rec)) continue;
        if (rec.to != userId) continue;

        bool deleted = (rec.flags & kFlagDeleted) != 0;
        if (!includeDeleted && deleted) continue;

        bool expired = false;
        if (rec.ttl != 0) {
            uint32_t exp = rec.ts + (uint32_t)rec.ttl;
            expired = (nowTs != 0 && exp < nowTs);
        }
        if (!includeExpired && expired) continue;

        refs[count++] = SlotRef{rec.seq, i};
    }

    // sort by seq (small N -> simple sort)
    for (int a = 0; a < count; a++) {
        for (int b = a + 1; b < count; b++) {
            if (refs[b].seq < refs[a].seq) {
                SlotRef t = refs[a];
                refs[a] = refs[b];
                refs[b] = t;
            }
        }
    }

    MsgView view;
    for (int i = 0; i < count; i++) {
        if (!readSlot(refs[i].idx, view)) continue;
        if (!cb(view, userCtx)) break;
    }
    return true;
}

bool PmStore::listSlotsToUserDesc(uint8_t toUserId, uint8_t typeFilter, uint32_t nowTs,
                                  bool includeDeleted, bool includeExpired,
                                  uint32_t* outSeq, uint16_t* outSlot, size_t maxOut, size_t* outCount,
                                  size_t* outTotal) const {
    if (!outSeq || !outSlot || !outCount || maxOut == 0) return false;

    struct SlotRef { uint32_t seq; uint16_t idx; };
    SlotRef refs[slotCount()];
    int count = 0;

    Record rec;
    for (uint16_t i = 0; i < slotCount(); i++) {
        if (!readSlotRaw_(i, rec)) continue;
        if (!validate_(rec)) continue;
        if (rec.to != toUserId || rec.type != typeFilter) continue;

        bool deleted = (rec.flags & kFlagDeleted) != 0;
        if (!includeDeleted && deleted) continue;

        bool expired = false;
        if (rec.ttl != 0 && nowTs != 0) {
            uint32_t exp = rec.ts + (uint32_t)rec.ttl;
            if (exp < nowTs) expired = true;
        }
        if (!includeExpired && expired) continue;

        refs[count++] = SlotRef{rec.seq, i};
    }

    // Sort by seq descending (newest first)
    for (int a = 0; a < count; a++) {
        for (int b = a + 1; b < count; b++) {
            if (refs[b].seq > refs[a].seq) {
                SlotRef t = refs[a];
                refs[a] = refs[b];
                refs[b] = t;
            }
        }
    }

    if (outTotal) *outTotal = (size_t)count;
    size_t n = (size_t)count <= maxOut ? (size_t)count : maxOut;
    for (size_t i = 0; i < n; i++) {
        outSeq[i] = refs[i].seq;
        outSlot[i] = refs[i].idx;
    }
    *outCount = n;
    return true;
}

int PmStore::purge(uint32_t nowTs, uint32_t olderThanTs,
                   bool purgeExpired,
                   bool /*purgeDeleted*/) {
    int purged = 0;
    Record rec;

    for (uint16_t i = 0; i < slotCount(); i++) {
        if (!readSlotRaw_(i, rec)) continue;
        if (!validate_(rec)) continue;

        bool should = false;

        if (olderThanTs != 0 && rec.ts < olderThanTs) should = true;

        if (purgeExpired && rec.ttl != 0 && nowTs != 0) {
            uint32_t exp = rec.ts + (uint32_t)rec.ttl;
            if (exp < nowTs) should = true;
        }

        if (should && ((rec.flags & kFlagDeleted) == 0)) {
            rec.flags |= kFlagDeleted;
            rec.crc32 = crc32_calc_((const uint8_t*)&rec, sizeof(Record) - sizeof(uint32_t));
            if (writeSlot_(i, rec)) purged++;
        }
    }

    return purged;
}

} // namespace kltinybbs