/* clipmath.h -- screen bounds of a portal that straddles the near plane.
 *
 * Pure math, no engine types, so it is host-testable
 * (tests/test_clipmath.cpp). visi.cpp transforms the portal's vertices to
 * clip space, calls fillclip_tight_bounds, and gets back pixel bounds mapped
 * with the same viewport math as Transform2Viewport.
 *
 * A portal is an opening, not a surface: the room beyond is seen through
 * every part of it that is in front of the eye, including the part nearer
 * than the near plane (rays pass the opening there and hit walls beyond the
 * near plane). So the polygon is clipped against the eye plane (w > 0) only,
 * never the near plane; clipping at the near plane would drop the screen
 * area seen through the near part of the doorway and leave it black.
 *
 * Contract: given clip-space points (x,y,z,w), compute a screen-pixel
 * bounding box covering the w>0 part of the polygon. Returns 1 with
 * bounds, 0 when that part is empty or the input degenerate (the caller
 * keeps its conservative fallback). Points just in front of the eye project
 * far off screen and clamp to the viewport edges, so the bounds are never
 * smaller than the visible region.
 */
#pragma once

/* Scratch capacity: portals are quads; 40 verts covers the clip pass with
 * wide margin. */
constexpr int FILLCLIP_CAP = 40;

/* One Sutherland-Hodgman pass against the eye plane, w >= eps.
 * in:  [n*4] xyzw, out: [cap*4]. Returns out count, or -1 on overflow. */
inline int fillclip_eye_plane(const float *in_pts, int n, float *out_pts, int cap)
{
	int i, j, k = 0;
	for (i = 0, j = n - 1; i < n; j = i++) {
		float di = in_pts[i * 4 + 3] - 1e-4f;
		float dj = in_pts[j * 4 + 3] - 1e-4f;
		int ini = di >= 0.0f, inj = dj >= 0.0f;
		if (ini) {
			if (!inj) {
				float t = dj / (dj - di);
				int o = k * 4, a = j * 4, b = i * 4;
				if (k >= cap)
					return -1;
				out_pts[o + 0] = in_pts[a + 0] + t * (in_pts[b + 0] - in_pts[a + 0]);
				out_pts[o + 1] = in_pts[a + 1] + t * (in_pts[b + 1] - in_pts[a + 1]);
				out_pts[o + 2] = in_pts[a + 2] + t * (in_pts[b + 2] - in_pts[a + 2]);
				out_pts[o + 3] = in_pts[a + 3] + t * (in_pts[b + 3] - in_pts[a + 3]);
				k++;
			}
			if (k >= cap)
				return -1;
			out_pts[k * 4 + 0] = in_pts[i * 4 + 0];
			out_pts[k * 4 + 1] = in_pts[i * 4 + 1];
			out_pts[k * 4 + 2] = in_pts[i * 4 + 2];
			out_pts[k * 4 + 3] = in_pts[i * 4 + 3];
			k++;
		} else if (inj) {
			float t = di / (di - dj);
			int o = k * 4, a = i * 4, b = j * 4;
			if (k >= cap)
				return -1;
			out_pts[o + 0] = in_pts[a + 0] + t * (in_pts[b + 0] - in_pts[a + 0]);
			out_pts[o + 1] = in_pts[a + 1] + t * (in_pts[b + 1] - in_pts[a + 1]);
			out_pts[o + 2] = in_pts[a + 2] + t * (in_pts[b + 2] - in_pts[a + 2]);
			out_pts[o + 3] = in_pts[a + 3] + t * (in_pts[b + 3] - in_pts[a + 3]);
			k++;
		}
	}
	return k;
}

/* Tight pixel bounds for a clip-space polygon. vpx/vpy/vpw/vph/vpsx/vpsy
 * are the viewport origin/size/scale (same convention as
 * Transform2Viewport). Writes *out_minx..maxy. Returns 1 on success,
 * 0 when the visible fragment is empty or inputs are degenerate
 * (caller keeps its conservative fallback). */
inline int fillclip_tight_bounds(const float *clip, int n,
								 float vpx, float vpy, float vpw, float vph,
								 float vpsx, float vpsy,
								 float *out_minx, float *out_miny,
								 float *out_maxx, float *out_maxy)
{
	/* Clipping adds at most one vertex per edge; over-capacity input
	 * falls back conservatively. */
	float buf[FILLCLIP_CAP * 4];
	int m, i;
	float minx, miny, maxx, maxy;

	if (n < 3 || n > 36)
		return 0;
	m = fillclip_eye_plane(clip, n, buf, FILLCLIP_CAP);
	if (m <= 0)
		return 0;
	minx = miny = 1e30f;
	maxx = maxy = -1e30f;
	for (i = 0; i < m; i++) {
		float w = buf[i * 4 + 3];
		float nx, ny, px, py;
		if (!(w > 0.0f))
			return 0;
		nx = buf[i * 4 + 0] / w;
		ny = buf[i * 4 + 1] / w;
		if (nx < -1.0F)
			px = vpx;
		else if (nx > 1.0F)
			px = vpx + vpw;
		else
			px = vpx + vpw * 0.5F + vpsx * nx;
		if (ny < -1.0F)
			py = vpy + vph;
		else if (ny > 1.0F)
			py = vpy;
		else
			py = vpy + vph * 0.5F - vpsy * ny;
		if (px < minx)
			minx = px;
		if (py < miny)
			miny = py;
		if (px > maxx)
			maxx = px;
		if (py > maxy)
			maxy = py;
	}
    *out_minx = minx;
    *out_miny = miny;
    *out_maxx = maxx;
    *out_maxy = maxy;
	return 1;
}
