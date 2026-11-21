/* ============================================================================
 * 02_level_cache.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Persistent level geometry (PersistCache: mono-frame level walls kept
 * converted in retained linear buffers), the vertex scratch and the
 * colour-to-float table (VertexScratch), and bind_vertex_buffer.
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/* ---- Persistent level geometry ----
 * Level-wall (VERT_OWNER_LEVEL) texgroups are converted once into
 * retained linear buffers and resubmitted every frame without
 * reconversion or scratch traffic. Only meshes whose content is
 * stable across frames qualify (memoEpochPass == SMEMO_PASS_UNSET;
 * XLight1Group marks white-out / underwater meshes PER_FRAME).
 * Validity is keyed by smemo_persist_gen(): level texture-animation
 * steps bump it (reconvert into the same allocations). The buffers are
 * freed when their level is released (c3d_release_level_cache).
 * Allocation failure or table pressure falls back to the scratch
 * path — never stale data, never a skipped draw. */
constexpr int PERSIST_BITS = 11;
constexpr auto PERSIST_SIZE = (1u << PERSIST_BITS);
constexpr auto PERSIST_MASK = (PERSIST_SIZE - 1u);
/* Linear memory the cache always leaves free (see PersistCache::store). */
constexpr auto PERSIST_LINEAR_RESERVE = (2u * 1024u * 1024u);
struct persist_entry_t {
	uintptr_t ro;
	int sub;
	gpu_vertex_t *buf;
	int cap;
	int count;
	unsigned gen;
};

namespace {

/* The retained level-wall buffers (one instance, g_persist): the open-
 * addressed table of converted ranges, the linear memory they hold, and
 * the per-frame gate. */
class PersistCache {
	public:
	/* Out of line, as persist_find was: inlined, both copies would land
	 * in draw_render_object. */
	[[gnu::noinline]] persist_entry_t *find(const void *ro, int sub, int create);  // was persist_find
	gpu_vertex_t *lookup(const void *ro, int sub, int *count);   // was persist_lookup
	gpu_vertex_t *store(const void *ro, int sub,
		const gpu_vertex_t *src, int count);                     // was persist_store
	void free_all(void);                                         // was persist_free_all

	unsigned bytes = 0;
	/* Runtime gate (owner policy 2026-11-14): persist pays in mono
	 * (-8.6% measured) and is wall-flat in stereo, where its stills are
	 * only LSB-equivalent across eyes. So it is active only in mono frames
	 * (slider down; see the stereo_frame gate in FSBeginScene), refreshed at
	 * every FSBeginScene; stereo keeps the memo-only path.
	 * Buffers stay allocated across slider moves and simply go unused
	 * in stereo (generation unchanged, still valid if mono resumes). */
	int active = 0;
	/* Static: its own symbol, as the old table was. In the instance, find
	 * needs an add for its offset on every probe (72 -> 75), or bytes and
	 * active sit 48 KB in, past the reach of a load offset. */
	static persist_entry_t entries[PERSIST_SIZE];
};

persist_entry_t PersistCache::entries[PERSIST_SIZE] = {};

PersistCache g_persist;

} // namespace

static unsigned persist_hash(uintptr_t ro, int sub)
{
	uint64_t z = (uint64_t)ro * 0x9E3779B97F4A7C15ULL + (uint64_t)sub;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	{
		unsigned h = (z ^ (z >> 31)) & PERSIST_MASK;
		return h;
	}
}

persist_entry_t *PersistCache::find(const void *ro, int sub, int create)
{
	unsigned i = persist_hash((uintptr_t)ro, sub);
	unsigned n;
	for (n = 0; n < 32; n++) {
		persist_entry_t *e = &entries[(i + n) & PERSIST_MASK];
		if (e->ro == (uintptr_t)ro && e->sub == sub)
			return e;
		if (create && e->buf == nullptr) {
			e->ro = (uintptr_t)ro;
			e->sub = sub;
			return e;
		}
	}
	return nullptr;
}

gpu_vertex_t *PersistCache::lookup(const void *ro, int sub, int *count)
{
	persist_entry_t *e = find(ro, sub, 0);
	if (e && e->buf && e->gen == smemo_persist_gen()) {
		*count = e->count;
		return e->buf;
	}
	return nullptr;
}

/* Retain a freshly converted range. Returns the retained buffer or
 * NULL (pressure/alloc failure — caller submits from scratch). */
gpu_vertex_t *PersistCache::store(const void *ro, int sub,
								   const gpu_vertex_t *src, int count)
{
	persist_entry_t *e = find(ro, sub, 1);
	if (!e)
		return nullptr;
	if (e->cap < count) {
		if (e->buf) {
			bytes -= e->cap * sizeof(gpu_vertex_t);
			linearFree(e->buf);
			e->buf = nullptr;
			e->cap = 0;
		}
		/* Never let the cache take the linear memory a texture or
		 * vertex buffer needs: refuse (the group converts per frame as
		 * before) unless PERSIST_LINEAR_RESERVE stays free. Checked
		 * only when allocating, never on a refill or a hit. */
		if (linearSpaceFree() < count * sizeof(gpu_vertex_t) + PERSIST_LINEAR_RESERVE)
			return nullptr;
		e->buf = (gpu_vertex_t *)linearAlloc(count * sizeof(gpu_vertex_t));
		if (!e->buf)
			return nullptr;
		e->cap = count;
		bytes += count * sizeof(gpu_vertex_t);
	}
	memcpy(e->buf, src, count * sizeof(gpu_vertex_t));
	e->count = count;
	e->gen = smemo_persist_gen();
	return e->buf;
}

void PersistCache::free_all(void)
{
	unsigned i;
	for (i = 0; i < PERSIST_SIZE; i++) {
		if (entries[i].buf) {
			linearFree(entries[i].buf);
			entries[i].buf = nullptr;
			entries[i].cap = 0;
			entries[i].count = 0;
			entries[i].gen = 0;
		}
	}
	bytes = 0;
}


/* Scratch buffer for vertex conversion.
 * 4MB = ~116000 verts at 36 bytes each.  Needs to handle heavy
 * firefight frames (many ships + projectiles + explosion particles +
 * laser beams + mipped BSP walls) without FrameSplit stalls or
 * silent geometry drops at the overflow-after-flush fallback.
 *
 * Same value on both the OG-tier and N3DS-tier paths of the unified
 * CIA. On OG (22 MB linear heap) this is ~18% of the budget — tight
 * but validated working on real hardware. */
constexpr auto GPU_SCRATCH_SIZE = (4 * 1024 * 1024);

namespace {

/* The vertex conversion scratch (one instance, g_scratch): the 4 MB linear
 * buffer, how much of it this frame has used, and the colour-to-float
 * table the conversion reads. */
class VertexScratch {
	public:
	void c255_init(void);

	gpu_vertex_t *scratch = nullptr;
	int scratchUsed = 0;
	int scratchMax = 0;

	/* Byte-to-float color table: replaces 4 vdiv.f32 per converted vertex
	 * (measured 7.4 ms/frame on Old 3DS, 12.3K verts/frame) with 4 L1D
	 * reads. Bit-identical output: each entry IS ((float)b)/255.0f, the same
	 * correctly-rounded quotient the loop computed, so pixels are unchanged
	 * (pixel-verified). 1 KiB, stays hot in L1D.
	 * Static: its own symbol, as the old table was, so the per-vertex
	 * loops index it from its own base. In the instance it held a register
	 * as g_scratch's base for the whole of draw_render_object and moved a
	 * value to the stack in the per-texgroup flush loops. */
	static float c255[256];
	int c255_ok = 0;
};

float VertexScratch::c255[256] = {};

VertexScratch g_scratch;

} // namespace

/* Point the vertex attribute buffer at v for the next draw. citro3d
 * re-sends the buffer config only when C3D_GetBufInfo() has marked it
 * dirty, and the draw that sends it clears the mark (C3Di_UpdateContext),
 * so every draw needs its own C3D_GetBufInfo(): editing the struct after
 * a draw is silently ignored and the next draw reads the previous
 * buffer. That is what drew corrupt sprite quads in the right eye once
 * FLUSH_BATCH split a batch into several runs. */
static inline void bind_vertex_buffer(const gpu_vertex_t *v)
{
	C3D_BufInfo *bi = C3D_GetBufInfo();
	BufInfo_Init(bi);
	BufInfo_Add(bi, v, sizeof(gpu_vertex_t), 3, 0x210);
}

void VertexScratch::c255_init(void)
{
	int b;
	if (c255_ok)
		return;
	for (b = 0; b < 256; b++)
		c255[b] = (float)b / 255.0f;
	c255_ok = 1;
}
