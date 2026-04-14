#include "UserStore.h"
#include <SHA256.h>


namespace kltinybbs {

static inline void safe_strncpy_(char* dst, const char* src, size_t n) {
    if (!dst || n == 0) return;
    if (!src) { dst[0] = 0; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = 0;
}

bool UserStore::begin(const char* dir) {
    safe_strncpy_(dir_, dir ? dir : "/kltinybbs", sizeof(dir_));

    // Defensive: ensure FS backend is begun (mount/format policy handled by FSCommon)
    (void)FSBegin();

    if (!FSCom.exists(dir_)) {
        bool ok = FSCom.mkdir(dir_);
        LOG_INFO("mkdir %s => %d", dir_, ok);
        if (!ok) return false;
    } else {
        LOG_INFO("UserStore dir %s exists", dir_);
    }

    (void)loadSnapshot_();
    rebuildUserCount_();
    return true;
}

bool UserStore::clearStorage() {
    char pathA[96], pathB[96];
    buildPath_(pathA, sizeof(pathA), "usersA.bin");
    buildPath_(pathB, sizeof(pathB), "usersB.bin");
    if (FSCom.exists(pathA) && !FSCom.remove(pathA)) return false;
    if (FSCom.exists(pathB) && !FSCom.remove(pathB)) return false;
    loadSnapshot_();
    rebuildUserCount_();
    dirty_ = false;
    lastFlush_ = 0;
    return true;
}

size_t UserStore::snapshotSizeBytes() const {
    return sizeof(SnapshotHeader) + sizeof(UserRecord) * KLTINYBBS_MAX_USERS;
}

void UserStore::buildPath_(char* out, size_t outSz, const char* leaf) const {
    if (!out || outSz == 0) return;
    snprintf(out, outSz, "%s/%s", dir_, leaf ? leaf : "");
}

void UserStore::normalizeName_(char out[8], const char* in) {
    memset(out, 0, 8);
    if (!in) return;

    // Lowercase ASCII; allow [a-z0-9_-], replace others with '_'
    size_t j = 0;
    for (size_t i = 0; in[i] && j < 8; i++) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '_') || (c == '-');
        out[j++] = ok ? c : '_';
    }
}

bool UserStore::nameEquals_(const char a[8], const char b[8]) {
    return memcmp(a, b, 8) == 0;
}

bool UserStore::constTimeEq16_(const uint8_t a[16], const uint8_t b[16]) {
    uint8_t diff = 0;
    for (int i = 0; i < 16; i++) diff |= (a[i] ^ b[i]);
    return diff == 0;
}

// CRC32 (IEEE 802.3, reflected)
uint32_t UserStore::crc32_update_(uint32_t crc, const uint8_t* data, size_t len) {
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

uint32_t UserStore::crc32_calc_(const uint8_t* data, size_t len) {
    return crc32_update_(0, data, len);
}

// ---- SHA256 stub ----
void UserStore::sha256_stub_(const uint8_t* data, size_t len, uint8_t out32[32]) {
    SHA256 hash;
    hash.reset();
    hash.update(data, len);
    hash.finalize(out32, 32);
}

void UserStore::computePassHash_(const char* password, uint32_t salt, uint8_t out16[16]) {
    // Iterated "SHA256" (currently stubbed). Replace sha256_stub_ with real sha256.
    const uint8_t domain[] = {'K','L','T','B','B','S','-','P','W',0};
    uint8_t h[32] = {0};

    // Build initial buffer: domain || saltLE || password
    uint8_t buf[16 + 4 + 128];
    size_t pos = 0;
    memcpy(buf + pos, domain, sizeof(domain));
    pos += sizeof(domain);

    buf[pos++] = uint8_t(salt & 0xFF);
    buf[pos++] = uint8_t((salt >> 8) & 0xFF);
    buf[pos++] = uint8_t((salt >> 16) & 0xFF);
    buf[pos++] = uint8_t((salt >> 24) & 0xFF);

    size_t pwLen = password ? strnlen(password, 120) : 0;
    if (pwLen) {
        memcpy(buf + pos, password, pwLen);
        pos += pwLen;
    }

    sha256_stub_(buf, pos, h);

    // Iterations
    for (int i = 1; i < KLTINYBBS_SHA_ITER; i++) {
        uint8_t ibuf[32 + 4 + 120];
        size_t ip = 0;
        memcpy(ibuf + ip, h, 32); ip += 32;

        ibuf[ip++] = uint8_t(salt & 0xFF);
        ibuf[ip++] = uint8_t((salt >> 8) & 0xFF);
        ibuf[ip++] = uint8_t((salt >> 16) & 0xFF);
        ibuf[ip++] = uint8_t((salt >> 24) & 0xFF);

        if (pwLen) { memcpy(ibuf + ip, password, pwLen); ip += pwLen; }
        sha256_stub_(ibuf, ip, h);
    }

    memcpy(out16, h, 16);
}

uint32_t UserStore::genSalt_() {
    // Minimal fallback PRNG; replace with real RNG if you want.
    static uint32_t s = 0xC001D00Du;
    s = s * 1664525u + 1013904223u;
    return s;
}

int UserStore::findUserIdByName(const char* name) const {
    char n[8]; normalizeName_(n, name);

    // Reject empty normalized name
    bool empty = true;
    for (int i = 0; i < 8; i++) { if (n[i] != 0) { empty = false; break; } }
    if (empty) return -1;

    for (int i = 0; i < (int)KLTINYBBS_MAX_USERS; i++) {
        const auto& r = users_[i];
        if (r.used && ((r.flags & kFlagDeleted) == 0) && nameEquals_(r.name, n)) {
            return i;
        }
    }
    return -1;
}

bool UserStore::getUser(uint8_t userId, UserInfo& out) const {
    if (userId >= KLTINYBBS_MAX_USERS) return false;
    const auto& r = users_[userId];

    out.userId = userId;
    out.flags = r.flags;
    memcpy(out.name, r.name, 8);
    out.name[8] = 0;
    out.last_login = r.last_login;
    out.used = (r.used != 0);
    out.deleted = ((r.flags & kFlagDeleted) != 0);
    return true;
}

int UserStore::findOldestInactiveUserId(uint32_t nowTs, uint32_t inactiveSecs) const {
    const uint32_t cutoff = (inactiveSecs >= nowTs) ? 0 : (nowTs - inactiveSecs);
    int found = -1;
    uint32_t oldest = 0;

    for (int i = 0; i < (int)KLTINYBBS_MAX_USERS; i++) {
        const auto& r = users_[i];
        if (!r.used || (r.flags & kFlagDeleted)) continue;
        if (r.last_login >= cutoff) continue;
        if (found < 0 || r.last_login < oldest) {
            found = i;
            oldest = r.last_login;
        }
    }
    return found;
}

void UserStore::rebuildUserCount_() {
    size_t c = 0;
    for (size_t i = 0; i < KLTINYBBS_MAX_USERS; i++) {
        if (users_[i].used && ((users_[i].flags & kFlagDeleted) == 0)) c++;
    }
    userCountCached_ = c;
}

bool UserStore::addUser(const char* name, const char* password, uint8_t* outUserId) {
    char n[8]; normalizeName_(n, name);

    bool empty = true;
    for (int i = 0; i < 8; i++) { if (n[i] != 0) { empty = false; break; } }
    if (empty) return false;

    if (findUserIdByName(name) >= 0) return false;

    int freeIdx = -1;
    for (int i = 0; i < (int)KLTINYBBS_MAX_USERS; i++) {
        if (!users_[i].used || (users_[i].flags & kFlagDeleted)) { freeIdx = i; break; }
    }
    if (freeIdx < 0) return false;

    auto& r = users_[freeIdx];
    memset(&r, 0, sizeof(r));
    r.used = 1;
    r.flags = kFlagEnabled;
    memcpy(r.name, n, 8);
    r.last_login = 0;
    r.salt = genSalt_();
    computePassHash_(password ? password : "", r.salt, r.pass_hash);

    dirty_ = true;
    rebuildUserCount_();
    if (outUserId) *outUserId = (uint8_t)freeIdx;
    return true;
}

bool UserStore::deleteUser(uint8_t userId) {
    if (userId >= KLTINYBBS_MAX_USERS) return false;
    auto& r = users_[userId];
    if (!r.used) return false;

    r.flags |= kFlagDeleted;
    dirty_ = true;
    rebuildUserCount_();
    return true;
}

bool UserStore::setPassword(uint8_t userId, const char* newPassword) {
    if (userId >= KLTINYBBS_MAX_USERS) return false;
    auto& r = users_[userId];
    if (!r.used || (r.flags & kFlagDeleted)) return false;

    r.salt = genSalt_();
    computePassHash_(newPassword ? newPassword : "", r.salt, r.pass_hash);
    dirty_ = true;
    return true;
}

bool UserStore::verifyLogin(const char* name, const char* password, uint8_t* outUserId) {
    int uid = findUserIdByName(name);
    if (uid < 0) return false;

    auto& r = users_[uid];
    if (!r.used || (r.flags & kFlagDeleted)) return false;

    uint8_t h[16];
    computePassHash_(password ? password : "", r.salt, h);
    if (!constTimeEq16_(h, r.pass_hash)) return false;

    // On success: caller decides timestamp; we just mark dirty if they later setLastLogin.
    if (outUserId) *outUserId = (uint8_t)uid;
    return true;
}

bool UserStore::setLastLogin(uint8_t userId, uint32_t ts) {
    if (userId >= KLTINYBBS_MAX_USERS) return false;
    auto& r = users_[userId];
    if (!r.used || (r.flags & kFlagDeleted)) return false;

    r.last_login = ts;
    dirty_ = true;
    return true;
}

// ---- Snapshot I/O ----

bool UserStore::loadFile_(const char* path, SnapshotHeader& hdrOut, UserRecord recOut[KLTINYBBS_MAX_USERS]) const {
    auto f = FSCom.open(path, FILE_O_READ);
    if (!f) return false;

    if (f.read((uint8_t*)&hdrOut, sizeof(hdrOut)) != (int)sizeof(hdrOut)) {
        f.close();
        return false;
    }
    if (hdrOut.magic != kMagic || hdrOut.version != kVersion) {
        f.close();
        return false;
    }

    const size_t recBytes = sizeof(UserRecord) * KLTINYBBS_MAX_USERS;
    if (f.read((uint8_t*)recOut, recBytes) != (int)recBytes) {
        f.close();
        return false;
    }
    f.close();

    // Verify CRC
    SnapshotHeader tmp = hdrOut;
    uint32_t expected = hdrOut.crc32;
    tmp.crc32 = 0;

    uint32_t crc = 0;
    crc = crc32_update_(crc, (const uint8_t*)&tmp, sizeof(tmp));
    crc = crc32_update_(crc, (const uint8_t*)recOut, recBytes);

    return crc == expected;
}

bool UserStore::loadSnapshot_() {
    char pathA[96], pathB[96];
    buildPath_(pathA, sizeof(pathA), "usersA.bin");
    buildPath_(pathB, sizeof(pathB), "usersB.bin");

    SnapshotHeader ha = {}, hb = {};
    UserRecord ra[KLTINYBBS_MAX_USERS] = {};
    UserRecord rb[KLTINYBBS_MAX_USERS] = {};

    bool va = loadFile_(pathA, ha, ra);
    bool vb = loadFile_(pathB, hb, rb);

    if (!va && !vb) {
        memset(users_, 0, sizeof(users_));
        activeSeq_ = 0;
        activeIsA_ = true;
        dirty_ = false;
        return false;
    }

    if (va && (!vb || ha.seq >= hb.seq)) {
        memcpy(users_, ra, sizeof(users_));
        activeSeq_ = ha.seq;
        activeIsA_ = true;
    } else {
        memcpy(users_, rb, sizeof(users_));
        activeSeq_ = hb.seq;
        activeIsA_ = false;
    }

    dirty_ = false;
    return true;
}

bool UserStore::writeFile_(const char* path, uint32_t seq, const UserRecord recIn[KLTINYBBS_MAX_USERS], uint8_t usedCount) {
    SnapshotHeader hdr = {};
    hdr.magic = kMagic;
    hdr.version = kVersion;
    hdr.seq = seq;
    hdr.user_count = usedCount;
    hdr.crc32 = 0;

    const size_t recBytes = sizeof(UserRecord) * KLTINYBBS_MAX_USERS;

    // Compute CRC over (header with crc32=0) + records
    uint32_t crc = 0;
    crc = crc32_update_(crc, (const uint8_t*)&hdr, sizeof(hdr));
    crc = crc32_update_(crc, (const uint8_t*)recIn, recBytes);
    hdr.crc32 = crc;

    auto f = FSCom.open(path, FILE_O_WRITE); // truncate/write
    if (!f) return false;

    if (f.write((const uint8_t*)&hdr, sizeof(hdr)) != (int)sizeof(hdr)) { f.close(); return false; }
    if (f.write((const uint8_t*)recIn, recBytes) != (int)recBytes) { f.close(); return false; }

    // flush if supported
    f.flush();
    f.close();
    return true;
}

bool UserStore::flush() {
    if (!dirty_) return true;

    // Count used non-deleted
    uint8_t usedCount = 0;
    for (size_t i = 0; i < KLTINYBBS_MAX_USERS; i++) {
        if (users_[i].used && ((users_[i].flags & kFlagDeleted) == 0)) usedCount++;
    }

    char pathA[96], pathB[96];
    buildPath_(pathA, sizeof(pathA), "usersA.bin");
    buildPath_(pathB, sizeof(pathB), "usersB.bin");

    const bool writeA = !activeIsA_;      // toggle
    const char* target = writeA ? pathA : pathB;
    const uint32_t newSeq = activeSeq_ + 1;

    if (!writeFile_(target, newSeq, users_, usedCount)) {
        return false; // keep dirty
    }

    // Optional verify: reload the target file and check seq/crc
    SnapshotHeader h = {};
    UserRecord r[KLTINYBBS_MAX_USERS] = {};
    if (!loadFile_(target, h, r) || h.seq != newSeq) {
        return false;
    }

    activeIsA_ = writeA;
    activeSeq_ = newSeq;
    dirty_ = false;
    return true;
}

bool UserStore::flushIfNeeded(uint32_t now, uint32_t minIntervalSeconds) {
    if (!dirty_) return true;
    if (minIntervalSeconds == 0) {
        bool ok = flush();
        if (ok) lastFlush_ = now;
        return ok;
    }
    if ((uint32_t)(now - lastFlush_) < minIntervalSeconds) return true;

    bool ok = flush();
    if (ok) lastFlush_ = now;
    return ok;
}

} // namespace kltinybbs
