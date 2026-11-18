#pragma once

/* version.h -- build name and multiplayer protocol version
 *
 * PXVersion is "ProjectX-<PXV>.<PXMPV>.<PXRV>-<PXBV>-<ARCH>", e.g.
 * ProjectX-1.18.0-3DS-ARM. The PC window title shows it, networking.cpp puts
 * it in its version messages, and the PC net tracker (net_tracker.cpp) sends
 * it in a space-separated "hosting" line, so it must contain no spaces and
 * fit in MAX_PXVersion. PXMPVINT is the number the join handshake compares
 * (networking.cpp); bump PXMPV and PXMPVINT together whenever multiplayer
 * compatibility breaks.
 */

// major version
#define PXV	 "1"

// multiplayer version (increase if you break multiplayer compatibility)
#define PXMPV	 "18"

// numeric PXV + PXMPV, sent and compared when a player joins
constexpr int PXMPVINT = 118;

// revision; override with -DPXRV=... (neither Makefile sets it)
#ifndef PXRV
	#define PXRV	 "0"
#endif

// tag name so we know what type of build this is
#ifndef PXBV
	#ifdef __3DS__
		#define PXBV  "3DS"
	#else
		#define PXBV  "LINUX"
	#endif
#endif

// architecture: "64" for every non-3DS build
#ifdef __3DS__
  #define ARCH "ARM"
#else
  #define ARCH "64"
#endif

// the full version name (must contain no spaces for net_tracker)
#define PXVersion "ProjectX-" PXV "." PXMPV "." PXRV "-" PXBV "-" ARCH

// space net_tracker.cpp reserves for PXVersion in its hosting message
constexpr int MAX_PXVersion = 45;
