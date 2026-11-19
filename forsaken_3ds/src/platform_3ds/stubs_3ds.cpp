/* stubs_3ds.cpp -- 3DS stand-ins for symbols the PC layer provides
 *
 * The core is shared with the PC build and still names a few things that
 * the PC platform layer or its libraries define. This file defines them:
 * the net_tracker.h calls (no tracker on 3DS), SDL_EnableUNICODE (an SDL 1.2
 * call title.cpp makes because SDL_VERSION_ATLEAST is always false on 3DS),
 * the bIgnoreWM_SIZE flag (input_sdl.cpp owns it on PC; title.cpp writes it,
 * nothing reads it here), and glob / globfree, which devkitARM declares but
 * does not implement (file.cpp's find_file uses them).
 */

#ifdef __3DS__

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "game/main.h"
#include "net/net_tracker.h"
#include "util/platform.h"

/* ---- net_tracker stubs ---- */

/* No master-server tracker on 3DS (single player only): net_tracker.h's
 * interface, doing nothing. */
void send_tracker_update(char *host, int port)
{
	(void)host; (void)port;
}

void send_tracker_finished(char *host, int port)
{
	(void)host; (void)port;
}

/* ---- SDL stubs ---- */

int SDL_EnableUNICODE(int enable)
{
	(void)enable;
	return 0;
}

/* ---- window-resize flag (input_sdl.cpp on PC) ---- */

bool bIgnoreWM_SIZE = false;	// input.h declares it bool

/* ---- glob stubs (devkitARM has the header but no implementation) ---- */

#include <glob.h>
#include <dirent.h>
#include <stdlib.h>

int glob(const char *pattern, int flags,
	int (*errfunc)(const char *, int), glob_t *pglob)
{
	/* Minimal glob: only the first '*' in the last path component is a
	 * wildcard; no '?' or bracket classes; flags and errfunc ignored. Without a '*' the
	 * name is compared case-insensitively; the '*' prefix and suffix are
	 * compared case-sensitively. Results are not sorted. */
	DIR *dir;
	struct dirent *entry;
	char dirpath[256], match[256];
	char *slash;
	int count = 0;

	(void)flags; (void)errfunc;

	pglob->gl_pathc = 0;
	pglob->gl_pathv = NULL;

	/* Split pattern into directory and filename parts */
	strncpy(dirpath, pattern, sizeof(dirpath) - 1);
	dirpath[sizeof(dirpath) - 1] = 0;
	slash = strrchr(dirpath, '/');
	if (slash) {
		strncpy(match, slash + 1, sizeof(match) - 1);
		match[sizeof(match) - 1] = 0;
		*(slash + 1) = 0;
	} else {
		strncpy(match, dirpath, sizeof(match) - 1);
		match[sizeof(match) - 1] = 0;
		strcpy(dirpath, "./");
	}

	/* Remove wildcard for simple matching */
	char *star = strchr(match, '*');
	char prefix[256] = "";
	char suffix[256] = "";
	if (star) {
		int plen = star - match;
		strncpy(prefix, match, plen);
		prefix[plen] = 0;
		strncpy(suffix, star + 1, sizeof(suffix) - 1);
		suffix[sizeof(suffix) - 1] = 0;
	}

	dir = opendir(dirpath);
	if (!dir) return GLOB_NOMATCH;

	/* First pass: count matches */
	while ((entry = readdir(dir)) != NULL) {
		char *name = entry->d_name;
		if (star) {
			int nlen = strlen(name);
			int plen = strlen(prefix);
			int slen = strlen(suffix);
			if (nlen >= plen + slen &&
				strncmp(name, prefix, plen) == 0 &&
				(slen == 0 || strcmp(name + nlen - slen, suffix) == 0))
				count++;
		} else {
			if (strcasecmp(name, match) == 0)
				count++;
		}
	}

	if (count == 0) { closedir(dir); return GLOB_NOMATCH; }

	pglob->gl_pathv = (char **)malloc((count + 1) * sizeof(char *));
	if (!pglob->gl_pathv) { closedir(dir); return GLOB_NOSPACE; }

	/* Second pass: collect matches */
	rewinddir(dir);
	int idx = 0;
	while ((entry = readdir(dir)) != NULL && idx < count) {
		char *name = entry->d_name;
		int matched = 0;
		if (star) {
			int nlen = strlen(name);
			int plen = strlen(prefix);
			int slen = strlen(suffix);
			if (nlen >= plen + slen &&
				strncmp(name, prefix, plen) == 0 &&
				(slen == 0 || strcmp(name + nlen - slen, suffix) == 0))
				matched = 1;
		} else {
			if (strcasecmp(name, match) == 0)
				matched = 1;
		}
		if (matched) {
			int pathlen = strlen(dirpath) + strlen(name) + 1;
			pglob->gl_pathv[idx] = (char *)malloc(pathlen);
			snprintf(pglob->gl_pathv[idx], pathlen, "%s%s", dirpath, name);
			idx++;
		}
	}
	pglob->gl_pathv[idx] = NULL;
	pglob->gl_pathc = idx;
	closedir(dir);
	return 0;
}

void globfree(glob_t *pglob)
{
	int i;
	if (!pglob || !pglob->gl_pathv) return;
	for (i = 0; i < pglob->gl_pathc; i++)
		free(pglob->gl_pathv[i]);
	free(pglob->gl_pathv);
	pglob->gl_pathv = NULL;
	pglob->gl_pathc = 0;
}

#endif /* __3DS__ */
