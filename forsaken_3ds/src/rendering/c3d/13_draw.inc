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

bool draw_render_object(RENDEROBJECT *renderObject, int primitive_type, bool orthographic)
{
	int group;

	/* Per-mesh atlas seed — hash of the renderObject pointer.
	 * Stable for the lifetime of the level, distinct per mesh, so
	 * walls in different meshes sharing the same base texture each
	 * land in their own ProcTex phase bucket. */
	s_atlasSeed   = (u32)((uintptr_t)renderObject) * 0x9E3779B1u;
	s_isOrthoDraw = orthographic;
	c255_init();

	/* For object-mode draws (s_objProcTexOn set by mxload/mxaload),
	 * sample the lit ambient at the object's world position and blend
	 * with the dominant nearby XLight to drive the Phong specular hue.
	 * world_matrix's translation row holds the object's world pos. */
	if (s_objProcTexOn && s_objLightReady && g_object_shine &&
		s_objShineEligible && !orthographic) {
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

	if (!s_shaderReady || !renderObject || !renderObject->vertexBuffer || !s_scratch)
		return false;

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

	/* ---- Texture sorting ----
	 * Sort texture groups by texture pointer so consecutive groups share
	 * the same texture, maximizing draw call batching below.  Uses a
	 * local index array (not modifying the renderObject) and insertion
	 * sort — fast for the small N (max 64 on 3DS). */
	int sorted_order[MAX_TEXTURE_GROUPS];
	int numGroups = renderObject->numTextureGroups;
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

	/* ---- Draw call batching ----
	 * Accumulate consecutive texture groups with the same GPU state
	 * (texture, colourkey, blend) into a single C3D_DrawArrays call.
	 * Vertices are contiguous in the scratch buffer so batching just
	 * extends the vertex count without issuing intermediate draws. */
	int   batch_startOffset = 0;
	int   batch_vertCount   = 0;
	void *batch_texture     = NULL;
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

/* Macro: flush the current batch. Eye 1 (and memo off): one draw call,
 * exactly as before. Eye 2 in lookup mode: one draw per texgroup range
 * in the batch, in walk order, with the same per-batch state. Splitting
 * only the submit (never state, order or uniforms) keeps pixels
 * identical while tolerating per-eye culling differences: a texgroup
 * the left eye never converted is simply converted live instead. */
#define FLUSH_BATCH() do { \
	if (batch_vertCount > 0) { \
		int _has_persist = 0; \
		{ int _pi; for (_pi = batch_tg0; _pi < group; _pi++) if (tg_persist_buf[_pi]) { _has_persist = 1; break; } } \
		if ((_has_persist || (s_memoMode == SMEMO_LOOKUP)) && !orthographic) { \
			int _si; \
			int _consumed = 0; \
			long _run_off = 0; \
			int _run_cnt = 0; \
			int _run_open = 0; \
			for (_si = batch_tg0; _si < group && _consumed < batch_vertCount; _si++) { \
				if (tg_persist_buf[_si]) { \
					if (_run_open) { \
						bind_vertex_buffer(s_scratch + _run_off); \
						C3D_DrawArrays(GPU_TRIANGLES, 0, _run_cnt); \
						_run_open = 0; \
					} \
					bind_vertex_buffer(tg_persist_buf[_si]); \
					C3D_DrawArrays(GPU_TRIANGLES, 0, tg_persist_cnt[_si]); \
					_consumed += tg_persist_cnt[_si]; \
					continue; \
				} \
				if (tg_memo_cnt[_si] <= 0 || (_run_open && tg_memo_off[_si] != _run_off + _run_cnt)) { \
					if (_run_open) { \
						bind_vertex_buffer(s_scratch + _run_off); \
						C3D_DrawArrays(GPU_TRIANGLES, 0, _run_cnt); \
						_run_open = 0; \
					} \
					if (tg_memo_cnt[_si] <= 0) \
						continue; \
				} \
				if (!_run_open) { \
					_run_off = tg_memo_off[_si]; \
					_run_cnt = 0; \
					_run_open = 1; \
				} \
				_run_cnt += tg_memo_cnt[_si]; \
				_consumed += tg_memo_cnt[_si]; \
			} \
			if (_run_open) { \
				bind_vertex_buffer(s_scratch + _run_off); \
				C3D_DrawArrays(GPU_TRIANGLES, 0, _run_cnt); \
			} \
		} else { \
			bind_vertex_buffer(s_scratch + batch_startOffset); \
			C3D_DrawArrays(GPU_TRIANGLES, 0, batch_vertCount); \
		} \
		if (batch_colourkey) \
			C3D_AlphaTest(false, GPU_ALWAYS, 0); \
		if (batch_hasTexture && !_additive_blend_active) \
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, \
				GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO); \
		batch_vertCount = 0; \
	} \
} while(0)

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
		if (s_persistActive && !orthographic && renderObject->vertOwner == VERT_OWNER_LEVEL &&
			renderObject->memoEpochPass == SMEMO_PASS_UNSET) {
			int pcnt = 0;
			gpu_vertex_t *pbuf = persist_lookup(renderObject,
				smemo_sub(sorted_order[group], 0, primitive_type), &pcnt);
			if (pbuf) {
				persist_hit = true;
				tg_persist_buf[group] = pbuf;
				tg_persist_cnt[group] = pcnt;
				totalVerts = pcnt;
			}
		}
		if (!persist_hit && s_memoMode == SMEMO_LOOKUP && !orthographic &&
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
		if (!persist_hit && !memo_hit && s_scratchUsed + numIndices > s_scratchMax)
		{
			if (s_inFrame)
			{
				FLUSH_BATCH();
				C3D_FrameSplit(0);
				s_scratchUsed = 0;
				/* Scratch reset recycles eye-1 recorded offsets: stop
				 * reusing them. The pending batch was already flushed
				 * above (valid ranges); the rest of the frame runs
				 * fully live. */
				if (s_memoMode != SMEMO_OFF)
					s_memoMode = SMEMO_OFF;
				s_memoSaved = SMEMO_OFF;	/* a suspended pass must not re-arm it */
				C3D_BindProgram(&s_shaderProgram);
				{
					C3D_AttrInfo *ai = C3D_GetAttrInfo();
					AttrInfo_Init(ai);
					AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
					AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
					AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
				}
				s_lastBoundTexture = NULL;
				s_lastTexEnvTextured = false;
				s_lastTexEnvMode = TEXENV_MODE_NONE;
				if (!orthographic)
					upload_matrices();
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
			if (s_scratchUsed + numIndices > s_scratchMax) {
				tg_memo_cnt[group] = 0;
				continue;
			}
		}

		dst = s_scratch + s_scratchUsed;

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
					dst[i].pos[0] = v->x;
					dst[i].pos[1] = v->y;
					dst[i].pos[2] = v->z;
					dst[i].color[0] = s_c255[(v->color >> 16) & 0xFF];
					dst[i].color[1] = s_c255[(v->color >> 8) & 0xFF];
					dst[i].color[2] = s_c255[(v->color) & 0xFF];
					dst[i].color[3] = s_c255[(v->color >> 24) & 0xFF];
					dst[i].texcoord[0] = v->tu;
					dst[i].texcoord[1] = v->tv;
				}
			}
			else
			{
				LVERTEX *verts = (LVERTEX*)renderObject->vertexBuffer;
				for (i = 0; i < numIndices; i++)
				{
					int vi = startVert + indices[i];
					LVERTEX *v = &verts[vi];
					dst[i].pos[0] = v->x;
					dst[i].pos[1] = v->y;
					dst[i].pos[2] = v->z;
					dst[i].color[0] = s_c255[(v->color >> 16) & 0xFF];
					dst[i].color[1] = s_c255[(v->color >> 8) & 0xFF];
					dst[i].color[2] = s_c255[(v->color) & 0xFF];
					dst[i].color[3] = s_c255[(v->color >> 24) & 0xFF];
					dst[i].texcoord[0] = v->tu;
					dst[i].texcoord[1] = v->tv;
				}
				totalVerts = numIndices;
			}
			totalVerts = numIndices;
		}
		else
		{
			LVERTEX *verts = (LVERTEX*)renderObject->vertexBuffer;
			int numVerts = tg->numVerts;
			if (s_scratchUsed + numVerts > s_scratchMax) {
				tg_memo_cnt[group] = 0;
				continue;
			}
			for (i = 0; i < numVerts; i++)
			{
				int vi = startVert + i;
				LVERTEX *v = &verts[vi];
				dst[i].pos[0] = v->x;
				dst[i].pos[1] = v->y;
				dst[i].pos[2] = v->z;
				dst[i].color[0] = s_c255[(v->color >> 16) & 0xFF];
				dst[i].color[1] = s_c255[(v->color >> 8) & 0xFF];
				dst[i].color[2] = s_c255[(v->color) & 0xFF];
				dst[i].color[3] = s_c255[(v->color >> 24) & 0xFF];
				dst[i].texcoord[0] = v->tu;
				dst[i].texcoord[1] = v->tv;
			}
			totalVerts = numVerts;
		}


		/* Record the converted range (eye 1). Runs ONLY on the live
		 * path: on a memo hit the tg arrays already hold the looked-up
		 * range and must not be overwritten with the cursor. The tg
		 * fill itself is unconditional on the live path so the
		 * per-group flush (persist/mono mixes) can always read it. */
		if (!orthographic) {
			tg_memo_off[group] = dst - s_scratch;
			tg_memo_cnt[group] = totalVerts;
			if (s_memoMode == SMEMO_RECORD && memo_reusable(renderObject)) {
				smemo_record(renderObject,
					smemo_sub(sorted_order[group], 0, primitive_type),
					dst - s_scratch, totalVerts,
					renderObject->memoEpochPass,
					renderObject->memoEpochGroup,
					renderObject->memoEpochResume);
			}
		}
		/* R05: retain this conversion for level walls. On success the
		 * flush submits the retained buffer instead of scratch. */
		if (s_persistActive && !orthographic && renderObject->vertOwner == VERT_OWNER_LEVEL &&
			renderObject->memoEpochPass == SMEMO_PASS_UNSET) {
			gpu_vertex_t *pbuf = persist_store(renderObject,
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
			FLUSH_BATCH();
		}

		/* Start a new batch if needed — set GPU state */
		if (batch_vertCount == 0)
		{
			batch_startOffset = s_scratchUsed;
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

			/* Texture binding with TexEnv cache. apply_texenv_*
			 * picks single-stage diffuse vs two-stage detail-add
			 * based on the texture's material_class. */
			if (eff_hasTexture)
				apply_texenv_for_texture((texture_t*)tg->texture);
			else
				apply_texenv_untextured();
		}

		/* Accumulate into current batch */
		batch_vertCount += totalVerts;
		if (!persist_hit && !memo_hit)	/* hits wrote no scratch */
			s_scratchUsed += totalVerts;
	}

	/* Flush the final pending batch */
	FLUSH_BATCH();

#undef FLUSH_BATCH

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
	s_skipAuxProcTex = true;
	bool ok = draw_render_object(renderObject, 0, false);
	s_skipAuxProcTex = false;
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
