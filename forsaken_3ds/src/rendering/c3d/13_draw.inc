/* ============================================================================
 * 13_draw.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * The draw functions: draw_render_object and its object / 2D / line
 * wrappers.
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/*===================================================================
	Draw functions — native citro3d path
	Submits triangles directly with C3D_DrawArrays.
===================================================================*/

/* Load the projection and modelview for this draw: the scene's matrices, or
 * a tilted orthographic screen projection with identity modelview for 2D. */
[[gnu::always_inline]] static inline void draw_set_view_matrices(bool orthographic)
{
	if (!orthographic)
	{
		upload_matrices();
	}
	else
	{
		C3D_Mtx ortho, identity;
		Mtx_OrthoTilt(&ortho, 0.0f, render_info.ThisMode.w,
			render_info.ThisMode.h, 0.0f, -1.0f, 1.0f, true);
		Mtx_Identity(&identity);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &ortho);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
	}
}

/* Sort texture groups by texture pointer so consecutive groups share
 * the same texture, maximizing draw call batching.  Uses a local index
 * array (not modifying the renderObject) and insertion sort -- fast for
 * the small N (max 64 on 3DS). */
[[gnu::always_inline]] static inline void draw_sort_texture_groups(RENDEROBJECT *renderObject, int numGroups, int (&sorted_order)[MAX_TEXTURE_GROUPS])
{
	int si, sj;
	for (si = 0; si < numGroups; si++)
		sorted_order[si] = si;
	for (si = 1; si < numGroups; si++)
	{
		int key = sorted_order[si];
		void *key_tex = (void*)renderObject->textureGroups[key].texture;
		sj = si - 1;
		while (sj >= 0 &&
			   (void*)renderObject->textureGroups[sorted_order[sj]].texture > key_tex)
		{
			sorted_order[sj + 1] = sorted_order[sj];
			sj--;
		}
		sorted_order[sj + 1] = key;
	}
}

/* Flush the current batch. Eye 1 (and memo off): one draw call. Eye 2 in
 * lookup mode, or a batch holding retained level buffers: one draw per
 * texgroup range in the batch, in walk order, with the same per-batch state.
 * Splitting only the submit (never state, order or uniforms) keeps pixels
 * identical while tolerating per-eye culling differences: a texgroup the
 * left eye never converted is simply converted live instead. */
[[gnu::always_inline]] static inline void draw_flush_batch( int & batch_vertCount, int batch_tg0, int group, bool orthographic,
	int batch_startOffset, bool batch_colourkey, bool batch_hasTexture,
	const int (&tg_memo_off)[MAX_TEXTURE_GROUPS], const int (&tg_memo_cnt)[MAX_TEXTURE_GROUPS],
	gpu_vertex_t * const (&tg_persist_buf)[MAX_TEXTURE_GROUPS], const int (&tg_persist_cnt)[MAX_TEXTURE_GROUPS] )
{
	if (batch_vertCount > 0) {
		int has_persist = 0;
		{ int pi; for (pi = batch_tg0; pi < group; pi++) if (tg_persist_buf[pi]) { has_persist = 1; break; } }
		if ((has_persist || (g_frame.memoMode == SMEMO_LOOKUP)) && !orthographic) {
			int si;
			int consumed = 0;
			long run_off = 0;
			int run_cnt = 0;
			int run_open = 0;
			for (si = batch_tg0; si < group && consumed < batch_vertCount; si++) {
				if (tg_persist_buf[si]) {
					if (run_open) {
						bind_vertex_buffer(g_scratch.scratch + run_off);
						C3D_DrawArrays(GPU_TRIANGLES, 0, run_cnt);
						run_open = 0;
					}
					bind_vertex_buffer(tg_persist_buf[si]);
					C3D_DrawArrays(GPU_TRIANGLES, 0, tg_persist_cnt[si]);
					consumed += tg_persist_cnt[si];
					continue;
				}
				if (tg_memo_cnt[si] <= 0 || (run_open && tg_memo_off[si] != run_off + run_cnt)) {
					if (run_open) {
						bind_vertex_buffer(g_scratch.scratch + run_off);
						C3D_DrawArrays(GPU_TRIANGLES, 0, run_cnt);
						run_open = 0;
					}
					if (tg_memo_cnt[si] <= 0)
						continue;
				}
				if (!run_open) {
					run_off = tg_memo_off[si];
					run_cnt = 0;
					run_open = 1;
				}
				run_cnt += tg_memo_cnt[si];
				consumed += tg_memo_cnt[si];
			}
			if (run_open) {
				bind_vertex_buffer(g_scratch.scratch + run_off);
				C3D_DrawArrays(GPU_TRIANGLES, 0, run_cnt);
			}
		} else {
			bind_vertex_buffer(g_scratch.scratch + batch_startOffset);
			C3D_DrawArrays(GPU_TRIANGLES, 0, batch_vertCount);
		}
		if (batch_colourkey)
			C3D_AlphaTest(false, GPU_ALWAYS, 0);
		if (batch_hasTexture && !_additive_blend_active)
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
				GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
		batch_vertCount = 0;
	}
}

/* After C3D_FrameSplit reset the scratch buffer: stop the stereo memo
 * (scratch reset recycles eye-1 recorded offsets; the pending batch was
 * already flushed with valid ranges, so the rest of the frame runs fully
 * live) and re-establish the shader, attribute layout, texture/TexEnv cache
 * and matrices the split dropped. */
[[gnu::always_inline]] static inline void draw_restore_after_frame_split(bool orthographic)
{
	if (g_frame.memoMode != SMEMO_OFF)
		g_frame.memoMode = SMEMO_OFF;
	g_frame.memoSaved = SMEMO_OFF;	/* a suspended pass must not re-arm it */
	C3D_BindProgram(&g_shader.shaderProgram);
	{
		C3D_AttrInfo *ai = C3D_GetAttrInfo();
		AttrInfo_Init(ai);
		AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
		AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
		AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
	}
	g_texenv.lastBoundTexture = nullptr;
	g_texenv.lastTexEnvTextured = false;
	g_texenv.lastTexEnvMode = TEXENV_MODE_NONE;
	draw_set_view_matrices(orthographic);
}

/* One source vertex to the GPU layout: position, colour through the 0..255
 * -> 0..1 table, texture coordinates. The TLVERTEX (screen-space) and
 * LVERTEX (world-space) loaders share it. */
[[gnu::always_inline]] static inline void draw_convert_vertex(gpu_vertex_t & d, const TLVERTEX *v)
{
	d.pos[0] = v->x;
	d.pos[1] = v->y;
	d.pos[2] = v->z;
	d.color[0] = g_scratch.c255[(v->color >> 16) & 0xFF];
	d.color[1] = g_scratch.c255[(v->color >> 8) & 0xFF];
	d.color[2] = g_scratch.c255[(v->color) & 0xFF];
	d.color[3] = g_scratch.c255[(v->color >> 24) & 0xFF];
	d.texcoord[0] = v->tu;
	d.texcoord[1] = v->tv;
}

[[gnu::always_inline]] static inline void draw_convert_vertex(gpu_vertex_t & d, const LVERTEX *v)
{
	d.pos[0] = v->x;
	d.pos[1] = v->y;
	d.pos[2] = v->z;
	d.color[0] = g_scratch.c255[(v->color >> 16) & 0xFF];
	d.color[1] = g_scratch.c255[(v->color >> 8) & 0xFF];
	d.color[2] = g_scratch.c255[(v->color) & 0xFF];
	d.color[3] = g_scratch.c255[(v->color >> 24) & 0xFF];
	d.texcoord[0] = v->tu;
	d.texcoord[1] = v->tv;
}

bool draw_render_object(RENDEROBJECT *renderObject, int primitive_type, bool orthographic)
{
	int group;

	/* Per-mesh atlas seed, set by the loader from the mesh's file, group
	 * and index (mesh_atlas_seed), so walls in different meshes sharing
	 * the same base texture each land in their own ProcTex phase bucket.
	 * It used to hash the renderObject pointer, which made every wall's
	 * grain depend on the build's memory layout. 10_texenv.inc reads bits
	 * 8..23. */
	g_texenv.atlasSeed   = (u32)renderObject->atlasSeed << 8;
	g_texenv.isOrthoDraw = orthographic;
	g_scratch.c255_init();

	/* For object-mode draws (g_texenv.objProcTexOn set by mxload/mxaload),
	 * sample the lit ambient at the object's world position and blend
	 * with the dominant nearby XLight to drive the Phong specular hue.
	 * world_matrix's translation row holds the object's world pos. */
	if (g_texenv.objProcTexOn && g_texenv.objLightReady && VideoOpts.g_object_shine &&
		g_texenv.objShineEligible && !orthographic) {
		VECTOR pos;
		pos.x = world_matrix._41;
		pos.y = world_matrix._42;
		pos.z = world_matrix._43;
		c3d_update_phong_tint_for_object_pos(&pos);
	}

	/* Single-pass stereo memo: lookup happens per texture group in
	 * the convert loop below (hit = reuse recorded verts, miss =
	 * convert live). Nothing is skipped here — traversal, culling,
	 * state, uniforms and draw order all run live per eye, so inset
	 * and HUD draws are unaffected by construction. */

	if (!g_shader.shaderReady || !renderObject || !renderObject->vertexBuffer || !g_scratch.scratch)
		return false;

	draw_set_view_matrices(orthographic);

	/* ---- Texture sorting: same-texture groups end up adjacent for batching ---- */
	int sorted_order[MAX_TEXTURE_GROUPS];
	int numGroups = renderObject->numTextureGroups;
	draw_sort_texture_groups(renderObject, numGroups, sorted_order);

	/* ---- Draw call batching ----
	 * Accumulate consecutive texture groups with the same GPU state
	 * (texture, colourkey, blend) into a single C3D_DrawArrays call.
	 * Vertices are contiguous in the scratch buffer so batching just
	 * extends the vertex count without issuing intermediate draws. */
	int   batch_startOffset = 0;
	int   batch_vertCount   = 0;
	void *batch_texture     = nullptr;
	bool  batch_colourkey   = false;
	bool  batch_hasTexture  = false;
	/* Eye-2 submit spans: per-texgroup converted ranges for the batch
	 * being flushed (batch_tg0 .. group-1). A skipped texgroup records
	 * count 0 and is not submitted. */
	int   batch_tg0 = 0;
	/* Must stay zero-initialized: ortho groups never set these, and the
	 * lookup flush below must read count 0 (skip), never stack garbage.
	 * Garbage counts were submitted to DrawArrays and wedged the GPU
	 * queue on defend2 (FrameBegin wait, deterministic). */
	int   tg_memo_off[MAX_TEXTURE_GROUPS] = {0};
	int   tg_memo_cnt[MAX_TEXTURE_GROUPS] = {0};
	/* R05: retained-buffer submit per texgroup (NULL = scratch path).
	 * Zero-initialized for the same reason as above. */
	gpu_vertex_t *tg_persist_buf[MAX_TEXTURE_GROUPS] = {0};
	int   tg_persist_cnt[MAX_TEXTURE_GROUPS] = {0};


	for (group = 0; group < numGroups; group++)
	{
		TEXTUREGROUP *tg = &renderObject->textureGroups[sorted_order[group]];
		int numIndices = tg->numTriangles * 3;
		int startVert = tg->startVert;
		int totalVerts, i;
		gpu_vertex_t *dst;

		if (numIndices <= 0) {
			tg_memo_cnt[group] = 0;
			continue;
		}

		/* Memo lookup (right eye only, never ortho): reuse the recorded
		 * range and skip conversion. Misses convert live below. */
		long memo_off = 0;
		int memo_cnt = 0;
		bool memo_hit = false;
		bool persist_hit = false;
		/* R05: level walls consult the retained buffers first (both
		 * eyes, any mode). A hit submits the retained buffer at flush
		 * time and converts nothing. */
		if (g_persist.active && !orthographic && renderObject->vertOwner == VERT_OWNER_LEVEL &&
			renderObject->memoEpochPass == SMEMO_PASS_UNSET) {
			int pcnt = 0;
			gpu_vertex_t *pbuf = g_persist.lookup(renderObject,
				smemo_sub(sorted_order[group], 0, primitive_type), &pcnt);
			if (pbuf) {
				persist_hit = true;
				tg_persist_buf[group] = pbuf;
				tg_persist_cnt[group] = pcnt;
				totalVerts = pcnt;
			}
		}
		if (!persist_hit && g_frame.memoMode == SMEMO_LOOKUP && !orthographic &&
			memo_reusable(renderObject)) {
			memo_hit = smemo_lookup(renderObject,
				smemo_sub(sorted_order[group], 0, primitive_type),
				&memo_off, &memo_cnt,
				renderObject->memoEpochPass,
				renderObject->memoEpochGroup,
				renderObject->memoEpochResume);
		}

		/* Mid-frame flush: if scratch is nearly full, flush pending batch
		 * and split the frame to reset the scratch pointer. Hits convert
		 * nothing and take no scratch, so only a live convert checks. */
		if (!persist_hit && !memo_hit && g_scratch.scratchUsed + numIndices > g_scratch.scratchMax)
		{
			if (g_frame.inFrame)
			{
				draw_flush_batch(batch_vertCount, batch_tg0, group, orthographic, batch_startOffset, batch_colourkey, batch_hasTexture, tg_memo_off, tg_memo_cnt, tg_persist_buf, tg_persist_cnt);
				C3D_FrameSplit(0);
				g_scratch.scratchUsed = 0;
				draw_restore_after_frame_split(orthographic);
			}
			if (g_scratch.scratchUsed + numIndices > g_scratch.scratchMax) {
				tg_memo_cnt[group] = 0;
				continue;
			}
		}

		dst = g_scratch.scratch + g_scratch.scratchUsed;

		/* Convert live unless the memo/persist hit (VERIFY converts
		 * anyway for the oracle compare below). */
		if (persist_hit) {
			/* totalVerts + tg_persist_* already set above */
		} else if (memo_hit) {
			totalVerts = memo_cnt;
			tg_memo_off[group] = memo_off;
			tg_memo_cnt[group] = memo_cnt;
		} else
		/* Convert LVERTEX/TLVERTEX → gpu_vertex_t, expanding indices */
		{
			if (renderObject->indexBuffer)
			{
				WORD *indices = (WORD*)renderObject->indexBuffer + tg->startIndex;


				if (orthographic)
				{
					TLVERTEX *verts = (TLVERTEX*)renderObject->vertexBuffer;
					for (i = 0; i < numIndices; i++)
					{
						TLVERTEX *v = &verts[startVert + indices[i]];
						draw_convert_vertex(dst[i], v);
					}
				}
				else
				{
					LVERTEX *verts = (LVERTEX*)renderObject->vertexBuffer;
					for (i = 0; i < numIndices; i++)
					{
						int vi = startVert + indices[i];
						LVERTEX *v = &verts[vi];
						draw_convert_vertex(dst[i], v);
					}
					totalVerts = numIndices;
				}
				totalVerts = numIndices;
			}
			else
			{
				LVERTEX *verts = (LVERTEX*)renderObject->vertexBuffer;
				int numVerts = tg->numVerts;
				if (g_scratch.scratchUsed + numVerts > g_scratch.scratchMax) {
					tg_memo_cnt[group] = 0;
					continue;
				}
				for (i = 0; i < numVerts; i++)
				{
					int vi = startVert + i;
					LVERTEX *v = &verts[vi];
					draw_convert_vertex(dst[i], v);
				}
				totalVerts = numVerts;
			}


			/* Record the converted range (eye 1). Runs ONLY on the live
			 * path: on a memo hit the tg arrays already hold the looked-up
			 * range and must not be overwritten with the cursor. The tg
			 * fill itself is unconditional on the live path so the
			 * per-group flush (persist/mono mixes) can always read it. */
			if (!orthographic) {
				tg_memo_off[group] = dst - g_scratch.scratch;
				tg_memo_cnt[group] = totalVerts;
				if (g_frame.memoMode == SMEMO_RECORD && memo_reusable(renderObject)) {
					smemo_record(renderObject,
						smemo_sub(sorted_order[group], 0, primitive_type),
						dst - g_scratch.scratch, totalVerts,
						renderObject->memoEpochPass,
						renderObject->memoEpochGroup,
						renderObject->memoEpochResume);
				}
			}
			/* R05: retain this conversion for level walls. On success the
			 * flush submits the retained buffer instead of scratch. */
			if (g_persist.active && !orthographic && renderObject->vertOwner == VERT_OWNER_LEVEL &&
				renderObject->memoEpochPass == SMEMO_PASS_UNSET) {
				gpu_vertex_t *pbuf = g_persist.store(renderObject,
					smemo_sub(sorted_order[group], 0, primitive_type),
					dst, totalVerts);
				if (pbuf) {
					tg_persist_buf[group] = pbuf;
					tg_persist_cnt[group] = totalVerts;
					tg_memo_cnt[group] = 0;
				}
			}
		}

		/* Determine effective texture state for this group */
		bool eff_hasTexture = false;
		if (tg->texture)
		{
			texture_t *texdata = (texture_t*)tg->texture;
			if (texdata->initialized)
				eff_hasTexture = true;
		}

		/* Check if we need to break the batch — state changed */
		if (batch_vertCount > 0 &&
			((void*)tg->texture != batch_texture ||
			 tg->colourkey      != batch_colourkey ||
			 eff_hasTexture     != batch_hasTexture))
		{
			draw_flush_batch(batch_vertCount, batch_tg0, group, orthographic, batch_startOffset, batch_colourkey, batch_hasTexture, tg_memo_off, tg_memo_cnt, tg_persist_buf, tg_persist_cnt);
		}

		/* Start a new batch if needed — set GPU state */
		if (batch_vertCount == 0)
		{
			batch_startOffset = g_scratch.scratchUsed;
			batch_tg0 = group;
			batch_texture     = (void*)tg->texture;
			batch_colourkey   = tg->colourkey;
			batch_hasTexture  = eff_hasTexture;

			if (tg->colourkey)
				C3D_AlphaTest(true, GPU_GREATER, 0x64);
			if (eff_hasTexture && !_additive_blend_active)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

			/* Texture binding with TexEnv cache. g_texenv.apply_*
			 * picks single-stage diffuse vs two-stage detail-add
			 * based on the texture's material_class. */
			if (eff_hasTexture)
				g_texenv.apply_for_texture((texture_t*)tg->texture);
			else
				g_texenv.apply_untextured();
		}

		/* Accumulate into current batch */
		batch_vertCount += totalVerts;
		if (!persist_hit && !memo_hit)	/* hits wrote no scratch */
			g_scratch.scratchUsed += totalVerts;
	}

	/* Flush the final pending batch */
	draw_flush_batch(batch_vertCount, batch_tg0, group, orthographic, batch_startOffset, batch_colourkey, batch_hasTexture, tg_memo_off, tg_memo_cnt, tg_persist_buf, tg_persist_cnt);

	return true;
}

bool draw_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 0, false);
}

/* World-space billboards / sprites / particles (explosions, weapon
 * trails, smoke). Same as draw_object but flags the batch as
 * not-eligible for AUX-mode ProcTex so wall grain doesn't apply. */
bool draw_billboard_object(RENDEROBJECT *renderObject)
{
	g_texenv.skipAuxProcTex = true;
	bool ok = draw_render_object(renderObject, 0, false);
	g_texenv.skipAuxProcTex = false;
	return ok;
}

bool draw_2d_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 0, true);
}

bool draw_line_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 1, false);
}
