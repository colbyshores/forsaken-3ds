/*
 * stubs_3ds.c - Stubs for functions not available on 3DS
 *
 * Provides no-op implementations of desktop/network functions
 * that the engine references but are not available on 3DS.
 */

#ifdef __3DS__

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "main.h"

/* ---- net_tracker stubs ---- */

void send_tracker_update(void *data, int size, int flags)
{
	(void)data; (void)size; (void)flags;
}

void send_tracker_finished(void *host)
{
	(void)host;
}

/* ---- SDL stubs ---- */

int SDL_EnableUNICODE(int enable)
{
	(void)enable;
	return 0;
}

/* ---- Windows API stubs ---- */

int bIgnoreWM_SIZE = 0;

/* ---- glob stubs (devkitARM has the header but no implementation) ---- */

#include <glob.h>
#include <dirent.h>
#include <stdlib.h>

int glob(const char *pattern, int flags,
	int (*errfunc)(const char *, int), glob_t *pglob)
{
	/* Simple glob implementation for 3DS - handles wildcards at end of path */
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

	pglob->gl_pathv = malloc((count + 1) * sizeof(char *));
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
			pglob->gl_pathv[idx] = malloc(pathlen);
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

/* ---- ftime replacement ---- */

#include <sys/types.h>

struct timeb {
	time_t time;
	unsigned short millitm;
	short timezone;
	short dstflag;
};

int ftime(struct timeb *tp)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	if (tp) {
		tp->time = tv.tv_sec;
		tp->millitm = tv.tv_usec / 1000;
		tp->timezone = 0;
		tp->dstflag = 0;
	}
	return 0;
}

#endif /* __3DS__ */
