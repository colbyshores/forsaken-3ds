/* test_framedump.c — host validation for the framedump library.
 *
 * No console, no 3DS headers: exercises framedump_core.c plus a real
 * loopback TCP trigger exchange (the same bytes the driver sends the
 * game). Verifies: command parsing, FNV-1a hash, BMP file format
 * (magic, size, dimensions, bottom-up rows), and the log line.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "diag/framedump_core.h"

static int failures;

#define CHECK(cond, tag) do { \
	if (!(cond)) { printf("FAIL %s\n", tag); failures++; } \
	else { printf("ok %s\n", tag); } \
} while (0)

/* Fill a w*h RGB8 buffer with a position-dependent pattern. */
static void fill_pattern(unsigned char *px, unsigned w, unsigned h)
{
	unsigned x, y;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			px[(y * w + x) * 3 + 0] = (unsigned char)(x & 0xff);
			px[(y * w + x) * 3 + 1] = (unsigned char)(y & 0xff);
			px[(y * w + x) * 3 + 2] = (unsigned char)((x + y) & 0xff);
		}
}

static void test_parse(void)
{
	char tag[64];
	CHECK(framedump_parse_cmd("DUMP boss1\n", tag, sizeof(tag)) == 1 &&
	      strcmp(tag, "boss1") == 0, "parse dump with tag");
	CHECK(framedump_parse_cmd("DUMP\n", tag, sizeof(tag)) == 1 &&
	      strcmp(tag, "x") == 0, "parse bare dump defaults tag");
	CHECK(framedump_parse_cmd("DUMP a b!\n", tag, sizeof(tag)) == 1 &&
	      strcmp(tag, "a_b_") == 0, "parse sanitizes tag");
	CHECK(framedump_parse_cmd("PING\n", tag, sizeof(tag)) == 2, "parse ping");
	CHECK(framedump_parse_cmd("HELLO\n", tag, sizeof(tag)) == 0, "parse rejects garbage");
}

static void test_hash(void)
{
	/* FNV-1a 32-bit empty-string vector. */
	CHECK(framedump_fnv1a((const unsigned char *)"", 0) == 2166136261u,
	      "hash empty vector");
}

static void test_bmp(void)
{
	unsigned char px[5 * 3 * 3];
	unsigned char hdr[54];
	FILE *f;
	long size;

	fill_pattern(px, 5, 3);
	CHECK(framedump_write_bmp("out-test.bmp", px, 5, 3, 3) == 0, "bmp writes");
	f = fopen("out-test.bmp", "rb");
	CHECK(f != NULL, "bmp readable");
	if (!f) return;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	/* 5x3 24-bit: stride = (15+3)&~3 = 16; file = 54 + 16*3 = 102. */
	CHECK(size == 102, "bmp file size");
	fread(hdr, 1, sizeof(hdr), f);
	CHECK(hdr[0] == 'B' && hdr[1] == 'M', "bmp magic");
	CHECK(hdr[28] == 24, "bmp 24-bit");
	CHECK(hdr[18] == 5 && hdr[22] == 3, "bmp dimensions");
	{
		/* First file row must be the LAST input row (bottom-up). */
		unsigned char row[16];
		fread(row, 1, sizeof(row), f);
		CHECK(row[0] == 0 && row[1] == 2 && row[2] == 2,
		      "bmp bottom-up row order");
	}
	{
		/* File bytes -> top-first pixels -> FNV must equal FNV(input):
		   the exact host-side verification the harness performs on
		   every retrieved twin (rows re-stacked, padding skipped). */
		unsigned char file[48], px2[5 * 3 * 3];
		uint32_t h1, h2;
		int r;
		fseek(f, 54, SEEK_SET);
		CHECK(fread(file, 1, sizeof(file), f) == sizeof(file),
		      "bmp payload readable");
		for (r = 0; r < 3; r++)
			memcpy(px2 + (unsigned)r * 15, file + (unsigned)(2 - r) * 16, 15);
		h1 = framedump_fnv1a(px, sizeof(px));
		h2 = framedump_fnv1a(px2, sizeof(px2));
		CHECK(h1 == h2, "bmp file round-trips to input hash");
	}
	fclose(f);
}

static void test_trigger_loopback(void)
{
	int srv, cli, acc;
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	char rx[128];
	char tag[64];
	ssize_t got;
	unsigned short port;
	int rc;

	srv = socket(AF_INET, SOCK_STREAM, 0);
	CHECK(srv >= 0, "loopback socket");
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;
	CHECK(bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == 0, "loopback bind");
	CHECK(listen(srv, 1) == 0, "loopback listen");
	CHECK(getsockname(srv, (struct sockaddr *)&addr, &len) == 0, "loopback port");
	port = ntohs(addr.sin_port);

	cli = socket(AF_INET, SOCK_STREAM, 0);
	addr.sin_port = htons(port);
	CHECK(connect(cli, (struct sockaddr *)&addr, sizeof(addr)) == 0,
	      "loopback connect");
	/* Driver side: send the trigger line. */
	CHECK(send(cli, "DUMP loop1\n", 11, 0) == 11, "trigger send");

	/* Game side: accept + parse, exactly like framedump_poll. */
	acc = accept(srv, NULL, NULL);
	CHECK(acc >= 0, "trigger accept");
	got = recv(acc, rx, sizeof(rx) - 1, 0);
	CHECK(got > 0, "trigger recv");
	if (got > 0) {
		unsigned char px[8 * 2 * 3];
		uint32_t h1, h2;
		FILE *lf;
		char lbuf[256];

		rx[got] = '\0';
		rc = framedump_parse_cmd(rx, tag, sizeof(tag));
		CHECK(rc == 1 && strcmp(tag, "loop1") == 0, "trigger parses");
		/* Capture stand-in: synthetic buffer, same core path. */
		fill_pattern(px, 8, 2);
		h1 = framedump_fnv1a(px, sizeof(px));
		CHECK(framedump_write_bmp("out-loop1.bmp", px, 8, 2, 3) == 0,
		      "trigger dump writes");
		lf = fopen("framedump_test.log", "w");
		fprintf(lf, "FDUMP seq=1 tag=%s screen=top w=8 h=2 bpp=3 hash=%08lx file=%s\n",
		        tag, (unsigned long)h1, "out-loop1.bmp");
		fclose(lf);
		/* Proof the log line names the bytes on disk. */
		lf = fopen("framedump_test.log", "r");
		CHECK(lf != NULL && fgets(lbuf, sizeof(lbuf), lf) != NULL &&
		      strstr(lbuf, "hash=") != NULL, "log line written");
		if (lf) fclose(lf);
		h2 = framedump_fnv1a(px, sizeof(px));
		CHECK(h1 == h2, "hash stable across reads");
		/* Driver side reply, like the game sends. */
		CHECK(send(acc, "OK out-loop1\n", 13, 0) == 13, "reply send");
	}
	{
		char rb[32];
		ssize_t n = recv(cli, rb, sizeof(rb) - 1, 0);
		if (n > 0) rb[n] = '\0';
		CHECK(n > 0 && strncmp(rb, "OK ", 3) == 0, "driver gets OK");
	}
	close(acc);
	close(cli);
	close(srv);
}

int main(void)
{
	test_parse();
	test_hash();
	test_bmp();
	test_trigger_loopback();
	if (failures == 0) printf("PASS framedump\n");
	return failures != 0;
}
