/*
 * atm_core - shared, platform-independent ATM logic.
 *
 * This translation unit contains NO 3DS, socket, filesystem or threading
 * code. It is compiled into Rosalina (ARM11) and into the host conformance
 * test binary (tests/atm_core_test.c) from the SAME source, so the fake
 * device / host tests and the production PoC cannot silently diverge on:
 *   - 3dslink prologue parsing and filename sanitisation
 *   - N3DL v1 header pack/unpack, FNV-1a, sequence spaces
 *   - controller state-machine transition rules
 *   - transfer size/hash validation and run-record formatting
 * Platform glue (sockets, FS, PM) lives in atm_poc.c and is NOT shared;
 * it is verified by cross-compilation + hardware tests, not by this core.
 *
 * Resource contract (docs/old-3ds-resource-budget.md): no allocation in
 * this file scales with payload size; all buffers are caller-owned and
 * bounded by the constants below.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ATM_PORT_3DSLINK_TCP   17491
#define ATM_PORT_DISCOVERY_UDP 17491
#define ATM_PORT_HTTP_TCP      8080
#define ATM_PORT_N3DL_DAEMON   8081   /* device -> host (host binds) */
#define ATM_PORT_N3DL_CMD      8082   /* host -> device commands */
#define ATM_ZLIB_FRAGMENT_MAX  (16 * 1024)  /* wire compat with stub/3dslink */
#define ATM_STAGE_BUFFER       (64 * 1024)  /* ATM decision, NOT a stub change */
#define ATM_NAME_MAX           128
#define ATM_PROJECT_ID         "luma_atm"

/* ---- 3dslink prologue -------------------------------------------------- */
/* Parse result mirrors netload_stub/main.c ST_HEADER behaviour. */
typedef enum {
    ATM_HDR_INCOMPLETE = 0,  /* need more bytes */
    ATM_HDR_OK,
    ATM_HDR_BAD_NAMELEN,     /* namelen <=0 or >=256: drop, no response word */
    ATM_HDR_BAD_NAME         /* sanitisation failed: drop, no response word */
} AtmHeaderStatus;

typedef struct {
    uint32_t filelen;        /* advertised uncompressed size (wire value) */
    char     name[ATM_NAME_MAX]; /* sanitised basename, NUL-terminated */
    bool     is_3dsx;
} AtmPrologue;

/* buf holds bytes accumulated so far. On ATM_HDR_OK, *consumed is the
 * prologue length (4+namelen+4); any extra bytes belong to the zlib stream. */
AtmHeaderStatus atm_parse_prologue(const uint8_t *buf, size_t len,
                                   AtmPrologue *out, size_t *consumed);
/* Mirror of stub sanitize_name(): basename only; rejects empty, "..",
 * control chars and : * ? " < > | ; result must fit ATM_NAME_MAX. */
bool atm_sanitize_name(const char *in, char *out, size_t out_sz);

/* ---- Restricted HTTP path policy (design 9.7) ---------------------------
 * Pure decision function for the read-only diagnostic HTTP pull service.
 * GET/HEAD only; percent-decoding happens BEFORE validation so encoded
 * traversal cannot sneak past; any ".." segment is forbidden; ATM
 * configuration files (netlog.txt, device_id, gate files, expected
 * digest) are never served, under any prefix. Servable: generated
 * status ("/", the ATM status path, and the stub-compatible status
 * path the host liveness probe uses), files under /luma/atm/ (run
 * records) and /luma/dumps/, run logs (log/txt files under /3ds/, log
 * at the SD root). Binaries and everything else: forbidden. */
typedef enum {
    ATM_HTTP_SERVE_FILE = 0,
    ATM_HTTP_SERVE_STATUS,
    ATM_HTTP_BAD_REQUEST,    /* 400 */
    ATM_HTTP_FORBIDDEN,      /* 403 */
    ATM_HTTP_METHOD,         /* 405 */
    ATM_HTTP_SERVE_ARTIFACTS /* generated pending-artifact listing */
} AtmHttpDecision;
/* raw_path is the raw request target (query allowed, stripped here).
 * On ATM_HTTP_SERVE_FILE, clean_out receives the normalised SD path
 * including the leading '/'. head_only (if non-NULL) is set for HEAD. */
AtmHttpDecision atm_http_decide(const char *method, const char *raw_path,
                                char *clean_out, size_t clean_cap,
                                bool *head_only);

/* ---- SHA-256 (streaming, bounded state) -------------------------------- */
typedef struct { uint32_t h[8]; uint64_t total; uint8_t block[64]; size_t used; } AtmSha256;
void atm_sha256_init(AtmSha256 *s);
void atm_sha256_update(AtmSha256 *s, const void *data, size_t len);
void atm_sha256_final(AtmSha256 *s, uint8_t out32[32]);
void atm_hex32(const uint8_t in32[32], char out65[65]);

/* ---- Authorization policy (single source of truth, audit Phase 2) ------
 * Every remote state-changing operation has an explicit policy. The
 * device-enrollment identity is NOT authentication (N3DL v1 and 3dslink
 * are unauthenticated); these gates prevent *accidental* operations on
 * an enrolled console and make each operation opt-in separately. */
typedef enum {
    ATM_OP_RECEIVE = 0,        /* accept + stage a deployment */
    ATM_OP_LAUNCH,             /* launch a staged application */
    ATM_OP_TERMINATE_RUNNING,  /* terminate the currently running app */
    ATM_OP_TERMINATE_RUN,      /* terminate the ATM-launched run */
    ATM_OP_CMD_DIAGNOSTIC,     /* hello/selftest/status: read-only class */
    ATM_OP_CMD_EXPECT_DIGEST,  /* set one-shot expected SHA-256 */
    ATM_OP_CMD_ACK_DUMP,       /* host confirms a dump is synchronized */
    ATM_OP_CMD_SCREENSHOT,     /* capture framebuffers (writes artifacts) */
    ATM_OP_CMD_ACK_SHOT,       /* host confirms a screenshot set is synchronized */
    ATM_OP_CMD_REBOOT,         /* intentional clean-state console reboot */
    ATM_OP_CMD_FWUPDATE        /* replace /boot.firm from a staged asset */
} AtmOp;
typedef struct {
    bool enrolled;        /* enabled + device_id present */
    bool deployment;      /* sdmc:/luma/atm/allow_deployment */
    bool launch;          /* covered by allow_remote_control */
    bool control;         /* sdmc:/luma/atm/allow_remote_control */
} AtmAuth;
bool atm_auth_permitted(const AtmAuth *a, AtmOp op);

/* ---- Transfer validation ------------------------------------------------ */
/* Feed decompressed bytes as they are written to staging. Validation only
 * passes at atm_transfer_finish() with stream_ended=true, byte count equal
 * to the advertised filelen (when enforce_length) and <= max_payload. */
typedef struct {
    uint64_t written;
    uint32_t advertised;
    uint64_t max_payload;
    bool     enforce_length;
    bool     has_expected;       /* out-of-band expected digest supplied */
    char     expected[65];       /* host-supplied SHA-256 hex, or "" */
    AtmSha256 sha;
} AtmTransferValidator;
void atm_transfer_init(AtmTransferValidator *v, uint32_t advertised,
                       uint64_t max_payload, bool enforce_length);
/* expected_hex: 64 lowercase hex chars from an out-of-band channel
 * (operator-provisioned file or gated CMD), NOT from the 3dslink stream
 * (the protocol carries no digest). NULL/"" = fingerprint-only mode. */
void atm_transfer_init_ex(AtmTransferValidator *v, uint32_t advertised,
                          uint64_t max_payload, bool enforce_length,
                          const char *expected_hex);
bool atm_valid_sha256_hex(const char *s);
bool atm_transfer_feed(AtmTransferValidator *v, const void *data, size_t len); /* false: size exceeded */
/* Returns NULL on success, else a static error tag
 * ("size_exceeded"/"size_mismatch"/"incomplete_stream"/"hash_mismatch").
 * "hash_mismatch" is only possible when an expected digest was supplied;
 * without one, the computed digest is a fingerprint, not verification. */
const char *atm_transfer_finish(AtmTransferValidator *v, bool stream_ended, char sha_hex65[65]);

/* ---- N3DL v1 ------------------------------------------------------------ */
#define ATM_N3DL_MAGIC   0x4C44334Eu
#define ATM_N3DL_VERSION 1
#define ATM_N3DL_HDR     24
#define ATM_N3DL_MTU     1400
typedef enum {
    ATM_N3DL_TEXT = 1, ATM_N3DL_BLOB_BEGIN = 2, ATM_N3DL_BLOB_CHUNK = 3,
    ATM_N3DL_BLOB_END = 4, ATM_N3DL_HELLO = 5, ATM_N3DL_STAT = 6,
    ATM_N3DL_CMD = 0x8001, ATM_N3DL_CMDACK = 0x8002
} AtmN3dlType;

uint32_t atm_fnv1a(const void *data, size_t len);
/* Returns total packet size, or 0 if payload exceeds MTU budget. */
size_t atm_n3dl_pack(uint8_t *out, size_t out_cap, uint16_t type, uint32_t session,
                     uint32_t stream, uint32_t seq, const void *payload, uint32_t len);
typedef struct { uint16_t version, type; uint32_t session, stream, seq, len; } AtmN3dlHeader;
/* 0 ok, -1 not ours (caller must drop silently), -2 ours but malformed. */
int atm_n3dl_unpack(const uint8_t *pkt, size_t n, AtmN3dlHeader *hdr, const uint8_t **payload);
/* Sequence spaces: text and ctl are independent counters on stream 0. */
typedef struct { uint32_t text, ctl, next_blob; } AtmN3dlSeq;
uint32_t atm_seq_next_text(AtmN3dlSeq *s);
uint32_t atm_seq_next_ctl(AtmN3dlSeq *s);
/* Command dedupe: ATM-only hardening (the stub does NOT dedupe; commands
 * arrive repeated 3x from n3dlctl). Returns true if (session,seq) is new. */
typedef struct { uint32_t session[8]; uint32_t seq[8]; unsigned idx; } AtmCmdDedupe;
bool atm_cmd_is_new(AtmCmdDedupe *d, uint32_t session, uint32_t seq);

/* ---- Application-exit observer (Phase 3) ----------------------------------
 * Classifies poll observations. A failed query is NEVER exit evidence.
 * Exit requires positive evidence: ATM_CONFIRM_SAMPLES consecutive
 * observations of "no application" (a pm:dbg NOTFOUND answer, which is a
 * successful query) or of a different process while the launched pid is
 * absent. Short-lived apps that start+exit inside one poll interval and
 * are never observed at all yield ATM_OBS_NEVER_OBSERVED after the
 * deadline, i.e. UNKNOWN, not EXITED. */
#define ATM_CONFIRM_SAMPLES 2
#define ATM_ERROR_LIMIT     5        /* consecutive query errors */
#define ATM_OBSERVE_DEADLINE_MS 15000
typedef enum {
    ATM_Q_RUNNING = 0,  /* query answered: an app runs with this pid */
    ATM_Q_NO_APP,       /* query answered: positively no application */
    ATM_Q_NEW_PID,      /* query answered: a different process runs */
    ATM_Q_ERROR         /* query failed: no information */
} AtmQueryKind;
typedef enum {
    ATM_OBS_RUNNING = 0, ATM_OBS_EXIT_CONFIRMED, ATM_OBS_IDENTITY_CHANGED,
    ATM_OBS_QUERY_ERROR, ATM_OBS_PERSISTENT_FAILURE, ATM_OBS_NEVER_OBSERVED,
    ATM_OBS_UNKNOWN
} AtmObsOutcome;
typedef struct {
    uint32_t launched_pid;
    bool     seen;
    int      gone_streak;
    int      err_streak;
    bool     done;
} AtmObserver;
void atm_observer_init(AtmObserver *o, uint32_t launched_pid);
/* now_ms/launched_ms are caller-supplied clocks (deterministic tests). */
AtmObsOutcome atm_observer_note(AtmObserver *o, AtmQueryKind kind,
                                uint32_t pid, uint64_t now_ms, uint64_t launched_ms);

/* ---- Controller state machine (executable spec, see docs) ---------------- */
typedef enum {
    ATM_ST_DISABLED = 0, ATM_ST_INITIALIZING, ATM_ST_LISTENING, ATM_ST_RECEIVING,
    ATM_ST_VERIFYING, ATM_ST_STAGED, ATM_ST_LAUNCH_PENDING, ATM_ST_RUNNING,
    ATM_ST_EXITED, ATM_ST_FAILED, ATM_ST_RECOVERY_PENDING, ATM_ST_REBOOT_PENDING,
    ATM_ST_RECONNECTING, ATM_ST_DIAGNOSTICS_AVAILABLE, ATM_ST_RECOVERY_FAILED,
    ATM_ST_COUNT
} AtmState;
typedef enum {
    ATM_EV_ENABLE = 0, ATM_EV_READY, ATM_EV_INIT_FAILED, ATM_EV_DISABLE,
    ATM_EV_TRANSFER_START, ATM_EV_TRANSFER_COMPLETE, ATM_EV_TRANSFER_FAILED,
    ATM_EV_DISCONNECT, ATM_EV_VERIFIED, ATM_EV_VERIFY_FAILED, ATM_EV_DISCARD,
    ATM_EV_LAUNCH_REQUEST, ATM_EV_LAUNCHED, ATM_EV_LAUNCH_REJECTED, ATM_EV_LAUNCH_TIMEOUT,
    ATM_EV_APP_EXITED, ATM_EV_APP_FAILED, ATM_EV_HANG_DETECTED, ATM_EV_RECOVERED,
    ATM_EV_RECOVERY_FAILED, ATM_EV_REBOOT, ATM_EV_REBOOTED, ATM_EV_RECONNECTED,
    ATM_EV_RECONNECT_FAILED, ATM_EV_DIAGNOSTICS_READY, ATM_EV_COLLECT_DONE,
    ATM_EV_CONTROLLER_RESTART, ATM_EV_SLEEP, ATM_EV_WAKE, ATM_EV_HEARTBEAT,
    ATM_EV_HELLO_SENT, ATM_EV_TELEMETRY_RECEIVED, ATM_EV_DEVICE_LOST, ATM_EV_COUNT
} AtmEvent;
const char *atm_state_name(AtmState s);
const char *atm_event_name(AtmEvent e);
/* true if the event is valid (state updated); false if invalid (unchanged);
 * idempotent duplicate events are valid no-ops. */
bool atm_sm_fire(AtmState *state, AtmEvent ev);

/* ---- Run record (key=value journal, one flush per transition) ----------
 * v3 adds firmware_id (compile-time build identity of this firmware),
 * exception_path (fatal|errdisp|unknown|none) and persistence
 * (persisted|failed_or_missing|n/a) so both Luma exception paths
 * report through one manifest. Readers must use atm_record_seq_if_valid
 * (checksum), never positional parsing, so v2 journals stay readable
 * field-by-field with the new keys absent. */
#define ATM_RUN_RECORD_VERSION 3
typedef struct {
    uint32_t seq;            /* journal sequence: increases on every persist */
    char     run_id[24];
    uint32_t session;
    char     device_id[32];
    char     filename[ATM_NAME_MAX];
    uint64_t size;
    char     sha256[65];
    char     hash_check[20];  /* "expected" (compared) or "fingerprint_only" */
    char     build_id[40];   /* app build identifier, host-supplied via 3dslink cmdline */
    char     firmware_id[40];/* firmware build identifier, compile-time */
    char     scenario[40];    /* named test workload, host-supplied */
    char     crash_class[32];/* failure classification or "none" */
    char     exception_path[16]; /* fatal|errdisp|unknown|none */
    char     persistence[20]; /* persisted|failed_or_missing|n/a */
    char     dump_ref[48];    /* correlated crash dump filename or "none" */
    char     recovery[32];    /* recovery outcome or "none" */
    char     state[24];
    int32_t  launch_result;   /* raw service Result, 0 = success/unprobed */
    char     failure[32];     /* error tag or "none" */
    char     diagnostics[64]; /* e.g. luma/dumps path or "none" */
} AtmRunRecord;
/* Serialises into out (bounded). Returns length, or 0 on overflow. */
size_t atm_run_record_format(const AtmRunRecord *r, char *out, size_t cap);
/* Launchability rule, single source of truth: a record is launchable only
 * in state "staged" with a complete 64-hex hash and nonzero size or an
 * explicitly empty-but-verified payload marker. */
bool atm_record_launchable(const AtmRunRecord *r);
/* Dual-slot journal validity/selection. A slot text is valid only if its
 * trailing checksum line matches the FNV-1a of everything before it (an
 * interrupted persist therefore invalidates only the slot being written;
 * the other slot remains the last known-good journal). */
long atm_record_seq_if_valid(const char *text); /* seq, or -1 if invalid/corrupt */
int atm_record_choose(const char *a, const char *b); /* 0=A, 1=B, -1=none */

/* Dump retention decision (pure): iterate newest-first; an acked dump may
 * be deleted only when at least keep_acked *newer acked* dumps are kept.
 * Unacked dumps are never deletable, regardless of count pressure. */
#define ATM_DUMP_KEEP_ACKED 8
bool atm_retention_should_delete(bool acked, unsigned acked_newer_count, unsigned keep_acked);

/* ---- Crash-dump storage capacity reserve ---------------------------------
 * ATM must stop accepting work before the SD card can no longer hold a
 * future crash dump: evidence preservation outranks starting another
 * run. The reserve is sized from the largest dump the arm9 auto-save
 * accepts (atmTryAutoSaveDump rejects totalSize > 32 MiB; keep
 * ATM_DUMP_MAX_BYTES in sync - a static test pins both) plus an
 * explicit margin covering: cluster slack on the dump file itself, the
 * auto_dump_status.txt write, run-journal/dump_sync rewrites, FAT and
 * directory updates, and bounded staleness in the free-space figure
 * (the FS service's free-cluster freshness is UNVERIFIED on hardware).
 * The margin does NOT protect against a running application writing
 * unbounded data after the check (documented limitation); it only
 * guarantees the reserve existed at deploy/launch time.
 * Capacity that cannot be measured is treated as blocked (fail
 * closed), never as available. */
#define ATM_DUMP_MAX_BYTES             (32ull * 1024 * 1024)
#define ATM_CRASH_RESERVE_MARGIN_BYTES (2ull * 1024 * 1024)
typedef enum { ATM_CAP_OK = 0, ATM_CAP_BLOCKED, ATM_CAP_UNKNOWN } AtmCapacity;
/* Convert an FS archive-resource report to free bytes. Returns false
 * (report not trustworthy) on cluster_size==0, total_clusters==0 or
 * free_clusters>total_clusters. */
bool atm_capacity_free_bytes(uint32_t free_clusters, uint32_t cluster_size,
                             uint32_t total_clusters, uint64_t *out);
/* Bytes that must be free before accepting pending_bytes of new data:
 * the crash reserve plus the pending write, each rounded up to whole
 * clusters (a file occupies whole clusters). cluster_size 0 = no
 * rounding information, treated as 1. */
uint64_t atm_capacity_required(uint64_t pending_bytes, uint32_t cluster_size);
/* have_free=false (free space could not be measured) yields
 * ATM_CAP_UNKNOWN regardless of free_bytes. *required_out (when
 * non-NULL) always receives atm_capacity_required(). */
AtmCapacity atm_capacity_check(bool have_free, uint64_t free_bytes,
                               uint64_t pending_bytes, uint32_t cluster_size,
                               uint64_t *required_out);

/* ---- Interrupted-run recovery + dump correlation --------------------------
 * Correlation is evidence-ranked, never guessed: a dump is CORRELATED
 * only when the arm9 auto-save status names it AND the previous run was
 * interrupted; timestamps alone give at most UNCERTAIN; a dump older
 * than the run start is NOT_OURS. */
typedef enum { ATM_DUMP_CORRELATED = 0, ATM_DUMP_UNCERTAIN, ATM_DUMP_NOT_OURS } AtmDumpCorrelation;
AtmDumpCorrelation atm_correlate_dump(bool prev_run_interrupted, bool autosave_status_ok,
                                      uint64_t run_start_ms, uint64_t dump_mtime_ms);
/* Bounded recovery policy: after max_consecutive interrupted/crashed
 * runs, automatic retry stops (safe stopped state), it never loops. */
typedef enum { ATM_RECOVERY_CONTINUE = 0, ATM_RECOVERY_STOPPED } AtmRecoveryAction;
AtmRecoveryAction atm_recovery_decide(int consecutive_failures, int max_attempts);
bool atm_run_was_interrupted(const char *persisted_state);

/* ---- Frame-time profiling (bounded, no duration-proportional memory) -----
 * Fixed histogram over frame-time buckets plus count/sum/min/max. Percentile
 * estimates come from bucket edges (documented approximation). The app-side
 * producer feeds samples; this structure only aggregates. */
#define ATM_FRAME_BUCKETS 32
typedef struct {
    uint32_t buckets[ATM_FRAME_BUCKETS]; /* bucket i covers [i ms,(i+1) ms), last = >=31ms overflow */
    uint64_t count;
    uint64_t sum_us;
    uint32_t min_us, max_us;
    uint32_t over_budget; /* frames > budget_us */
} AtmFrameStats;
void atm_frame_init(AtmFrameStats *f);
void atm_frame_add(AtmFrameStats *f, uint32_t frame_us, uint32_t budget_us);
uint32_t atm_frame_percentile_us(const AtmFrameStats *f, unsigned percentile);
size_t atm_frame_format_summary(const AtmFrameStats *f, char *out, size_t cap);
