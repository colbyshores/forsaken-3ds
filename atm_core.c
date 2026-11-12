/* atm_core implementation - see atm_core.h. No platform calls, no malloc. */
#include "atm/atm_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void wr16le(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static int atm_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool atm_http_is_config_base(const char *base) {
    static const char *const names[] = {
        "netlog.txt", "device_id", "enabled", "allow_deployment",
        "allow_remote_control", "expected_sha256", "auto_dump", "dump_sync.txt"
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(base, names[i]) == 0) return true;
    return false;
}
AtmHttpDecision atm_http_decide(const char *method, const char *raw_path,
                                char *clean_out, size_t clean_cap,
                                bool *head_only) {
    if (head_only) *head_only = false;
    if (!method || !*method) return ATM_HTTP_BAD_REQUEST;
    bool is_get = strcmp(method, "GET") == 0;
    bool is_head = strcmp(method, "HEAD") == 0;
    if (!is_get && !is_head) return ATM_HTTP_METHOD;
    if (head_only) *head_only = is_head;
    if (!raw_path || raw_path[0] != '/' || !clean_out || clean_cap < 2)
        return ATM_HTTP_BAD_REQUEST;

    char decoded[256];
    size_t di = 0;
    for (size_t i = 0; raw_path[i] && raw_path[i] != '?'; i++) {
        char c = raw_path[i];
        if (c == '%') {
            int hi = atm_hex_val(raw_path[i + 1]);
            int lo = hi < 0 ? -1 : atm_hex_val(raw_path[i + 2]);
            if (lo < 0) return ATM_HTTP_BAD_REQUEST;
            c = (char)(hi * 16 + lo);
            i += 2;
        }
        unsigned char uc = (unsigned char)c;
        if (uc < 0x20 || uc == 0x7f || c == '\\') return ATM_HTTP_BAD_REQUEST;
        if (di + 1 >= sizeof(decoded)) return ATM_HTTP_BAD_REQUEST;
        decoded[di++] = c;
    }
    decoded[di] = 0;

    /* Normalise into clean_out segment by segment. */
    size_t ci = 0;
    clean_out[ci++] = '/';
    clean_out[ci] = 0;
    int nseg = 0;
    const char *base = "";
    const char *p = decoded;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *seg = p;
        while (*p && *p != '/') p++;
        size_t slen = (size_t)(p - seg);
        if (slen == 1 && seg[0] == '.') continue;
        if (slen == 2 && seg[0] == '.' && seg[1] == '.') return ATM_HTTP_FORBIDDEN;
        if (nseg > 0) {
            if (ci + 1 >= clean_cap) return ATM_HTTP_BAD_REQUEST;
            clean_out[ci++] = '/';
        }
        if (ci + slen >= clean_cap) return ATM_HTTP_BAD_REQUEST;
        memcpy(clean_out + ci, seg, slen);
        ci += slen;
        clean_out[ci] = 0;
        base = seg;
        nseg++;
    }
    char basebuf[256];
    { size_t bl = strlen(base);
      const char *slash = strchr(base, '/');
      if (slash) bl = (size_t)(slash - base);
      if (bl >= sizeof(basebuf)) return ATM_HTTP_BAD_REQUEST;
      memcpy(basebuf, base, bl); basebuf[bl] = 0; }

    if (strcmp(clean_out, "/") == 0 ||
        strcmp(clean_out, "/luma/atm/status.txt") == 0 ||
        strcmp(clean_out, "/3ds/netload_stub/status.txt") == 0)
        return ATM_HTTP_SERVE_STATUS;
    if (strcmp(clean_out, "/luma/atm/artifacts.txt") == 0)
        return ATM_HTTP_SERVE_ARTIFACTS;
    if (strcmp(clean_out, "/luma/errdisp.txt") == 0)
        return ATM_HTTP_SERVE_FILE; /* stock ErrDisp append log */
    /* Temporary capture files are never served: only commit-manifest
     * published screenshot sets are artifacts. */
    { size_t cl = strlen(clean_out);
      if (cl > 5 && strcmp(clean_out + cl - 5, ".part") == 0)
          return ATM_HTTP_FORBIDDEN; }
    if (atm_http_is_config_base(basebuf)) return ATM_HTTP_FORBIDDEN;
    if (strncmp(clean_out, "/luma/atm/", 10) == 0 ||
        strncmp(clean_out, "/luma/dumps/", 12) == 0)
        return ATM_HTTP_SERVE_FILE;
    size_t bl2 = strlen(basebuf);
    bool is_log = bl2 > 4 && strcmp(basebuf + bl2 - 4, ".log") == 0;
    bool is_txt = bl2 > 4 && strcmp(basebuf + bl2 - 4, ".txt") == 0;
    if (strncmp(clean_out, "/3ds/", 5) == 0 && (is_log || is_txt))
        return ATM_HTTP_SERVE_FILE;
    if (nseg == 1 && is_log) return ATM_HTTP_SERVE_FILE;
    return ATM_HTTP_FORBIDDEN;
}

bool atm_sanitize_name(const char *in, char *out, size_t out_sz) {
    if (!in || !out || out_sz == 0) return false;
    const char *base = in;
    for (const char *p = in; *p; p++) if (*p == '/' || *p == '\\') base = p + 1;
    if (!*base) return false;
    if (strlen(base) >= out_sz) return false;
    if (strstr(base, "..")) return false;
    for (const char *p = base; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == 0x7f) return false;
        if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false;
    }
    strcpy(out, base);
    return true;
}

AtmHeaderStatus atm_parse_prologue(const uint8_t *buf, size_t len,
                                   AtmPrologue *out, size_t *consumed) {
    if (len < 4) return ATM_HDR_INCOMPLETE;
    uint32_t namelen = rd32le(buf);
    if (namelen == 0 || namelen >= 256) return ATM_HDR_BAD_NAMELEN;
    if (len < 4u + namelen + 4u) return ATM_HDR_INCOMPLETE;
    char raw[256];
    memcpy(raw, buf + 4, namelen);
    raw[namelen] = 0;
    if (!atm_sanitize_name(raw, out->name, sizeof(out->name))) return ATM_HDR_BAD_NAME;
    out->filelen = rd32le(buf + 4 + namelen);
    size_t nl = strlen(out->name);
    out->is_3dsx = (nl > 5 && strcmp(out->name + nl - 5, ".3dsx") == 0);
    if (consumed) *consumed = 4u + namelen + 4u;
    return ATM_HDR_OK;
}

/* ---- SHA-256 ---- */
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static void sha_block(AtmSha256 *s, const uint8_t *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4*i] << 24) | ((uint32_t)p[4*i+1] << 16) | ((uint32_t)p[4*i+2] << 8) | p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K256[i] + w[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d; s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}
void atm_sha256_init(AtmSha256 *s) {
    static const uint32_t H0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(s->h, H0, sizeof(H0)); s->total = 0; s->used = 0;
}
void atm_sha256_update(AtmSha256 *s, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    s->total += len;
    while (len) {
        size_t take = 64 - s->used; if (take > len) take = len;
        memcpy(s->block + s->used, p, take); s->used += take; p += take; len -= take;
        if (s->used == 64) { sha_block(s, s->block); s->used = 0; }
    }
}
void atm_sha256_final(AtmSha256 *s, uint8_t out32[32]) {
    uint64_t bits = s->total * 8;
    uint8_t pad = 0x80;
    atm_sha256_update(s, &pad, 1);
    pad = 0;
    while (s->used != 56) atm_sha256_update(s, &pad, 1);
    /* append length manually (avoid recursing through update accounting) */
    uint8_t lenbuf[8];
    for (int i = 0; i < 8; i++) lenbuf[i] = (uint8_t)(bits >> (56 - 8*i));
    memcpy(s->block + s->used, lenbuf, 8); s->used = 64;
    sha_block(s, s->block);
    for (int i = 0; i < 8; i++) {
        out32[4*i] = (uint8_t)(s->h[i] >> 24); out32[4*i+1] = (uint8_t)(s->h[i] >> 16);
        out32[4*i+2] = (uint8_t)(s->h[i] >> 8); out32[4*i+3] = (uint8_t)s->h[i];
    }
}
void atm_hex32(const uint8_t in32[32], char out65[65]) {
    static const char hexd[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { out65[2*i] = hexd[in32[i] >> 4]; out65[2*i+1] = hexd[in32[i] & 15]; }
    out65[64] = 0;
}

/* ---- authorization ---- */
bool atm_auth_permitted(const AtmAuth *a, AtmOp op) {
    if (!a || !a->enrolled) return false;
    switch (op) {
        case ATM_OP_RECEIVE:           return a->deployment;
        case ATM_OP_LAUNCH:            return a->launch;
        case ATM_OP_TERMINATE_RUNNING: return a->control;
        case ATM_OP_TERMINATE_RUN:     return a->control;
        case ATM_OP_CMD_DIAGNOSTIC:    return true;
        case ATM_OP_CMD_EXPECT_DIGEST: return a->deployment;
        case ATM_OP_CMD_ACK_DUMP:      return a->control;
        case ATM_OP_CMD_SCREENSHOT:    return a->control;
        case ATM_OP_CMD_ACK_SHOT:      return a->control;
        case ATM_OP_CMD_REBOOT:        return a->control;
        case ATM_OP_CMD_FWUPDATE:        return a->control;
        default:                       return false;
    }
}

/* ---- transfer validator ---- */
bool atm_valid_sha256_hex(const char *s) {
    if (!s || strlen(s) != 64) return false;
    for (int i = 0; i < 64; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
void atm_transfer_init_ex(AtmTransferValidator *v, uint32_t advertised,
                          uint64_t max_payload, bool enforce_length,
                          const char *expected_hex) {
    v->written = 0; v->advertised = advertised; v->max_payload = max_payload;
    v->enforce_length = enforce_length; atm_sha256_init(&v->sha);
    v->has_expected = atm_valid_sha256_hex(expected_hex);
    if (v->has_expected) strcpy(v->expected, expected_hex);
    else v->expected[0] = 0;
}
void atm_transfer_init(AtmTransferValidator *v, uint32_t advertised,
                       uint64_t max_payload, bool enforce_length) {
    atm_transfer_init_ex(v, advertised, max_payload, enforce_length, NULL);
}
bool atm_transfer_feed(AtmTransferValidator *v, const void *data, size_t len) {
    if (v->written + len > v->max_payload) return false;
    /* When length is enforced, the advertised size is a hard streaming
     * cap, not just a finish-time check: staging must never write past
     * the size the pre-deploy capacity check accounted for. Transfers
     * that could ever pass finish (written == advertised) are
     * unaffected. advertised==0 keeps the legacy unenforced meaning. */
    if (v->enforce_length && v->advertised != 0 &&
        v->written + len > v->advertised) return false;
    atm_sha256_update(&v->sha, data, len);
    v->written += len;
    return true;
}
const char *atm_transfer_finish(AtmTransferValidator *v, bool stream_ended, char sha_hex65[65]) {
    uint8_t digest[32];
    AtmSha256 copy = v->sha;
    atm_sha256_final(&copy, digest);
    if (sha_hex65) atm_hex32(digest, sha_hex65);
    if (!stream_ended) return "incomplete_stream";
    if (v->written > v->max_payload) return "size_exceeded";
    if (v->enforce_length && v->advertised != 0 && v->written != v->advertised) return "size_mismatch";
    if (v->has_expected && sha_hex65 && strcmp(sha_hex65, v->expected) != 0) return "hash_mismatch";
    return NULL;
}

/* ---- exit observer ---- */
void atm_observer_init(AtmObserver *o, uint32_t launched_pid) {
    o->launched_pid = launched_pid;
    o->seen = false; o->gone_streak = 0; o->err_streak = 0; o->done = false;
}
AtmObsOutcome atm_observer_note(AtmObserver *o, AtmQueryKind kind,
                                uint32_t pid, uint64_t now_ms, uint64_t launched_ms) {
    if (o->done) return ATM_OBS_EXIT_CONFIRMED;
    switch (kind) {
        case ATM_Q_ERROR:
            o->err_streak++;
            o->gone_streak = 0; /* an error breaks the positive-evidence chain */
            return o->err_streak >= ATM_ERROR_LIMIT ? ATM_OBS_PERSISTENT_FAILURE
                                                     : ATM_OBS_QUERY_ERROR;
        case ATM_Q_RUNNING:
            o->err_streak = 0; o->gone_streak = 0;
            if (o->launched_pid == 0) o->launched_pid = pid;
            o->seen = true;
            return ATM_OBS_RUNNING;
        case ATM_Q_NEW_PID: {
            o->err_streak = 0;
            if (!o->seen) return ATM_OBS_IDENTITY_CHANGED;
            o->gone_streak++;
            if (o->gone_streak >= ATM_CONFIRM_SAMPLES) { o->done = true; return ATM_OBS_EXIT_CONFIRMED; }
            return ATM_OBS_IDENTITY_CHANGED;
        }
        case ATM_Q_NO_APP:
            o->err_streak = 0;
            if (!o->seen) {
                return (now_ms - launched_ms >= ATM_OBSERVE_DEADLINE_MS)
                       ? ATM_OBS_NEVER_OBSERVED : ATM_OBS_UNKNOWN;
            }
            o->gone_streak++;
            if (o->gone_streak >= ATM_CONFIRM_SAMPLES) { o->done = true; return ATM_OBS_EXIT_CONFIRMED; }
            return ATM_OBS_UNKNOWN;
        default:
            return ATM_OBS_UNKNOWN;
    }
}

/* ---- N3DL ---- */
uint32_t atm_fnv1a(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
size_t atm_n3dl_pack(uint8_t *out, size_t out_cap, uint16_t type, uint32_t session,
                     uint32_t stream, uint32_t seq, const void *payload, uint32_t len) {
    if (len > (uint32_t)(ATM_N3DL_MTU - ATM_N3DL_HDR)) return 0;
    if (out_cap < (size_t)ATM_N3DL_HDR + len) return 0;
    wr32le(out + 0, ATM_N3DL_MAGIC);
    wr16le(out + 4, ATM_N3DL_VERSION);
    wr16le(out + 6, type);
    wr32le(out + 8, session); wr32le(out + 12, stream);
    wr32le(out + 16, seq); wr32le(out + 20, len);
    if (len && payload) memcpy(out + ATM_N3DL_HDR, payload, len);
    return (size_t)ATM_N3DL_HDR + len;
}
int atm_n3dl_unpack(const uint8_t *pkt, size_t n, AtmN3dlHeader *hdr, const uint8_t **payload) {
    if (n < 4 || rd32le(pkt) != ATM_N3DL_MAGIC) return -1;
    if (n < ATM_N3DL_HDR) return -2;
    hdr->version = (uint16_t)(pkt[4] | (pkt[5] << 8));
    hdr->type = (uint16_t)(pkt[6] | (pkt[7] << 8));
    hdr->session = rd32le(pkt + 8); hdr->stream = rd32le(pkt + 12);
    hdr->seq = rd32le(pkt + 16); hdr->len = rd32le(pkt + 20);
    if (hdr->len > n - ATM_N3DL_HDR) return -2;
    if (payload) *payload = pkt + ATM_N3DL_HDR;
    return 0;
}
uint32_t atm_seq_next_text(AtmN3dlSeq *s) { return s->text++; }
uint32_t atm_seq_next_ctl(AtmN3dlSeq *s) { return s->ctl++; }
bool atm_cmd_is_new(AtmCmdDedupe *d, uint32_t session, uint32_t seq) {
    for (int i = 0; i < 8; i++) if (d->session[i] == session && d->seq[i] == seq && (session || seq)) {
        /* slot 0 doubles as empty marker when all-zero; (0,0) never a real key here */
        if (i == 0 && session == 0 && seq == 0) break;
        return false;
    }
    d->session[d->idx] = session; d->seq[d->idx] = seq;
    d->idx = (d->idx + 1) & 7;
    return true;
}

/* ---- state machine (mirror of tests/atm/state_machine.py) ---- */
static const char *const STATE_NAMES[ATM_ST_COUNT] = {
    "disabled","initializing","listening","receiving","verifying","staged",
    "launch_pending","running","exited","failed","recovery_pending",
    "reboot_pending","reconnecting","diagnostics_available","recovery_failed"
};
static const char *const EVENT_NAMES[ATM_EV_COUNT] = {
    "enable","ready","init_failed","disable","transfer_start","transfer_complete",
    "transfer_failed","disconnect","verified","verify_failed","discard",
    "launch_request","launched","launch_rejected","launch_timeout","app_exited",
    "app_failed","hang_detected","recovered","recovery_failed","reboot","rebooted",
    "reconnected","reconnect_failed","diagnostics_ready","collect_done",
    "controller_restart","sleep","wake","heartbeat","hello_sent","telemetry_received",
    "device_lost"
};
const char *atm_state_name(AtmState s) { return ((unsigned)s < (unsigned)ATM_ST_COUNT) ? STATE_NAMES[s] : "?"; }
const char *atm_event_name(AtmEvent e) { return ((unsigned)e < (unsigned)ATM_EV_COUNT) ? EVENT_NAMES[e] : "?"; }

typedef struct { AtmState from; AtmEvent ev; AtmState to; } Trans;
static const Trans TRANS[] = {
    {ATM_ST_DISABLED, ATM_EV_ENABLE, ATM_ST_INITIALIZING},
    {ATM_ST_INITIALIZING, ATM_EV_READY, ATM_ST_LISTENING},
    {ATM_ST_INITIALIZING, ATM_EV_INIT_FAILED, ATM_ST_RECOVERY_FAILED},
    {ATM_ST_INITIALIZING, ATM_EV_DISABLE, ATM_ST_DISABLED},
    {ATM_ST_LISTENING, ATM_EV_TRANSFER_START, ATM_ST_RECEIVING},
    {ATM_ST_LISTENING, ATM_EV_DISABLE, ATM_ST_DISABLED},
    {ATM_ST_LISTENING, ATM_EV_SLEEP, ATM_ST_LISTENING},
    {ATM_ST_LISTENING, ATM_EV_WAKE, ATM_ST_LISTENING},
    {ATM_ST_LISTENING, ATM_EV_CONTROLLER_RESTART, ATM_ST_INITIALIZING},
    {ATM_ST_LISTENING, ATM_EV_REBOOT, ATM_ST_REBOOT_PENDING},
    {ATM_ST_RECEIVING, ATM_EV_TRANSFER_COMPLETE, ATM_ST_VERIFYING},
    {ATM_ST_RECEIVING, ATM_EV_TRANSFER_FAILED, ATM_ST_LISTENING},
    {ATM_ST_RECEIVING, ATM_EV_DISCONNECT, ATM_ST_LISTENING},
    {ATM_ST_RECEIVING, ATM_EV_REBOOT, ATM_ST_REBOOT_PENDING},
    {ATM_ST_VERIFYING, ATM_EV_VERIFIED, ATM_ST_STAGED},
    {ATM_ST_VERIFYING, ATM_EV_VERIFY_FAILED, ATM_ST_LISTENING},
    {ATM_ST_STAGED, ATM_EV_LAUNCH_REQUEST, ATM_ST_LAUNCH_PENDING},
    {ATM_ST_STAGED, ATM_EV_DISCARD, ATM_ST_LISTENING},
    {ATM_ST_STAGED, ATM_EV_TRANSFER_START, ATM_ST_RECEIVING},
    {ATM_ST_LAUNCH_PENDING, ATM_EV_LAUNCHED, ATM_ST_RUNNING},
    {ATM_ST_LAUNCH_PENDING, ATM_EV_LAUNCH_REJECTED, ATM_ST_STAGED},
    {ATM_ST_LAUNCH_PENDING, ATM_EV_LAUNCH_TIMEOUT, ATM_ST_FAILED},
    {ATM_ST_RUNNING, ATM_EV_APP_EXITED, ATM_ST_EXITED},
    {ATM_ST_RUNNING, ATM_EV_APP_FAILED, ATM_ST_FAILED},
    {ATM_ST_RUNNING, ATM_EV_HANG_DETECTED, ATM_ST_RECOVERY_PENDING},
    {ATM_ST_RUNNING, ATM_EV_REBOOT, ATM_ST_REBOOT_PENDING},
    {ATM_ST_RUNNING, ATM_EV_DEVICE_LOST, ATM_ST_RECONNECTING},
    {ATM_ST_EXITED, ATM_EV_COLLECT_DONE, ATM_ST_LISTENING},
    {ATM_ST_EXITED, ATM_EV_TRANSFER_START, ATM_ST_RECEIVING},
    {ATM_ST_FAILED, ATM_EV_DIAGNOSTICS_READY, ATM_ST_DIAGNOSTICS_AVAILABLE},
    {ATM_ST_FAILED, ATM_EV_COLLECT_DONE, ATM_ST_LISTENING},
    {ATM_ST_FAILED, ATM_EV_TRANSFER_START, ATM_ST_RECEIVING},
    {ATM_ST_DIAGNOSTICS_AVAILABLE, ATM_EV_COLLECT_DONE, ATM_ST_LISTENING},
    {ATM_ST_DIAGNOSTICS_AVAILABLE, ATM_EV_TRANSFER_START, ATM_ST_RECEIVING},
    {ATM_ST_RECOVERY_PENDING, ATM_EV_RECOVERED, ATM_ST_LISTENING},
    {ATM_ST_RECOVERY_PENDING, ATM_EV_RECOVERY_FAILED, ATM_ST_RECOVERY_FAILED},
    {ATM_ST_RECOVERY_PENDING, ATM_EV_REBOOT, ATM_ST_REBOOT_PENDING},
    {ATM_ST_RECOVERY_PENDING, ATM_EV_APP_EXITED, ATM_ST_EXITED},
    {ATM_ST_RECOVERY_FAILED, ATM_EV_CONTROLLER_RESTART, ATM_ST_INITIALIZING},
    {ATM_ST_RECOVERY_FAILED, ATM_EV_REBOOT, ATM_ST_REBOOT_PENDING},
    {ATM_ST_RECOVERY_FAILED, ATM_EV_DISABLE, ATM_ST_DISABLED},
    {ATM_ST_REBOOT_PENDING, ATM_EV_REBOOTED, ATM_ST_RECONNECTING},
    {ATM_ST_RECONNECTING, ATM_EV_RECONNECTED, ATM_ST_LISTENING},
    {ATM_ST_RECONNECTING, ATM_EV_RECONNECT_FAILED, ATM_ST_RECOVERY_FAILED},
    {ATM_ST_RECONNECTING, ATM_EV_DIAGNOSTICS_READY, ATM_ST_DIAGNOSTICS_AVAILABLE},
};
bool atm_sm_fire(AtmState *state, AtmEvent ev) {
    AtmState cur = *state;
    /* idempotent duplicates */
    if ((cur == ATM_ST_LISTENING || cur == ATM_ST_STAGED) &&
        (ev == ATM_EV_HEARTBEAT || ev == ATM_EV_HELLO_SENT)) return true;
    if (cur == ATM_ST_RUNNING && (ev == ATM_EV_HEARTBEAT || ev == ATM_EV_TELEMETRY_RECEIVED)) return true;
    if (cur == ATM_ST_DISABLED && ev == ATM_EV_DISABLE) return true;
    for (size_t i = 0; i < sizeof(TRANS) / sizeof(TRANS[0]); i++)
        if (TRANS[i].from == cur && TRANS[i].ev == ev) { *state = TRANS[i].to; return true; }
    return false;
}

/* ---- run record ---- */
size_t atm_run_record_format(const AtmRunRecord *r, char *out, size_t cap) {
    /* Bounded manual formatting (no snprintf dependency in sysmodule core). */
    size_t n = 0;
    #define APP(...) do { if (n < cap) { int w = snprintf(out + n, cap - n, __VA_ARGS__); if (w < 0) return 0; n += (size_t)w; if (n >= cap) return 0; } else return 0; } while (0)
    APP("atm_run_record v=%d\n", ATM_RUN_RECORD_VERSION);
    APP("seq=%lu\n", (unsigned long)r->seq);
    APP("run_id=%s\n", r->run_id);
    APP("session=%08lx\n", (unsigned long)r->session);
    APP("device_id=%s\n", r->device_id);
    APP("file=%s\n", r->filename);
    APP("size=%llu\n", (unsigned long long)r->size);
    APP("sha256=%s\n", r->sha256);
    APP("hash_check=%s\n", r->hash_check);
    APP("build_id=%s\n", r->build_id);
    APP("firmware_id=%s\n", r->firmware_id);
    APP("scenario=%s\n", r->scenario);
    APP("crash_class=%s\n", r->crash_class);
    APP("exception_path=%s\n", r->exception_path);
    APP("persistence=%s\n", r->persistence);
    APP("dump_ref=%s\n", r->dump_ref);
    APP("recovery=%s\n", r->recovery);
    APP("state=%s\n", r->state);
    APP("launch_result=0x%08lx\n", (unsigned long)r->launch_result);
    APP("failure=%s\n", r->failure);
    APP("diagnostics=%s\n", r->diagnostics);
    { uint32_t ck = atm_fnv1a(out, n);
      APP("checksum=%08lx\n", (unsigned long)ck); }
    #undef APP
    return n;
}
bool atm_record_launchable(const AtmRunRecord *r) {
    if (strcmp(r->state, "staged") != 0) return false;
    if (strlen(r->sha256) != 64) return false;
    for (int i = 0; i < 64; i++) {
        char c = r->sha256[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}


/* ---- recovery + correlation ---- */
AtmDumpCorrelation atm_correlate_dump(bool prev_run_interrupted, bool autosave_status_ok,
                                      uint64_t run_start_ms, uint64_t dump_mtime_ms) {
    if (!prev_run_interrupted) return ATM_DUMP_NOT_OURS;
    if (autosave_status_ok) return ATM_DUMP_CORRELATED;
    if (dump_mtime_ms != 0 && run_start_ms != 0 && dump_mtime_ms >= run_start_ms)
        return ATM_DUMP_UNCERTAIN;
    return ATM_DUMP_NOT_OURS;
}
AtmRecoveryAction atm_recovery_decide(int consecutive_failures, int max_attempts) {
    if (max_attempts <= 0) max_attempts = 3;
    return consecutive_failures >= max_attempts ? ATM_RECOVERY_STOPPED : ATM_RECOVERY_CONTINUE;
}
bool atm_run_was_interrupted(const char *persisted_state) {
    if (!persisted_state) return false;
    return strcmp(persisted_state, "running") == 0 ||
           strcmp(persisted_state, "launch_pending") == 0 ||
           strcmp(persisted_state, "receiving") == 0 ||
           strcmp(persisted_state, "verifying") == 0;
}

/* ---- frame stats ---- */
void atm_frame_init(AtmFrameStats *f) { memset(f, 0, sizeof(*f)); f->min_us = 0xFFFFFFFFu; }
void atm_frame_add(AtmFrameStats *f, uint32_t frame_us, uint32_t budget_us) {
    unsigned idx = frame_us / 1000;
    if (idx >= ATM_FRAME_BUCKETS) idx = ATM_FRAME_BUCKETS - 1;
    f->buckets[idx]++;
    f->count++;
    f->sum_us += frame_us;
    if (frame_us < f->min_us) f->min_us = frame_us;
    if (frame_us > f->max_us) f->max_us = frame_us;
    if (budget_us && frame_us > budget_us) f->over_budget++;
}
uint32_t atm_frame_percentile_us(const AtmFrameStats *f, unsigned percentile) {
    if (!f->count) return 0;
    uint64_t need = (f->count * percentile + 99) / 100;
    if (need == 0) need = 1;
    uint64_t acc = 0;
    for (unsigned i = 0; i < ATM_FRAME_BUCKETS; i++) {
        acc += f->buckets[i];
        if (acc >= need) return (i + 1) * 1000;
    }
    return ATM_FRAME_BUCKETS * 1000;
}
size_t atm_frame_format_summary(const AtmFrameStats *f, char *out, size_t cap) {
    if (!f->count) {
        int w = snprintf(out, cap, "@n3dl frames=0\n");
        return w < 0 ? 0 : (size_t)w;
    }
    int w = snprintf(out, cap,
        "@n3dl frames=%llu avg_us=%llu p50_us=%lu p95_us=%lu p99_us=%lu max_us=%lu over_budget=%lu min_us=%lu\n",
        (unsigned long long)f->count,
        /* R11: cold once-per-run summary; f64 divide is exact here
         * (sum_us/count magnitudes << 2^53) and lowers to vdiv.f64. */
        (unsigned long long)((double)f->sum_us / (double)f->count),
        (unsigned long)atm_frame_percentile_us(f, 50),
        (unsigned long)atm_frame_percentile_us(f, 95),
        (unsigned long)atm_frame_percentile_us(f, 99), (unsigned long)f->max_us,
        (unsigned long)f->over_budget,
        (unsigned long)(f->min_us == 0xFFFFFFFFu ? 0 : f->min_us));
    return w < 0 ? 0 : (size_t)w;
}


long atm_record_seq_if_valid(const char *text) {
    if (!text) return -1;
    const char *ck = strstr(text, "\nchecksum=");
    if (!ck) return -1;
    char *end = NULL;
    unsigned long stored = strtoul(ck + 10, &end, 16);
    if (end == ck + 10) return -1;
    if (atm_fnv1a(text, (size_t)(ck + 1 - text)) != (uint32_t)stored) return -1;
    const char *sq = strstr(text, "seq=");
    if (!sq) return -1;
    long seq = strtol(sq + 4, &end, 10);
    return end == sq + 4 ? -1 : seq;
}
int atm_record_choose(const char *a, const char *b) {
    long sa = atm_record_seq_if_valid(a), sb = atm_record_seq_if_valid(b);
    if (sa < 0 && sb < 0) return -1;
    if (sb < 0) return 0;
    if (sa < 0) return 1;
    return sb > sa ? 1 : 0; /* deterministic tie-break: slot A */
}
bool atm_retention_should_delete(bool acked, unsigned acked_newer_count, unsigned keep_acked) {
    return acked && acked_newer_count >= keep_acked;
}

/* ---- crash-dump storage capacity reserve ---- */
bool atm_capacity_free_bytes(uint32_t free_clusters, uint32_t cluster_size,
                             uint32_t total_clusters, uint64_t *out) {
    if (cluster_size == 0 || total_clusters == 0 || free_clusters > total_clusters)
        return false;
    if (out) *out = (uint64_t)free_clusters * (uint64_t)cluster_size;
    return true;
}
static uint64_t atm_round_up_clusters(uint64_t bytes, uint32_t cluster_size) {
    if (cluster_size <= 1) return bytes;
    uint64_t c = cluster_size;
    /* R11: cold dump-time capacity math; exact for magnitudes << 2^53. */
    return (uint64_t)((double)(bytes + c - 1) / (double)c) * c;
}
uint64_t atm_capacity_required(uint64_t pending_bytes, uint32_t cluster_size) {
    uint64_t reserve = ATM_DUMP_MAX_BYTES + ATM_CRASH_RESERVE_MARGIN_BYTES;
    return atm_round_up_clusters(reserve, cluster_size) +
           atm_round_up_clusters(pending_bytes, cluster_size);
}
AtmCapacity atm_capacity_check(bool have_free, uint64_t free_bytes,
                               uint64_t pending_bytes, uint32_t cluster_size,
                               uint64_t *required_out) {
    uint64_t required = atm_capacity_required(pending_bytes, cluster_size);
    if (required_out) *required_out = required;
    if (!have_free) return ATM_CAP_UNKNOWN;
    return free_bytes >= required ? ATM_CAP_OK : ATM_CAP_BLOCKED;
}
