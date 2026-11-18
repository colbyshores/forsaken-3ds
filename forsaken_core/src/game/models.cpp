#include "util/platform.h"



/*===================================================================
*	m o d e l s . c
*	All routines to do with 3d Model Objects
===================================================================*/
#include <stdio.h>

#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "game/triggers.h"
#include "game/pickups.h"
#include "game/models.h"
#include "game/secondary.h"
#include "game/primary.h"
#include "game/enemies.h"
#include "ui/title.h"
#include "rendering/text.h"
#include "rendering/lines.h"
#include "rendering/2dpolys.h"
#include "rendering/polys.h"
#include "game/spotfx.h"
#include "game/water.h"
#include "game/main.h"
#include "input/controls.h"
#include "ui/local.h"
#include "util/util.h"
#include "game/oct2.h"
#include "data/mload.h"
#include "data/mxaload.h"
#include "game/camera.h"
#include "game/stats.h"
#include "net/multiplayer.h"
#include "rendering/2dtextures.h"
#include "rendering/lights.h"
#include "world/visi.h"


constexpr auto BALL_RADIUS = ( 256 * GLOBAL_SCALE );

/*===================================================================
	Globals
===================================================================*/
void CheckModelLinkList( void );
MODEL	Models[MAXNUMOFMODELS];
u_int16_t	FirstModelUsed;
u_int16_t	FirstModelFree;
static u_int32_t ModelVisGeneration;

int16_t	NextNewModel = -1;
bool	ShowBoundingBoxes = false;


/* calloc'd by PreInitModel, sized to cover the highest populated
 * ModelNames[] slot, and freed by the next PreInitModel (so the array
 * outlives ReleaseView: level teardown still reads it). It replaced a
 * MAXMODELHEADERS-sized static array to give back ~6 MB of BSS. */
MXLOADHEADER *ModelHeaders = NULL;

constexpr auto RETAIN_POLYS = true;
constexpr auto DISCARD_POLYS = false;
void GetRealLightAmbient( VECTOR * Pos , float * Red , float * Green , float * Blue );
bool	ENVMxa( MXALOADHEADER * Mxaloadheader , MATRIX * Mat ,VECTOR * Pos);

/*===================================================================
	In game models
===================================================================*/
MODELNAME	ModelNames[MAXMODELHEADERS] ={	

	{ "heat.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Heatseaker missile
	{ "fheat.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "grav.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Gravgon
	{ "fgrav.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "troj.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Trojax
	{ "stroj.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "prlt.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Pyrolyte
	{ "sprlt.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "trans.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Transpulse
	{ "strans.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "sus.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Suss Gun
	{ "ssus.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// LOD 1..
	{ "beam.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Beam Laser
	{ "sbeam.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "pod.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// powerpod..
	{ "spod.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// LOD 1..
	{ "fuel.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Fuel				
//	{ "sfuel.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..				
	{ "shld.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Shield
	{ "sshld.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "ammo.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Suss Gun
	{ "sammo.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "nrg.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Energy
	{ "snrg.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// LOD 1..
	{ "mug.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Mug Missile
	{ "fmug.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// LOD 1..
	{ "mugs.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Mug pickup
	{ "smugs.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "sctr.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Scatter Missile
	{ "fsctr.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
//	{ "thef.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Thief Missile
//	{ "fthef.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "titan.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Timer Missile
	{ "titan.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "lnch.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Rocket Launcher
	{ "slnch.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "comp.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Computer Targetting
	{ "scomp.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "pin.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Pine Mine
	{ "spin.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// LOD 1 ..
	{ "pine.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Pine Pickup
	{ "spine.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "qpod.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Quantum Pickup
	{ "sqpod.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "qpik.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Quantum Mine
	{ "sqpik.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "prge.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Purge Mine
	{ "sprge.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
//	{ "spid.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Spider Mine
//	{ "sspid.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "heatpk.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Heatseaker
	{ "sheatpk.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
//	{ "smok.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Smoke Streamer
//	{ "ssmok.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "prgpod.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Purge Pickup
	{ "sprgpod.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
//	{ "para.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Parasite Mine
//	{ "spara.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "ntro.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Nitro
	{ "sntro.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1
//	{ "gogl.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Goggles
//	{ "sgogl.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "vuln.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Invulnerability
	{ "svuln.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "gold.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "mant.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Stealth Mantle
	{ "smant.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
//	{ "flar.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Flare Pod
//	{ "sflar.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
//	{ "spdpod.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Spider Pod
//	{ "sspdpod.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "crys.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "orb.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Orbital Pulsar
	{ "sorb.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// LOD 1 ..
	{ "xtra.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Extra Life
	{ "sxtra.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1 ..
	{ "fpin.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "ball1.mx" , 2 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Ball
	{ "ball2.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "ball3.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 2..
//	{ "chaf.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Chaff
//	{ "schaf.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
//	{ "pnlmesh.mx" , 0 , true, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// the Panel....
	{ "gpod.mx" , 1 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Golden Powerpod
	{ "sgldpod.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1..
	{ "orb2.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Orbit pulsar
	{ "orb3.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Orbit pulsar ( Light Model )
	{ "gren.mx" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Titan Star Shrapnel
	{ "tracker.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Ian's Tracker
	{ "ping.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Ian's ping
	{ "brain.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Daniel's Brain
	{ "ear.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Daniel's Ear
	{ "eyeball.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Daniel's Eyeball
	{ "heart.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Daniel's Heart
	{ "kidney.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Daniel's Kidney
	{ "leg.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Daniel's Leg
	{ "shockwave.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Explosion Donut
	{ "nmemisil.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// NME Missile
	{ "dna.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// DNA Pickup
	{ "key.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Skeleton Key Pickup
	{ "shrap1.mx", 1, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Shrapnel
	{ "lshrap1.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// LOD 1
	{ "shrap2.mx", 1, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Shrapnel2
	{ "lshrap2.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// LOD 1
	{ "glass.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Shrapnel
	{ "rock1.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// LOD 1
	{ "rock2.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Shrapnel2
	{ "bomb.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Bomb Pickup
	{ "goldfig.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Gold Figure
	{ "flagmrphwave000.mxa" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD }, //Flag
	{ "redflagwave000.mxa" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD }, // Red Flag
	{ "greenflagwave000.mxa" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD }, //Green Flag
	{ "blueflagwave000.mxa" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD }, //Blue Flag
	{ "yellowflagwave000.mxa" , 0 , false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD }, //Yellow Flag


	{ "tc-01.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Tom-Cruise Anim
	{ "tc-02.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-03.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-04.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-05.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-06.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-07.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-08.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-09.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-10.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-11.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-12.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-13.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-14.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-15.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-16.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-17.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-18.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-19.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-20.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-21.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-22.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-23.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-24.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-25.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-26.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "tc-27.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "borg400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "borg300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "borg200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "borg100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "borg050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sborg400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "sborg300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sborg200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sborg100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sborg050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "brd400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "brd300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "brd200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "brd100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "brd050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sbrd400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "sbrd300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sbrd200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sbrd100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sbrd050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },


	{ "laj400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "laj300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "laj200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "laj100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "laj050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "slaj400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "slaj300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "slaj200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "slaj100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "slaj050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },


	{ "xcop400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "xcop300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "xcop200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "xcop100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "xcop050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sxcop400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "sxcop300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sxcop200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sxcop100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sxcop050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "truk400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "truk300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "truk200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "truk100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "truk050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "struk400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "struk300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "struk200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "struk100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "struk050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },


	{ "foet400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "foet300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "foet200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "foet100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "foet050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sfoet400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "sfoet300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sfoet200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sfoet100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sfoet050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "jap400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "jap300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "jap200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "jap100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "jap050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sjap400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "sjap300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sjap200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sjap100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sjap050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "nut400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "nut300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "nut200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "nut100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "nut050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "snut400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "snut300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "snut200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "snut100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "snut050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "rhes400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "rhes300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "rhes200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "rhes100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "rhes050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "srhes400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "srhes300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "srhes200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "srhes100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "srhes050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "jo400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Unstealthed
	{ "jo300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "jo200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "jo100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "jo050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sjo400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "sjo300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sjo200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sjo100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sjo050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "shrk400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "shrk300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "shrk200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "shrk100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "shrk050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sshrk400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "sshrk300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sshrk200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sshrk100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sshrk050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "hk400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Unstealthed
	{ "hk300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "hk200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "hk100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "hk050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "shk400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "shk300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "shk200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "shk100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "shk050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "nbia400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "nbia300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "nbia200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "nbia100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "nbia050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "snbia400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "snbia300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "snbia200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "snbia100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "snbia050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "mofi400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "mofi300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "mofi200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "mofi100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "mofi050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "smofi400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "smofi300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "smofi200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "smofi100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "smofi050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "cerb400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "cerb300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "cerb200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "cerb100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "cerb050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "scerb400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "scerb300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "scerb200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "scerb100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "scerb050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },

	{ "slick400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Unstealthed
	{ "slick300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "slick200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "slick100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "slick050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sslick400.mxa", 4, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Stealthed
	{ "sslick300.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sslick200.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sslick100.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "sslick050.mxa", 0, false, MODELTYPE_Morphing, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },


	{ "" , 0 , false, MODELTYPE_Static, 0, false, DONTLOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD }
};

/*===================================================================
	Title screen models
===================================================================*/
MODELNAME *TitleModelSet;

MODELNAME	TitleModelNames[ MAXMODELHEADERS ] ={	
	{ "disc1.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 1
	{ "disc2.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 2
	{ "disc3.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 3
	{ "disc4.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 4
	{ "disc5.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 5
	{ "disc6.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 6
	{ "mbot.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Bot. of mech. device
	{ "menuroom.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// main title screen room
	{ "vdu.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// VDU
	{ "holopad.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Holopad
	{ "mtop.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Top of mech. device
	{ "menutvdummy.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// TV screen
	{ "menutv.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// TV screen
	{ "hglaire.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// translucent glare effect for holopad
	{ "bracket.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// VDU bracket
	{ "borg400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "brd400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "laj400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "xcop400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "truk400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "foet400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "jap400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "nut400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "rhes400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "jo400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			
	{ "shrk400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "hk400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			
	{ "nbia400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "mofi400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		
	{ "cerb400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "slick400.mxa", 0, false, MODELTYPE_Morphing, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "" , 0 , false, MODELTYPE_Static, 0, false, DONTLOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD }
};

MODELNAME	InterLevelModelNames[ NUM_INTERLEVEL_MODELS + 1 ] ={
	{ "disc1.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 1
	{ "disc2.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 2
	{ "disc3.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 3
	{ "disc4.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 4
	{ "disc5.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 5
	{ "disc6.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Disc 6
	{ "mbot.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Bot. of mech. device
	{ "menuroom.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// main title screen room
	{ "vdu.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// VDU
	{ "holopad.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// Holopad
	{ "mtop.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// Top of mech. device
	{ "menutvdummy.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// TV screen
	{ "menutv.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },			// TV screen
	{ "hglaire.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// translucent glare effect for holopad
	{ "bracket.mx", 0, false, MODELTYPE_Static, 0, DISCARD_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },		// VDU bracket

	{ "crys.mx" , 0 , false, MODELTYPE_Static, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "gold.mx" , 0 , false, MODELTYPE_Static, 0, RETAIN_POLYS,  LOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
	{ "" , 0 , false, MODELTYPE_Static, 0, false, DONTLOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD }
};

MODELNAME	SplashModelNames[ MAXMODELHEADERS ] ={
	{ "" , 0 , false, MODELTYPE_Static, 0, false, DONTLOAD_TPAGES, NOT_LEVEL_SPECIFIC, DO_LOAD },
};

/*===================================================================
*		Set up 2d exec buff etc...
===================================================================*/
void OnceOnlyInitModel( void )
{
	int i;
	int	Count;

	FirstModelUsed = INDEX_NONE;
	FirstModelFree = 0;
	
	for( i=0;i<MAXNUMOFMODELS;i++)
	{
		memset( &Models[i], 0, sizeof( MODEL ) );
		Models[i].Func = MODFUNC_Nothing;
		Models[i].LifeCount = -1.0F;
		Models[i].Scale = 1.0F;
		Models[i].Visible = true;
		Models[i].TimeInterval = (float) 1;

		for( Count = 0; Count < 12; Count++ ) Models[i].TempLines[ Count ] = INDEX_NONE;

		Models[i].Next = i + 1;
		Models[i].Prev = INDEX_NONE;
	}

	Models[MAXNUMOFMODELS-1].Next = INDEX_NONE;

	NextNewModel = MODEL_ExtraModels;

	for( i = MODEL_ExtraModels; i < MAXMODELHEADERS ; i++ )
	{
		memset( &ModelNames[ i ], 0, sizeof( MODELNAME ) );
	}

	for( i = TITLE_MODEL_ExtraModels; i < MAXMODELHEADERS ; i++ )
	{
		memset( &TitleModelNames[ i ], 0, sizeof( MODELNAME ) );
	}

	for( i = 0; i < MAXMODELHEADERS ; i++ )
	{
		memset( &SplashModelNames[ i ], 0, sizeof( MODELNAME ) );
	}
}

/*===================================================================
	Procedure	:	Pre-Init models
	Input		:	MODELNAME	*		Current Model Name
	Output		:	bool				true/false
===================================================================*/
bool PreInitModel( MODELNAME *NamePnt )
{
	int			i;
	int8_t		TempFilename[ 256 ];
	int8_t		Ext[ 32 ];

	/* Free + re-allocate ModelHeaders / MxaModelHeaders sized to cover
	 * every populated MODELNAME slot. Slots are NOT laid out densely:
	 * regular models occupy [0..*_ExtraModels-1], then PreLoadCompObj
	 * (called by PreLoadFlyGirl, PreLoadShips, PreLoadBGOFiles, etc.)
	 * registers component-object children into [*_ExtraModels..NextNewModel-1]
	 * with a zero-terminator after each contiguous run. So a naive
	 * "count up to first empty Name" stops at the regular section's
	 * terminator and misses the appended FlyGirl/ship/bgobject entries —
	 * SetupModelSpotFX(ModelHeaders[high_index]) then reads past the
	 * malloc'd buffer and faults on a misaligned SpotFX pointer.
	 *
	 * Fix: scan the full MODELNAME array for the highest populated
	 * index, then size to that. Adjacent runs are separated by a
	 * terminator entry; ignore it and keep scanning.
	 *
	 * The arrays stay live through ReleaseView so ReleaseLevel teardown
	 * can still read MxaModelHeaders[i].NumSpotFX safely (set to 0 by
	 * the preceding ReleaseModels). Any previous allocation is freed
	 * here, just before we size the new one. */
	{
		free(ModelHeaders);    ModelHeaders    = NULL;
		free(MxaModelHeaders); MxaModelHeaders = NULL;
	}
	if (!ModelHeaders || !MxaModelHeaders)
	{
		int count = 0;
		int max_idx = -1;
		MODELNAME *p = NamePnt;
		for (int idx = 0; idx < MAXMODELHEADERS; idx++)
		{
			if (p[idx].Name[0]) max_idx = idx;
		}
		count = max_idx + 1;
		if (count == 0) count = 1; /* defensive: never alloc 0 */
		ModelHeaders    = (MXLOADHEADER *)  calloc(count, sizeof(MXLOADHEADER));
		MxaModelHeaders = (MXALOADHEADER *) calloc(count, sizeof(MXALOADHEADER));
		if (!ModelHeaders || !MxaModelHeaders)
		{
			free(ModelHeaders);    ModelHeaders    = NULL;
			free(MxaModelHeaders); MxaModelHeaders = NULL;
			return false;
		}
	}

	for( i = 0 ; i < MAXMODELHEADERS ; i++ )
	{
		if( NamePnt->Name[0] )
		{
			DebugPrintf("Loading %s\n",NamePnt->Name);
			if( NamePnt->LoadEnable )
			{
				if( NamePnt->LevelSpecific )
				{
					sprintf( (char*)&TempFilename[0], "data\\levels\\%s\\models\\%s", &ShortLevelNames[ LevelNum ][ 0 ], NamePnt->Name );
				}
				else
				{
					sprintf( (char*)&TempFilename[0], "data\\models\\%s", NamePnt->Name );
				}

				Get_Ext( (u_int8_t*)NamePnt->Name, (u_int8_t*)&Ext[ 0 ] );

				if( !strcasecmp( (char*)&Ext[ 0 ], "MX" ) )
				{
					NamePnt->DoIMorph = false;

					if( !PreMxload( (char*)&TempFilename[0], &ModelHeaders[i] , NamePnt->Panel, NamePnt->LevelSpecific ) ) return false;	// the model and visipoly data
				}
				else
				{
					if( !strcasecmp( (char*)&Ext[ 0 ], "MXA" ) )
					{
						NamePnt->DoIMorph = true;

						MxaModelHeaders[i].AllocateTPage = NamePnt->AllocateTpage;

						if( !PreMxaload( (char*)&TempFilename[0], MxaModelHeaders, i, NamePnt->LevelSpecific ) ) return false;	// the model and visipoly data
					}
					else
					{
						DebugPrintf( "Unknown format for file %s\n", &TempFilename[ 0 ] );
						return false;
					}
				}
			}

			NamePnt++;

		}
		else break;
	}
	return	true;
}

/*===================================================================
	Procedure	:	Init models
	Input		:	MODELNAME	*		Current Model Name
	Output		:	bool				true/false
===================================================================*/
bool InitModel( MODELNAME * NamePnt)
{
	int i;
	int8_t		TempFilename[ 256 ];
	VECTOR		TempVector = { 0.0F, 0.0F, 0.0F };
	VECTOR		TopLeft;
	VECTOR		BottomRight;

	for( i = 0 ; i < MAXMODELHEADERS ; i++ )
	{
		if( NamePnt->Name[0] != 0 )
		{
			if( NamePnt->LoadEnable )
			{
				strcpy( (char*)&TempFilename[0], (char*)NamePnt->Name );

#ifdef __3DS__
				/* Periodic linear-heap snapshot during model load — every
				 * 50th model.  Gives us a curve so we can see where the
				 * heap drains. */
#endif

				if( NamePnt->DoIMorph )
				{

					if( !Mxaload( (char*)&TempFilename[0] , &MxaModelHeaders[i], NamePnt->StoreTriangles ) ) {
						return false;	// the model and visipoly data
					}
					ModelHeaders[i].LOD = NamePnt->LOD;

					TopLeft = TempVector;
					BottomRight = TempVector;
					GetMXABoundingBox( &MxaModelHeaders[i], &MATRIX_Identity, &TempVector, &TopLeft, &BottomRight );
					ModelHeaders[i].Center.x = ( TopLeft.x + ( ( BottomRight.x - TopLeft.x ) * 0.5F ) );
					ModelHeaders[i].Center.y = ( TopLeft.y + ( ( BottomRight.y - TopLeft.y ) * 0.5F ) );
					ModelHeaders[i].Center.z = ( TopLeft.z + ( ( BottomRight.z - TopLeft.z ) * 0.5F ) );
					ModelHeaders[i].Sizes.x = ( BottomRight.x - TopLeft.x );
					ModelHeaders[i].Sizes.y = ( BottomRight.y - TopLeft.y );
					ModelHeaders[i].Sizes.z = ( BottomRight.z - TopLeft.z );
				}
				else
				{

					if( !Mxload( (char*)&TempFilename[0] , &ModelHeaders[i] , NamePnt->Panel, NamePnt->StoreTriangles) ) {
						return false;	// the model and visipoly data
					}
					ModelHeaders[i].LOD = NamePnt->LOD;

					TopLeft = TempVector;
					BottomRight = TempVector;
					GetMXBoundingBox( &ModelHeaders[i], &MATRIX_Identity, &TempVector, &TopLeft, &BottomRight );
					ModelHeaders[i].Center.x = ( TopLeft.x + ( ( BottomRight.x - TopLeft.x ) * 0.5F ) );
					ModelHeaders[i].Center.y = ( TopLeft.y + ( ( BottomRight.y - TopLeft.y ) * 0.5F ) );
					ModelHeaders[i].Center.z = ( TopLeft.z + ( ( BottomRight.z - TopLeft.z ) * 0.5F ) );
					ModelHeaders[i].Sizes.x = ( BottomRight.x - TopLeft.x );
					ModelHeaders[i].Sizes.y = ( BottomRight.y - TopLeft.y );
					ModelHeaders[i].Sizes.z = ( BottomRight.z - TopLeft.z );
				}
			}

			NamePnt++;
		}
		else break;
	}
	return	true;
}

/*===================================================================
	Procedure	:	Find a free Model and move it from the free list to
					the used list
	Input		:	nothing
	Output		:	u_int16_t number of Model free....
===================================================================*/
u_int16_t	FindFreeModel()
{
	u_int16_t i;

#ifdef DEBUG_ON
	CheckModelLinkList();
#endif

	i = FirstModelFree;
	
	if ( i == INDEX_NONE)
		return i;
 
	Models[i].Prev = FirstModelUsed;
	if ( FirstModelUsed != INDEX_NONE)
	{
		Models[FirstModelUsed].Next = i;
	}

	Models[i].Flags = MODFLAG_Nothing;
	Models[i].Func = MODFUNC_Nothing;
	Models[i].LifeCount = -1.0F;
	Models[i].Scale = 1.0F;
	Models[i].Visible = true;
	Models[i].TimeInterval = (float) 1;
	Models[i].TimeCount = 0.0F;

	Models[i].Red = 255;
	Models[i].Green = 255;
	Models[i].Blue = 255;

	FirstModelUsed = i;
	FirstModelFree = Models[i].Next;
	ModelVisGeneration++;

#ifdef DEBUG_ON
	CheckModelLinkList();
#endif
	return i ;
}

/*===================================================================
	Procedure	:	Kill a used Model and move it from the used list to
					the free list
	Input		:	u_int16_t number of Model free....
	Output		:	nothing
===================================================================*/
void	KillUsedModel( u_int16_t i )
{
	u_int16_t		its_prev;
	u_int16_t		its_next;
	int16_t		Count;
	
#ifdef DEBUG_ON
	CheckModelLinkList();
#endif
	its_prev = Models[i].Prev;
	its_next = Models[i].Next;

	if ( i == FirstModelUsed )
		FirstModelUsed = Models[i].Prev;

	if( its_prev != INDEX_NONE)
		Models[its_prev].Next = its_next;

	if( its_next != INDEX_NONE)
		Models[its_next].Prev = its_prev;

	for( Count = 0; Count < 12; Count++ )
	{
		if( Models[i].TempLines[ Count ] != INDEX_NONE )
		{
			KillUsedLine( Models[i].TempLines[ Count ] );
			Models[i].TempLines[ Count ] = INDEX_NONE;
		}
	}

	KillAttachedSpotFX( i );
	KillAttachedSoundFX( i );

	Models[i].Func = MODFUNC_Nothing;
	Models[i].Prev = INDEX_NONE;
	Models[i].Next = FirstModelFree;
	Models[i].LifeCount = -1.0F;
	Models[i].Scale = 1.0F;
	FirstModelFree	= i;
	ModelVisGeneration++;

#ifdef DEBUG_ON
	CheckModelLinkList();
#endif
}


/*===================================================================
	Procedure	:	Kill Attached SFX still playing
	Input		:	u_int16_t	number of Model
	Output		:	nothing
===================================================================*/
void KillAttachedSoundFX( u_int16_t i )
{
	int16_t		Count;
	u_int16_t		NumSpotFX = 0;
	PVSPOTFX *	SpotFXPtr = NULL;

	switch( Models[i].Func )
	{
		case MODFUNC_ProcessSpotFX:
			if( ModelNames[ Models[i].ModelNum ].DoIMorph )
			{
				if( MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
					MxaModelHeaders[ Models[i].ModelNum ].SpotFX )
				{
					SpotFXPtr = MxaModelHeaders[ Models[i].ModelNum ].SpotFX;
					NumSpotFX = MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX;
				}
			}
			else
			{
				if( ModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
					ModelHeaders[ Models[i].ModelNum ].SpotFX )
				{
					SpotFXPtr = ModelHeaders[ Models[i].ModelNum ].SpotFX;
					NumSpotFX = ModelHeaders[ Models[i].ModelNum ].NumSpotFX;
				}
			}

			if( SpotFXPtr && NumSpotFX )
			{
				for( Count = 0; Count < NumSpotFX; Count++ )
				{
					switch( SpotFXPtr->Type )
					{
						case SPOTFX_SoundFX:
							if( SpotFXPtr->SoundFX != -1 )
							{
								if( Models[i].SpotFXSFX_ID[ Count ] )
								{
									StopSfx( Models[i].SpotFXSFX_ID[ Count ] );
									Models[i].SpotFXSFX_ID[ Count ] = 0;
									Models[i].SpotFXFlags[ Count ] &= ~SPOTFX_FLAGS_DoneOnce;
								}
							}
							break;
						default:
							break;
					}
					SpotFXPtr++;
				}
			}
			break;

		default:
			break;
	}
}

/*===================================================================
	Procedure	:	Kill Attached Spot still playing
	Input		:	u_int16_t	number of Model
	Output		:	nothing
===================================================================*/
void KillAttachedSpotFX( u_int16_t i )
{
	int16_t		Count;
	u_int16_t		NumSpotFX = 0;
	PVSPOTFX *	SpotFXPtr = NULL;

	switch( Models[i].Func )
	{
		case MODFUNC_ProcessSpotFX:
			if( ModelNames[ Models[i].ModelNum ].DoIMorph )
			{
				if( MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
					MxaModelHeaders[ Models[i].ModelNum ].SpotFX )
				{
					SpotFXPtr = MxaModelHeaders[ Models[i].ModelNum ].SpotFX;
					NumSpotFX = MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX;
				}
			}
			else
			{
				if( ModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
					ModelHeaders[ Models[i].ModelNum ].SpotFX )
				{
					SpotFXPtr = ModelHeaders[ Models[i].ModelNum ].SpotFX;
					NumSpotFX = ModelHeaders[ Models[i].ModelNum ].NumSpotFX;
				}
			}

			if( SpotFXPtr && NumSpotFX )
			{
				for( Count = 0; Count < NumSpotFX; Count++ )
				{
					switch( SpotFXPtr->Type )
					{
						case SPOTFX_FirePrimary:
							KillPrimBullsByOwner( OWNER_MODELSPOTFX, i );
							break;
						default:
							break;
					}
					SpotFXPtr++;
				}
			}
			break;

		default:
			break;
	}
}

/*===================================================================
*		Per-camera model visibility buckets
*
*	ModelDisp is called once per visible group and used to walk the WHOLE
*	Models[] list (~500 entries) each time, re-testing visibility for every
*	model. That test depends only on (Models[i].Visible, Models[i].Group,
*	IsGroupVisible[], the camera view group, InTitle) plus the per-model
*	ClipGroup, none of which changes during one camera traversal, so the
*	visible models are bucketed by clip group once per camera and each
*	ModelDisp call walks only its own bucket, in Models[] list order. The
*	submission sequence - and every drawn polygon - is unchanged.
===================================================================*/

static u_int16_t VisBucketHead[MAXGROUPS];
static u_int16_t VisBucketTail[MAXGROUPS];
static u_int16_t VisBucketNext[MAXNUMOFMODELS];
static u_int16_t VisBucketClip[MAXNUMOFMODELS];
static u_int8_t  VisBucketTitle;
static u_int8_t VisBucketDirty = 1;
static u_int32_t VisBucketGeneration;


static void ModelVisBucket_Build( int InTitle )
{
	int i;
	int g;
	for( g = 0; g < MAXGROUPS; g++ )
	{
		VisBucketHead[g] = INDEX_NONE;
		VisBucketTail[g] = INDEX_NONE;
	}
	i = FirstModelUsed;
	while( i != INDEX_NONE )
	{
		u_int16_t clip;
		int vis = Models[i].Visible &&
			( InTitle || IsGroupVisible[ Models[i].Group ] || (
				CAMERA_VIEW_IS_VALID &&
				VisibleOverlap( Ships[ Current_Camera_View ].Object.Group, Models[i].Group, NULL )
			) );
		if( vis )
		{
			clip = ( Models[i].Flags & MODFLAG_UseClipGroup ) ? Models[i].ClipGroup : Models[i].Group;
			if( clip >= MAXGROUPS )
			{
				i = Models[i].Prev;
				continue;
			}
			VisBucketClip[i] = clip;
			VisBucketNext[i] = INDEX_NONE;
			if( VisBucketHead[clip] == INDEX_NONE )
				VisBucketHead[clip] = i;
			else
				VisBucketNext[ VisBucketTail[clip] ] = i;
			VisBucketTail[clip] = i;
		}
		i = Models[i].Prev;
	}
	VisBucketTitle = (u_int8_t)( InTitle ? 1 : 0 );
	VisBucketGeneration = ModelVisGeneration;
	VisBucketDirty = 0;
}

void ModelVisCache_NewFrame( void )
{
	VisBucketDirty = 1;
}

/* Rebuild the buckets when the camera, the title/game pass or the model
 * list changed since the last build. */
#define MODEL_BUCKET_REFRESH(InTitle) \
	do { if( VisBucketDirty || VisBucketTitle != (u_int8_t)((InTitle)?1:0) || VisBucketGeneration != ModelVisGeneration ) \
		ModelVisBucket_Build( InTitle ); } while(0)


bool ModelDisp( u_int16_t group, MODELNAME * NamePnt  )
{
	RENDERMATRIX TempWorld;
	u_int16_t	i;
	u_int16_t	nextmodel;
	u_int16_t	ModelNum;
	float	temp, temp2;
	VECTOR	TempVector;
	QUAT	TempQuat;
	MATRIX	TempMatrix, TempRotMatrix;
	VECTOR	x1, x2;
	u_int16_t	ClipGroup;
	float r , g , b;
	bool	InTitle;
	bool	DoDisplay;

	if( NamePnt == &ModelNames[0] ) InTitle = false;
	else InTitle = true;
	MODEL_BUCKET_REFRESH( InTitle );

	/* Walk only this group's models, in list order (the bucket chains are
	 * tail-appended in Models[] list order). ClipGroup is implied. */
	i = VisBucketHead[ group ];
	while( i != INDEX_NONE )
	{
		nextmodel = VisBucketNext[i];
		ClipGroup = VisBucketClip[i];

		if( ClipGroup == group )
		{
			switch( Models[i].Func )
			{
				case MODFUNC_Fade:
					break;
				case MODFUNC_ReScale:
					break;

				case MODFUNC_Explode:
				case MODFUNC_Regen:
				case MODFUNC_Scale:
				case MODFUNC_Scale2:
				case MODFUNC_SphereZone:
					ApplyMatrix( &CurrentCamera.Mat, &Forward, &TempVector );
					TempQuat.FromVector2(&TempVector);
					TempQuat.ToMatrix(&Models[ i ].Mat);
					TempMatrix = MATRIX_Identity;
					TempMatrix._11 = Models[i].Scale;
					TempMatrix._22 = Models[i].Scale;
					TempMatrix._33 = Models[i].Scale;
					MatrixMultiply( &TempMatrix, &Models[i].Mat, &Models[i].Mat );
					MatrixTranspose( &Models[ i ].Mat, &Models[ i ].InvMat );
					break;
			
				case MODFUNC_RealScale:
					Models[ i ].Mat = MATRIX_Identity;
					Models[ i ].Mat._11 = Models[i].Scale;
					Models[ i ].Mat._22 = 1.0F; //Models[i].Scale;
					Models[ i ].Mat._33 = Models[i].Scale;
					MatrixTranspose( &Models[ i ].Mat, &Models[ i ].InvMat );
					break;

				case MODFUNC_ScaleXYZandRotate:
					TempMatrix = MATRIX_Identity;
					TempMatrix._11 *= Models[i].Xscale;
					TempMatrix._22 *= Models[i].Yscale;
					TempMatrix._33 *= Models[i].Zscale;

					BuildRotMatrix(HoloRot.x, HoloRot.y, HoloRot.z, &TempRotMatrix);
							

					MatrixMultiply( &TempMatrix, &TempRotMatrix, &Models[i].Mat );
					MatrixTranspose( &Models[ i ].Mat, &Models[ i ].InvMat );
					break;
						
				case MODFUNC_ScaleDonut:
					Models[ i ].Mat = MATRIX_Identity;
					Models[ i ].Mat._11 = Models[i].Scale;
					Models[ i ].Mat._22 = Models[i].Scale;
					Models[ i ].Mat._33 = Models[i].Scale;
					MatrixTranspose( &Models[ i ].Mat, &Models[ i ].InvMat );
					break;

				case MODFUNC_Nothing:
				default:
					break;

			}

			ENV( &ModelHeaders[Models[i].ModelNum] , &Models[i].Mat , &Models[i].Pos);
				
			TempWorld._11 = Models[i].InvMat._11;
			TempWorld._12 = Models[i].InvMat._12;
			TempWorld._13 = Models[i].InvMat._13;
			TempWorld._14 = Models[i].InvMat._14;
			TempWorld._21 = Models[i].InvMat._21;
			TempWorld._22 = Models[i].InvMat._22;
			TempWorld._23 = Models[i].InvMat._23;
			TempWorld._24 = Models[i].InvMat._24;
			TempWorld._31 = Models[i].InvMat._31;
			TempWorld._32 = Models[i].InvMat._32;
			TempWorld._33 = Models[i].InvMat._33;
			TempWorld._34 = Models[i].InvMat._34;
			TempWorld._41 = Models[i].Pos.x;
			TempWorld._42 = Models[i].Pos.y;
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			TempWorld._43 = Models[i].Pos.z;
			TempWorld._44 = Models[i].InvMat._44;

			if (!FSSetWorld(&TempWorld))
			{
				Msg( "ModelDisp() SetMatrix1 Failed\n" );
				return false;
			}
					
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			// TODO - BEEF UP LOD LEVEL HANDLING (read notes bellow)

			// set ModelNum = 0 bellow for best lod level
			// set ModelNum = ModelHeaders[Models[i].ModelNum].LOD bellow for worst lod level
					
			// should probably add a switch to completely disable this...

			/*	Execute it	*/
			temp = DistanceVector2Vector( &Models[i].Pos, &CurrentCamera.Pos ); 
		
			// this is for objects that dont' support diff lod values like in title room
 			if( ModelHeaders[Models[i].ModelNum].LOD == 0 )
			{
				ModelNum = 0;
			}

			// objects that support lod
			else
			{
				{
					// appears the larger you make this number 
					// the farther away we can see the object with better lod's
					/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
					// we should probably make this a global so people can control it on it's own
					float distance = CurrentCamera.UseLowestLOD ? 
									6144.0F	:	// down grade for smaller cams
									12288.0F;	// larger value for full screen

					// appears to be based on how many many pickups need to be rendered
					temp2 = ( temp + ( ( (float) NumPickupsPerGroup[ group ] ) * ( 128.0F * GLOBAL_SCALE ) ) );
								
					// wtf is this?
					if( temp2 >= ( ( distance * GLOBAL_SCALE )  * ( (float) ModelHeaders[Models[i].ModelNum].LOD ) )  )
					{
						// sets it to the lowest value
						ModelNum = ModelHeaders[Models[i].ModelNum].LOD;
					}
					else
					{
						// wtf is this ?
						ModelNum = (u_int16_t) ( temp2 / (distance*GLOBAL_SCALE )  );

						// make sure we don't select a value larger than acceptable
						if( ModelNum > ModelHeaders[ Models[i].ModelNum ].LOD )
						{
							ModelNum = ModelHeaders[Models[i].ModelNum].LOD;
						}
					}
				}
			}

			// END OF TODO

			ModelNum += Models[i].ModelNum;
		
			if( ( temp > ( PICKUP_RADIUS * 0.75F ) ) || ( Models[i].Type == MODTYPE_Field ) )
			{
				DoDisplay = true;

				if( NamePnt[ ModelNum ].DoIMorph )
				{
					if( ( Models[i].Flags & MODFLAG_StealthOffset ) && ( Models[i].Flags & MODFLAG_Stealth ) )
					{
						if( !InterpFrames( &MxaModelHeaders[ ModelNum + Models[i].StealthOffset ], Models[i].Frame, Models[i].InterpFrame, Models[i].InterpTime ) )
						{
							Msg( "ModelDisp() InterpFrames() Failed\n" );
							return false;
						}
					}
					else
					{
						if( !InterpFrames( &MxaModelHeaders[ ModelNum ], Models[i].Frame, Models[i].InterpFrame, Models[i].InterpTime ) )
						{
							Msg( "ModelDisp() InterpFrames() Failed\n" );
							return false;
						}
					}

					x1.x = 0.0F;
					x1.y = 0.0F;
					x1.z = 0.0F;
					x2 = x1;

					CreateMXABoundingBox( &MxaModelHeaders[ ModelNum ],	&Models[i].Mat, &Models[i].Pos,
										  &Models[i].TempLines[ 0 ], Models[i].Group );

					ENVMxa( &MxaModelHeaders[ ModelNum ] , &Models[i].Mat , &Models[i].Pos);


					if( Models[i].Flags & MODFLAG_AmbientLight )
					{
						GetRealLightAmbient( &Models[i].Pos , &r , &g , &b );
						AmbientLightMxaModel( &MxaModelHeaders[ ModelNum ],
										Models[i].Red, Models[i].Green, Models[i].Blue, 0 , r , g , b);
					}else if( Models[i].Flags & MODFLAG_RealLight )
					{
						TempMatrix = Models[i].Mat;
						TempMatrix._41 = Models[i].Pos.x;
						TempMatrix._42 = Models[i].Pos.y;
						TempMatrix._43 = Models[i].Pos.z;
						XLightMxaloadHeader( &MxaModelHeaders[ ModelNum ], &Models[i].Pos, Models[i].Radius, &TempMatrix );
					}

					if( Models[i].Flags & MODFLAG_Stealth )
					{
						if( Models[i].Flags & MODFLAG_StealthOffset )
						{
							ModelNum += Models[i].StealthOffset;
						}
						else
						{
							ModelNum += ( ModelHeaders[Models[i].ModelNum].LOD + 1 );
						}

						if( !LightMxaModel( ModelNum, &Models[i].Pos, (float) Models[i].Red, (float) Models[i].Green, (float) Models[i].Blue, 255.0F ) ) DoDisplay = false;
					}

					if( !( Models[i].Flags & MODFLAG_RealLight ) &&
						!( Models[i].Flags & MODFLAG_Stealth ) &&
						!( Models[i].Flags & MODFLAG_AmbientLight ) &&
						!( Models[i].Flags & MODFLAG_Original ) )
					{
						UpdateMxaModel( &MxaModelHeaders[ ModelNum ] );
					}


					if( DoDisplay )
					{
#ifdef __3DS__
						/* Same eligibility check as the
						 * static-mxload path: BGOBJECT models
						 * (animated doors, vents) get no shine. */
						{
							c3d_set_object_shine_eligible(
							    Models[i].OwnerType != OWNER_BGOBJECT);
						}
#endif
					if( ExecuteMxaloadHeader( &MxaModelHeaders[ModelNum], ClipGroup ) != true)
					{
#ifdef __3DS__
						{ c3d_set_object_shine_eligible(true); }
#endif
						Msg( "ModelDisp() ExecuteMxaloadHeader for %s Failed\n", &ModelNames[ Models[ i ].ModelNum ].Name[ 0 ] );
						return false;
					}
#ifdef __3DS__
						{ c3d_set_object_shine_eligible(true); }
#endif
					}

				}
				else
				{
					x1.x = 0.0F;
					x1.y = 0.0F;
					x1.z = 0.0F;
					x2 = x1;

					CreateMXBoundingBox( &ModelHeaders[ ModelNum ],	&Models[i].Mat, &Models[i].Pos,
										 &Models[i].TempLines[ 0 ], Models[i].Group );

					if( Models[i].Flags & MODFLAG_AmbientLight )
					{
						GetRealLightAmbient( &Models[i].Pos , &r , &g , &b );
						AmbientLightMxModel( &ModelHeaders[ ModelNum ],
										Models[i].Red, Models[i].Green, Models[i].Blue, 0 , r , g , b);
					}else if( Models[i].Flags & MODFLAG_RealLight )
					{
						TempMatrix = Models[i].Mat;
						TempMatrix._41 = Models[i].Pos.x;
						TempMatrix._42 = Models[i].Pos.y;
						TempMatrix._43 = Models[i].Pos.z;
						XLightMxloadHeader( &ModelHeaders[ ModelNum ], &Models[i].Pos, 
							Models[i].Radius, &TempMatrix );
					}

					if( Models[i].Flags & MODFLAG_Stealth )
					{
						if( Models[i].Flags & MODFLAG_StealthOffset )
						{
							ModelNum += Models[i].StealthOffset;
						}
						else
						{
							ModelNum += ( ModelHeaders[Models[i].ModelNum].LOD + 1 );
						}

						if( !LightMxModel( ModelNum, &Models[i].Pos, (float) Models[i].Red, (float) Models[i].Green, (float) Models[i].Blue, 255.0F ) ) DoDisplay = false;

					}


					if( DoDisplay )
					{
#ifdef __3DS__
						/* Phong specular reads as tacky on
						 * BGOBJECT models (doors, vents, large
						 * flat decorations). Disable shine for
						 * those, keep it on enemies / pickups /
						 * mines / nobody-owned. */
						{
							c3d_set_object_shine_eligible(
							    Models[i].OwnerType != OWNER_BGOBJECT);
						}
#endif
						if( ExecuteMxloadHeader( &ModelHeaders[ ModelNum ], i ) != true)
						{
#ifdef __3DS__
							{ c3d_set_object_shine_eligible(true); }
#endif
							Msg( "ModelDisp() ExecuteMxloadHeader for %s Failed\n",
								&ModelNames[ Models[i].ModelNum ].Name[ 0 ] );
							return false;
						}
#ifdef __3DS__
						{ c3d_set_object_shine_eligible(true); }
#endif
					}

				}
			}
		}

		/*	go on to the next	*/
		i = nextmodel;
	}

	if (!FSSetWorld(&identity))
	{
		Msg( "ModelDisp() SetMatrix2 Failed\n" );
		return false;
	}
	return true;
}

/*===================================================================
*		Set up 2d exec buff etc...
===================================================================*/
bool ReleaseTitleModels( )
{
	int i;

	/* ModelHeaders / MxaModelHeaders are allocated by PreInitModel.
	 * If neither has been allocated yet (e.g. ReleaseView fires before
	 * the first PreInitModel on the boot path), there's nothing to
	 * release — bail out cleanly. */
	if (!ModelHeaders || !MxaModelHeaders) return true;

	for( i = 0 ; i < MAXMODELHEADERS; i++ )
	{
		if( TitleModelSet[i].Name[0] != 0 )
		{
			if( TitleModelSet[i].LoadEnable )
			{
				if( TitleModelSet[i].DoIMorph )
				{
					ReleaseMxaloadheader( &MxaModelHeaders[i] );
				}
				else
				{
					ReleaseMxloadheader( &ModelHeaders[i] );
				}
			}
		}
		else break;
	}
	return	true;
}

/*===================================================================
*		Set up 2d exec buff etc...
===================================================================*/
bool ReleaseModels( )
{
	int i;

	if (!ModelHeaders || !MxaModelHeaders) return true;

	for( i = 0 ; i < MAXMODELHEADERS ; i++ )
	{
		if( ModelNames[i].Name[0] != 0 )
		{
			if( ModelNames[i].LoadEnable )
			{
				if( ModelNames[i].DoIMorph )
				{
					ReleaseMxaloadheader( &MxaModelHeaders[i] );
				}
				else
				{
					ReleaseMxloadheader( &ModelHeaders[i] );
				}
			}
		}
		else break;
	}
	return	true;
}

/*===================================================================
	Procedure	:	Process Models
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
VECTOR BloodOffsets[] = {

	{ 0.0F, -1.0F, 0.0F }, { 0.0F, -25.0F, 0.0F },					// Head Dir, Offsets
	{ -0.276F, 0.92F, -0.276F },	{ -30.0F, 100.0F, -30.0F },		// Leg Dir, Offsets
	{ 0.0F, 0.0F, 1.0F }, { 0.0F, 35.0F, 40.0F },					// Arm Dir, Offsets

};

float TomFrame = 0.0F;

void ProcessModels( void )
{
	u_int16_t		i;
	u_int16_t		nextmodel;
	VECTOR		Int_Point;
	NORMAL		Int_Normal;
	VECTOR		TempVector;
	float		Speed;
	float		Cos;
	QUAT		TempQuat;
	VECTOR		FrameLag_Rot;
	VECTOR		Pos;
	VECTOR		TempDir;
	VECTOR		OffVector;
	VECTOR	*	BloodPtr;
	VECTOR		Rotation;
	VECTOR		UpVector;
	VECTOR		DirVector;
	MATRIX		UpMatrix;
	MATRIX		InvUpMatrix;
	u_int16_t		NumSpotFX;
	PVSPOTFX *	SpotFXPtr;
	int16_t		Count;
	float		WaterDamage;
	u_int16_t		SpotFXGroup;
	int16_t		VisNum;
	u_int16_t		VisGroups[ MAXGROUPS ];
	int16_t		Count2;
	float		ShipSpeed;

	i =  FirstModelUsed;

	while( i != INDEX_NONE )
	{
		nextmodel = Models[i].Prev;

		if( Models[ i ].Visible )
		{
			switch( Models[i].Func )
			{
				case MODFUNC_Nothing:
					break;

				case MODFUNC_Explode:
					Models[i].Scale += ( ( Models[i].MaxScale / 40.0F ) * framelag );
					if( Models[i].Scale <= Models[i].MaxScale )			// 10.0F
					{
						ShockWave( &Models[i].Pos, ( Models[i].Scale * BALL_RADIUS ), Models[i].OwnerType, Models[i].Owner, 16.0F, Models[i].Group, Models[i].SecWeapon, i );
					}
					else
					{
						KillUsedModel( i );
					}
					break;

				case MODFUNC_Regen:
					Models[i].Pos = Ships[Models[i].Owner].Object.Pos;
					Models[i].Scale += ( framelag * 0.01F );
					if( Models[i].Scale > Models[i].MaxScale )
					{
						KillUsedModel( i );
						break;
					}
					break;

				case MODFUNC_SphereZone:
					break;

				// gravgon field 1
				// each gravgon is really 2 fields
				case MODFUNC_Scale:
					Models[i].LifeCount -= framelag;
					if( Models[i].LifeCount <= 0.0F )
					{
						Models[i].LifeCount = 0.0F;
					}

					Models[i].Scale -= ( framelag * GRAVGON_SCALE_DEC );
					if( Models[i].Scale < 0.0F )
					{
						Models[i].Scale = Models[i].MaxScale;
						if( Models[i].LifeCount == 0.0F )
						{
							KillUsedModel( i );
							break;
						}
					}
					GravityWave( &Models[i].Pos, ( BALL_RADIUS * Models[i].MaxScale ), Models[i].Owner, 10.0F, Models[i].Group );
					break;

				// gravgon field 2
				// each gravgon is really 2 fields
				case MODFUNC_Scale2:
					Models[i].LifeCount -= framelag;
					if( Models[i].LifeCount <= 0.0F )
					{
						Models[i].LifeCount = 0.0F;
					}

					Models[i].Scale -= ( framelag * GRAVGON_SCALE_DEC );
					if( Models[i].Scale < 0.0F )
					{
						Models[i].Scale = Models[i].MaxScale;
						if( Models[i].LifeCount == 0.0F )
						{
							KillUsedModel( i );
							break;
						}
					}
					GravityWave( &Models[i].Pos, ( BALL_RADIUS * Models[i].MaxScale ), Models[i].Owner, 10.0F, Models[i].Group );
					break;

				case MODFUNC_BodyPart:
					if( Models[i].Group != INDEX_NONE )
					{
						TempQuat.From2Vectors(&Mloadheader.Group[ Models[i].Group ].up, &SlideUp);
						TempQuat.ToMatrix(&UpMatrix);
						MatrixTranspose( &UpMatrix, &InvUpMatrix );
					}
					else
					{
						TempQuat.From2Vectors(&SlideUp, &SlideUp);
						TempQuat.ToMatrix(&UpMatrix);
						MatrixTranspose( &UpMatrix, &InvUpMatrix );
					}

					Models[i].LifeCount -= framelag;
					if( Models[i].LifeCount < 0.0F )
					{
						Models[i].LifeCount = 0.0F;
						KillUsedModel( i );
						break;
					}
					else
					{
						if( AmIOutsideGroup( &Mloadheader, &Models[i].Pos, Models[i].Group ) )
						{
							Models[i].LifeCount = 0.0F;
							KillUsedModel( i );
							break;
						}

	     				TempDir.x = ( Models[i].Dir.x * framelag );
	     				TempDir.y = ( Models[i].Dir.y * framelag );
	     				TempDir.z = ( Models[i].Dir.z * framelag );
		  
	     				if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, &Models[i].Pos,
	     									  Models[i].Group, &TempDir, (VECTOR *) &Int_Point,
	     									  &Models[i].Group, &Int_Normal, &TempVector, true, NULL ) == true )
	     				{
							Models[i].Pos = Int_Point;
	     					CreateBloodSplat( &Int_Point, (VECTOR *) &Int_Normal, Models[i].Group );
		
	     					TempVector = Models[i].Dir;
	     					NormaliseVector( &TempVector );
	     					Speed = VectorLength( &Models[i].Dir );
	     					ReflectVector( &TempVector, &Int_Normal, &Models[i].Dir );
	     					Speed *= 0.5F;
		  
							PlayPannedSfx( SFX_Squish, Models[i].Group , &Models[i].Pos, 0.0F );
		
	     					if( Speed < ( 5.0F * GLOBAL_SCALE ) )
	     					{
		  						KillUsedModel( i );
	     						break;
	     					}
		  
	     					Models[i].Dir.x *= Speed;
	     					Models[i].Dir.y *= Speed;
	     					Models[i].Dir.z *= Speed;
	     					Models[i].Rot.x = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     					Models[i].Rot.y = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     					Models[i].Rot.z = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     					Models[i].Rot.x *= Speed;
	     					Models[i].Rot.y *= Speed;
	     					Models[i].Rot.z *= Speed;
	     				}
						else
						{
		     				Models[i].Pos.x += ( Models[i].Dir.x * framelag );
		     				Models[i].Pos.y += ( Models[i].Dir.y * framelag );
		     				Models[i].Pos.z += ( Models[i].Dir.z * framelag );
						}

						ApplyMatrix( &UpMatrix, &Models[i].Dir, &TempDir );

	     				if( TempDir.x )
	     				{
	     					if( TempDir.x > 0.0F )
	     					{
	     						TempDir.x -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.x < 0.0F ) TempDir.x = 0.0F;
	     					}
	     					else
	     					{
	     						TempDir.x += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.x > 0.0F ) TempDir.x = 0.0F;
	     					}
	     				}
	     				if( TempDir.z )
	     				{
	     					if( TempDir.z > 0.0F )
	     					{
	     						TempDir.z -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.z < 0.0F ) TempDir.z = 0.0F;
	     					}
	     					else
	     					{
	     						TempDir.z += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.z > 0.0F ) TempDir.z = 0.0F;
	     					}
	     				}
	     				TempDir.y -= ( ( 0.5F * GLOBAL_SCALE ) * framelag );

						ApplyMatrix( &InvUpMatrix, &TempDir, &Models[i].Dir );
		  
	     	   			FrameLag_Rot.x = ( Models[i].Rot.x * framelag );
	     	   			FrameLag_Rot.y = ( Models[i].Rot.y * framelag );
	     	   			FrameLag_Rot.z = ( Models[i].Rot.z * framelag );
	     	   			TempQuat.MakeEuler(FrameLag_Rot.x, FrameLag_Rot.y, FrameLag_Rot.z);
	     	   			TempQuat.Multiply(&Models[i].Quat, &Models[i].Quat);
	     	   			Models[i].Quat.ToMatrix(&Models[i].Mat);							// Create Dir Matrix
	     	   			MatrixTranspose( &Models[i].Mat, &Models[i].InvMat );						// Create Dir Matrix
		
						BloodPtr = &BloodOffsets[ 0 ];
						
						ApplyMatrix( &Models[i].Mat, BloodPtr, &TempVector );
	     				NormaliseVector( &TempVector );
						BloodPtr++;
						ApplyMatrix( &Models[i].Mat, BloodPtr, &OffVector );
	     				Cos = DotProduct( &TempVector, &SlideUp );
	     				if( Cos < 0.0F )
	     				{
							if( IsGroupVisible[ Models[i].Group ] )
							{
								Models[i].TimeCount += framelag;
		
								if( Models[i].TimeCount >= Models[i].TimeInterval )
								{
									Models[i].TimeCount = 0.0F;
									Pos.x = ( Models[i].Pos.x + ( OffVector.x * GLOBAL_SCALE ) );
									Pos.y = ( Models[i].Pos.y + ( OffVector.y * GLOBAL_SCALE ) );
									Pos.z = ( Models[i].Pos.z + ( OffVector.z * GLOBAL_SCALE ) );
			     					CreateBloodTrail( &Pos, &TempVector, Models[i].Group );
								}
							}
	     				}
						else
						{
							Models[i].TimeCount = 0.0F;
						}
					}
	 				break;
				case MODFUNC_TomCruise:
					if( Models[i].Group != INDEX_NONE )
					{
						TempQuat.From2Vectors(&Mloadheader.Group[ Models[i].Group ].up, &SlideUp);
						TempQuat.ToMatrix(&UpMatrix);
						MatrixTranspose( &UpMatrix, &InvUpMatrix );
					}
					else
					{
						TempQuat.From2Vectors(&SlideUp, &SlideUp);
						TempQuat.ToMatrix(&UpMatrix);
						MatrixTranspose( &UpMatrix, &InvUpMatrix );
					}

					TomFrame += ( framelag * 0.2F );
					while( TomFrame >= 27.0F ) TomFrame -= 27.0F;

					Models[ i ].ModelNum = (u_int16_t) ( MODEL_Tom0 + TomFrame );

					Models[i].LifeCount -= framelag;
					if( Models[i].LifeCount < 0.0F )
					{
						Models[i].LifeCount = 0.0F;
						KillUsedModel( i );
						break;
					}
					else
					{
						if( AmIOutsideGroup( &Mloadheader, &Models[i].Pos, Models[i].Group ) )
						{
							Models[i].LifeCount = 0.0F;
							KillUsedModel( i );
							break;
						}

	     				TempDir.x = ( Models[i].Dir.x * framelag );
	     				TempDir.y = ( Models[i].Dir.y * framelag );
	     				TempDir.z = ( Models[i].Dir.z * framelag );
		  
	     				if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, &Models[i].Pos,
	     									  Models[i].Group, &TempDir, (VECTOR *) &Int_Point,
	     									  &Models[i].Group, &Int_Normal, &TempVector, true, NULL ) == true )
	     				{
							PlayPannedSfx( SFX_Squish, Models[i].Group , &Models[i].Pos, 0.0F );
							NormaliseVector( &Models[i].Dir );
							ExplodeBody( &Models[i].Pos, &Models[i].Dir, Models[i].Group );
	  						KillUsedModel( i );
     						break;
	     				}
						else
						{
		     				Models[i].Pos.x += ( Models[i].Dir.x * framelag );
		     				Models[i].Pos.y += ( Models[i].Dir.y * framelag );
		     				Models[i].Pos.z += ( Models[i].Dir.z * framelag );
						}

						ApplyMatrix( &UpMatrix, &Models[i].Dir, &TempDir );

	     				if( TempDir.x )
	     				{
	     					if( TempDir.x > 0.0F )
	     					{
	     						TempDir.x -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.x < 0.0F ) TempDir.x = 0.0F;
	     					}
	     					else
	     					{
	     						TempDir.x += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.x > 0.0F ) TempDir.x = 0.0F;
	     					}
	     				}
	     				if( TempDir.z )
	     				{
	     					if( TempDir.z > 0.0F )
	     					{
	     						TempDir.z -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.z < 0.0F ) TempDir.z = 0.0F;
	     					}
	     					else
	     					{
	     						TempDir.z += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.z > 0.0F ) TempDir.z = 0.0F;
	     					}
	     				}
	     				TempDir.y -= ( ( 0.5F * GLOBAL_SCALE ) * framelag );

						ApplyMatrix( &InvUpMatrix, &TempDir, &Models[i].Dir );
					}
	 				break;
				case MODFUNC_OrbitPulsar:
					if( ( ( GameStatus[ Models[ i ].Ship ] == STATUS_SinglePlayer ) || ( GameStatus[ Models[ i ].Ship ] == STATUS_Normal ) ) &&
						( Ships[ Models[ i ].Ship ].NumMultiples ) )
					{
						if( ( Ships[ Models[ i ].Ship ].Object.Flags & SHIP_Stealth ) ) Models[ i ].ModelNum = MODEL_OrbitPulsar_Light;
						else Models[ i ].ModelNum = MODEL_OrbitPulsar;

						Rotation.x = (float) fast_sinf( D2R( Models[ i ].AxisRot ) );
						Rotation.y = (float) fast_cosf( D2R( Models[ i ].AxisRot ) );
						Rotation.z = 0.0F;
						ApplyMatrix( &Ships[ Models[ i ].Ship ].Object.FinalMat, &Rotation, &UpVector );	// Calc Up Vector
						ApplyMatrix( &Ships[ Models[ i ].Ship ].Object.FinalMat, &Forward, &DirVector );	// Calc Dir Vector
						Models[i].Quat.FromDirAndUp(&DirVector, &UpVector);					// Create Quat
	     	   			Models[i].Quat.ToMatrix(&Models[i].Mat);							// Create Matrix
	     	   			MatrixTranspose( &Models[i].Mat, &Models[i].InvMat );						// Create Inv Matrix
						Models[ i ].Pos.x = ( Ships[ Models[ i ].Ship ].Object.Pos.x + ( UpVector.x * ORBIT_RADIUS ) ); //+ ( DirVector.x * ( ORBIT_RADIUS * 0.85F ) ) );
						Models[ i ].Pos.y = ( Ships[ Models[ i ].Ship ].Object.Pos.y + ( UpVector.y * ORBIT_RADIUS ) ); //+ ( DirVector.y * ( ORBIT_RADIUS * 0.85F ) ) );
						Models[ i ].Pos.z = ( Ships[ Models[ i ].Ship ].Object.Pos.z + ( UpVector.z * ORBIT_RADIUS ) ); //+ ( DirVector.z * ( ORBIT_RADIUS * 0.85F ) ) );
						Models[ i ].AxisRot += ( 6.0F * framelag );
						Models[ i ].Group = Ships[ Models[ i ].Ship ].Object.Group;
					}
					else
					{
						KillUsedModel( i );
					}
					break;

				case MODFUNC_ReScale:
					break;

				case MODFUNC_RealScale:
					break;

				case MODFUNC_ScaleDonut:
					Models[i].Scale += ( ( Models[i].MaxScale / 60.0F ) * framelag );
					if( Models[i].Scale > Models[i].MaxScale ) KillUsedModel( i );
					break;

				case MODFUNC_LargeShrapnel:
					if( Models[i].Group != INDEX_NONE )
					{
						TempQuat.From2Vectors(&Mloadheader.Group[ Models[i].Group ].up, &SlideUp);
						TempQuat.ToMatrix(&UpMatrix);
						MatrixTranspose( &UpMatrix, &InvUpMatrix );
					}
					else
					{
						TempQuat.From2Vectors(&SlideUp, &SlideUp);
						TempQuat.ToMatrix(&UpMatrix);
						MatrixTranspose( &UpMatrix, &InvUpMatrix );
					}

					Models[i].LifeCount -= framelag;
					if( Models[i].LifeCount < 0.0F )
					{
						Models[i].LifeCount = 0.0F;
						KillUsedModel( i );
						break;
					}
					else
					{
	     				TempDir.x = ( Models[i].Dir.x * framelag );
	     				TempDir.y = ( Models[i].Dir.y * framelag );
	     				TempDir.z = ( Models[i].Dir.z * framelag );
		  
						WaterObjectCollide( Models[i].Group, &Models[i].Pos, &TempDir, &Int_Point, 255 );

	     				if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, &Models[i].Pos,
	     									  Models[i].Group, &TempDir, (VECTOR *) &Int_Point,
	     									  &Models[i].Group, &Int_Normal, &TempVector, true, NULL ) == true )
	     				{
							if( Random_Range( 3 ) )
							{
	     						TempVector = Models[i].Dir;
	     						NormaliseVector( &TempVector );
	     						Speed = VectorLength( &Models[i].Dir );
	     						ReflectVector( &TempVector, &Int_Normal, &Models[i].Dir );
	     						Speed *= 0.8F;

								CreateSpotFXSmallShrapnel( &Int_Point, &Models[i].Dir, Speed, Models[i].Group );
	  							KillUsedModel( i );
								break;
							}
							else
							{
								Models[i].Pos = Int_Point;

	     						TempVector = Models[i].Dir;
	     						NormaliseVector( &TempVector );
	     						Speed = VectorLength( &Models[i].Dir );
	     						ReflectVector( &TempVector, &Int_Normal, &Models[i].Dir );
	     						Speed *= 0.5F;
			  
	     						if( Speed < ( 5.0F * GLOBAL_SCALE ) )
	     						{
		  							KillUsedModel( i );
	     							break;
	     						}
			  
	     						Models[i].Dir.x *= Speed;
	     						Models[i].Dir.y *= Speed;
	     						Models[i].Dir.z *= Speed;
	     						Models[i].Rot.x = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     						Models[i].Rot.y = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     						Models[i].Rot.z = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     						Models[i].Rot.x *= Speed;
	     						Models[i].Rot.y *= Speed;
	     						Models[i].Rot.z *= Speed;
							}
	     				}
						else
						{
		     				Models[i].Pos.x += ( Models[i].Dir.x * framelag );
		     				Models[i].Pos.y += ( Models[i].Dir.y * framelag );
		     				Models[i].Pos.z += ( Models[i].Dir.z * framelag );
						}

						ApplyMatrix( &UpMatrix, &Models[i].Dir, &TempDir );

	     				if( TempDir.x )
	     				{
	     					if( TempDir.x > 0.0F )
	     					{
	     						TempDir.x -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.x < 0.0F ) TempDir.x = 0.0F;
	     					}
	     					else
	     					{
	     						TempDir.x += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.x > 0.0F ) TempDir.x = 0.0F;
	     					}
	     				}
	     				if( TempDir.z )
	     				{
	     					if( TempDir.z > 0.0F )
	     					{
	     						TempDir.z -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.z < 0.0F ) TempDir.z = 0.0F;
	     					}
	     					else
	     					{
	     						TempDir.z += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.z > 0.0F ) TempDir.z = 0.0F;
	     					}
	     				}
	     				TempDir.y -= ( ( 0.5F * GLOBAL_SCALE ) * framelag );

						ApplyMatrix( &InvUpMatrix, &TempDir, &Models[i].Dir );
		  
	     	   			FrameLag_Rot.x = ( Models[i].Rot.x * framelag );
	     	   			FrameLag_Rot.y = ( Models[i].Rot.y * framelag );
	     	   			FrameLag_Rot.z = ( Models[i].Rot.z * framelag );
	     	   			TempQuat.MakeEuler(FrameLag_Rot.x, FrameLag_Rot.y, FrameLag_Rot.z);
	     	   			TempQuat.Multiply(&Models[i].Quat, &Models[i].Quat);
	     	   			Models[i].Quat.ToMatrix(&Models[i].Mat);							// Create Dir Matrix
	     	   			MatrixTranspose( &Models[i].Mat, &Models[i].InvMat );						// Create Dir Matrix

						Models[i].TimeInterval = (float) 1;

						if( IsGroupVisible[ Models[i].Group ] )
						{
							Models[i].TimeCount += framelag;

							if( Models[i].TimeCount >= Models[i].TimeInterval )
							{
								Models[i].TimeCount = FMOD( Models[i].TimeCount, Models[i].TimeInterval );

								TempDir = Models[i].Dir;
								NormaliseVector( &TempDir );

								if( !InWater( Models[i].Group, &Models[i].Pos, &WaterDamage ) )
								{
									CreateShrapnelTrail( &Models[i].Pos, &TempDir, ( 8.0F * GLOBAL_SCALE ), Models[i].Group, 200, 100, 80 );
								}
								else
								{
									CreateBubbleTrail( &Models[i].Pos, &TempDir, Models[i].Group, 64, 64, 64 );
								}
							}
						}

					}
	 				break;

				case MODFUNC_SmallShrapnel:
					if( Models[i].Group != INDEX_NONE )
					{
						TempQuat.From2Vectors(&Mloadheader.Group[ Models[i].Group ].up, &SlideUp);
						TempQuat.ToMatrix(&UpMatrix);
						MatrixTranspose( &UpMatrix, &InvUpMatrix );
					}
					else
					{
						TempQuat.From2Vectors(&SlideUp, &SlideUp);
						TempQuat.ToMatrix(&UpMatrix);
						MatrixTranspose( &UpMatrix, &InvUpMatrix );
					}

					Models[i].LifeCount -= framelag;
					if( Models[i].LifeCount < 0.0F )
					{
						Models[i].LifeCount = 0.0F;
						KillUsedModel( i );
						break;
					}
					else
					{
	     				TempDir.x = ( Models[i].Dir.x * framelag );
	     				TempDir.y = ( Models[i].Dir.y * framelag );
	     				TempDir.z = ( Models[i].Dir.z * framelag );
		  
						WaterObjectCollide( Models[i].Group, &Models[i].Pos, &TempDir, &Int_Point, 128 );

	     				if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, &Models[i].Pos,
	     									  Models[i].Group, &TempDir, (VECTOR *) &Int_Point,
	     									  &Models[i].Group, &Int_Normal, &TempVector, true, NULL ) == true )
	     				{
							Models[i].Pos = Int_Point;

	     					TempVector = Models[i].Dir;
	     					NormaliseVector( &TempVector );
	     					Speed = VectorLength( &Models[i].Dir );
	     					ReflectVector( &TempVector, &Int_Normal, &Models[i].Dir );
	     					Speed *= 0.5F;
		  
	     					if( Speed < ( 5.0F * GLOBAL_SCALE ) )
	     					{
		  						KillUsedModel( i );
	     						break;
	     					}
		  
	     					Models[i].Dir.x *= Speed;
	     					Models[i].Dir.y *= Speed;
	     					Models[i].Dir.z *= Speed;
	     					Models[i].Rot.x = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     					Models[i].Rot.y = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     					Models[i].Rot.z = ( ( (float) Random_Range( 10000 ) ) / 10000.0F );
	     					Models[i].Rot.x *= Speed;
	     					Models[i].Rot.y *= Speed;
	     					Models[i].Rot.z *= Speed;
	     				}
						else
						{
		     				Models[i].Pos.x += ( Models[i].Dir.x * framelag );
		     				Models[i].Pos.y += ( Models[i].Dir.y * framelag );
		     				Models[i].Pos.z += ( Models[i].Dir.z * framelag );
						}

						ApplyMatrix( &UpMatrix, &Models[i].Dir, &TempDir );

	     				if( TempDir.x )
	     				{
	     					if( TempDir.x > 0.0F )
	     					{
	     						TempDir.x -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.x < 0.0F ) TempDir.x = 0.0F;
	     					}
	     					else
	     					{
	     						TempDir.x += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.x > 0.0F ) TempDir.x = 0.0F;
	     					}
	     				}
	     				if( TempDir.z )
	     				{
	     					if( TempDir.z > 0.0F )
	     					{
	     						TempDir.z -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.z < 0.0F ) TempDir.z = 0.0F;
	     					}
	     					else
	     					{
	     						TempDir.z += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
	     						if( TempDir.z > 0.0F ) TempDir.z = 0.0F;
	     					}
	     				}
	     				TempDir.y -= ( ( 0.5F * GLOBAL_SCALE ) * framelag );

						ApplyMatrix( &InvUpMatrix, &TempDir, &Models[i].Dir );
		  
	     	   			FrameLag_Rot.x = ( Models[i].Rot.x * framelag );
	     	   			FrameLag_Rot.y = ( Models[i].Rot.y * framelag );
	     	   			FrameLag_Rot.z = ( Models[i].Rot.z * framelag );
	     	   			TempQuat.MakeEuler(FrameLag_Rot.x, FrameLag_Rot.y, FrameLag_Rot.z);
	     	   			TempQuat.Multiply(&Models[i].Quat, &Models[i].Quat);
	     	   			Models[i].Quat.ToMatrix(&Models[i].Mat);							// Create Dir Matrix
	     	   			MatrixTranspose( &Models[i].Mat, &Models[i].InvMat );						// Create Dir Matrix

						if( IsGroupVisible[ Models[i].Group ] )
						{
							Models[i].TimeCount += framelag;

							if( Models[i].TimeCount >= Models[i].TimeInterval )
							{
								Models[i].TimeCount = FMOD( Models[i].TimeCount, Models[i].TimeInterval );

								TempDir = Models[i].Dir;
								NormaliseVector( &TempDir );

								if( !InWater( Models[i].Group, &Models[i].Pos, &WaterDamage ) )
								{
									CreateShrapnelTrail( &Models[i].Pos, &TempDir, ( 4.0F * GLOBAL_SCALE ), Models[i].Group, 200, 100, 80 );
								}
								else
								{
									CreateBubbleTrail( &Models[i].Pos, &TempDir, Models[i].Group, 64, 64, 64 );
								}
							}
						}
					}
	 				break;

				case MODFUNC_ProcessSpotFX:
					if( Models[i].Flags & MODFLAG_SpotFXDisabled ) break;
					if( framelag == 0.0F ) break;

					SpotFXPtr = NULL;
					NumSpotFX = 0;

					if( ModelNames[ Models[i].ModelNum ].DoIMorph )
					{
						if( MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
							MxaModelHeaders[ Models[i].ModelNum ].SpotFX )
						{
							SpotFXPtr = MxaModelHeaders[ Models[i].ModelNum ].SpotFX;
							NumSpotFX = MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX;
						}
					}
					else
					{
						if( ModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
							ModelHeaders[ Models[i].ModelNum ].SpotFX )
						{
							SpotFXPtr = ModelHeaders[ Models[i].ModelNum ].SpotFX;
							NumSpotFX = ModelHeaders[ Models[i].ModelNum ].NumSpotFX;
						}
					}

					if( !SpotFXPtr || !NumSpotFX ) break;

					TempDir.x = ( Models[i].Pos.x - Models[i].OldPos.x );
					TempDir.y = ( Models[i].Pos.y - Models[i].OldPos.y );
					TempDir.z = ( Models[i].Pos.z - Models[i].OldPos.z );
					ApplyMatrix( &Models[i].InvMat, &TempDir, &TempDir );
					Speed = TempDir.z;
					ShipSpeed = VectorLength( &Models[i].Dir );

					for( Count = 0; Count < NumSpotFX; Count++ )
					{
						if( !( Models[i].Flags & MODFLAG_Stealth ) || ( SpotFXPtr->Type == SPOTFX_SoundFX ) )
						{
							Models[i].SpotFXTimeInterval[ Count ] = (float) 1;

							if ( ! CAMERA_VIEW_IS_VALID ) return;
							VisNum = VisibleOverlap( Ships[ Current_Camera_View ].Object.Group, Models[i].Group, &VisGroups[ 0 ] );

							switch( Models[i].SpotFXState[ Count ] )
							{
								case SPOTFX_STATE_WaitingForDelay:
									Models[i].SpotFXDelay[ Count ] -= framelag;

									if( Models[i].SpotFXDelay[ Count ] < 0.0F )
									{
										Models[i].SpotFXDelay[ Count ] += SpotFXPtr->ActiveDelay;
										Models[i].SpotFXState[ Count ] = SPOTFX_STATE_Active;
					 					Models[i].SpotFXTimeCount[ Count ] = 0.0F;
									}
									break;

								case SPOTFX_STATE_Active:
									if( Models[i].Visible )
									{
										switch( SpotFXPtr->Type )
										{
											case SPOTFX_Flame:
											case SPOTFX_Smoke:
											case SPOTFX_Steam:
											case SPOTFX_Sparks:
											case SPOTFX_ElectricBeams:
											case SPOTFX_GravgonTrail:
											case SPOTFX_NmeTrail:
											case SPOTFX_NmeVapourTrail:
											case SPOTFX_FireWall:
											case SPOTFX_GravitySparks:
											case SPOTFX_Bubbles:
											case SPOTFX_BeardAfterburner:
											case SPOTFX_Hk5Afterburner:
											case SPOTFX_SoundFX:
											default:
												Models[i].SpotFXTimeCount[ Count ] += framelag;

												if( Models[i].SpotFXTimeCount[ Count ] >= Models[i].SpotFXTimeInterval[ Count ] )
												{
					 								Models[i].SpotFXTimeCount[ Count ] = (float) FMOD( Models[i].SpotFXTimeCount[ Count ], Models[i].SpotFXTimeInterval[ Count ] );

													ApplyMatrix( &Models[i].Mat, &SpotFXPtr->Pos, &Pos );
													ApplyMatrix( &Models[i].Mat, &SpotFXPtr->DirVector, &DirVector );
													ApplyMatrix( &Models[i].Mat, &SpotFXPtr->UpVector, &UpVector );

													Pos.x += Models[i].Pos.x;
													Pos.y += Models[i].Pos.y;
													Pos.z += Models[i].Pos.z;

													SpotFXGroup = Models[i].Group;

													for( Count2 = 0; Count2 < VisNum; Count2++ )
													{
														if( Bsp_Header[ 0 ].State )
														{
															if( PointInsideSkin( &Pos, VisGroups[ Count2 ] ) )
															{
																SpotFXGroup = VisGroups[ Count2 ];
																break;
															}
														}
														else
														{
															if( PointInGroupBoundingBox( &Mloadheader, &Pos, VisGroups[ Count2 ] ) )
															{
																SpotFXGroup = VisGroups[ Count2 ];
																break;
															}
														}
													}

													if( CAMERA_VIEW_IS_VALID && GroupsAreVisible( SpotFXGroup, Ships[ Current_Camera_View ].Object.Group ) )
													{
														switch( SpotFXPtr->Type )
														{
															case SPOTFX_Flame:
																CreateSpotFXFlame( &Pos, &DirVector, SpotFXGroup );
																break;

															case SPOTFX_Smoke:
																CreateSpotFXSmoke( &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue );
																break;

															case SPOTFX_Steam:
																CreateSpotFXSteam( &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue );
																break;

															case SPOTFX_Sparks:
																CreateSpotFXSparks( &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue );
																break;

															case SPOTFX_ElectricBeams:
																CreateSpotFXElectricBeams( &Pos, &DirVector, &UpVector, SpotFXGroup );
																break;

															case SPOTFX_GravgonTrail:
																CreateSpotFXGravgonTrail( &Pos, &DirVector, SpotFXGroup );
																break;

															case SPOTFX_NmeTrail:
																CreateSpotFXNmeTrail( &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue );
																break;

															case SPOTFX_NmeVapourTrail:
																CreateSpotFXNmeVapourTrail( &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue );
																break;

															case SPOTFX_FireWall:
																CreateSpotFXFireWall( &Pos, &DirVector, &UpVector, SpotFXGroup );
																break;

															case SPOTFX_GravitySparks:
																CreateSpotFXRealSparks(  &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue );
																break;

															case SPOTFX_Bubbles:
																CreateSpotFXBubbles( &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue, ( 2.0F * BLOCKSIZE ) );
																break;

															case SPOTFX_BeardAfterburner:
																if( BikeExhausts ) CreateSpotFXBeardAfterburner( &Pos, &DirVector, SpotFXGroup, Speed );
																break;

															case SPOTFX_Hk5Afterburner:
																if( BikeExhausts ) CreateSpotFXHk5Afterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed, 32, 32, 128 );
																break;

															case SPOTFX_SoundFX:
																if( !( Models[ i ].SpotFXFlags[ Count ] & SPOTFX_FLAGS_DoneOnce ) )
																{
																	if( SpotFXPtr->SoundFX != -1 )
																	{
																		if( EngineEnabled( Models[i].OwnerType, Models[i].Owner ) )
																		{
																			if( !Models[ i ].SpotFXSFX_ID[ Count ] )
																			{
																				Models[ i ].SpotFXSFX_ID[ Count ] = PlaySpotSfx( SpotFXPtr->SoundFX, &Models[i].Group, &Models[i].Pos,
																												  SpotFXPtr->SoundFXSpeed, SpotFXPtr->SoundFXVolume, SPOT_SFX_TYPE_Normal );
																				Models[ i ].SpotFXFlags[ Count ] |= SPOTFX_FLAGS_DoneOnce;
																			}
																			else
																			{
																				StopSfx( Models[i].SpotFXSFX_ID[ Count ] );
																				Models[i].SpotFXSFX_ID[ Count ] = 0;

																				Models[ i ].SpotFXSFX_ID[ Count ] = PlaySpotSfx( SpotFXPtr->SoundFX, &Models[i].Group, &Models[i].Pos,
																												  SpotFXPtr->SoundFXSpeed, SpotFXPtr->SoundFXVolume, SPOT_SFX_TYPE_Normal );
																				Models[ i ].SpotFXFlags[ Count ] |= SPOTFX_FLAGS_DoneOnce;
#ifdef DEBUG_SPOTFX_SOUND
																				Msg( "Model:SpotFX_SoundFX Overwriting SFX ID %d\n", Models[i].ModelNum );
#endif
																			}
																		}
																		else
																		{
																			if( Models[ i ].SpotFXSFX_ID[ Count ] )
																			{
																				StopSfx( Models[i].SpotFXSFX_ID[ Count ] );
																				Models[i].SpotFXSFX_ID[ Count ] = 0;
																			}
																		}
																	}

																}
																break;

															default:
																break;
														}
													}
												}
												break;

											case SPOTFX_Explosion:
											case SPOTFX_FireSecondary:
											case SPOTFX_FirePrimary:
											case SPOTFX_NmeGlow:
											case SPOTFX_Shrapnel:
											case SPOTFX_Drip:
											case SPOTFX_BorgAfterburner:
											case SPOTFX_ExcopAfterburner:
											case SPOTFX_TruckerAfterburner:
											case SPOTFX_NubiaAfterburner:
											case SPOTFX_CerberoAfterburner:
											case SPOTFX_FoetoidAfterburner:
											case SPOTFX_FoetoidSmallAfterburner:
											case SPOTFX_JapBirdAfterburner:
											case SPOTFX_JoAfterburner:
											case SPOTFX_JoSmallAfterburner:
											case SPOTFX_LaJayAfterburner:
											case SPOTFX_MofistoAfterburner:
											case SPOTFX_NutterAfterburner:
											case SPOTFX_RhesusAfterburner:
											case SPOTFX_SharkAfterburner:
											case SPOTFX_SlickAfterburner:
												ApplyMatrix( &Models[i].Mat, &SpotFXPtr->Pos, &Pos );
												ApplyMatrix( &Models[i].Mat, &SpotFXPtr->DirVector, &DirVector );
												ApplyMatrix( &Models[i].Mat, &SpotFXPtr->UpVector, &UpVector );

												Pos.x += Models[i].Pos.x;
												Pos.y += Models[i].Pos.y;
												Pos.z += Models[i].Pos.z;

												SpotFXGroup = Models[i].Group;

												for( Count2 = 0; Count2 < VisNum; Count2++ )
												{
													if( Bsp_Header[ 0 ].State )
													{
														if( PointInsideSkin( &Pos, VisGroups[ Count2 ] ) )
														{
															SpotFXGroup = VisGroups[ Count2 ];
															break;
														}
													}
													else
													{
														if( PointInGroupBoundingBox( &Mloadheader, &Pos, VisGroups[ Count2 ] ) )
														{
															SpotFXGroup = VisGroups[ Count2 ];
															break;
														}
													}
												}

												if( CAMERA_VIEW_IS_VALID && GroupsAreVisible( SpotFXGroup, Ships[ Current_Camera_View ].Object.Group ) )
												{
													switch( SpotFXPtr->Type )
													{
														case SPOTFX_Explosion:
															if( !( Models[ i ].SpotFXFlags[ Count ] & SPOTFX_FLAGS_DoneOnce ) )
															{
																CreateSpotFXExplosion( &Pos, &DirVector, SpotFXGroup );
																Models[ i ].SpotFXFlags[ Count ] |= SPOTFX_FLAGS_DoneOnce;
															}
															break;

														case SPOTFX_FireSecondary:
															if( !( Models[ i ].SpotFXFlags[ Count ] & SPOTFX_FLAGS_DoneOnce ) )
															{
																CreateSpotFXFireSecondary( &Pos, &DirVector, &UpVector, SpotFXGroup, SpotFXPtr->Secondary, Models[ i ].OwnerType, Models[ i ].Owner );
																Models[ i ].SpotFXFlags[ Count ] |= SPOTFX_FLAGS_DoneOnce;
															}
															break;

														case SPOTFX_FirePrimary:
															if( !( Models[ i ].SpotFXFlags[ Count ] & SPOTFX_FLAGS_DoneOnce ) )
															{
																CreateModelSpotFXFirePrimary( &Pos, &DirVector, &UpVector, SpotFXGroup, SpotFXPtr->Primary, i, Count );
																Models[ i ].SpotFXFlags[ Count ] |= SPOTFX_FLAGS_DoneOnce;
															}
															break;

														case SPOTFX_NmeGlow:
															CreateSpotFXNmeGlow( &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue );
															break;

														case SPOTFX_Shrapnel:
															if( !( Models[ i ].SpotFXFlags[ Count ] & SPOTFX_FLAGS_DoneOnce ) )
															{
																CreateSpotFXShrapnel( &Pos, &DirVector, SpotFXGroup );
																Models[ i ].SpotFXFlags[ Count ] |= SPOTFX_FLAGS_DoneOnce;
															}
															break;

														case SPOTFX_Drip:
															if( !( Models[ i ].SpotFXFlags[ Count ] & SPOTFX_FLAGS_DoneOnce ) )
															{
																CreateSpotFXDrip( &Pos, &DirVector, SpotFXGroup, SpotFXPtr->Red, SpotFXPtr->Green, SpotFXPtr->Blue );
																Models[ i ].SpotFXFlags[ Count ] |= SPOTFX_FLAGS_DoneOnce;
															}
															break;

														case SPOTFX_BorgAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_ExcopAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_TruckerAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_NubiaAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_CerberoAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_FoetoidAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_FoetoidSmallAfterburner:
															if( BikeExhausts ) CreateSpotFXSmallAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_JapBirdAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_JoAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_JoSmallAfterburner:
															if( BikeExhausts ) CreateSpotFXSmallAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_LaJayAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_MofistoAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_NutterAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_RhesusAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_SharkAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														case SPOTFX_SlickAfterburner:
															if( BikeExhausts ) CreateSpotFXBorgAfterburner( &Pos, &DirVector, SpotFXGroup, ShipSpeed );
															break;

														default:
															break;
													}
												}
												break;
										}
									}

									Models[i].SpotFXDelay[ Count ] -= framelag;

									if( Models[i].SpotFXDelay[ Count ] < 0.0F )
									{
										if( SpotFXPtr->InactiveDelay < 0.0F )
										{
											Models[i].SpotFXDelay[ Count ] = -1.0F;

											switch( SpotFXPtr->Type )
											{
												case SPOTFX_SoundFX:
													if( ( SpotFXPtr->SoundFX != -1 ) &&
														( Models[ i ].SpotFXSFX_ID[ Count ] ) )
													{
														if( EngineEnabled( Models[i].OwnerType, Models[i].Owner ) )
														{
															Models[i].SpotFXSFX_ID[ Count ] = EngineCode( Models[i].OwnerType, Models[i].Owner, Models[i].SpotFXSFX_ID[ Count ], Speed );
														}
														else
														{
															StopSfx( Models[i].SpotFXSFX_ID[ Count ] );
															Models[i].SpotFXSFX_ID[ Count ] = 0;
															Models[i].SpotFXFlags[ Count ] &= ~SPOTFX_FLAGS_DoneOnce;
														}
													}
													break;

												default:
													break;
											}
										}
										else
										{
											if( SpotFXPtr->InactiveDelay == 0.0F )
											{
												Models[i].SpotFXDelay[ Count ] += SpotFXPtr->ActiveDelay;
											}
											else
											{
												if( SpotFXPtr->SoundFX != -1 )
												{
													if( Models[i].SpotFXSFX_ID[ Count ] )
													{
														StopSfx( Models[i].SpotFXSFX_ID[ Count ] );
														Models[i].SpotFXSFX_ID[ Count ] = 0;
													}
												}

												Models[i].SpotFXDelay[ Count ] += SpotFXPtr->InactiveDelay;
												Models[i].SpotFXState[ Count ] = SPOTFX_STATE_Inactive;
											}
										}
									}
									break;

								case SPOTFX_STATE_Inactive:
									Models[i].SpotFXDelay[ Count ] -= framelag;

									if( Models[i].SpotFXDelay[ Count ] < 0.0F )
									{
										Models[i].SpotFXFlags[ Count ] &= ~SPOTFX_FLAGS_DoneOnce;
										Models[i].SpotFXDelay[ Count ] += SpotFXPtr->ActiveDelay;
										Models[i].SpotFXState[ Count ] = SPOTFX_STATE_Active;
										Models[i].SpotFXTimeCount[ Count ] = 0.0F;
									}
									break;
							}
						}

						SpotFXPtr++;
					}
					break;
			}

			Models[i].OldPos = Models[i].Pos;
		}
		else
		{
			switch( Models[i].Func )
			{
				case MODFUNC_ProcessSpotFX:
					SpotFXPtr = NULL;
					NumSpotFX = 0;

					if( ModelNames[ Models[i].ModelNum ].DoIMorph )
					{
						if( MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
							MxaModelHeaders[ Models[i].ModelNum ].SpotFX )
						{
							SpotFXPtr = MxaModelHeaders[ Models[i].ModelNum ].SpotFX;
							NumSpotFX = MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX;
						}
					}
					else
					{
						if( ModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
							ModelHeaders[ Models[i].ModelNum ].SpotFX )
						{
							SpotFXPtr = ModelHeaders[ Models[i].ModelNum ].SpotFX;
							NumSpotFX = ModelHeaders[ Models[i].ModelNum ].NumSpotFX;
						}
					}

					if( !SpotFXPtr || !NumSpotFX ) break;

					for( Count = 0; Count < NumSpotFX; Count++ )
					{
						switch( SpotFXPtr->Type )
						{
							case SPOTFX_SoundFX:
								if( SpotFXPtr->SoundFX != -1 )
								{
									if( Models[i].SpotFXSFX_ID[ Count ] )
									{
										StopSfx( Models[i].SpotFXSFX_ID[ Count ] );
										Models[i].SpotFXSFX_ID[ Count ] = 0;
										Models[i].SpotFXFlags[ Count ] &= ~SPOTFX_FLAGS_DoneOnce;
									}
								}
								break;

							default:
								break;
						}
						SpotFXPtr++;
					}
					break;
			}
		}

		i = nextmodel;
	}
}

/*===================================================================
	Procedure	:	Create SpotFX Fire Primary
	Input		:	VECTOR	*	Position
				:	VECTOR	*	Direction
				:	VECTOR	*	Up
				:	u_int16_t		Group
				:	u_int16_t		Weapon
				:	u_int16_t		ModelIndex
				:	u_int16_t		SpotFXIndex
	Output		:	Nothing
===================================================================*/
void CreateModelSpotFXFirePrimary( VECTOR * Pos, VECTOR * Dir, VECTOR * Up,
							    u_int16_t Group, u_int8_t Weapon, u_int16_t ModelIndex, u_int16_t SpotFXIndex )
{
	VECTOR	TempVector = { 0.0F, 0.0F, 0.0F };
	u_int16_t	i;

	if( Weapon != (u_int8_t) -1 )
	{
		i = EnemyFirePrimary( OWNER_MODELSPOTFX, ModelIndex, 0, Weapon, Group, Pos, &TempVector, Dir, Up, 2, 0.0F, false, NULL );

		if( i != INDEX_NONE )
		{
			PrimBulls[i].SpotFX = SpotFXIndex;
			GetLaserLocalVector( i, &PrimBulls[i].LocalDir );
		}
	}
}

/*===================================================================
	Procedure	:	Process model
	Input		:	u_int16_t	Model Number
				:	float	Scale
				:	float	MaxScale
				:	int8_t	R
				:	int8_t	G
				:	int8_t	B
	Output		:	bool	True/False
===================================================================*/
bool ProcessModel( MXLOADHEADER * DstMloadheader, float Scale, float MaxScale, int8_t R, int8_t G, int8_t B )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	COLOR				Colour;
	float					Col2;
	u_int8_t					Col;
	u_int8_t					Red;
	u_int8_t					Green;
	u_int8_t					Blue;

	Col2 = ( ( Scale * ( 128.0F / MaxScale ) ) + 128.0F );
	if( Col2 > 255.0F ) Col2 = 255.0F;
	Col = (u_int8_t) ( 255.0F - Col2 );

	if( R ) Red = Col;
	else Red = 0;
	if( G ) Green = Col;
	else Green = 0;
	if( B ) Blue = Col;
	else Blue = 0;

		Colour = RGBA_MAKE( Red, Green, Blue, 255 );

	for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
	{
		for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
		{
			if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
			{
				return false;
			}

			Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];

			while( Vert-- )
			{
				DstlpD3DLVERTEX->color = Colour;
				DstlpD3DLVERTEX++;
			}

			if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
			{
				return false;
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Process model scaling original gouraud
	Input		:	u_int16_t	Model Number
				:	float	Scale
				:	float	MaxScale
				:	int8_t	R
				:	int8_t	G
				:	int8_t	B
	Output		:	bool	True/False
===================================================================*/
bool ProcessModel2( MXLOADHEADER * DstMloadheader, float Scale, float MaxScale, int8_t R, int8_t G, int8_t B )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	float					Col2;
	u_int16_t					Col;
	u_int16_t					Red;
	u_int16_t					Green;
	u_int16_t					Blue;
	
	LVERTEX			*VertPtr;

	COLOR_RGBA *			DestRGBA;

	Col2 = ( ( Scale / MaxScale ) * 255.0F );
	if( Col2 > 255.0F ) Col2 = 255.0F;
	Col = (u_int16_t) ( 255.0F - Col2 );

	if( R ) Red = Col;
	else Red = 0;
	if( G ) Green = Col;
	else Green = 0;
	if( B ) Blue = Col;
	else Blue = 0;

	for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
	{
		for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
		{
			if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
			{
				return false;
			}

			Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];
			VertPtr = DstMloadheader->Group[ Group ].originalVerts[ ExecBuf ];

			while( Vert-- )
			{
				DestRGBA = (COLOR_RGBA *) &DstlpD3DLVERTEX->color;

				DestRGBA->r = (u_int8_t) ( ( (u_int16_t) (RGBA_GETRED(VertPtr->color) * Red) ) >> 8 );
				DestRGBA->g = (u_int8_t) ( ( (u_int16_t) (RGBA_GETGREEN(VertPtr->color) * Green) ) >> 8 );
				DestRGBA->b = (u_int8_t) ( ( (u_int16_t) (RGBA_GETBLUE(VertPtr->color) * Blue) ) >> 8 );

				VertPtr++;
				DstlpD3DLVERTEX++;
				
			}

			if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
			{
				return false;
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Process model
===================================================================*/
bool ProcessModelExec( RENDEROBJECT *renderObject, int16_t NumVerts, float Scale, float MaxScale, int8_t R, int8_t G, int8_t B )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	COLOR				Colour;
	float					Col2;
	u_int8_t					Col;
	u_int8_t					Red;
	u_int8_t					Green;
	u_int8_t					Blue;

	Col2 = ( ( Scale * ( 128.0F / MaxScale ) ) + 128.0F );
	if( Col2 > 255.0F ) Col2 = 255.0F;
	Col = (u_int8_t) ( 255.0F - Col2 );

	if( R ) Red = Col;
	else Red = 0;
	if( G ) Green = Col;
	else Green = 0;
	if( B ) Blue = Col;
	else Blue = 0;

		Colour = RGBA_MAKE( Red, Green, Blue, 255 );

	if (!(FSLockVertexBuffer(renderObject, &DstlpD3DLVERTEX)))
	{
		return false;
	}
	
	while( NumVerts-- )
	{
		DstlpD3DLVERTEX->color = Colour;
		DstlpD3DLVERTEX++;
	}

	if (!(FSUnlockVertexBuffer(renderObject)))
	{
		return false;
	}

	return true;
}

/*===================================================================
	Procedure	:	Process Sphere Zone model
	Input		:	RENDEROBJECT *	renderObject
				:	int16_t	NumVerts
				:	u_int8_t	R
				:	u_int8_t	G
				:	u_int8_t	B
	Output		:	bool	True/False
===================================================================*/
bool ProcessSphereZoneModelExec( RENDEROBJECT *renderObject, int16_t NumVerts, u_int8_t R, u_int8_t G, u_int8_t B )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	COLOR				Colour;

		Colour = RGBA_MAKE( R, G, B, 255 );

	if (!(FSLockVertexBuffer(renderObject, &DstlpD3DLVERTEX)))
	{
		return false;
	}

	while( NumVerts-- )
	{
		DstlpD3DLVERTEX->color = Colour;
		DstlpD3DLVERTEX++;
	}

	if (!(FSUnlockVertexBuffer(renderObject)))
	{
		return false;
	}

	return true;
}


/*===================================================================
	Procedure	:	Light model
	Input		:	u_int16_t		Model Number
				:	float		R,G,B,Trans
	Output		:	bool		True/False
===================================================================*/
bool TintModel( u_int16_t Model, float RF, float GF, float BF, float TF )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	MXLOADHEADER	*		DstMloadheader;
	COLOR				Colour;

	DstMloadheader = &ModelHeaders[ Model ];

	Colour = RGBA_MAKE( (u_int8_t) RF, (u_int8_t) GF, (u_int8_t) BF, (u_int8_t) TF );
	if( !Colour ) return false;

	for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
	{
		for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
		{
			if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
			{
				return false;
			}

			Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];

			while( Vert-- )
			{
				DstlpD3DLVERTEX->color = Colour;
				DstlpD3DLVERTEX++;
			}

			if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
			{
				return false;
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Update Colours
	Input		:	MXALOADHEADER	ModelHeader
	Output		:	bool	True/False
===================================================================*/
bool UpdateMxaModel( MXALOADHEADER * MXAloadheader )
{
    LVERTEX	*lpPointer = NULL;
	LVERTEX	*lpLVERTEX;
    LVERTEX	*lpLVERTEX2;
	int		group;
	int		execbuf;
	int		vert;


	group = MXAloadheader->num_groups;

	while( group--)
	{
		execbuf = MXAloadheader->Group[group].num_execbufs;
		while( execbuf--)
		{
			/*	lock the vertex buffer	*/

			if (!(FSLockVertexBuffer(&MXAloadheader->Group[group].renderObject[execbuf], &lpPointer)))
			{
				return false;
			}

			lpLVERTEX2 = MXAloadheader->Group[group].originalVerts[execbuf];
			
			lpLVERTEX = lpPointer;
			vert = MXAloadheader->Group[group].num_verts_per_execbuf[execbuf];
			while( vert --)
			{
				lpLVERTEX->color = lpLVERTEX2->color;
				lpLVERTEX->color |= 0xFF000000;
				lpLVERTEX++;		
				lpLVERTEX2++;		
			}
			/*	unlock the vertex buffer	*/
			if (!(FSUnlockVertexBuffer(&MXAloadheader->Group[group].renderObject[execbuf])))
			{
				return false;
			}
		}
	}
	return true;
}


/*===================================================================
	Procedure	:	Ambient Light model
	Input		:	u_int16_t		Model Number
				:	float		R,G,B,Trans
	Output		:	bool		True/False
===================================================================*/
bool AmbientLightMxaModel( MXALOADHEADER * DstMloadheader, int R, int G, int B, int /*A*/ , float rp , float gp , float bp)
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	LVERTEX			*SrclpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	COLOR				Colour;
	u_int32_t					tempiA;
	u_int32_t					carry, clamp;
	u_int32_t					tempiR, tempiG, tempiB;
	u_int32_t					col_inc, inc_inv, inc_carry;


	R -= (int) rp;
	G -= (int) gp;
	B -= (int) bp;

	if( R < 0 )
		R = 0;
	if( G < 0 )
		G = 0;
	if( B < 0 )
		B = 0;

	tempiR = ( (int) -( R ) ) & 0xFF;
	tempiG = ( (int) -( G ) ) & 0xFF;
	tempiB = ( (int) -( B ) ) & 0xFF;
	// so far so much nearly the same...
	col_inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
	inc_inv = col_inc ^ 0x00FFFFFF;
	inc_carry = ( ( inc_inv + 0x00010101) ^ ( inc_inv ^ 0x00010101 ) ) & 0x01010100;
	for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
	{
		for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
		{
			if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
			{
				return false;
			}

			SrclpD3DLVERTEX = DstMloadheader->Group[ Group ].originalVerts[ExecBuf];
			Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];

			while( Vert-- )
			{
				Colour = SrclpD3DLVERTEX->color;
				tempiA = Colour & 0xff000000;
				// for subtraction, simply add the negative
				carry = ( ( Colour + col_inc ) ^ ( Colour ^ col_inc ) ) & 0x01010100;
				Colour = Colour + col_inc - carry;
				// set the carry also for those channels where we are subtracting 0 (this seems wrong!)
				carry |= inc_carry;
				// ...and now it's nearly the same as for additive saturation
				clamp = carry - ( carry >> 8 );
				Colour = ( Colour & clamp ) & 0x00ffffff;
				Colour |= tempiA;
				DstlpD3DLVERTEX->color = Colour;
				DstlpD3DLVERTEX++;
				SrclpD3DLVERTEX++;
			}

			if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
			{
				return false;
			}
		}
	}
	return true;
}
/*===================================================================
	Procedure	:	Ambient Light MX model
	Input		:	u_int16_t		Model Number
				:	float		R,G,B,Trans
	Output		:	bool		True/False
===================================================================*/
bool AmbientLightMxModel( MXLOADHEADER * DstMloadheader, int R, int G, int B, int /*A*/ , float rp , float gp , float bp)
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	LVERTEX			*SrclpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	COLOR				Colour;
	u_int32_t					tempiA;
	u_int32_t					carry, clamp;
	u_int32_t					tempiR, tempiG, tempiB;
	u_int32_t					col_inc, inc_inv, inc_carry;


	R -= (int) rp;
	G -= (int) gp;
	B -= (int) bp;

	if( R < 0 )
		R = 0;
	if( G < 0 )
		G = 0;
	if( B < 0 )
		B = 0;


	tempiR = ( (int) -( R ) ) & 0xFF;
	tempiG = ( (int) -( G ) ) & 0xFF;
	tempiB = ( (int) -( B ) ) & 0xFF;
	// so far so much nearly the same...
	col_inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
	inc_inv = col_inc ^ 0x00FFFFFF;
	inc_carry = ( ( inc_inv + 0x00010101) ^ ( inc_inv ^ 0x00010101 ) ) & 0x01010100;
	for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
	{
		for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
		{
			if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
			{
				return false;
			}

			SrclpD3DLVERTEX = DstMloadheader->Group[ Group ].originalVerts[ExecBuf];

			Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];

			while( Vert-- )
			{
				Colour = SrclpD3DLVERTEX->color;
				tempiA = Colour & 0xff000000;
				// for subtraction, simply add the negative
				carry = ( ( Colour + col_inc ) ^ ( Colour ^ col_inc ) ) & 0x01010100;
				Colour = Colour + col_inc - carry;
				// set the carry also for those channels where we are subtracting 0 (this seems wrong!)
				carry |= inc_carry;
				// ...and now it's nearly the same as for additive saturation
				clamp = carry - ( carry >> 8 );
				Colour = ( Colour & clamp ) & 0x00ffffff;
				Colour |= tempiA;
				DstlpD3DLVERTEX->color = Colour;
				DstlpD3DLVERTEX++;
				SrclpD3DLVERTEX++;
			}

			if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
			{
				return false;
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Light model
	Input		:	u_int16_t		Model Number
				:	MATRIX	*	Matrix
				:	float		Z Translation
				:	float		Range
	Output		:	bool		True/False
===================================================================*/
bool ShadeModel( u_int16_t Model, MATRIX * Matrix, float ZTrans, float Range )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	MXLOADHEADER	*		DstMloadheader;
	COLOR				Colour;
	float					RF, GF, BF, TF;
	VECTOR					TempPoint;

	DstMloadheader = &ModelHeaders[ Model ];

	for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
	{
		for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
		{
			if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
			{
				return false;
			}

			Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];

			while( Vert-- )
			{
				ApplyMatrix( Matrix, (VECTOR*) DstlpD3DLVERTEX, &TempPoint );
				TempPoint.z += ZTrans;
				if( TempPoint.z > Range )
				{
					RF = 0.0F; GF = 0.0F; BF = 0.0F; TF = 0.0F;
				}
				else
				{
					if( TempPoint.z < 0.0F ) 
					{
						RF = 255.0F; GF = 255.0F; BF = 255.0F; TF = 255.0F;
					}
					else
					{
						RF = ( 255.0F - ( ( TempPoint.z / Range ) * 255.0F ) );
						GF = RF; BF = RF;
						TF = 255.0F;
					}
				}

				Colour = RGBA_MAKE( (u_int8_t) RF, (u_int8_t) GF, (u_int8_t) BF, (u_int8_t) TF );

				DstlpD3DLVERTEX->color = Colour;
				DstlpD3DLVERTEX++;
			}

			if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
			{
				return false;
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Light model
	Input		:	u_int16_t		Model Number
				:	MATRIX	*	Matrix
				:	VECTOR	*	Pos
	Output		:	bool	True/False
===================================================================*/
bool LightModel( u_int16_t Model, VECTOR * Pos )
{
	XLIGHT * LightPnt;
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	VECTOR					DistVector;
	float					Dist;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	MXLOADHEADER	*		DstMloadheader;
	COLOR				Colour;
    float					Val;
	float					TF = 0.0F;
	float					RF = 0.0F;
	float					GF = 0.0F;
	float					BF = 0.0F;


	DstMloadheader = &ModelHeaders[ Model ];

	LightPnt = FirstLightVisible;
	
	while( LightPnt )
	{
		DistVector.x = ( Pos->x - LightPnt->Pos.x );
		DistVector.y = ( Pos->y - LightPnt->Pos.y );
		DistVector.z = ( Pos->z - LightPnt->Pos.z );
		Dist = VectorLength( &DistVector );
		
		if( Dist < LightPnt->Size )
		{
			Val = ( 1.0F - ( Dist / LightPnt->Size ) );
	
			RF += ( LightPnt->r * Val );
			GF += ( LightPnt->g * Val );
			BF += ( LightPnt->b * Val );
			TF += ( 255.0F * Val );
		}
		LightPnt = LightPnt->NextVisible;
	}
		
	if( RF > 255.0F ) RF = 255.0F;
	if( GF > 255.0F ) GF = 255.0F;
	if( BF > 255.0F ) BF = 255.0F;
	if( TF > 255.0F ) TF = 255.0F;

	Colour = RGBA_MAKE( (u_int8_t) RF, (u_int8_t) GF, (u_int8_t) BF, (u_int8_t) TF );
	if( !Colour ) return false;

	for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
	{
		for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
		{

			if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
			{
				return false;
			}

			Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];

			while( Vert-- )
			{
				DstlpD3DLVERTEX->color = Colour;
				DstlpD3DLVERTEX++;
			}

			if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
			{
				return false;
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Light model
	Input		:	u_int16_t		Model Number
				:	MATRIX	*	Matrix
				:	VECTOR	*	Pos
	Output		:	bool	True/False
===================================================================*/
bool LightModel2( u_int16_t Model, VECTOR * Pos )
{
	XLIGHT * LightPnt;
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	VECTOR					DistVector;
	float					Dist;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	MXLOADHEADER	*		DstMloadheader;
	COLOR				Colour;
    float					Val;
	float					TF = 255.0F;
	float					RF = 128.0F;
	float					GF = 128.0F;
	float					BF = 128.0F;


	DstMloadheader = &ModelHeaders[ Model ];

	LightPnt = FirstLightVisible;
	
	while( LightPnt )
	{
		DistVector.x = ( Pos->x - LightPnt->Pos.x );
		DistVector.y = ( Pos->y - LightPnt->Pos.y );
		DistVector.z = ( Pos->z - LightPnt->Pos.z );
		Dist = VectorLength( &DistVector );
		
		if( Dist < LightPnt->Size )
		{
			Val = ( 1.0F - ( Dist / LightPnt->Size ) );
	
			RF += ( LightPnt->r * Val );
			GF += ( LightPnt->g * Val );
			BF += ( LightPnt->b * Val );
			TF += ( 255.0F * Val );
		}
		LightPnt = LightPnt->NextVisible;
	}
		
	if( RF > 255.0F ) RF = 255.0F;
	if( GF > 255.0F ) GF = 255.0F;
	if( BF > 255.0F ) BF = 255.0F;
	if( TF > 255.0F ) TF = 255.0F;

	Colour = RGBA_MAKE( (u_int8_t) RF, (u_int8_t) GF, (u_int8_t) BF, (u_int8_t) TF );
	if( !Colour ) return false;

	for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
	{
		for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
		{

			if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
			{
				return false;
			}

			Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];

			while( Vert-- )
			{
				DstlpD3DLVERTEX->color = Colour;
				DstlpD3DLVERTEX++;
			}

			if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
			{
				return false;
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Light MX model
	Input		:	u_int16_t		Model Number
				:	MATRIX	*	Matrix
				:	VECTOR	*	Pos
				:	float		Starting RED ( 0-255 )
				:	float		Starting GREEN ( 0-255 )
				:	float		Starting BLUE ( 0-255 )
				:	float		Starting TRANSPARANCY ( 0-255 )
	Output		:	bool	True/False ( Visible/Not )
===================================================================*/
bool LightMxModel( u_int16_t Model, VECTOR * Pos, float RF, float GF, float BF, float TF )
{
	XLIGHT * LightPnt;
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	VECTOR					DistVector;
	float					Dist;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	MXLOADHEADER	*		DstMloadheader;
	COLOR				Colour;
    float					Val;


	DstMloadheader = &ModelHeaders[ Model ];

	if( DstMloadheader->state == true )
	{
		LightPnt = FirstLightVisible;
		while( LightPnt )
		{
			DistVector.x = ( Pos->x - LightPnt->Pos.x );
			DistVector.y = ( Pos->y - LightPnt->Pos.y );
			DistVector.z = ( Pos->z - LightPnt->Pos.z );
			Dist = VectorLength( &DistVector );
			
			if( Dist < LightPnt->Size )
			{
				Val = ( 1.0F - ( Dist / LightPnt->Size ) );
		
				RF += ( LightPnt->r * Val );
				GF += ( LightPnt->g * Val );
				BF += ( LightPnt->b * Val );
				TF += ( 255.0F * Val );
			}
			LightPnt = LightPnt->NextVisible;
		}
		
		if( RF > 255.0F ) RF = 255.0F;
		if( GF > 255.0F ) GF = 255.0F;
		if( BF > 255.0F ) BF = 255.0F;
		if( TF > 255.0F ) TF = 255.0F;

		Colour = RGBA_MAKE( (u_int8_t) RF, (u_int8_t) GF, (u_int8_t) BF, (u_int8_t) TF );
		if( !Colour ) return false;

		for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
		{
			for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
			{

				if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
				{
					return false;
				}
	
				Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];
	
				while( Vert-- )
				{
					DstlpD3DLVERTEX->color = Colour;
					DstlpD3DLVERTEX++;
				}
	
				if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
				{
					return false;
				}
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Light MXA model
	Input		:	u_int16_t		Model Number
				:	MATRIX	*	Matrix
				:	VECTOR	*	Pos
				:	float		Starting RED ( 0-255 )
				:	float		Starting GREEN ( 0-255 )
				:	float		Starting BLUE ( 0-255 )
				:	float		Starting TRANSPARANCY ( 0-255 )
	Output		:	bool	True/False ( Visible/Not )
===================================================================*/
bool LightMxaModel( u_int16_t Model, VECTOR * Pos, float RF, float GF, float BF, float TF )
{
	XLIGHT * LightPnt;
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	VECTOR					DistVector;
	float					Dist;
	u_int16_t					Group;
	u_int16_t					Vert;
	u_int16_t					ExecBuf;
	MXALOADHEADER	*		DstMloadheader;
	COLOR				Colour;
    float					Val;


	DstMloadheader = &MxaModelHeaders[ Model ];

	if( DstMloadheader->state == true )
	{
		LightPnt = FirstLightVisible;
	
		while( LightPnt )
		{
			DistVector.x = ( Pos->x - LightPnt->Pos.x );
			DistVector.y = ( Pos->y - LightPnt->Pos.y );
			DistVector.z = ( Pos->z - LightPnt->Pos.z );
			Dist = VectorLength( &DistVector );
			
			if( Dist < LightPnt->Size )
			{
				Val = ( 1.0F - ( Dist / LightPnt->Size ) );
		
				RF += ( LightPnt->r * Val );
				GF += ( LightPnt->g * Val );
				BF += ( LightPnt->b * Val );
				TF += ( 255.0F * Val );
			}
			LightPnt = LightPnt->NextVisible;
		}
		
		if( RF > 255.0F ) RF = 255.0F;
		if( GF > 255.0F ) GF = 255.0F;
		if( BF > 255.0F ) BF = 255.0F;
		if( TF > 255.0F ) TF = 255.0F;

		Colour = RGBA_MAKE( (u_int8_t) RF, (u_int8_t) GF, (u_int8_t) BF, (u_int8_t) TF );
		if( !Colour ) return false;

		for( Group = 0; Group < DstMloadheader->num_groups; Group++ )
		{
			for( ExecBuf = 0; ExecBuf < DstMloadheader->Group[ Group ].num_execbufs; ExecBuf++ )
			{
				if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
				{
					return false;
				}
	
				Vert = DstMloadheader->Group[ Group ].num_verts_per_execbuf[ ExecBuf ];
	
				while( Vert-- )
				{
					DstlpD3DLVERTEX->color = Colour;
					DstlpD3DLVERTEX++;
				}
	
				if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
				{
					return false;
				}
			}
		}
	}
	return true;
}

/*===================================================================
	Procedure	:	Shock Wave Damage to Ships and Mines.
	Input		:	VECTOR	*	Position
				:	float		Radius
				:	u_int16_t		OwnerType
				:	u_int16_t		Owner
				:	float		Damage at center
				:	u_int16_t		Group
				:	BYTE		Weapon
	Output		:	Nothing
===================================================================*/
void ShockWave( VECTOR * Pos, float Radius, u_int16_t OwnerType, u_int16_t Owner, float Center_Damage, u_int16_t Group, BYTE Weapon, u_int16_t model )
{
	float		Damage;
	u_int16_t		EndGroup;
	VECTOR		DistVector;
	float		DistFromCenter;
	VERT		Int_Point;
	NORMAL		Int_Normal;
	VECTOR		TempVector;
	VECTOR		Recoil;
	u_int16_t		i, Next;
	u_int16_t		nSecWalk;
	ENEMY	*	Enemy;
	ENEMY	*	NextEnemy;

	switch( ColPerspective )
	{
		case COLPERS_Forsaken:
		case COLPERS_Descent:
			if( ( Ships[ WhoIAm ].enable ) && ( Ships[ WhoIAm ].Object.Mode != LIMBO_MODE ) )
			{
				if( !SoundInfo[ Ships[ WhoIAm ].Object.Group ][ Group ] )
				{
					DistVector.x = ( Ships[ WhoIAm ].Object.Pos.x - Pos->x );
					DistVector.y = ( Ships[ WhoIAm ].Object.Pos.y - Pos->y );
					DistVector.z = ( Ships[ WhoIAm ].Object.Pos.z - Pos->z );
					DistFromCenter = VectorLength( &DistVector );
			
					if( DistFromCenter < ( Radius + SHIP_RADIUS ) )
					{
						if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
									  Group, &DistVector, (VECTOR *) &Int_Point,
									  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
						{
							Damage = ( Center_Damage - ( DistFromCenter / ( ( Radius + SHIP_RADIUS ) / Center_Damage ) ) );
			
							Recoil.x = ( ( DistVector.x / DistFromCenter ) * ( Damage / 4.0F ) );
							Recoil.y = ( ( DistVector.y / DistFromCenter ) * ( Damage / 4.0F ) );
							Recoil.z = ( ( DistVector.z / DistFromCenter ) * ( Damage / 4.0F ) );
			
							Damage = ( Damage * framelag );
							if( OwnerType == OWNER_ENEMY ) Damage *= NmeDamageModifier;
			

							HitMe( OwnerType, Owner, Damage, WEPTYPE_Secondary, Weapon );
							ForceExternal( WhoIAm, &Recoil );
							if ( !( Models[ model ].Flags & MODFLAG_ShockwaveHitMe ) )
							{
								Models[ model ].Flags |= MODFLAG_ShockwaveHitMe;
								Damage = ( Center_Damage - ( DistFromCenter / ( ( ( Models[ model ].MaxScale * BALL_RADIUS ) + SHIP_RADIUS ) / Center_Damage ) ) );
								Damage = ( Damage * framelag );
								Recoil.x = ( ( DistVector.x / DistFromCenter ) * ( Damage / 4.0F ) );
								Recoil.y = ( ( DistVector.y / DistFromCenter ) * ( Damage / 4.0F ) );
								Recoil.z = ( ( DistVector.z / DistFromCenter ) * ( Damage / 4.0F ) );
							}
						}
					}
				}
			}
			break;
	}

/*===================================================================
	Shockwave hitting any mines?
===================================================================*/
	i = FirstSecBullUsed;
	nSecWalk = 0;

	/* Bound the used-chain walk: a corrupt/empty Prev chain must terminate. */
	while( i != INDEX_NONE && nSecWalk < MAXSECONDARYWEAPONBULLETS )
	{
		nSecWalk++;
		Next = SecBulls[ i ].Prev;							/* Next Secondary Bullet */

		if( SecBulls[ i ].SecType == SEC_MINE )
		{

			// calculate distance between objects
   			DistVector.x = ( SecBulls[ i ].Pos.x - Pos->x );
   			DistVector.y = ( SecBulls[ i ].Pos.y - Pos->y );
   			DistVector.z = ( SecBulls[ i ].Pos.z - Pos->z );
   			DistFromCenter = VectorLength( &DistVector );
   
   			if( DistFromCenter < ( Radius + MINE_RADIUS ) )
   			{
   				if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
   							  Group, &DistVector, (VECTOR *) &Int_Point,
   							  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
   				{
					Damage = ( ( Center_Damage - ( DistFromCenter / ( ( Radius + MINE_RADIUS ) / Center_Damage ) ) ) * framelag );

					if( OwnerType == OWNER_ENEMY ) Damage *= NmeDamageModifier;

					SecBulls[ i ].Shield -= Damage;
					if( SecBulls[ i ].Shield < 0.0F ) SecBulls[ i ].Shield = 0.0F;

					if( SecBulls[ i ].Shield == 0.0F )
					{
						DestroySecondary( i, &SecBulls[i].Pos );
					}
				}
			}
		}

		i = Next;
	}

/*===================================================================
	Shockwave hitting any Enemies?
===================================================================*/
	Enemy = FirstEnemyUsed;

	while( Enemy != NULL )
	{
		NextEnemy = Enemy->NextUsed;

		if( ( Enemy->Status & ENEMY_STATUS_Enable ) && !( Enemy->Object.Flags & SHIP_Invul ) )
		{
			if( OwnerType != OWNER_ENEMY )
			{
				if( !SoundInfo[ Enemy->Object.Group ][ Group ] )
				{
					DistVector.x = ( Enemy->Object.Pos.x - Pos->x );
					DistVector.y = ( Enemy->Object.Pos.y - Pos->y );
					DistVector.z = ( Enemy->Object.Pos.z - Pos->z );
					DistFromCenter = VectorLength( &DistVector );
				   
					if( DistFromCenter < ( Radius + SHIP_RADIUS ) )
					{
						if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
									  Group, &DistVector, (VECTOR *) &Int_Point,
									  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
						{
							Damage = ( ( Center_Damage - ( DistFromCenter / ( ( Radius + SHIP_RADIUS ) / Center_Damage ) ) ) * framelag );
				
							Recoil.x = ( ( DistVector.x / DistFromCenter ) * ( Damage / 4.0F ) );
							Recoil.y = ( ( DistVector.y / DistFromCenter ) * ( Damage / 4.0F ) );
							Recoil.z = ( ( DistVector.z / DistFromCenter ) * ( Damage / 4.0F ) );
				
							Enemy->Object.ExternalForce.x = ( Recoil.x * framelag );
							Enemy->Object.ExternalForce.y = ( Recoil.y * framelag );
							Enemy->Object.ExternalForce.z = ( Recoil.z * framelag );
				
							if( OwnerType == OWNER_ENEMY ) Damage *= NmeDamageModifier;

							Enemy->Object.Shield -= Damage;
							if( Enemy->Object.Shield < 0 ) Enemy->Object.Shield = 0;
							if( Enemy->Object.Shield == 0 )
							{
								CreateNewExplosion( &Enemy->Object.Pos, &Enemy->Object.DirVector, Enemy->Object.Group );
								KillUsedEnemy( Enemy );
							}
						}
					}
				}
			}
		}

		Enemy = NextEnemy;
	}

}

/*===================================================================
	Procedure	:	Gravity Wave Damage to me
	Input		:	VECTOR	*	Position
				:	float		Radius
				:	u_int16_t		Owner
				:	float		Damage at center
				:	u_int16_t		Group
	Output		:	Nothing
===================================================================*/
void GravityWave( VECTOR * Pos, float Radius, u_int16_t /*Owner*/, float Center_Grav, u_int16_t Group )
{
	int16_t	Gravity;
	u_int16_t	EndGroup;
	VECTOR	DistVector;
	float	DistFromCenter;
	VERT	Int_Point;
	NORMAL	Int_Normal;
	VECTOR	TempVector;
	VECTOR	force;
	ENEMY	*	Enemy;
	ENEMY	*	NextEnemy;

  	if( ( Ships[ WhoIAm ].enable ) && ( Ships[ WhoIAm ].Object.Mode != LIMBO_MODE ) && ( WhoIAm < MAX_PLAYERS ) )
   	{
		if( !SoundInfo[ Ships[ WhoIAm ].Object.Group ][ Group ] )
		{
	   		DistVector.x = ( Ships[ WhoIAm ].Object.Pos.x - Pos->x );
	   		DistVector.y = ( Ships[ WhoIAm ].Object.Pos.y - Pos->y );
	   		DistVector.z = ( Ships[ WhoIAm ].Object.Pos.z - Pos->z );
	   		DistFromCenter = VectorLength( &DistVector );
	   
	   		if( ( DistFromCenter < ( Radius + SHIP_RADIUS ) ) && ( DistFromCenter > ( ( Radius / 8.0F ) + SHIP_RADIUS ) ) )
	   		{
	   			if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
	   						  Group, &DistVector, (VECTOR *) &Int_Point,
	   						  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
	   			{
	   				Gravity = (int16_t) ( Center_Grav - ( DistFromCenter / ( ( Radius + SHIP_RADIUS ) / Center_Grav ) ) );
	   
	   				force.x = -( ( ( DistVector.x / DistFromCenter ) * Gravity ) * GLOBAL_SCALE ) * 0.5F;
	   				force.y = -( ( ( DistVector.y / DistFromCenter ) * Gravity ) * GLOBAL_SCALE ) * 0.5F;
	   				force.z = -( ( ( DistVector.z / DistFromCenter ) * Gravity ) * GLOBAL_SCALE ) * 0.5F;
	   				ForceExternal( WhoIAm, &force );
	   			}
	   		}
		}
   	}

/*===================================================================
	GravityWave effecting any Enemies?
===================================================================*/
	Enemy = FirstEnemyUsed;

	while( Enemy != NULL )
	{
		NextEnemy = Enemy->NextUsed;

		if( ( Enemy->Status & ENEMY_STATUS_Enable ) )
		{
			if( !SoundInfo[ Enemy->Object.Group ][ Group ] )
			{
				DistVector.x = ( Enemy->Object.Pos.x - Pos->x );
				DistVector.y = ( Enemy->Object.Pos.y - Pos->y );
				DistVector.z = ( Enemy->Object.Pos.z - Pos->z );
				DistFromCenter = VectorLength( &DistVector );
			   
		   		if( ( DistFromCenter < ( Radius + SHIP_RADIUS ) ) && ( DistFromCenter > ( ( Radius / 8.0F ) + SHIP_RADIUS ) ) )
				{
					if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
								  Group, &DistVector, (VECTOR *) &Int_Point,
								  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
					{
	   					Gravity = (int16_t) ( Center_Grav - ( DistFromCenter / ( ( Radius + SHIP_RADIUS ) / Center_Grav ) ) );
		   
	   					force.x = -( ( ( DistVector.x / DistFromCenter ) * Gravity ) * GLOBAL_SCALE ) * 0.5F;
	   					force.y = -( ( ( DistVector.y / DistFromCenter ) * Gravity ) * GLOBAL_SCALE ) * 0.5F;
	   					force.z = -( ( ( DistVector.z / DistFromCenter ) * Gravity ) * GLOBAL_SCALE ) * 0.5F;

						Enemy->Object.ExternalForce.x += ( force.x * framelag );
						Enemy->Object.ExternalForce.y += ( force.y * framelag );
						Enemy->Object.ExternalForce.z += ( force.z * framelag );
					}
				}
			}
		}

		Enemy = NextEnemy;
	}
}

/*===================================================================
	Procedure	:	Check if within range of gravgon
	Input		:	VECTOR	*	Pos
	Output		:	bool		True/False ( True if collision )
===================================================================*/
bool CheckForGravgons( VECTOR * Pos )
{
	u_int16_t		i;
	u_int16_t		nextmodel;
	VECTOR		DistVector;
	float		DistToGravgon;

	i =  FirstModelUsed;

	while( i != INDEX_NONE )
	{
		nextmodel = Models[i].Prev;

		switch( Models[i].Func )
		{
			case MODFUNC_Scale:
				DistVector.x = ( Pos->x - Models[ i ].Pos.x );
				DistVector.y = ( Pos->y - Models[ i ].Pos.y );
				DistVector.z = ( Pos->z - Models[ i ].Pos.z );
				DistToGravgon = VectorLength( &DistVector );
	
				if( DistToGravgon <= ( ( BALL_RADIUS * Models[i].MaxScale ) + ( SHIP_RADIUS * 8.0F ) ) )
				{
					return true;
				}
				break;

			default:
				break;
		}

		i = nextmodel;
	}

	return false;
}

/*===================================================================
	Procedure	:	Throw Rider toward all players
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
void ThrowOutRider( u_int16_t Ship )
{
	VECTOR	Dir;

	if ( ! CAMERA_VIEW_IS_VALID ) return;
	if(!SoundInfo[ Ships[ Ship ].Object.Group ][ Ships[ Current_Camera_View ].Object.Group ] )
	{
		Dir = Ships[ Ship ].LastMove;
		NormaliseVector( &Dir );

		if(MatureContent == 1)
			CreateTomCruise( &Ships[ Ship ].Object.Pos, Ships[ Ship ].Object.Group );
		else if(MatureContent == 2)
			ExplodeBody( &Ships[ Ship ].Object.Pos, &Dir, Ships[ Ship ].Object.Group );
	}
}

/*===================================================================
	Procedure	:	Explode Body into Bits
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
void ExplodeBody( VECTOR * Pos, VECTOR * Dir, u_int16_t Group )
{
	CreateBodyPart( Pos, Dir, Group, MODEL_Brain );
	CreateBodyPart( Pos, Dir, Group, MODEL_Ear );
	CreateBodyPart( Pos, Dir, Group, MODEL_Eyeball );
	CreateBodyPart( Pos, Dir, Group, MODEL_Heart );
	CreateBodyPart( Pos, Dir, Group, MODEL_Kidney );
	CreateBodyPart( Pos, Dir, Group, MODEL_Leg );
}

/*===================================================================
	Procedure	:	Create Body Part
	Input		:	VECTOR	*	Pos
				:	u_int16_t		Group
	Output		:	Nothing
===================================================================*/
void CreateBodyPart( VECTOR * Pos, VECTOR * Dir, u_int16_t Group, u_int16_t ModelNum )
{
	u_int16_t	Model;

	Model =	FindFreeModel();
	if( Model != INDEX_NONE )
	{
		Models[ Model ].OwnerType = OWNER_NOBODY;
		Models[ Model ].Owner = 0;
		Models[ Model ].ModelNum = ModelNum;
		Models[ Model ].Type = MODTYPE_Missile;
		Models[ Model ].Flags = MODFLAG_Clip;
		Models[ Model ].Visible = true;
		Models[ Model ].Pos = *Pos;

		Models[ Model ].Dir = *Dir;
		Models[ Model ].Dir.x += ( ( ( (float) Random_Range( 10000 ) ) / 10000.0F ) - 0.5F );
		Models[ Model ].Dir.y += ( ( ( (float) Random_Range( 10000 ) ) / 10000.0F ) - 0.5F );
		Models[ Model ].Dir.z += ( ( ( (float) Random_Range( 10000 ) ) / 10000.0F ) - 0.5F );
		NormaliseVector( &Models[ Model ].Dir );
		Models[ Model ].Dir.x *= 5.0F;
		Models[ Model ].Dir.y *= 5.0F;
		Models[ Model ].Dir.y += 5.0F;
		Models[ Model ].Dir.z *= 5.0F;

		Models[ Model ].Rot.x = ( ( (float) Random_Range( 10000 ) ) - 5000.0F ) / 1000.0F;
		Models[ Model ].Rot.y = ( ( (float) Random_Range( 10000 ) ) - 5000.0F ) / 1000.0F;
		Models[ Model ].Rot.z = ( ( (float) Random_Range( 10000 ) ) - 5000.0F ) / 1000.0F;
		Models[ Model ].Quat.w = 1.0F;
		Models[ Model ].Quat.x = 0.0F;
		Models[ Model ].Quat.y = 0.0F;
		Models[ Model ].Quat.z = 0.0F;
		Models[ Model ].Mat = MATRIX_Identity;
		MatrixTranspose( &Models[ Model ].Mat, &Models[ Model ].InvMat );
		Models[ Model ].Func = MODFUNC_BodyPart;
		Models[ Model ].Scale = 1.0F;
		Models[ Model ].MaxScale = 1.0F;
		Models[ Model ].Group = Group;
		Models[ Model ].LifeCount = (float) ( 10.0F * ANIM_SECOND );
	}
}

/*===================================================================
	Procedure	:	Create Tom Cruise
	Input		:	VECTOR	*	Pos
				:	u_int16_t		Group
	Output		:	Nothing
===================================================================*/
void CreateTomCruise( VECTOR * Pos, u_int16_t Group )
{
	u_int16_t	Model;
	VECTOR	TempDir;
	VECTOR	TempPos;
	float	Time;
	float	Distance;
	float	Height;

	Model =	FindFreeModel();
	if( Model != INDEX_NONE )
	{
		Models[ Model ].OwnerType = OWNER_NOBODY;
		Models[ Model ].Owner = 0;
		Models[ Model ].ModelNum = MODEL_Tom0;
		Models[ Model ].Type = MODTYPE_Field;
		Models[ Model ].Flags = MODFLAG_Clip;
		Models[ Model ].Visible = true;
		Models[ Model ].Pos = *Pos;

		TempDir.x = ( CurrentCamera.Pos.x - Pos->x );
		TempDir.y = ( CurrentCamera.Pos.y - Pos->y );
		TempDir.z = ( CurrentCamera.Pos.z - Pos->z );
		Distance = VectorLength( &TempDir );
		Time = ( Distance / ( 20.0F * GLOBAL_SCALE ) );
		Height = ( Time * ( 5.0F * GLOBAL_SCALE ) );

		TempPos.x = CurrentCamera.Pos.x + ( Mloadheader.Group[ Group ].up.x * Height );
		TempPos.y = CurrentCamera.Pos.y + ( Mloadheader.Group[ Group ].up.y * Height );
		TempPos.z = CurrentCamera.Pos.z + ( Mloadheader.Group[ Group ].up.z * Height );

		TempDir.x = ( TempPos.x - Pos->x );
		TempDir.y = ( TempPos.y - Pos->y );
		TempDir.z = ( TempPos.z - Pos->z );
		NormaliseVector( &TempDir );
		TempDir.x = ( TempDir.x * 20.0F );
		TempDir.y = ( TempDir.y * 20.0F );
		TempDir.z = ( TempDir.z * 20.0F );
		Models[ Model ].Dir = TempDir;

		TempDir.x = ( CurrentCamera.Pos.x - Pos->x );
		TempDir.y = 0.0F;
		TempDir.z = ( CurrentCamera.Pos.z - Pos->z );
		NormaliseVector( &TempDir );
		Models[ Model ].Quat.From2Vectors(&Forward, &TempDir);
		Models[ Model ].Quat.ToMatrix(&Models[ Model ].Mat);
		MatrixTranspose( &Models[ Model ].Mat, &Models[ Model ].InvMat );

		Models[ Model ].Rot.x = 0.0F;
		Models[ Model ].Rot.y = 0.0F;
		Models[ Model ].Rot.z = 0.0F;
		Models[ Model ].Func = MODFUNC_TomCruise;
		Models[ Model ].Scale = 1.0F;
		Models[ Model ].MaxScale = 1.0F;
		Models[ Model ].Group = Group;
		Models[ Model ].LifeCount = (float) ( 10.0F * ANIM_SECOND );
	}
}

/*===================================================================
	Procedure	:	Missile Shock Wave Damage to Ships and Mines.
	Input		:	VECTOR	*	Position
				:	float		Radius
				:	u_int16_t		Owner
				:	float		Damage at center
				:	u_int16_t		Group
				:	BYTE		Weapon
	Output		:	Nothing
===================================================================*/
void MissileShockWave( VECTOR * Pos, float Radius, u_int16_t Owner, float Center_Damage, u_int16_t Group, BYTE Weapon )
{
	int16_t		Count;
	float		Damage;
	u_int16_t		EndGroup;
	VECTOR		DistVector;
	float		DistFromCenter;
	VERT		Int_Point;
	NORMAL		Int_Normal;
	VECTOR		TempVector;
	VECTOR		Recoil;
	u_int16_t		i, Next;
	u_int16_t		nSecWalk;
	char		methodstr[256];
	ENEMY	*	Enemy;
	ENEMY	*	NextEnemy;

	switch( ColPerspective )
	{
		case COLPERS_Forsaken:
			if( Owner == WhoIAm )
			{
				for( Count = 0; Count < MAX_PLAYERS; Count++ )
				{
					if( ( Ships[ Count ].enable ) && ( Ships[ Count ].Object.Mode != LIMBO_MODE ) )
					{
						if( !SoundInfo[ Ships[ Count ].Object.Group ][ Group ] )
						{
							DistVector.x = ( Ships[ Count ].Object.Pos.x - Pos->x );
							DistVector.y = ( Ships[ Count ].Object.Pos.y - Pos->y );
							DistVector.z = ( Ships[ Count ].Object.Pos.z - Pos->z );
							DistFromCenter = VectorLength( &DistVector );
							
							if( DistFromCenter < ( Radius + SHIP_RADIUS ) )
							{
								if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
											  Group, &DistVector, (VECTOR *) &Int_Point,
											  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
								{
									Damage = ( Center_Damage - ( DistFromCenter / ( ( Radius + SHIP_RADIUS ) / Center_Damage ) ) );
							
									Recoil.x = ( ( DistVector.x / DistFromCenter ) * ( Damage / 10.0F ) );
									Recoil.y = ( ( DistVector.y / DistFromCenter ) * ( Damage / 10.0F ) );
									Recoil.z = ( ( DistVector.z / DistFromCenter ) * ( Damage / 10.0F ) );
							
									Damage = ( Damage * framelag );
							
									if( Count != WhoIAm )
									{
										IHitYou( (BYTE) Count, Damage, &Recoil, &TempVector, &TempVector, 0.0F, WEPTYPE_Secondary, Weapon, FRAMELAGED_RECOIL );
									}
									else
									{
										if( !Ships[ WhoIAm ].Invul )
										{
											Ships[ WhoIAm ].Damage = Damage;
											Ships[WhoIAm].ShipThatLastHitMe = WhoIAm;
								
											if( DoDamage( DONT_OVERRIDE_INVUL ) == 1 )						// Did I Die?
											{
												GetDeathString( WEPTYPE_Secondary, Weapon, &methodstr[0] );
							
												Ships[ WhoIAm ].ShipThatLastKilledMe = WhoIAm;
												Ships[ WhoIAm ].Object.Mode = DEATH_MODE;
												Ships[ WhoIAm ].Timer = 0.0F;
												PlaySfx( SFX_BIKECOMP_DY, 1.0F );
												// killed yourself with missile splash damage (e.g. mfrl)
												AddColourMessageToQue( KillMessageColour, YOU_KILLED_YOURSELF_HOW, &methodstr[0] );
												// update stats 4 (stats.cpp) -- killed yourself with missile splash damage
												UpdateKillStats(WhoIAm,WhoIAm,WEPTYPE_Secondary, Weapon);
												ShipDiedSend( WEPTYPE_Secondary, Weapon );
											}

											ForceExternalOneOff( WhoIAm, &Recoil );
										}
									}
								}
							}
						}
					}
				}
			}
			break;

		case COLPERS_Descent:
			if( ( Ships[ WhoIAm ].enable ) && ( Ships[ WhoIAm ].Object.Mode != LIMBO_MODE ) )
			{
				if( !SoundInfo[ Ships[ WhoIAm ].Object.Group ][ Group ] )
				{
					DistVector.x = ( Ships[ WhoIAm ].Object.Pos.x - Pos->x );
					DistVector.y = ( Ships[ WhoIAm ].Object.Pos.y - Pos->y );
					DistVector.z = ( Ships[ WhoIAm ].Object.Pos.z - Pos->z );
					DistFromCenter = VectorLength( &DistVector );
					
					if( DistFromCenter < ( Radius + SHIP_RADIUS ) )
					{
						if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
									  Group, &DistVector, (VECTOR *) &Int_Point,
									  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
						{
							Damage = ( Center_Damage - ( DistFromCenter / ( ( Radius + SHIP_RADIUS ) / Center_Damage ) ) );
					
							Recoil.x = ( ( DistVector.x / DistFromCenter ) * ( Damage / 10.0F ) );
							Recoil.y = ( ( DistVector.y / DistFromCenter ) * ( Damage / 10.0F ) );
							Recoil.z = ( ( DistVector.z / DistFromCenter ) * ( Damage / 10.0F ) );
					
							Damage = ( Damage * framelag );
							HitMe( OWNER_SHIP, Owner, Damage, WEPTYPE_Secondary, Weapon );
							ForceExternalOneOff( WhoIAm, &Recoil );
						}
					}
				}
			}
			break;
	}

/*===================================================================
	Shockwave hitting any mines?
===================================================================*/

	i = FirstSecBullUsed;
	nSecWalk = 0;

	/* Bound the used-chain walk: a corrupt/empty Prev chain must terminate. */
	while( i != INDEX_NONE && nSecWalk < MAXSECONDARYWEAPONBULLETS )
	{
		nSecWalk++;
		Next = SecBulls[ i ].Prev;							/* Next Secondary Bullet */

 		if( SecBulls[ i ].SecType == SEC_MINE )
		{
			DistVector.x = ( SecBulls[ i ].Pos.x - Pos->x );
			DistVector.y = ( SecBulls[ i ].Pos.y - Pos->y );
			DistVector.z = ( SecBulls[ i ].Pos.z - Pos->z );
			DistFromCenter = VectorLength( &DistVector );
			
			if( DistFromCenter < ( Radius + MINE_RADIUS ) )
			{
				if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
							  Group, &DistVector, (VECTOR *) &Int_Point,
							  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
				{
					Damage = ( ( Center_Damage - ( DistFromCenter / ( ( Radius + MINE_RADIUS ) / Center_Damage ) ) ) * framelag );
					SecBulls[ i ].Shield -= Damage;
					if( SecBulls[ i ].Shield < 0.0F ) SecBulls[ i ].Shield = 0.0F;
			
					if( SecBulls[ i ].Shield == 0.0F )
					{
						DestroySecondary( i, &SecBulls[i].Pos );
					}
				}
			}
		}

		i = Next;
	}

/*===================================================================
	Shockwave hitting any Enemies?
===================================================================*/
	Enemy = FirstEnemyUsed;

	while( Enemy != NULL )
	{
		NextEnemy = Enemy->NextUsed;

		if( ( Enemy->Status & ENEMY_STATUS_Enable ) && !( Enemy->Object.Flags & SHIP_Invul ) )
		{
			if( !SoundInfo[ Enemy->Object.Group ][ Group ] )
			{
				DistVector.x = ( Enemy->Object.Pos.x - Pos->x );
				DistVector.y = ( Enemy->Object.Pos.y - Pos->y );
				DistVector.z = ( Enemy->Object.Pos.z - Pos->z );
				DistFromCenter = VectorLength( &DistVector );
			   
				if( DistFromCenter < ( Radius + SHIP_RADIUS ) )
				{
					if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, Pos,	 	/* Hit Background? */
								  Group, &DistVector, (VECTOR *) &Int_Point,
								  &EndGroup, &Int_Normal, &TempVector, true, NULL ) != true )
					{
						Damage = ( ( Center_Damage - ( DistFromCenter / ( ( Radius + SHIP_RADIUS ) / Center_Damage ) ) ) * framelag );
			
						Recoil.x = ( ( DistVector.x / DistFromCenter ) * ( Damage / 4.0F ) );
						Recoil.y = ( ( DistVector.y / DistFromCenter ) * ( Damage / 4.0F ) );
						Recoil.z = ( ( DistVector.z / DistFromCenter ) * ( Damage / 4.0F ) );
			
						Enemy->Object.ExternalForce.x = ( Recoil.x * framelag );
						Enemy->Object.ExternalForce.y = ( Recoil.y * framelag );
						Enemy->Object.ExternalForce.z = ( Recoil.z * framelag );
			
						Enemy->Object.Shield -= Damage;
						if( Enemy->Object.Shield < 0 ) Enemy->Object.Shield = 0;
						if( Enemy->Object.Shield == 0 )
						{
							CreateNewExplosion( &Enemy->Object.Pos, &Enemy->Object.DirVector, Enemy->Object.Group );
							KillUsedEnemy( Enemy );
						}
					}
				}
			}
		}

		Enemy = NextEnemy;
	}
}

/*===================================================================
	Procedure	:	Hit me
	Input		:	u_int16_t		OwnerType
				:	u_int16_t		OwnerID
				:	float		Damage
				:	u_int16_t		WeaponType
				:	u_int16_t		Weapon
	Output		:	Nothing
===================================================================*/
void HitMe( u_int16_t OwnerType, u_int16_t OwnerID, float Damage, u_int8_t WeaponType, u_int8_t Weapon )
{
	char	methodstr[256];
	char	teamstr[256];

	if( !(Ships[ WhoIAm ].Invul && GameStatus[ WhoIAm ] == STATUS_SinglePlayer) )
	{
		Ships[ WhoIAm ].Damage = Damage;
		if( OwnerType != OWNER_SHIP ) Ships[ WhoIAm ].ShipThatLastHitMe = MAX_PLAYERS;
		else Ships[ WhoIAm ].ShipThatLastHitMe = (u_int8_t) OwnerID;

		if( DoDamage( DONT_OVERRIDE_INVUL ) == 1 )		// Did I Die?
		{
			GetDeathString( WeaponType, Weapon, &methodstr[0] );

			if( OwnerType != OWNER_SHIP ) Ships[ WhoIAm ].ShipThatLastKilledMe = MAX_PLAYERS;
			else Ships[ WhoIAm ].ShipThatLastKilledMe = (u_int8_t) OwnerID;
			Ships[ WhoIAm ].Object.Mode = DEATH_MODE;
			Ships[ WhoIAm ].Timer = 0.0F;
			
			// i killed myself lolzers
			if( ( OwnerType == OWNER_SHIP ) && ( OwnerID == WhoIAm ) )
			{
				PlaySfx( SFX_BIKECOMP_DY, 1.0F );
				AddColourMessageToQue(KillMessageColour, YOU_KILLED_YOURSELF_HOW, &methodstr[0] ); // called in both multiplayer  & single player
				// update stats 5 (stats.cpp) -- i killed myself
				UpdateKillStats(WhoIAm,WhoIAm,WeaponType,Weapon); 
			}
			else
			{
				// single player enemy killed me
				if( OwnerType == OWNER_ENEMY )
				{
					AddColourMessageToQue( KillMessageColour, ENEMY_KILLED_YOU, &methodstr[0] );
				}
				// someone killed me in multiplayer
				else
				{
					if( OwnerType == OWNER_SHIP )
					{
						if (TeamGame && ( TeamNumber[Ships[WhoIAm].ShipThatLastKilledMe] == TeamNumber[WhoIAm]))
						{
							strcpy(&teamstr[0], FROM_YOUR_OWN_TEAM );
						}
						else
						{
							strcpy(&teamstr[0], "");
						}

						// somebody killed me
						AddColourMessageToQue( KillMessageColour, SOMEONE_KILLED_YOU, &Names[Ships[WhoIAm].ShipThatLastKilledMe][0], &methodstr[0], &teamstr[0] );
						// update stats 6 (stats.cpp) -- somebody killed me
						UpdateKillStats(Ships[WhoIAm].ShipThatLastKilledMe,WhoIAm,WeaponType,Weapon); 
						
					}
					// wtf i died? why? please tell me!
					else
					{
						AddColourMessageToQue( KillMessageColour, YOU_DIED );
					}
				}
			}
			ShipDiedSend( WeaponType, Weapon );
		}
	}
}
	
/*===================================================================
	Procedure	:	Create Orbit Pulsar
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
void CreateOrbitPulsar( u_int16_t Ship )
{
	float	Rot;
	float	Inc;
	int16_t	Count;
	u_int16_t	Model;

	Rot = 0.0F;
	Inc = ( 360.0F / Ships[ Ship ].NumMultiples );

	for( Count = 0; Count < Ships[ Ship ].NumMultiples; Count++ )
	{
		if( Ships[ Ship ].OrbModels[ Count ] == INDEX_NONE )
		{
			Model =	FindFreeModel();

			if( Model != INDEX_NONE )
			{
				Ships[ Ship ].OrbModels[ Count ] = Model;
				Models[ Model ].OwnerType = OWNER_SHIP;
				Models[ Model ].Owner = Ship;
				Models[ Model ].Ship = Ship;
				Models[ Model ].ModelNum = MODEL_OrbitPulsar;
				Models[ Model ].Type = MODTYPE_Field; //Missile;
				Models[ Model ].Flags = MODFLAG_Nothing;
				Models[ Model ].Visible = true;
				Models[ Model ].Pos = Ships[ Ship ].Object.Pos;
				Models[ Model ].Dir.x = 0.0F;
				Models[ Model ].Dir.y = 0.0F;
				Models[ Model ].Dir.z = 0.0F;
				Models[ Model ].Rot.x = 0.0F;
				Models[ Model ].Rot.y = 0.0F;
				Models[ Model ].Rot.z = 0.0F;
				Models[ Model ].Quat.w = 1.0F;
				Models[ Model ].Quat.x = 0.0F;
				Models[ Model ].Quat.y = 0.0F;
				Models[ Model ].Quat.z = 0.0F;
				Models[ Model ].Mat = MATRIX_Identity;
				MatrixTranspose( &Models[ Model ].Mat, &Models[ Model ].InvMat );
				Models[ Model ].Func = MODFUNC_OrbitPulsar;
				Models[ Model ].AxisRot = Rot;
				Models[ Model ].Scale = 1.0F;
				Models[ Model ].MaxScale = 1.0F;
				Models[ Model ].Group = Ships[ Ship ].Object.Group;
				Models[ Model ].LifeCount = (float) ( 10.0F * ANIM_SECOND );
			}
		}
		else
		{
			Model = Ships[ Ship ].OrbModels[ Count ];
			Models[ Model ].AxisRot = Rot;
		}
		Rot += Inc;
	}

	for( Count = Ships[ Ship ].NumMultiples; Count < MAXMULTIPLES; Count++ )
	{
		if( Ships[ Ship ].OrbModels[ Count ] != INDEX_NONE )
		{
			KillUsedModel( Ships[ Ship ].OrbModels[ Count ] );
			Ships[ Ship ].OrbModels[ Count ] = INDEX_NONE;
		}
	}
}

/*===================================================================
	Procedure	:	Create Body Part
	Input		:	VECTOR	*	Pos
				:	VECTOR	*	UpVector
				:	u_int16_t		Group
	Output		:	Nothing
===================================================================*/
void CreateExplosionDonut( VECTOR * Pos, VECTOR * Up, u_int16_t Group )
{
	u_int16_t	Model;

	Model =	FindFreeModel();
	if( Model != INDEX_NONE )
	{
		Models[ Model ].OwnerType = OWNER_NOBODY;
		Models[ Model ].Owner = 0;
		Models[ Model ].ModelNum = MODEL_ExplosionDonut;
		Models[ Model ].Type = MODTYPE_Field;
		Models[ Model ].Flags = MODFLAG_Nothing;
		Models[ Model ].Visible = true;
		Models[ Model ].Pos.x = ( Pos->x + ( Up->x * SHIP_RADIUS ) );
		Models[ Model ].Pos.y = ( Pos->y + ( Up->y * SHIP_RADIUS ) );
		Models[ Model ].Pos.z = ( Pos->z + ( Up->z * SHIP_RADIUS ) );
		Models[ Model ].Dir.x = 0.0F;
		Models[ Model ].Dir.y = 0.0F;
		Models[ Model ].Dir.z = 0.0F;
		Models[ Model ].Rot.x = 0.0F;
		Models[ Model ].Rot.y = 0.0F;
		Models[ Model ].Rot.z = 0.0F;
		Models[ Model ].Quat.FromVector(Up);
		Models[ Model ].Quat.ToMatrix(&Models[ Model ].Mat);
		MatrixTranspose( &Models[ Model ].Mat, &Models[ Model ].InvMat );
		Models[ Model ].Func = MODFUNC_ScaleDonut;
		Models[ Model ].Scale = 0.1F;
		Models[ Model ].MaxScale = 10.0F;
		Models[ Model ].Group = Group;
		Models[ Model ].LifeCount = (float) ( 10.0F * ANIM_SECOND );
	}
}

/*===================================================================
	Procedure	:	Create Line
	Input		:	float		x1
				:	float		y1
				:	float		z1
				:	float		x2
				:	float		y2
				:	float		z2
				:	u_int16_t		Group
	Output		:	u_int16_t		Line Index
===================================================================*/
u_int16_t CreateLine( float x1, float y1, float z1, float x2, float y2, float z2, u_int16_t Group )
{
	u_int16_t	line;

	line = FindFreeLine();						// Line attached

	if( line != INDEX_NONE )
	{
		Lines[ line ].StartPos.x = x1;
		Lines[ line ].StartPos.y = y1;
		Lines[ line ].StartPos.z = z1;
		Lines[ line ].EndPos.x = x2;
		Lines[ line ].EndPos.y = y2;
		Lines[ line ].EndPos.z = z2;
		Lines[ line ].StartCol.R = 255;
		Lines[ line ].StartCol.G = 255;
		Lines[ line ].StartCol.B = 255;
		Lines[ line ].EndCol.R = 255;
		Lines[ line ].EndCol.G = 255;
		Lines[ line ].EndCol.B = 255;
		Lines[ line ].StartTrans = 255;
		Lines[ line ].EndTrans = 255;
		Lines[ line ].Group = Group;
	}
	return( line );
}

/*===================================================================
	Procedure	:	Get .MX File Bouning Box
	Input		:	MXLOADHEADER * DstMloadheader
				:	MATRIX	*	Matrix
				:	VECTOR	*	Pos
				:	VECTOR	*	TopLeft
				:	VECTOR	*	BottomRight
	Output		:	bool		True/False
===================================================================*/
bool GetMXBoundingBox( MXLOADHEADER * DstMloadheader, MATRIX * Matrix, VECTOR * Pos, VECTOR * TopLeft,
					   VECTOR * BottomRight )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Count;
	u_int16_t					ExecBuf;
	VECTOR					TempVert;

	Group = 0;
	ExecBuf = 0;

	if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
	{
		return false;
	}

	for( Count = 0; Count < 8; Count++ )
	{
		TempVert.x = DstlpD3DLVERTEX->x;
		TempVert.y = DstlpD3DLVERTEX->y;
		TempVert.z = DstlpD3DLVERTEX->z;
		ApplyMatrix( Matrix, &TempVert, &TempVert );
		TempVert.x += Pos->x;
		TempVert.y += Pos->y;
		TempVert.z += Pos->z;

		if( ( TopLeft->x == BottomRight->x ) && ( TopLeft->y == BottomRight->y ) && ( TopLeft->z == BottomRight->z ) &&
			( TopLeft->x == 0.0F ) && ( TopLeft->y == 0.0F ) && ( TopLeft->z == 0.0F ) )
		{
			TopLeft->x = TempVert.x;
			TopLeft->y = TempVert.y;
			TopLeft->z = TempVert.z;
			BottomRight->x = TempVert.x;
			BottomRight->y = TempVert.y;
			BottomRight->z = TempVert.z;
		}
		else
		{
			if( TempVert.x < TopLeft->x ) TopLeft->x = TempVert.x;
			if( TempVert.x > BottomRight->x ) BottomRight->x = TempVert.x;
			if( TempVert.y < TopLeft->y ) TopLeft->y = TempVert.y;
			if( TempVert.y > BottomRight->y ) BottomRight->y = TempVert.y;
			if( TempVert.z < TopLeft->z ) TopLeft->z = TempVert.z;
			if( TempVert.z > BottomRight->z ) BottomRight->z = TempVert.z;
		}

		DstlpD3DLVERTEX++;
	}

	if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
	{
		return false;
	}
	return true;
}

/*===================================================================
	Procedure	:	Returns a player's short name...
	Input		:	 player id
	Output		:	player's short name
===================================================================*/
char *GetName(int Player)
{
	return &Names[Player][0];
}


/*===================================================================
	Procedure	:	Get .MXA File Bouning Box
	Input		:	MXALOADHEADER * DstMloadheader
				:	MATRIX	*	Matrix
				:	VECTOR	*	Pos
				:	VECTOR	*	TopLeft
				:	VECTOR	*	BottomRight
	Output		:	bool	True/False
===================================================================*/
bool GetMXABoundingBox( MXALOADHEADER * DstMloadheader, MATRIX * Matrix, VECTOR * Pos, VECTOR * TopLeft,
					    VECTOR * BottomRight )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Count;
	u_int16_t					ExecBuf;
	VECTOR					TempVert;

	Group = 0;
	ExecBuf = 0;
	if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
	{
		return false;
	}

	for( Count = 0; Count < 8; Count++ )
	{
		TempVert.x = DstlpD3DLVERTEX->x;
		TempVert.y = DstlpD3DLVERTEX->y;
		TempVert.z = DstlpD3DLVERTEX->z;
		ApplyMatrix( Matrix, &TempVert, &TempVert );
		TempVert.x += Pos->x;
		TempVert.y += Pos->y;
		TempVert.z += Pos->z;

		if( ( TopLeft->x == BottomRight->x ) && ( TopLeft->y == BottomRight->y ) && ( TopLeft->z == BottomRight->z ) &&
			( TopLeft->x == 0.0F ) && ( TopLeft->y == 0.0F ) && ( TopLeft->z == 0.0F ) )
		{
			TopLeft->x = TempVert.x;
			TopLeft->y = TempVert.y;
			TopLeft->z = TempVert.z;
			BottomRight->x = TempVert.x;
			BottomRight->y = TempVert.y;
			BottomRight->z = TempVert.z;
		}
		else
		{
			if( TempVert.x < TopLeft->x ) TopLeft->x = TempVert.x;
			if( TempVert.x > BottomRight->x ) BottomRight->x = TempVert.x;
			if( TempVert.y < TopLeft->y ) TopLeft->y = TempVert.y;
			if( TempVert.y > BottomRight->y ) BottomRight->y = TempVert.y;
			if( TempVert.z < TopLeft->z ) TopLeft->z = TempVert.z;
			if( TempVert.z > BottomRight->z ) BottomRight->z = TempVert.z;
		}

		DstlpD3DLVERTEX++;
	}

	if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
	{
		return false;
	}
	
	return true;
}

/*===================================================================
	Procedure	:	Create .MX File Bouning Box ( Lines )
	Input		:	MXLOADHEADER * DstMloadheader
				:	MATRIX	*	Matrix
				:	VECTOR	*	Pos
				:	u_int16_t	*	Line Array
				:	u_int16_t		Group
	Output		:	bool		True/False
===================================================================*/
bool CreateMXBoundingBox( MXLOADHEADER * DstMloadheader, MATRIX * Matrix, VECTOR * Pos, u_int16_t * LineArray, u_int16_t Group2 )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Count;
	u_int16_t					ExecBuf;
	VECTOR					Verts[ 8 ];

	for( Count = 0; Count < 12; Count++ )
	{
		if( LineArray[ Count ] != INDEX_NONE )
		{
			KillUsedLine( LineArray[ Count ] );
			LineArray[ Count ] = INDEX_NONE;
		}

	}
	if( !ShowBoundingBoxes ) return true;

	Group = 0;
	ExecBuf = 0;
	if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
	{
		return false;
	}

	for( Count = 0; Count < 8; Count++ )
	{
		Verts[ Count ].x = DstlpD3DLVERTEX->x;
		Verts[ Count ].y = DstlpD3DLVERTEX->y;
		Verts[ Count ].z = DstlpD3DLVERTEX->z;
		ApplyMatrix( Matrix, &Verts[ Count ], &Verts[ Count ] );
		Verts[ Count ].x += Pos->x;
		Verts[ Count ].y += Pos->y;
		Verts[ Count ].z += Pos->z;

		DstlpD3DLVERTEX++;
	}

	LineArray[ 0 ] = CreateLine( Verts[ 0 ].x, Verts[ 0 ].y, Verts[ 0 ].z, Verts[ 1 ].x, Verts[ 1 ].y, Verts[ 1 ].z, Group2 );
	LineArray[ 1 ] = CreateLine( Verts[ 1 ].x, Verts[ 1 ].y, Verts[ 1 ].z, Verts[ 3 ].x, Verts[ 3 ].y, Verts[ 3 ].z, Group2 );
	LineArray[ 2 ] = CreateLine( Verts[ 3 ].x, Verts[ 3 ].y, Verts[ 3 ].z, Verts[ 2 ].x, Verts[ 2 ].y, Verts[ 2 ].z, Group2 );
	LineArray[ 3 ] = CreateLine( Verts[ 2 ].x, Verts[ 2 ].y, Verts[ 2 ].z, Verts[ 0 ].x, Verts[ 0 ].y, Verts[ 0 ].z, Group2 );

	LineArray[ 4 ] = CreateLine( Verts[ 4 ].x, Verts[ 4 ].y, Verts[ 4 ].z, Verts[ 5 ].x, Verts[ 5 ].y, Verts[ 5 ].z, Group2 );
	LineArray[ 5 ] = CreateLine( Verts[ 5 ].x, Verts[ 5 ].y, Verts[ 5 ].z, Verts[ 7 ].x, Verts[ 7 ].y, Verts[ 7 ].z, Group2 );
	LineArray[ 6 ] = CreateLine( Verts[ 7 ].x, Verts[ 7 ].y, Verts[ 7 ].z, Verts[ 6 ].x, Verts[ 6 ].y, Verts[ 6 ].z, Group2 );
	LineArray[ 7 ] = CreateLine( Verts[ 6 ].x, Verts[ 6 ].y, Verts[ 6 ].z, Verts[ 4 ].x, Verts[ 4 ].y, Verts[ 4 ].z, Group2 );
			   
	LineArray[ 8 ] = CreateLine( Verts[ 0 ].x, Verts[ 0 ].y, Verts[ 0 ].z, Verts[ 4 ].x, Verts[ 4 ].y, Verts[ 4 ].z, Group2 );
	LineArray[ 9 ] = CreateLine( Verts[ 1 ].x, Verts[ 1 ].y, Verts[ 1 ].z, Verts[ 5 ].x, Verts[ 5 ].y, Verts[ 5 ].z, Group2 );
	LineArray[ 10 ] = CreateLine( Verts[ 2 ].x, Verts[ 2 ].y, Verts[ 2 ].z, Verts[ 6 ].x, Verts[ 6 ].y, Verts[ 6 ].z, Group2 );
	LineArray[ 11 ] = CreateLine( Verts[ 3 ].x, Verts[ 3 ].y, Verts[ 3 ].z, Verts[ 7 ].x, Verts[ 7 ].y, Verts[ 7 ].z, Group2 );

	if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
	{
		return false;
	}
	return true;
}

/*===================================================================
	Procedure	:	Create .MXA File Bouning Box ( Lines )
	Input		:	MXALOADHEADER * DstMloadheader
				:	MATRIX	*	Matrix
				:	VECTOR	*	Pos
				:	u_int16_t	*	Line Array
				:	u_int16_t		Group
	Output		:	bool	True/False
===================================================================*/
bool CreateMXABoundingBox( MXALOADHEADER * DstMloadheader, MATRIX * Matrix, VECTOR * Pos, u_int16_t * LineArray, u_int16_t Group2 )
{
	LVERTEX			*DstlpD3DLVERTEX = NULL;
	u_int16_t					Group;
	u_int16_t					Count;
	u_int16_t					ExecBuf;
	VECTOR					Verts[ 8 ];

	for( Count = 0; Count < 12; Count++ )
	{
		if( LineArray[ Count ] != INDEX_NONE )
		{
			KillUsedLine( LineArray[ Count ] );
			LineArray[ Count ] = INDEX_NONE;
		}

	}
	if( !ShowBoundingBoxes ) return true;

	Group = 0;
	ExecBuf = 0;
	if (!(FSLockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf], &DstlpD3DLVERTEX)))
	{
		return false;
	}

	for( Count = 0; Count < 8; Count++ )
	{
		Verts[ Count ].x = DstlpD3DLVERTEX->x;
		Verts[ Count ].y = DstlpD3DLVERTEX->y;
		Verts[ Count ].z = DstlpD3DLVERTEX->z;
		ApplyMatrix( Matrix, &Verts[ Count ], &Verts[ Count ] );
		Verts[ Count ].x += Pos->x;
		Verts[ Count ].y += Pos->y;
		Verts[ Count ].z += Pos->z;

		DstlpD3DLVERTEX++;
	}

	LineArray[ 0 ] = CreateLine( Verts[ 0 ].x, Verts[ 0 ].y, Verts[ 0 ].z, Verts[ 1 ].x, Verts[ 1 ].y, Verts[ 1 ].z, Group2 );
	LineArray[ 1 ] = CreateLine( Verts[ 1 ].x, Verts[ 1 ].y, Verts[ 1 ].z, Verts[ 3 ].x, Verts[ 3 ].y, Verts[ 3 ].z, Group2 );
	LineArray[ 2 ] = CreateLine( Verts[ 3 ].x, Verts[ 3 ].y, Verts[ 3 ].z, Verts[ 2 ].x, Verts[ 2 ].y, Verts[ 2 ].z, Group2 );
	LineArray[ 3 ] = CreateLine( Verts[ 2 ].x, Verts[ 2 ].y, Verts[ 2 ].z, Verts[ 0 ].x, Verts[ 0 ].y, Verts[ 0 ].z, Group2 );

	LineArray[ 4 ] = CreateLine( Verts[ 4 ].x, Verts[ 4 ].y, Verts[ 4 ].z, Verts[ 5 ].x, Verts[ 5 ].y, Verts[ 5 ].z, Group2 );
	LineArray[ 5 ] = CreateLine( Verts[ 5 ].x, Verts[ 5 ].y, Verts[ 5 ].z, Verts[ 7 ].x, Verts[ 7 ].y, Verts[ 7 ].z, Group2 );
	LineArray[ 6 ] = CreateLine( Verts[ 7 ].x, Verts[ 7 ].y, Verts[ 7 ].z, Verts[ 6 ].x, Verts[ 6 ].y, Verts[ 6 ].z, Group2 );
	LineArray[ 7 ] = CreateLine( Verts[ 6 ].x, Verts[ 6 ].y, Verts[ 6 ].z, Verts[ 4 ].x, Verts[ 4 ].y, Verts[ 4 ].z, Group2 );
			   
	LineArray[ 8 ] = CreateLine( Verts[ 0 ].x, Verts[ 0 ].y, Verts[ 0 ].z, Verts[ 4 ].x, Verts[ 4 ].y, Verts[ 4 ].z, Group2 );
	LineArray[ 9 ] = CreateLine( Verts[ 1 ].x, Verts[ 1 ].y, Verts[ 1 ].z, Verts[ 5 ].x, Verts[ 5 ].y, Verts[ 5 ].z, Group2 );
	LineArray[ 10 ] = CreateLine( Verts[ 2 ].x, Verts[ 2 ].y, Verts[ 2 ].z, Verts[ 6 ].x, Verts[ 6 ].y, Verts[ 6 ].z, Group2 );
	LineArray[ 11 ] = CreateLine( Verts[ 3 ].x, Verts[ 3 ].y, Verts[ 3 ].z, Verts[ 7 ].x, Verts[ 7 ].y, Verts[ 7 ].z, Group2 );

	if (!(FSUnlockVertexBuffer(&DstMloadheader->Group[ Group ].renderObject[ExecBuf])))
	{
		return false;
	}
	
	return true;
}

/*===================================================================
	Procedure	:	Setup Model SpotFX
	Input		:	u_int16_t		ModelIndex
	Output		:	Nothing
===================================================================*/
void SetupModelSpotFX( u_int16_t i )
{
	int16_t		Count;
	int16_t		NumSpotFX = 0;
	PVSPOTFX *	SpotFXPtr = NULL;

	if( ModelNames[ Models[i].ModelNum ].DoIMorph )
	{
		if( MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
			MxaModelHeaders[ Models[i].ModelNum ].SpotFX )
		{
			SpotFXPtr = MxaModelHeaders[ Models[i].ModelNum ].SpotFX;
			NumSpotFX = MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX;
		}
	}
	else
	{
		if( ModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
			ModelHeaders[ Models[i].ModelNum ].SpotFX )
		{
			SpotFXPtr = ModelHeaders[ Models[i].ModelNum ].SpotFX;
			NumSpotFX = ModelHeaders[ Models[i].ModelNum ].NumSpotFX;
		}
	}

	for( Count = 0; Count < NumSpotFX; Count++ )
	{
		Models[i].SpotFXSFX_ID[ Count ] = 0;

		if( SpotFXPtr->StartDelay )
		{
			Models[i].SpotFXFlags[ Count ] = SPOTFX_FLAGS_Nothing;
			Models[i].SpotFXState[ Count ] = SPOTFX_STATE_WaitingForDelay;
			Models[i].SpotFXDelay[ Count ] = SpotFXPtr->StartDelay;

		}
		else
		{
			Models[i].SpotFXFlags[ Count ] = SPOTFX_FLAGS_Nothing;
			Models[i].SpotFXState[ Count ] = SPOTFX_STATE_Active;
			Models[i].SpotFXDelay[ Count ] = SpotFXPtr->ActiveDelay;
		}

		SpotFXPtr++;
	}
}

/*===================================================================
	Procedure	:	Update Ship Model
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/

void UpdateShipModel( u_int16_t Ship )
{
	u_int16_t	Model;

	if( Ship == MAX_PLAYERS ) return;

	if( Ships[ Ship ].enable )
	{
		if( BikeCompFiles[ ( Ships[ Ship ].BikeNum % MAXBIKETYPES ) ].Filename[ 0 ] )
		{
			if( Ships[ Ship ].ModelNum != INDEX_NONE )
			{

				KillUsedModel( Ships[ Ship ].ModelNum );
				Ships[ Ship ].ModelNum = INDEX_NONE;
			}

			if( !Ships[ Ship ].Object.Components )
			{
				AllocateCompShip( Ship );
			}

			if( Ships[ Ship ].Object.Components )
			{
				Ships[ Ship ].Object.Time += ( framelag * Ships[ Ship ].Object.AnimSpeed );

				if( Ships[ Ship ].Object.Time > Ships[ Ship ].Object.OverallTime )
				{
					Ships[ Ship ].Object.Time = FMOD( Ships[ Ship ].Object.Time, Ships[ Ship ].Object.OverallTime );
				}

				if( ( Ships[ Ship ].Object.Flags & SHIP_Stealth ) )
				{
					SetCompObjStealth( Ships[ Ship ].Object.Components, 1 );

					if( Ships[ Ship ].Object.Flags & SHIP_Charging )
					{
						Ships[ Ship ].Object.Red = 0;
						Ships[ Ship ].Object.Green = 0;
						Ships[ Ship ].Object.Blue = 128;
					}
					else
					{
						Ships[ Ship ].Object.Red = 0;
						Ships[ Ship ].Object.Green = 0;
						Ships[ Ship ].Object.Blue = 0;
					}
				}
				else
				{
					SetCompObjNonStealth( Ships[ Ship ].Object.Components, 1 );
					UpdateObjectAmbientColour( &Ships[ Ship ].Object );
				}

				UpdateCompObjColours( Ships[ Ship ].Object.Components, 1, (int) Ships[ Ship ].Object.Red,
									  (int) Ships[ Ship ].Object.Green, (int) Ships[ Ship ].Object.Blue );
				UpdateCompObjChildren( Ships[ Ship ].Object.Components, 1, &Ships[ Ship ].Object.FinalMat,
									   &Ships[ Ship ].Object.Pos, Ships[ Ship ].Object.Time,
									   Ships[ Ship ].Object.Group, &Ships[ Ship ].Object.Pos );

				if( ( Ships[ Ship ].enable != 0 ) &&
					( Ships[ Ship ].Object.Mode != GAMEOVER_MODE ) &&
					( Ships[ Ship ].Object.Mode != LIMBO_MODE ) &&
					( (GameStatus[ Ship ] == STATUS_Normal ) || (GameStatus[ Ship ] == STATUS_SinglePlayer ) ) &&
					( Current_Camera_View != Ship ) )
				{
					SetCompObjModelsState( Ships[ Ship ].Object.Components, 1, true );
				}
				else
				{
					SetCompObjModelsState( Ships[ Ship ].Object.Components, 1, false );
				}
			}
		}
		else
		{
			if( Ships[ Ship ].Object.Components )
			{

				FreeCompObjChildren( Ships[ Ship ].Object.Components, 1 );
				free( Ships[ Ship ].Object.Components );
				Ships[ Ship ].Object.Components = NULL;
			}

			if( Ships[ Ship ].ModelNum == INDEX_NONE )
			{
#ifdef DEBUG_ON
				PickupModelValid();
#endif
				Model =	FindFreeModel();

				if( Model != INDEX_NONE )
				{
					Models[ Model ].OwnerType = OWNER_SHIP;
					Models[ Model ].Owner = WhoIAm;
					Models[ Model ].ModelNum = BikeModels[ ( Ships[ Ship ].BikeNum % MAXBIKETYPES ) ];
					Models[ Model ].Type = MODTYPE_Pickup;
					Models[ Model ].Flags = MODFLAG_AmbientLight;
					Models[ Model ].Visible = true;
					Models[ Model ].Pos = Ships[ Ship ].Object.Pos;
					Models[ Model ].Dir = Ships[ Ship ].Move_Off;
					Models[ Model ].Rot.x = 0.0F;
					Models[ Model ].Rot.y = 0.0F;
					Models[ Model ].Rot.z = 0.0F;
					Models[ Model ].Mat = Ships[ Ship ].Object.FinalMat;
					MatrixTranspose( &Models[ Model ].Mat, &Models[ Model ].InvMat );
					Models[ Model ].Func = MODFUNC_ProcessSpotFX; //MODFUNC_Nothing;
					Models[ Model ].Scale = 1.0F;
					Models[ Model ].MaxScale = 1.0F;
					Models[ Model ].Group = Ships[ Ship ].Object.Group;
					Models[ Model ].LifeCount = (float) 10000.0F;
					Ships[ Ship ].ModelNum = Model;

#ifdef DEBUG_ON
					PickupModelValid();
#endif
				}
			}

			if( Ships[ Ship ].ModelNum != INDEX_NONE )
			{
				Model = Ships[ Ship ].ModelNum;

				if( ( Ships[ Ship ].Object.Flags & SHIP_Stealth ) )
				{
					Models[ Model ].Flags &= ~MODFLAG_AmbientLight;
					Models[ Model ].Flags |= MODFLAG_Stealth;

					if( Ships[ Ship ].Object.Flags & SHIP_Charging )
					{
						Models[ Model ].Red = 0;
						Models[ Model ].Green = 0;
						Models[ Model ].Blue = 128;
					}
					else
					{
						Models[ Model ].Red = 0;
						Models[ Model ].Green = 0;
						Models[ Model ].Blue = 0;
					}
				}
				else
				{
					Models[ Model ].Flags |= MODFLAG_AmbientLight;
					Models[ Model ].Flags &= ~MODFLAG_Stealth;
					UpdateObjectAmbientColour( &Ships[Ship].Object );
					Models[ Model ].Red = (int) Ships[ Ship ].Object.Red;
					Models[ Model ].Green = (int) Ships[ Ship ].Object.Green;
					Models[ Model ].Blue = (int) Ships[ Ship ].Object.Blue;
				}

				Models[ Model ].ModelNum = 	BikeModels[ ( Ships[ Ship ].BikeNum % MAXBIKETYPES ) ];
				Models[ Model ].Pos = Ships[ Ship ].Object.Pos;
				Models[ Model ].Dir = Ships[ Ship ].Move_Off;
				Models[ Model ].Mat = Ships[ Ship ].Object.FinalMat;
				MatrixTranspose( &Models[ Model ].Mat, &Models[ Model ].InvMat );
				Models[ Model ].Group = Ships[ Ship ].Object.Group;

				if( ( Ships[ Ship ].enable != 0 ) &&
					( Ships[ Ship ].Object.Mode != GAMEOVER_MODE ) &&
					( Ships[ Ship ].Object.Mode != LIMBO_MODE ) &&
					( (GameStatus[ Ship ] == STATUS_Normal ) || (GameStatus[ Ship ] == STATUS_SinglePlayer ) ) &&
					( Current_Camera_View != Ship ) )
				{
					Models[ Model ].Visible = true;
				}
				else
				{
					Models[ Model ].Visible = false;
				}
			}
		}
	}
	else
	{
		Ships[ Ship ].NumMultiples = 0;
		CreateOrbitPulsar( Ship );

		if( Ships[ Ship ].ModelNum != INDEX_NONE )
		{

			KillUsedModel( Ships[ Ship ].ModelNum );
			Ships[ Ship ].ModelNum = INDEX_NONE;
		}

		if( Ships[ Ship ].Object.Components )
		{

			FreeCompObjChildren( Ships[ Ship ].Object.Components, 1 );
			free( Ships[ Ship ].Object.Components );
			Ships[ Ship ].Object.Components = NULL;
		}
	}

	if( TeamGame && TintBikeTeamColor && Ships[Ship].enable )
	{
		int team = TeamNumber[Ship]; if(team >= MAX_TEAMS || team < 0) return;
		int color = TeamCol[team]; if(color >= MAXFONTCOLOURS || color < 0) return;
		Ships[ Ship ].Object.Red   = 255.0f-Colourtrans[color][0];
		Ships[ Ship ].Object.Green = 255.0f-Colourtrans[color][1];
		Ships[ Ship ].Object.Blue  = 255.0f-Colourtrans[color][2];
	}
}

/*===================================================================
	Procedure	:	Get Ambient Colour for Object
	Input		:	OBJECT * Object
	Output		:	Nothing
===================================================================*/
void UpdateObjectAmbientColour( OBJECT * Object )
{
	COLOR	Col;
	float		Red,Green,Blue,Speed;

	Col = FindNearestCellColour( &Mloadheader, &Object->Pos, Object->Group );
	Red = (float)RGBA_GETRED(Col);
	Green = (float)RGBA_GETGREEN(Col);
	Blue = (float)RGBA_GETBLUE(Col);

	Speed = VectorLength( &Object->Speed );
	Speed += VectorLength( &Object->ExternalForce );
	Speed *= framelag;
	Speed *= 0.1F;

	if( Object->Red < Red )
	{
		Object->Red+= 1.0F + Speed;
		if( Object->Red > Red )
			Object->Red = Red;
	}
	else if( Object->Red > Red )
	{
		Object->Red-= 1.0F + Speed;
		if( Object->Red < Red )
			Object->Red = Red;
	}
	if( Object->Green < Green )
	{
		Object->Green+= 1.0F + Speed;
		if( Object->Green > Green )
			Object->Green = Green;
	}
	else if( Object->Green > Green )
	{
		Object->Green-= 1.0F + Speed;
		if( Object->Green < Green )
			Object->Green = Green;
	}
	if( Object->Blue < Blue )
	{
		Object->Blue+= 1.0F + Speed;
		if( Object->Blue > Blue )
			Object->Blue = Blue;
	}
	else if( Object->Blue > Blue )
	{
		Object->Blue-= 1.0F + Speed;
		if( Object->Blue < Blue )
			Object->Blue = Blue;
	}
}

/*===================================================================
	Procedure	:	Get An Overall Light level for a Position...Based on
				:	Real lights...
	Input		:	VECTOR * Pos , float * Red , float * Green , float * Blue
	Output		:	Nothing
===================================================================*/
void GetRealLightAmbient( VECTOR * Pos , float * Red , float * Green , float * Blue )
{
	VECTOR					DistVector;
	float					Dist;
    float					Val;
	XLIGHT * LightPnt;
	float	RF,GF,BF;
	float Size2;


	RF = GF = BF = 0.0F;
	
	LightPnt = FirstLightVisible;
	
	while( LightPnt )
	{
		DistVector.x = ( Pos->x - LightPnt->Pos.x );
		DistVector.y = ( Pos->y - LightPnt->Pos.y );
		DistVector.z = ( Pos->z - LightPnt->Pos.z );

		Size2 = LightPnt->Size;
		Size2 *= Size2;
		
		Dist = ( DistVector.x * DistVector.x ) + ( DistVector.y * DistVector.y ) + ( DistVector.z * DistVector.z );
	
		if( Dist < Size2 )
		{
			Val = ( 1.0F - ( Dist / Size2 ) );
	
			RF += ( LightPnt->r * Val );
			GF += ( LightPnt->g * Val );
			BF += ( LightPnt->b * Val );
		}
		LightPnt = LightPnt->NextVisible;
	}
	if( RF > 255.0F ) RF = 255.0F;
	if( GF > 255.0F ) GF = 255.0F;
	if( BF > 255.0F ) BF = 255.0F;

	*Red = RF;
	*Green = GF;
	*Blue = BF;
}

#define POINT_TO_PLANE( P, N ) ( (P)->x * (N)->Normal.x + (P)->y * (N)->Normal.y + (P)->z * (N)->Normal.z + ( (N)->Offset) )

constexpr float SHIP_MIN_FREQ = 0.5F;
constexpr float SHIP_MAX_FREQ = 1.0F;
constexpr int ENGINEUPDATEPAUSEFRAMES = 5;

float FrameLagStore[ MAX_PLAYERS ];
int CurrentStoredSpeed[ MAX_PLAYERS ];
float SpeedStore[ MAX_PLAYERS ][ ENGINEUPDATEPAUSEFRAMES ];

void InitShipSpeeds( void )
{
	memset( FrameLagStore, 0, sizeof( FrameLagStore ) );
	memset( CurrentStoredSpeed, 0, sizeof( CurrentStoredSpeed ) );
	memset( SpeedStore, 0, sizeof( SpeedStore ) );
}

/*===================================================================
	Procedure	:	Update SFX for Ships/Enemies
	Input		:	u_int16_t		OwnerType
				:	u_int16_t		OwenID
				:	u_int32_t		SoundFX ID
				:	float		Speed
	Output		:	u_int32_t		New SoundFX ID
===================================================================*/
u_int32_t EngineCode( u_int16_t OwnerType, u_int16_t OwnerID, u_int32_t SoundFX_ID, float Speed )
{
	float fracspeed, newfreq;
	int i;

	if ( OwnerType != OWNER_SHIP )
		return SoundFX_ID;

	if( OwnerID >= MAX_PLAYERS )
		return SoundFX_ID;
	
	if ( framelag )
	  	Speed /= framelag;
	else
		Speed = 0.0F;

	FrameLagStore[ OwnerID ] += framelag;

	while( ( FrameLagStore[ OwnerID ] >= 1.0F ) && ( CurrentStoredSpeed[ OwnerID ] < ENGINEUPDATEPAUSEFRAMES ) )
	{
		FrameLagStore[ OwnerID ] -= 1.0F;
		SpeedStore[ OwnerID ][ CurrentStoredSpeed[ OwnerID ]++ ] = Speed;
	}

	if ( CurrentStoredSpeed[ OwnerID ] >= ENGINEUPDATEPAUSEFRAMES )
	{
		CurrentStoredSpeed[ OwnerID ] = 0;
	}else
	{
		return SoundFX_ID;
	}

	Speed = 0;
	for( i = 0; i < ENGINEUPDATEPAUSEFRAMES; i++ )
	{
		Speed += SpeedStore[ OwnerID ][ i ];
	}
	
	Speed /= (float)ENGINEUPDATEPAUSEFRAMES;
	if ( Speed > MAXMOVESPEED )
		Speed = MAXMOVESPEED;
	
	switch( OwnerType )
	{
		case OWNER_SHIP:
			fracspeed = (float)fabsf( (double) ( Speed / MAXMOVESPEED ) );
			newfreq = fracspeed * ( SHIP_MAX_FREQ - SHIP_MIN_FREQ ) + SHIP_MIN_FREQ;
			ModifyLoopingSfx( SoundFX_ID, newfreq, 0.0F );
			break;
		case OWNER_ENEMY:
			switch( Enemies[ OwnerID ].Type )
			{
				case ENEMY_BeamTurret:			// 0
				case ENEMY_DuelTurret:			// 1
				case ENEMY_PulseTurret:			// 2
				case ENEMY_MissileTurret:		// 3
				case ENEMY_SnubTurret:			// 4
				case ENEMY_LazerBot:			// 5
				case ENEMY_SnubBot:				// 6
				case ENEMY_AirMoble:			// 7
				case ENEMY_AmmoDump:			// 8
				case ENEMY_LeviTank:			// 9
				case ENEMY_Max:					// 10
				case ENEMY_Mekton:				// 11
				case ENEMY_Boss_Nukecrane:		// 12
				case ENEMY_Supressor:			// 13
				case ENEMY_MineSweeper:			// 14
				case ENEMY_Swarm:				// 15
				case ENEMY_Shade:				// 16
				case ENEMY_MineLayer:			// 17
				case ENEMY_Hunter:				// 18
				case ENEMY_Legz:				// 19
				case ENEMY_GuardBot:			// 20
				case ENEMY_Boss_Metatank:		// 21
				case ENEMY_Boss_BigGeek:		// 22
				case ENEMY_Boss_Avatar:			// 23
				case ENEMY_Boss_FleshMorph:		// 24
				case ENEMY_Boss_Exogenon:		// 25
				case ENEMY_Bike_Lokasenna:  	// 26
				case ENEMY_Bike_Beard:       	// 27
				case ENEMY_Bike_LAJay:    		// 28
				case ENEMY_Bike_ExCop:     		// 29
				case ENEMY_Bike_RexHardy:  		// 30
				case ENEMY_Bike_Foetoid:    	// 31
				case ENEMY_Bike_NimSoo:    		// 32
				case ENEMY_Bike_Nutta:      	// 33
				case ENEMY_Bike_Sceptre:    	// 34
				case ENEMY_Bike_Jo:         	// 35
				case ENEMY_Bike_CuvelClark:		// 36
				case ENEMY_Bike_HK5:       		// 37
				case ENEMY_Bike_Nubia:      	// 38
				case ENEMY_Bike_Mofisto:    	// 39
				case ENEMY_Bike_Cerbero:    	// 40
				case ENEMY_Bike_Slick:      	// 41
				case ENEMY_LEADER_Swarm:		// 42
				case ENEMY_LEADER_Hunter:		// 43
				case ENEMY_LEADER_Mekton:		// 44
				case ENEMY_LEADER_SnubBot:		// 45
				case ENEMY_LEADER_Legz:			// 46
				case ENEMY_LEADER_Shade:		// 47
				case ENEMY_LEADER_Supressor:	// 48
				case ENEMY_LEADER_LeviTank:		// 49
				case ENEMY_LEADER_LazerBot:		// 50
				case ENEMY_LEADER_Max:			// 51
				case ENEMY_LEADER_AirMoble:		// 52
				case ENEMY_Fodder1:				// 53
				case ENEMY_Boss_LittleGeek:		// 54
					break;

				default:
					break;
			}
			break;

		default:
			break;
	}

	return( SoundFX_ID );
}

/*===================================================================
	Procedure	:	Save Models Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * SaveModels( FILE * fp )
{
	u_int16_t i;

	if( fp )
	{
		fwrite( &FirstModelUsed, sizeof( FirstModelUsed ), 1, fp );
		fwrite( &FirstModelFree, sizeof( FirstModelFree ), 1, fp );
		
		i = FirstModelUsed;

		while( i != INDEX_NONE )
		{
			fwrite( &Models[ i ].Next, sizeof( u_int16_t ), 1, fp );
			fwrite( &Models[ i ].Prev, sizeof( u_int16_t ), 1, fp );
			fwrite( &Models[ i ].Type, sizeof( int16_t ), 1, fp );
			fwrite( &Models[ i ].Flags, sizeof( u_int16_t ), 1, fp );
			fwrite( &Models[ i ].SecWeapon, sizeof( BYTE ), 1, fp );
			fwrite( &Models[ i ].Visible, sizeof( bool ), 1, fp );
			fwrite( &Models[ i ].ModelNum, sizeof( u_int16_t ), 1, fp );
			fwrite( &Models[ i ].Group, sizeof( u_int16_t ), 1, fp );
			fwrite( &Models[ i ].OwnerType, sizeof( u_int16_t ), 1, fp );
			fwrite( &Models[ i ].Owner, sizeof( u_int16_t ), 1, fp );
			fwrite( &Models[ i ].Func, sizeof( int16_t ), 1, fp );
			fwrite( &Models[ i ].Scale, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].Xscale, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].Yscale, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].Zscale, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].MaxScale, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].LifeCount, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].AxisRot, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].Speed, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].TimeInterval, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].TimeCount, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].Frame, sizeof( int16_t ), 1, fp );
			fwrite( &Models[ i ].InterpFrame, sizeof( int16_t ), 1, fp );
			fwrite( &Models[ i ].InterpTime, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].Qlerp, sizeof( QUATLERP ), 1, fp );
			fwrite( &Models[ i ].Quat, sizeof( QUAT ), 1, fp );
			fwrite( &Models[ i ].Ship, sizeof( u_int16_t ), 1, fp );
			fwrite( &Models[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fwrite( &Models[ i ].OldPos, sizeof( VECTOR ), 1, fp );
			fwrite( &Models[ i ].Dir, sizeof( VECTOR ), 1, fp );
			fwrite( &Models[ i ].Rot, sizeof( VECTOR ), 1, fp );
			fwrite( &Models[ i ].Mat, sizeof( MATRIX ), 1, fp );
			fwrite( &Models[ i ].Radius, sizeof( float ), 1, fp );
			fwrite( &Models[ i ].Red, sizeof( int ), 1, fp );
			fwrite( &Models[ i ].Green, sizeof( int ), 1, fp );
			fwrite( &Models[ i ].Blue, sizeof( int ), 1, fp );
			fwrite( &Models[ i ].SpotFXFlags[ 0 ], sizeof( Models[i].SpotFXFlags ), 1, fp );
			fwrite( &Models[ i ].SpotFXState[ 0 ], sizeof( Models[i].SpotFXState ), 1, fp );
			fwrite( &Models[ i ].SpotFXDelay[ 0 ], sizeof( Models[i].SpotFXDelay ), 1, fp );
			fwrite( &Models[ i ].SpotFXTimeInterval[ 0 ], sizeof( Models[i].SpotFXTimeInterval ), 1, fp );
			fwrite( &Models[ i ].SpotFXTimeCount[ 0 ], sizeof( Models[i].SpotFXTimeCount ), 1, fp );
			fwrite( &Models[ i ].SpotFXSFX_ID[ 0 ], sizeof( Models[i].SpotFXSFX_ID ), 1, fp );
			fwrite( &Models[ i ].ClipGroup, sizeof( u_int16_t ), 1, fp );
			i = Models[ i ].Prev;
		}

		i = FirstModelFree;

		while( i != INDEX_NONE )
		{
			fwrite( &Models[ i ].Next, sizeof( u_int16_t ), 1, fp );
			i = Models[ i ].Next;
		}

	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load Models Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * LoadModels( FILE * fp )
{
	u_int16_t	i;
	int16_t	Count;

	if( fp )
	{
		fread( &FirstModelUsed, sizeof( FirstModelUsed ), 1, fp );
		fread( &FirstModelFree, sizeof( FirstModelFree ), 1, fp );
		
		i = FirstModelUsed;

		while( i != INDEX_NONE )
		{
			fread( &Models[ i ].Next, sizeof( u_int16_t ), 1, fp );
			fread( &Models[ i ].Prev, sizeof( u_int16_t ), 1, fp );
			fread( &Models[ i ].Type, sizeof( int16_t ), 1, fp );
			fread( &Models[ i ].Flags, sizeof( u_int16_t ), 1, fp );
			fread( &Models[ i ].SecWeapon, sizeof( BYTE ), 1, fp );
			fread( &Models[ i ].Visible, sizeof( bool ), 1, fp );
			fread( &Models[ i ].ModelNum, sizeof( u_int16_t ), 1, fp );
			fread( &Models[ i ].Group, sizeof( u_int16_t ), 1, fp );
			fread( &Models[ i ].OwnerType, sizeof( u_int16_t ), 1, fp );
			fread( &Models[ i ].Owner, sizeof( u_int16_t ), 1, fp );
			fread( &Models[ i ].Func, sizeof( int16_t ), 1, fp );
			fread( &Models[ i ].Scale, sizeof( float ), 1, fp );
			fread( &Models[ i ].Xscale, sizeof( float ), 1, fp );
			fread( &Models[ i ].Yscale, sizeof( float ), 1, fp );
			fread( &Models[ i ].Zscale, sizeof( float ), 1, fp );
			fread( &Models[ i ].MaxScale, sizeof( float ), 1, fp );
			fread( &Models[ i ].LifeCount, sizeof( float ), 1, fp );
			fread( &Models[ i ].AxisRot, sizeof( float ), 1, fp );
			fread( &Models[ i ].Speed, sizeof( float ), 1, fp );
			fread( &Models[ i ].TimeInterval, sizeof( float ), 1, fp );
			fread( &Models[ i ].TimeCount, sizeof( float ), 1, fp );
			fread( &Models[ i ].Frame, sizeof( int16_t ), 1, fp );
			fread( &Models[ i ].InterpFrame, sizeof( int16_t ), 1, fp );
			fread( &Models[ i ].InterpTime, sizeof( float ), 1, fp );
			fread( &Models[ i ].Qlerp, sizeof( QUATLERP ), 1, fp );
			Models[ i ].Qlerp.crnt = &Models[ i ].Quat;
			fread( &Models[ i ].Quat, sizeof( QUAT ), 1, fp );
			fread( &Models[ i ].Ship, sizeof( u_int16_t ), 1, fp );
			fread( &Models[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fread( &Models[ i ].OldPos, sizeof( VECTOR ), 1, fp );
			fread( &Models[ i ].Dir, sizeof( VECTOR ), 1, fp );
			fread( &Models[ i ].Rot, sizeof( VECTOR ), 1, fp );
			fread( &Models[ i ].Mat, sizeof( MATRIX ), 1, fp );
			MatrixTranspose( &Models[ i ].Mat, &Models[ i ].InvMat );
			fread( &Models[ i ].Radius, sizeof( float ), 1, fp );
			fread( &Models[ i ].Red, sizeof( int ), 1, fp );
			fread( &Models[ i ].Green, sizeof( int ), 1, fp );
			fread( &Models[ i ].Blue, sizeof( int ), 1, fp );
			fread( &Models[ i ].SpotFXFlags[ 0 ], sizeof( Models[i].SpotFXFlags ), 1, fp );
			fread( &Models[ i ].SpotFXState[ 0 ], sizeof( Models[i].SpotFXState ), 1, fp );
			fread( &Models[ i ].SpotFXDelay[ 0 ], sizeof( Models[i].SpotFXDelay ), 1, fp );
			fread( &Models[ i ].SpotFXTimeInterval[ 0 ], sizeof( Models[i].SpotFXTimeInterval ), 1, fp );
			fread( &Models[ i ].SpotFXTimeCount[ 0 ], sizeof( Models[i].SpotFXTimeCount ), 1, fp );
			fread( &Models[ i ].SpotFXSFX_ID[ 0 ], sizeof( Models[i].SpotFXSFX_ID ), 1, fp );
			fread( &Models[ i ].ClipGroup, sizeof( u_int16_t ), 1, fp );

			ReinitSpotFXSFX( i );

			i = Models[ i ].Prev;
		}

		i = FirstModelFree;

		while( i != INDEX_NONE )
		{
			memset( &Models[ i ], 0, sizeof( MODEL ) );
			Models[i].Func = MODFUNC_Nothing;
			Models[i].LifeCount = -1.0F;
			Models[i].Scale = 1.0F;
			Models[i].Visible = true;
			Models[i].TimeInterval = (float) 1;
			for( Count = 0; Count < 12; Count++ ) Models[i].TempLines[ Count ] = INDEX_NONE;
			Models[i].Prev = INDEX_NONE;

			fread( &Models[ i ].Next, sizeof( u_int16_t ), 1, fp );
			i = Models[ i ].Next;
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Reinitialise Sound FX on Models
	Input		:	u_int16_t		Model Index
	Output		:	bool		True/False
===================================================================*/
bool ReinitSpotFXSFX( u_int16_t i )
{
	int16_t		Count;
	u_int16_t		NumSpotFX = 0;
	PVSPOTFX *	SpotFXPtr = NULL;

	if( ModelNames[ Models[i].ModelNum ].DoIMorph )
	{
		if( MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
			MxaModelHeaders[ Models[i].ModelNum ].SpotFX )
		{
			SpotFXPtr = MxaModelHeaders[ Models[i].ModelNum ].SpotFX;
			NumSpotFX = MxaModelHeaders[ Models[i].ModelNum ].NumSpotFX;
		}
	}
	else
	{
		if( ModelHeaders[ Models[i].ModelNum ].NumSpotFX &&
			ModelHeaders[ Models[i].ModelNum ].SpotFX )
		{
			SpotFXPtr = ModelHeaders[ Models[i].ModelNum ].SpotFX;
			NumSpotFX = ModelHeaders[ Models[i].ModelNum ].NumSpotFX;
		}
	}

	if( SpotFXPtr && NumSpotFX )
	{
		for( Count = 0; Count < NumSpotFX; Count++ )
		{
			switch( SpotFXPtr->Type )
			{
				case SPOTFX_SoundFX:
					if( SpotFXPtr->SoundFX != -1 )
					{
						if( Models[i].SpotFXSFX_ID[ Count ] )
						{
							RestoreSfxData( Models[i].SpotFXSFX_ID[ Count ], &Models[i].Pos, &Models[i].Group );
						}
					}
					break;
				default:
					break;
			}
			SpotFXPtr++;
		}
	}
	return( true );
}

/*===================================================================
	Procedure	:	Set Ship Display/Not depending on camera
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void SetShipsVisibleFlag( void )
{
	int16_t	Ship;
	u_int16_t	Model;
	bool	ShipVisible;

	for( Ship = 0; Ship < MAX_PLAYERS; Ship++ )
	{
		ShipVisible = false;

		if( ( Ships[ Ship ].enable ) &&
			( Ships[ Ship ].Object.Mode != GAMEOVER_MODE ) &&
			( Ships[ Ship ].Object.Mode != LIMBO_MODE ) &&
			( (GameStatus[ Ship ] == STATUS_Normal ) || (GameStatus[ Ship ] == STATUS_SinglePlayer ) ) )
		{
			if( Ship != WhoIAm )
			{
				if( PlayDemo && ( Current_Camera_View == Ship ) )
				{
					ShipVisible = false;
				}
				else
				{
					ShipVisible = true;
				}
			}
			else
			{
				if( ( CameraRendering != CamRendering::Main ) &&
					( CameraRendering != CamRendering::Rear ) )
				{
					ShipVisible = true;
				}
			}
		}
			
		if( BikeCompFiles[ ( Ships[ Ship ].BikeNum % MAXBIKETYPES ) ].Filename[ 0 ] )
		{
			if( Ships[ Ship ].Object.Components )
			{
				SetCompObjModelsState( Ships[ Ship ].Object.Components, 1, ShipVisible );
			}
		}
		else
		{
			if( Ships[ Ship ].ModelNum != INDEX_NONE )
			{
				Model = Ships[ Ship ].ModelNum;
				Models[ Model ].Visible = ShipVisible;
			}
		}
	}
}

/*===================================================================
	Procedure	:	Enable and Disable Models dependant on
				:	Level/Game mode
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void EnableRelavantModels( MODELNAME * ModelNamesPtr )
{
	if( !ModelNamesPtr ) return;
	if( ModelNamesPtr != &ModelNames[0] ) return;

	if( ( ChangeLevel_MyGameStatus == STATUS_SinglePlayer ) ||
		( ChangeLevel_MyGameStatus == STATUS_PostStartingSinglePlayer ) ||
		( ChangeLevel_MyGameStatus == STATUS_TitleLoadGamePostStartingSinglePlayer) )
	{
		ModelNamesPtr[ MODEL_Tracker ].LoadEnable = false;
		ModelNamesPtr[ MODEL_Ping ].LoadEnable = false;
		ModelNamesPtr[ MODEL_Flag ].LoadEnable = false;
		ModelNamesPtr[ MODEL_RedFlag ].LoadEnable = false;
		ModelNamesPtr[ MODEL_GreenFlag ].LoadEnable = false;
		ModelNamesPtr[ MODEL_BlueFlag ].LoadEnable = false;
		ModelNamesPtr[ MODEL_YellowFlag ].LoadEnable = false;
	}
	else
	{
		ModelNamesPtr[ MODEL_Tracker ].LoadEnable = false;
		ModelNamesPtr[ MODEL_Ping ].LoadEnable = false;

		if( CaptureTheFlag ) ModelNamesPtr[ MODEL_Flag ].LoadEnable = true;
		else ModelNamesPtr[ MODEL_Flag ].LoadEnable = false;

		if( CTF )
		{
			ModelNamesPtr[ MODEL_RedFlag ].LoadEnable = true;
			ModelNamesPtr[ MODEL_GreenFlag ].LoadEnable = true;
			ModelNamesPtr[ MODEL_BlueFlag ].LoadEnable = true;
			ModelNamesPtr[ MODEL_YellowFlag ].LoadEnable = true;
		}
		else
		{
			ModelNamesPtr[ MODEL_RedFlag ].LoadEnable = false;
			ModelNamesPtr[ MODEL_GreenFlag ].LoadEnable = false;
			ModelNamesPtr[ MODEL_BlueFlag ].LoadEnable = false;
			ModelNamesPtr[ MODEL_YellowFlag ].LoadEnable = false;
		}
	}
}

void CheckModelLinkList( void )
{
	u_int16_t i;
	u_int16_t Count;


	
	i = 0;
	Count = FirstModelFree;
	while( ( Count != INDEX_NONE ) && i < MAXNUMOFMODELS*2 )
	{
		Count = Models[Count].Next;
		i++;
	}
	if( i == MAXNUMOFMODELS*2 )
	{
		// oh shit
        DebugPrintf( "Model Free link list Gone up its ass\n" );
		i = 0;
	}

	i = 0;

	Count = FirstModelUsed;
	while( ( Count != INDEX_NONE ) && i < MAXNUMOFMODELS*2 )
	{
		if( Models[Count].Prev == Models[Count].Next )
		{
			i = MAXNUMOFMODELS*2;
			break;
		}
		Count = Models[Count].Prev;
		i++;
	}
	if( i == MAXNUMOFMODELS*2 )
	{
		// oh shit
        DebugPrintf( "Model Used link list Gone up its ass\n" );
		i = 0;
	}
}

/*===================================================================
	Procedure	:	Check if bike engine enabled and is ship if so then
				:	return true else return false
	Output		:	Nothing
===================================================================*/
bool EngineEnabled( u_int16_t OwnerType, u_int16_t Owner )
{
	if( BikeEnginesOn ) return( true );
	if( ( OwnerType != OWNER_SHIP ) || ( Owner >= MAX_PLAYERS ) ) return( true );
	return( false );
}

/*===================================================================
		True EnviroMent Mapping for an Mxaloadheader...
===================================================================*/
bool	ENVMxa( MXALOADHEADER * Mxloadheader , MATRIX * Mat ,VECTOR * /*Pos*/)
{
	VECTOR Temp;
	u_int16_t group;
	u_int16_t vert;
	u_int16_t execbuf;
	LVERTEX	*lpLVERTEX = NULL;
	float	u,v;

	for( group = 0 ; group < Mxloadheader->num_groups ; group ++ )
	{
		for( execbuf = 0 ; execbuf < Mxloadheader->Group[group].num_execbufs ; execbuf++ )
		{
			if( (Mxloadheader->Group[group].exec_type[execbuf]&HASENVIROMENTMAP) != 0 )
			{
				/*	lock the vertex buffer	*/

				if (!(FSLockVertexBuffer(&Mxloadheader->Group[group].renderObject[execbuf], &lpLVERTEX)))
				{
					return false;
				}

				for( vert = 0 ; vert < 	Mxloadheader->Group[group].num_verts_per_execbuf[execbuf] ; vert ++ )
				{
					ApplyMatrix( Mat , (VECTOR *) lpLVERTEX,  &Temp);

					NormaliseVector( &Temp );

					u = (0.25f + (-0.25f * Temp.x) );
					v = (0.5f + (-0.5f * Temp.y) );

					lpLVERTEX->tu = u;//+(Pos->x*0.001F)+(Pos->z*0.001F);
					lpLVERTEX->tv = v;//+(Pos->y*0.001F);
					lpLVERTEX++;
				}
				/*	unlock the vertex buffer	*/
				if (!(FSUnlockVertexBuffer(&Mxloadheader->Group[group].renderObject[execbuf])))
				{
					return false;
				}
			}
		}
	}
	return true;
}
