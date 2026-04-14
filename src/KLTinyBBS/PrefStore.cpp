#include "PrefStore.h"
#include <SHA256.h>

namespace kltinybbs {

static const char kDomainAdmin[] = "KLTINYBBS-ADMIN";
static const char kDomainPrivate[] = "KLTINYBBS-PRIVATE";
static const char kDefaultPassword[] = "12345678";

void PrefStore::sha256_(const uint8_t* data, size_t len, uint8_t out32[32]) {
    SHA256 hash;
    hash.reset();
    hash.update(data, len);
    hash.finalize(out32, 32);
}

void PrefStore::computePassHash_(const char* password, const char* domain, uint8_t out32[32]) {
    uint8_t h[32] = {0};
    const uint32_t salt = 0;
    uint8_t buf[64 + 4 + 128];
    size_t pos = 0;
    size_t dLen = domain ? strlen(domain) : 0;
    if (dLen > 0 && dLen < sizeof(buf) - 4) {
        memcpy(buf + pos, domain, dLen);
        pos += dLen;
    }
    buf[pos++] = (uint8_t)(salt & 0xFF);
    buf[pos++] = (uint8_t)((salt >> 8) & 0xFF);
    buf[pos++] = (uint8_t)((salt >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((salt >> 24) & 0xFF);
    size_t pwLen = password ? strnlen(password, 120) : 0;
    if (pwLen && pos + pwLen <= sizeof(buf)) {
        memcpy(buf + pos, password, pwLen);
        pos += pwLen;
    }
    sha256_(buf, pos, h);

    for (int i = 1; i < KLTINYBBS_SHA_ITER; i++) {
        uint8_t ibuf[32 + 4 + 120];
        size_t ip = 0;
        memcpy(ibuf + ip, h, 32);
        ip += 32;
        ibuf[ip++] = (uint8_t)(salt & 0xFF);
        ibuf[ip++] = (uint8_t)((salt >> 8) & 0xFF);
        ibuf[ip++] = (uint8_t)((salt >> 16) & 0xFF);
        ibuf[ip++] = (uint8_t)((salt >> 24) & 0xFF);
        if (pwLen && ip + pwLen <= sizeof(ibuf)) {
            memcpy(ibuf + ip, password, pwLen);
            ip += pwLen;
        }
        sha256_(ibuf, ip, h);
    }
    memcpy(out32, h, 32);
}

void PrefStore::setDefaults_() {
    memset(admin_hash_, 0, sizeof(admin_hash_));
    memset(private_hash_, 0, sizeof(private_hash_));
    computePassHash_(kDefaultPassword, kDomainAdmin, admin_hash_);
    computePassHash_(kDefaultPassword, kDomainPrivate, private_hash_);
    privateMode_ = kPrivateModeOff;
    welcome_[0] = '\0';
}

bool PrefStore::load_() {
    char path[96];
    snprintf(path, sizeof(path), "%s/%s", dir_, filename_);
    if (!FSCom.exists(path)) {
        setDefaults_();
        return true;
    }
    File f = FSCom.open(path, FILE_O_READ);
    if (!f) return false;
    Record rec;
    memset(&rec, 0, sizeof(rec));
    int n = f.read((uint8_t*)&rec, sizeof(rec));
    f.close();
    if (n != (int)sizeof(rec)) return false;
    if (rec.magic != kMagic || rec.version != kVersion) {
        setDefaults_();
        return true;
    }
    memcpy(admin_hash_, rec.admin_hash, sizeof(admin_hash_));
    memcpy(private_hash_, rec.private_hash, sizeof(private_hash_));
    privateMode_ = rec.private_mode;
    memcpy(welcome_, rec.welcome, sizeof(welcome_));
    welcome_[sizeof(welcome_) - 1] = '\0';
    return true;
}

bool PrefStore::write_() const {
    char path[96];
    snprintf(path, sizeof(path), "%s/%s", dir_, filename_);
    File f = FSCom.open(path, FILE_O_WRITE);
    if (!f) return false;
    Record rec;
    memset(&rec, 0, sizeof(rec));
    rec.magic = kMagic;
    rec.version = kVersion;
    memcpy(rec.admin_hash, admin_hash_, sizeof(admin_hash_));
    memcpy(rec.private_hash, private_hash_, sizeof(private_hash_));
    rec.private_mode = privateMode_;
    memcpy(rec.welcome, welcome_, sizeof(rec.welcome));
    size_t w = f.write((const uint8_t*)&rec, sizeof(rec));
    f.flush();
    f.close();
    return w == sizeof(rec);
}

bool PrefStore::begin(const char* dir, const char* filename) {
    if (dir) {
        strncpy(dir_, dir, sizeof(dir_) - 1);
        dir_[sizeof(dir_) - 1] = '\0';
    } else {
        dir_[0] = '\0';
    }
    if (filename) {
        strncpy(filename_, filename, sizeof(filename_) - 1);
        filename_[sizeof(filename_) - 1] = '\0';
    } else {
        filename_[0] = '\0';
    }
    (void)FSBegin();
    if (!FSCom.exists(dir_)) {
        if (!FSCom.mkdir(dir_)) return false;
    }
    return load_();
}

bool PrefStore::clearStorage() {
    char path[96];
    snprintf(path, sizeof(path), "%s/%s", dir_, filename_);
    if (FSCom.exists(path) && !FSCom.remove(path)) return false;
    setDefaults_();
    dirty_ = false;
    return true;
}

bool PrefStore::flush() {
    if (!dirty_) return true;
    bool ok = write_();
    if (ok) dirty_ = false;
    return ok;
}

bool PrefStore::verifyAdminPassword(const char* password) const {
    if (!password) return false;
    uint8_t h[32];
    computePassHash_(password, kDomainAdmin, h);
    for (size_t i = 0; i < 32; i++)
        if (h[i] != admin_hash_[i]) return false;
    return true;
}

bool PrefStore::verifyPrivatePassword(const char* password) const {
    if (!password) return false;
    uint8_t h[32];
    computePassHash_(password, kDomainPrivate, h);
    for (size_t i = 0; i < 32; i++)
        if (h[i] != private_hash_[i]) return false;
    return true;
}

void PrefStore::setAdminPassword(const char* password) {
    if (!password) return;
    computePassHash_(password, kDomainAdmin, admin_hash_);
    dirty_ = true;
}

void PrefStore::setPrivatePassword(const char* password) {
    if (!password) return;
    computePassHash_(password, kDomainPrivate, private_hash_);
    dirty_ = true;
}

void PrefStore::getWelcome(char* out, size_t outSz) const {
    if (!out || outSz == 0) return;
    if (outSz > sizeof(welcome_)) outSz = sizeof(welcome_);
    memcpy(out, welcome_, outSz - 1);
    out[outSz - 1] = '\0';
}

bool PrefStore::setWelcome(const char* text) {
    if (!text) return true;
    size_t len = strnlen(text, (size_t)KLTINYBBS_PREF_WELCOME_MAX);
    if (len >= (size_t)KLTINYBBS_PREF_WELCOME_MAX) return false;
    memcpy(welcome_, text, len + 1);
    dirty_ = true;
    return true;
}

} // namespace kltinybbs
