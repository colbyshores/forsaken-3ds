/* framedump_3ds.c — 3DS backend: trigger listener + framebuffer capture.
 *
 * Compiles to nothing unless -DFRAMEDUMP is set (same pattern as
 * fidelity_3ds.c), so default builds are unaffected. The game owns no
 * other socket (net_3ds.c is single-player stubs), so this library owns
 * socInit/socExit here.
 */
#ifdef FRAMEDUMP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <3ds.h>

#include "framedump.h"
#include "framedump_core.h"

/* SOC context: 64 KiB, page-aligned (socInit requires 0x1000 alignment),
 * lives for the process lifetime. First-attempt socInit can fail
 * transiently after a chainload handoff (the stub's own boot loop
 * retries the same call), so framedump_init retries boundedly. */
static u32 s_socMem[0x4000] __attribute__((aligned(0x1000)));
static int s_listenFd = -1;
static int s_seq;

/* Convert one screen to BMP + log line. Returns 0 on success. */
static int dump_screen(const char *setname, const char *screen,
                       gfxScreen_t gs)
{
	u16 w = 0, h = 0;
	u8 *fb;
	GSPGPU_FramebufferFormat fmt;
	char path[96];
	char line[192];
	int len, rc;
	unsigned bpp;
	unsigned long nbytes;
	uint32_t hash;
	static unsigned char *snap;
	static unsigned long snapCap;

	fb = gfxGetFramebuffer(gs, GFX_LEFT, &w, &h);
	if (!fb || w == 0 || h == 0) return -1;
	fmt = gfxGetScreenFormat(gs);
	if (fmt == GSP_RGBA8_OES) bpp = 4;
	else if (fmt == GSP_BGR8_OES) bpp = 3;
	else if (fmt == GSP_RGB565_OES) bpp = 2;
	else return -1;
	if (bpp != 2) {
		nbytes = (unsigned long)w * h * bpp;
	} else {
		/* RGB565 is expanded below; hash the raw bytes as stored. */
		nbytes = (unsigned long)w * h * 2u;
	}
	/* Freeze the live framebuffer first: the two BMP writes below take
	 * seconds on SD while the game keeps rendering into this buffer,
	 * so hashing/writing the live pointer tears on any moving scene
	 * (proven on hardware: bottom/static verified, top/live did not).
	 * Hash and both files read the frozen copy. */
	if (nbytes > snapCap) {
		unsigned char *nb = realloc(snap, nbytes);
		if (!nb) return -1;
		snap = nb;
		snapCap = nbytes;
	}
	memcpy(snap, fb, nbytes);
	hash = framedump_fnv1a(snap, nbytes);

	snprintf(path, sizeof(path), "sdmc:/%s_%s.bmp", setname, screen);
	{
		/* RGB565 is expanded to this temp RGB8 buffer for the writer;
		 * kept at function scope so the retrievable twin below can
		 * reuse the same bytes. */
		static unsigned char *tmp;
		static unsigned long tmpCap;
		const unsigned char *rgb;
		unsigned rgbBpp;
		if (bpp == 2) {
			unsigned long np = (unsigned long)w * h;
			unsigned long k;
			if (np * 3ul > tmpCap) {
				unsigned char *nb = realloc(tmp, np * 3ul);
				if (!nb) return -1;
				tmp = nb;
				tmpCap = np * 3ul;
			}
			for (k = 0; k < np; k++) {
				unsigned v = (unsigned)snap[k * 2] |
				             ((unsigned)snap[k * 2 + 1] << 8);
				tmp[k * 3 + 0] = (unsigned char)((v & 0x1f) << 3);
				tmp[k * 3 + 1] = (unsigned char)(((v >> 5) & 0x3f) << 2);
				tmp[k * 3 + 2] = (unsigned char)(((v >> 11) & 0x1f) << 3);
			}
			rgb = tmp;
			rgbBpp = 3;
		} else {
			rgb = snap;
			rgbBpp = bpp;
		}
		rc = framedump_write_bmp(path, rgb, w, h, (int)rgbBpp);
		if (rc != 0) return -1;
		/* Retrievable twin for the ATM HTTP allowlist, which serves
		 * dot-log and dot-txt under /3ds/ but never dot-bmp. The twin
		 * is byte-identical to the canonical file; curl it as
		 * http://<3ds-ip>/3ds/<alias> and verify the BM magic locally.
		 * Remove the twin when the firmware serves the dump dir. */
		{
			char alias[96];
			snprintf(alias, sizeof(alias), "sdmc:/3ds/%s_%s.bmp.log",
			         setname, screen);
			if (framedump_write_bmp(alias, rgb, w, h, (int)rgbBpp) != 0)
				return -1;
		}
	}
	len = snprintf(line, sizeof(line),
	               "FDUMP seq=%d tag=%s screen=%s w=%u h=%u bpp=%u hash=%08lx file=%s\n",
	               s_seq, setname, screen, w, h, bpp,
	               (unsigned long)hash, path);
	if (len > 0) {
		FILE *f = fopen("sdmc:/framedump.log", "a");
		if (f) {
			fwrite(line, 1, (size_t)len, f);
			fclose(f);
		}
	}
	return 0;
}

int framedump_request(const char *tag)
{
	char setname[64];
	char clean[32];
	unsigned i;

	if (!tag) tag = "";
	for (i = 0; i < sizeof(clean) - 1 && tag[i]; i++) {
		char c = tag[i];
		clean[i] = ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		            (c >= '0' && c <= '9') || c == '_' || c == '-') ? c : '_';
	}
	clean[i] = '\0';
	if (i == 0) {
		clean[0] = 'x';
		clean[1] = '\0';
	}
	s_seq++;
	snprintf(setname, sizeof(setname), "framedump_%04d_%s", s_seq, clean);
	dump_screen(setname, "top", GFX_TOP);
	dump_screen(setname, "bottom", GFX_BOTTOM);
	return s_seq;
}

int framedump_init(unsigned short port)
{
	struct sockaddr_in addr;
	int fd, one = 1;
	Result r;
	FILE *diag;

	if (s_listenFd >= 0) return 0;
	{
		/* Bounded retry: first-attempt socInit can fail transiently
		 * after a chainload handoff. Five tries, two seconds apart,
		 * then silent no-network operation. */
		int tries;
		r = 0xFFFFFFFF;
		for (tries = 0; tries < 5; tries++) {
			r = socInit(s_socMem, sizeof(s_socMem));
			if (r == 0) break;
			svcSleepThread(2000000000ULL);
		}
	}
	/* One-shot network self-report, appended to the served boot log
	 * (http://<3ds-ip>/forsaken_boot.log) since only known paths are
	 * served over HTTP. */
	diag = fopen("sdmc:/forsaken_boot.log", "a");
	if (diag) {
		fprintf(diag, "[fdnet] socInit=0x%08lx\n", (unsigned long)r);
		fclose(diag);
		diag = NULL;
	}
	if (r != 0) return -1;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		diag = fopen("sdmc:/forsaken_boot.log", "a");
		if (diag) {
			fprintf(diag, "[fdnet] socket errno=%d\n", errno);
			fclose(diag);
		}
		return -1;
	}
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		diag = fopen("sdmc:/forsaken_boot.log", "a");
		if (diag) {
			fprintf(diag, "[fdnet] bind errno=%d\n", errno);
			fclose(diag);
		}
		close(fd);
		return -1;
	}
	if (listen(fd, 1) != 0) {
		diag = fopen("sdmc:/forsaken_boot.log", "a");
		if (diag) {
			fprintf(diag, "[fdnet] listen errno=%d\n", errno);
			fclose(diag);
		}
		close(fd);
		return -1;
	}
	diag = fopen("sdmc:/forsaken_boot.log", "a");
	if (diag) {
		fprintf(diag, "[fdnet] listen ok port=%u fd=%d\n", port, fd);
		fclose(diag);
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);
	s_listenFd = fd;
	return 0;
}

/* Default trigger port: clear of the stub (17491), ATM deploy (17492),
 * HTTP (8080), telemetry (8081) and commands (8082). */
#ifndef FRAMEDUMP_PORT
#define FRAMEDUMP_PORT 17493
#endif

void framedump_poll(void)
{
	int cfd;
	char buf[128];
	ssize_t got;
	char tag[64];
	char reply[96];
	int rc;
	static int tried;
#ifdef FRAMEDUMP_SELFTEST_AT
	static int s_stFrames;
#endif

	/* Lazy first-use init so the game needs no boot-sequence edit:
	 * the first poll brings the listener up (or stays silent). */
	if (s_listenFd < 0 && !tried) {
		tried = 1;
#ifdef FRAMEDUMP_SELFTEST_AT
		/* Emulator end-to-end has no network: skip the listener, keep
		 * the capture path (request() below) fully exercised. */
		(void)0;
#else
		framedump_init(FRAMEDUMP_PORT);
#endif
	}
#ifdef FRAMEDUMP_SELFTEST_AT
	/* Emulator end-to-end: fire one in-code dump at frame N. Emulators
	 * do not bridge the TCP trigger, so this is the network-free path.
	 * The .tick marker proves poll() executes even if capture fails. */
	if (s_stFrames == 0) {
		FILE *m = fopen("sdmc:/framedump_poll.tick", "w");
		if (m) {
			fputs("poll-live\n", m);
			fclose(m);
		}
	}
	if (++s_stFrames == FRAMEDUMP_SELFTEST_AT)
		framedump_request("selftest");
#endif
	if (s_listenFd < 0) return;
	cfd = accept(s_listenFd, NULL, NULL);
	if (cfd < 0) return; /* nobody waiting: the normal idle case */
	got = recv(cfd, buf, sizeof(buf) - 1, 0);
	if (got <= 0) {
		close(cfd);
		return;
	}
	buf[got < (ssize_t)sizeof(buf) - 1 ? got : (ssize_t)sizeof(buf) - 1] = '\0';
	rc = framedump_parse_cmd(buf, tag, sizeof(tag));
	if (rc == 1) {
		int seq = framedump_request(tag);
		snprintf(reply, sizeof(reply), "OK framedump_%04d_%s\n", seq, tag);
	} else if (rc == 2) {
		snprintf(reply, sizeof(reply), "PONG\n");
	} else {
		snprintf(reply, sizeof(reply), "ERR\n");
	}
	send(cfd, reply, strlen(reply), 0);
	close(cfd);
}

#endif /* FRAMEDUMP */
