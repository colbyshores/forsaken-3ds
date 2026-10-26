/* Forsaken 3DS Probe — diagnostic CIA for OG 3DS Application-mode memory
 * budget reporting.  Built with a minimal libctru heap (8 MB malloc + 4 MB
 * linear) so it boots on any reasonable Application budget.  At runtime it
 * gathers system info + probes the actual memory ceiling and writes both
 * to sdmc:/forsaken_probe.log AND displays everything on-screen so the
 * user can photograph the results.
 *
 * UniqueId 0xff4bf.  Application mode RSF (no HIMEM grant) — matches the
 * memory mode the production LOMEM CIA runs in.
 *
 * Goal: produce a single log/photograph that answers "what's this user's
 * actual usable budget?" so we can size the production CIA correctly
 * without further iteration.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>

/* Heap declarations consumed by libctru before main() runs.  Conservative
 * sizes — fit in any plausible Application-mode budget on OG.  These
 * cannot be overridden after libctru init, so we keep them small and use
 * runtime probes (linearAlloc, svcControlMemory) to find the actual
 * ceiling beyond what we asked for. */
u32 __ctru_heap_size        = 8  * 1024 * 1024;   /* 8 MB malloc */
u32 __ctru_linear_heap_size = 4  * 1024 * 1024;   /* 4 MB linear */

static FILE *log_fp = NULL;

/* Captured summary lines — only the load-bearing numbers, displayed
 * on-screen at the end after all detail is logged.  Keeps the screen
 * stable for the user to read / photograph, while the full per-step
 * detail goes only to the log file. */
#define MAX_SUMMARY_LINES 20
static char summary[MAX_SUMMARY_LINES][96];
static int  summary_n = 0;

static void summary_add(const char *fmt, ...)
{
	if (summary_n >= MAX_SUMMARY_LINES) return;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(summary[summary_n], sizeof(summary[0]), fmt, ap);
	va_end(ap);
	summary_n++;
}

/* Log to file only (no screen).  Per-step detail uses this. */
static void plog(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (log_fp) {
		fputs(buf, log_fp);
		fputc('\n', log_fp);
		fflush(log_fp);
	}
}

static const char *model_name(u8 id)
{
	switch (id) {
		case 0: return "O3DS";
		case 1: return "O3DS-XL";
		case 2: return "N3DS";
		case 3: return "O2DS";
		case 4: return "N3DS-XL";
		case 5: return "N2DS-XL";
		default: return "unknown";
	}
}

static const char *region_name(u8 id)
{
	switch (id) {
		case 0: return "JPN";
		case 1: return "USA";
		case 2: return "EUR";
		case 3: return "AUS";
		case 4: return "CHN";
		case 5: return "KOR";
		case 6: return "TWN";
		default: return "unknown";
	}
}

/* Probe the linear-heap ceiling by attempting allocations of increasing
 * sizes.  linearAlloc goes through libctru but uses svcControlMemory
 * under the hood, which can allocate beyond our initial __ctru_linear_heap_size
 * if the kernel has Application budget remaining.  Each attempt is freed
 * before the next so we measure the absolute ceiling, not the cumulative. */
static void probe_linear_ceiling(void)
{
	plog("");
	plog("=== Linear ceiling probe (one-shot, freed each time) ===");
	int sizes[] = {1, 2, 4, 8, 12, 16, 20, 24, 26, 28, 30, 32, 40, 48};
	int n = sizeof(sizes)/sizeof(sizes[0]);
	int max_ok = 0;
	for (int i = 0; i < n; i++) {
		size_t sz = (size_t)sizes[i] * 1024 * 1024;
		void *p = linearAlloc(sz);
		plog("  linearAlloc(%dMB) -> %s", sizes[i], p ? "OK" : "NULL");
		if (p) { linearFree(p); max_ok = sizes[i]; }
		else break;  /* stop on first failure */
	}
	plog("Linear ceiling: %d MB", max_ok);
	summary_add("Linear ceiling: %d MB", max_ok);
}

static void probe_malloc_ceiling(void)
{
	plog("");
	plog("=== Malloc ceiling probe (bounded by __ctru_heap_size=8MB) ===");
	plog("  NB: malloc is limited to libctru's pre-allocated heap.");
	plog("  Production CIA requests up to 30 MB malloc; this probe");
	plog("  cannot directly measure that ceiling — uses Application");
	plog("  budget total - linear ceiling - static as proxy.");
	int sizes[] = {1, 2, 4, 6};  /* won't exceed 8MB */
	int n = sizeof(sizes)/sizeof(sizes[0]);
	int max_ok = 0;
	for (int i = 0; i < n; i++) {
		size_t sz = (size_t)sizes[i] * 1024 * 1024;
		void *p = malloc(sz);
		plog("  malloc(%dMB) -> %s", sizes[i], p ? "OK" : "NULL");
		if (p) { free(p); max_ok = sizes[i]; }
		else break;
	}
	plog("Malloc ceiling within 8MB heap: %d MB", max_ok);
}

/* Capture the application memory region size — this is the authoritative
 * total budget the kernel granted this title.  Use this + the linear
 * ceiling + the production CIA's static (9.34 MB) to determine the
 * largest heap split that will fit. */
static void probe_memory_budget(void)
{
	u32 app_size = osGetMemRegionSize(MEMREGION_APPLICATION);
	u32 app_used = osGetMemRegionUsed(MEMREGION_APPLICATION);
	u32 app_free = osGetMemRegionFree(MEMREGION_APPLICATION);
	plog("");
	plog("=== Memory regions ===");
	plog("APPLICATION region:");
	plog("  size: %u bytes (%u MB)", app_size, app_size >> 20);
	plog("  used: %u bytes (%u MB)", app_used, app_used >> 20);
	plog("  free: %u bytes (%u MB)", app_free, app_free >> 20);
	plog("SYSTEM region:");
	plog("  size: %u bytes (%u MB)",
		osGetMemRegionSize(MEMREGION_SYSTEM),
		osGetMemRegionSize(MEMREGION_SYSTEM) >> 20);
	plog("BASE region:");
	plog("  size: %u bytes (%u MB)",
		osGetMemRegionSize(MEMREGION_BASE),
		osGetMemRegionSize(MEMREGION_BASE) >> 20);
	plog("Linear free (initial): %u bytes (%u MB)",
		linearSpaceFree(), linearSpaceFree() >> 20);

	summary_add("Application total: %u MB", app_size >> 20);
	summary_add("Application free:  %u MB", app_free >> 20);
}

static void probe_system_info(void)
{
	plog("=== System info ===");

	/* Title ID — confirms which CIA the user actually installed */
	u64 tid = 0;
	APT_GetProgramID(&tid);
	plog("Title ID: 0x%08X%08X", (u32)(tid >> 32), (u32)tid);

	/* OG vs New 3DS */
	bool isN3DS = false;
	APT_CheckNew3DS(&isN3DS);
	plog("APT_CheckNew3DS: %s", isN3DS ? "TRUE (N3DS family)" : "FALSE (OG family)");

	/* Hardware model + region */
	u8 model_id = 0xFF;
	u8 region   = 0xFF;
	if (R_SUCCEEDED(cfguInit())) {
		CFGU_GetSystemModel(&model_id);
		CFGU_SecureInfoGetRegion(&region);
		cfguExit();
		plog("System model: %u (%s)", model_id, model_name(model_id));
		plog("Region: %u (%s)", region, region_name(region));
	} else {
		plog("cfgu init failed — model/region unknown");
	}

	/* Firmware version */
	u32 firm_version = osGetFirmVersion();
	plog("Firmware version: 0x%08X (%u.%u.%u-%u)",
		firm_version,
		(firm_version >> 24) & 0xFF,
		(firm_version >> 16) & 0xFF,
		(firm_version >> 8)  & 0xFF,
		firm_version & 0xFF);

	/* Kernel version */
	u32 kern_version = osGetKernelVersion();
	plog("Kernel version: 0x%08X (%u.%u.%u-%u)",
		kern_version,
		(kern_version >> 24) & 0xFF,
		(kern_version >> 16) & 0xFF,
		(kern_version >> 8)  & 0xFF,
		kern_version & 0xFF);

	summary_add("Model: %s (id=%u)", model_name(model_id), model_id);
	summary_add("Region: %s", region_name(region));
	summary_add("Firmware: %u.%u.%u-%u",
		(firm_version >> 24) & 0xFF,
		(firm_version >> 16) & 0xFF,
		(firm_version >> 8)  & 0xFF,
		firm_version & 0xFF);
	summary_add("N3DS family: %s", isN3DS ? "yes" : "no");
}

/* Estimate what the production LOMEM CIA can actually request.
 * The production CIA has static footprint ~9.34 MB (.text + .data + .bss)
 * plus 0.25 MB stack.  Whatever's left in Application budget after
 * libctru's bookkeeping overhead (~1-2 MB) is the heap headroom. */
static void estimate_production_fit(void)
{
	u32 app_total = osGetMemRegionSize(MEMREGION_APPLICATION);
	u32 app_total_mb = app_total >> 20;
	/* Forsaken static: text 1.27 + data 2.04 + bss 6.03 = 9.34 MB */
	const u32 prod_static_mb = 10;  /* round up */
	const u32 prod_stack_mb  = 1;
	const u32 ctru_overhead_mb = 2;  /* libctru's own state */
	u32 usable_for_heap_mb =
		(app_total_mb > prod_static_mb + prod_stack_mb + ctru_overhead_mb)
		? app_total_mb - prod_static_mb - prod_stack_mb - ctru_overhead_mb
		: 0;
	plog("");
	plog("=== Production-fit estimate ===");
	plog("Application total: %u MB", app_total_mb);
	plog("Production static + stack + libctru overhead: ~%u MB",
		prod_static_mb + prod_stack_mb + ctru_overhead_mb);
	plog("Available for heap (malloc + linear combined): ~%u MB",
		usable_for_heap_mb);
	plog("Suggested heap split for production:");
	char suggestion[96];
	if (usable_for_heap_mb >= 60) {
		snprintf(suggestion, sizeof(suggestion),
			"30+30 (current LOMEM works)");
	} else if (usable_for_heap_mb >= 50) {
		snprintf(suggestion, sizeof(suggestion),
			"30 malloc + %u linear", usable_for_heap_mb - 30);
	} else if (usable_for_heap_mb >= 40) {
		snprintf(suggestion, sizeof(suggestion),
			"24 + %u (military may OOM)", usable_for_heap_mb - 24);
	} else {
		snprintf(suggestion, sizeof(suggestion),
			"~%u+%u (heavy levels fail)",
			usable_for_heap_mb / 2, usable_for_heap_mb / 2);
	}
	plog("  %s", suggestion);

	summary_add("Heap room: ~%u MB", usable_for_heap_mb);
	summary_add("Recommended: %s", suggestion);
}

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;
	gfxInitDefault();
	/* Initialize TOP only.  Calling consoleSelect(NULL) literally NULLs
	 * the global currentConsole — subsequent printf -> consolePrintChar
	 * then derefs NULL and faults at FAR=0x40 (the console struct's
	 * tab-width field offset).  Single-screen TOP-only init avoids the
	 * footgun. */
	consoleInit(GFX_TOP, NULL);

	/* Show "Probing..." while the detail runs, so the screen isn't
	 * frozen if a probe step takes a while. */
	printf("Forsaken 3DS Probe\n");
	printf("\n");
	printf("Probing...\n");
	gfxFlushBuffers();
	gfxSwapBuffers();

	/* Open log; all per-step detail goes here (NOT to screen). */
	log_fp = fopen("sdmc:/forsaken_probe.log", "w");

	plog("=== Forsaken 3DS Probe ===");
	plog("Build: %s %s", __DATE__, __TIME__);
	plog("");

	probe_system_info();
	probe_memory_budget();
	probe_linear_ceiling();
	probe_malloc_ceiling();
	estimate_production_fit();

	plog("");
	plog("=== Done ===");

	if (log_fp) {
		fclose(log_fp);
		log_fp = NULL;
	}

	/* Now show a STABLE summary screen.  Clear and redraw — this is the
	 * screen the user will read or photograph.  Doesn't scroll. */
	consoleClear();
	printf("=== Forsaken 3DS Probe Results ===\n\n");
	for (int i = 0; i < summary_n; i++)
		printf("%s\n", summary[i]);
	printf("\n");
	printf("Full log: sdmc:/forsaken_probe.log\n");
	printf("Pull via FTP (ftpd-pro / FBI) and DM\n");
	printf("the file to colbyshores.\n");
	printf("\n");
	printf("Press START to exit.\n");

	/* Wait for user to exit (so they can read or photograph) */
	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_START) break;
		gspWaitForVBlank();
	}

	gfxExit();
	return 0;
}
