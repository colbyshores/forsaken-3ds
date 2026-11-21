#pragma once

/* screenpolys.h -- ScrPolys: screen-space HUD quads, countdown, lens flare
 *
 * ScrPolys[] is a fixed pool of MAXNUMOFSCRPOLYS pre-transformed 2D quads:
 * HUD text (AddScreenPolyText), the countdown digits, lens flares, screen
 * flashes, the thermo bar, and flag / bounty markers over ships. InitScrPolys
 * resets the whole pool; it runs from InitScene and, on 3DS, again when
 * single-player gameplay starts and at the last loading-screen redraw, so
 * polys left over from the title screen are dropped. FindFreeScrPoly /
 * KillUsedScrPoly move an entry between the free list and the used list
 * (FirstScrPolyUsed, walked through Prev); KillUsedScrPoly also unlinks the
 * texture page and resets the entry to defaults. SCRTYPE_LastAFrame polys are
 * killed by the next ScreenPolyProcess, so callers re-add them every frame;
 * SCRTYPE_Normal polys persist until killed and are animated by their
 * ScrSeq sequence in ScreenPolyProcess.
 *
 * A quad is placed at Pos plus its FRAME_INFO bit offsets times Xscale /
 * Yscale (SCRFLAG_Scale also scales by viewport width / 320), or at x1,y1 -
 * x3,y3 with SCRFLAG_UseCoords. Polys with no Frm_Info sit on the extra
 * untextured page (MAXTPAGESPERTLOAD), which these fills do draw. Every quad
 * is clipped to CurrentCamera.Viewport by ClipBox. DisplaySolidScrPolys and
 * DisplayNonSolidScrPolys draw the SCRFLAG_Solid and other polys separately,
 * in batches of at most MAXSCREENPOLYVERTS verts with one texture group per
 * quad, via draw_2d_object with z-buffer and culling off. On 3DS a full
 * textureGroups array is flushed mid-fill (see the comments in the fills).
 *
 * 3DS HUD routing: while ScrPolySetRouteBottom(true) is in effect, polys
 * added or reset get SCRFLAG_BottomHUD; ScrPolySetDisplayMode(1 / 2) then
 * limits a draw to the top-screen or bottom-screen subset (0 draws all).
 *
 * SaveScreenPolys / LoadScreenPolys store only the countdown and thermo
 * state; the pool itself is rebuilt by its owners.
 */

/*===================================================================
	Includes
===================================================================*/
#include <math.h>
#include "game/main.h"

#include "math/new3d.h"
#include "game/ships.h"
#include "data/mload.h"
#include "world/collision.h"
#include "rendering/lights.h"
#include "rendering/2dtextures.h"
#include "data/tload.h"
#include "util/timer.h"

/*===================================================================
	Defines
===================================================================*/
constexpr int MAXNUMOFSCRPOLYS = 1000;
constexpr int MAXSCREENPOLYVERTS = 800;
constexpr auto TIMERSTARTSCREENX = ( 320.0F - 64.0F );
constexpr auto TIMERSTARTSCREENY = ( 240.0F - 36.0F );
constexpr float TIMERSTARTSCALE = ( 0.3F );
constexpr float TIMERENDSCREENX = ( 160.0F );
constexpr float TIMERENDSCREENY = ( 120.0F );
constexpr float TIMERENDSCALE = ( 1.0F );

/*===================================================================
	Types
===================================================================*/
constexpr int SCRTYPE_Normal = 0;
constexpr int SCRTYPE_LastAFrame = 1;

/*===================================================================
	Flags
===================================================================*/
constexpr int SCRFLAG_Nothing = 0;
constexpr int SCRFLAG_Scale = 1;
constexpr int SCRFLAG_UseCoords = 2;
constexpr int SCRFLAG_Solid = 8;
constexpr int SCRFLAG_Rot90 = 16;
constexpr int SCRFLAG_BottomHUD = 32; /* [3DS] route this poly to the mono bottom screen
									  instead of the stereo top screen */

/*===================================================================
	Process Sequences
===================================================================*/
enum class ScrSeq : int16_t
{
	Nothing,
	Anim,
	Fade,
	Thermo,
	Logo,
	FadeUp,
};

/*===================================================================
	Structures
===================================================================*/
struct SCRPOLY_RGB {

	u_int8_t	R;
	u_int8_t	G;
	u_int8_t	B;
	u_int8_t	Trans;

};

struct SCRPOLY {

	u_int16_t			Next;
	u_int16_t			Prev;

	u_int16_t			NextInTPage;
	u_int16_t			PrevInTPage;

	u_int16_t			Type;				// Type
	u_int16_t			Flags;				// Flags
	VECTOR			Pos;				// Position
	u_int8_t			R;					// Colour
	u_int8_t			G;					// Colour
	u_int8_t			B;					// Colour
	u_int8_t			Trans;				// Amount of transparency
	float			Xscale;				// XScale
	float			Yscale;				// YScale
	float			Frame;				// Animation Frame
	float			FrameRate;			// FrameRate
	FRAME_INFO	**	Frm_Info;			// Offset Info
	ScrSeq			SeqNum;				// process sequence
	float			LifeCount;			// LifeCount
	float			FadeRed;			// Red
	float			FadeGreen;			// Green
	float			FadeBlue;			// Blue
	float			FadeTrans;			// Trans
	float			x1;					// x1
	float			y1;					// y1
	float			x2;					// x2
	float			y2;					// y2
	float			x3;					// x3
	float			y3;					// y3
	float			x4;					// x4
	float			y4;					// y4
	SCRPOLY_RGB		Col1;				// Colour 1
	SCRPOLY_RGB		Col2;				// Colour 2
	SCRPOLY_RGB		Col3;				// Colour 3
	SCRPOLY_RGB		Col4;				// Colour 4

};

/*===================================================================
	Protptypes
===================================================================*/
void InitScrPolys( void );
void KillUsedScrPoly( u_int16_t i );
u_int16_t FindFreeScrPoly( void );

/* [3DS bottom-screen HUD routing] */
void ScrPolySetRouteBottom(bool b);
void ScrPolySetDisplayMode(int mode);
bool ScrPolyGetRouteBottom(void);
void ScreenPolyProcess( void );
void DoLensflareEffect( void );
void Conv3DTo2D( VECTOR * SrcVert, VECTOR * DstVert, MATRIX * FinalMat );
void DoAllSecBullLensflare( void );
void SecBullLensflare( u_int16_t i );
void CreateCountdownDigits( void );
void DeleteCountdownDigits( void );
void CreateDigit( u_int16_t * DigitArray, float XPos, float YPos );
void UpdateCountdownDigits( void );
void UpdateDigit( u_int16_t * DigitArray, float XPos, float YPos, int16_t Number, float Scale, float Col ,FRAME_INFO	** Graphics );
void CreateSeperatorDigit( u_int16_t * DigitArray, float XPos, float YPos );
void DeleteDigit( u_int16_t * DigitArray );
void DeleteSeperatorDigit( u_int16_t * DigitArray );
void StartCountDown( int16_t Minutes, int16_t Seconds );
void ClearCountdownBuffers( void );
void AddScreenPolyText( u_int16_t Frame, float XPos, float YPos, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );
void AddScreenPolyTextScale( u_int16_t Frame, float XPos, float YPos, float XScale, float YScale, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );
void ShowScreenMultiples( void );
void MakeScreenFlash( u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans, u_int16_t *ScreenPolyStore, ScrSeq SeqNum );
void InitThermo( void );
void KillThermo( void );
void ResetCountDownBombTag( float Amount );
void InitScrPolyTPages( void );
void AddScrPolyToTPage( u_int16_t i, int16_t TPage );
void RemoveScrPolyFromTPage( u_int16_t i, int16_t TPage );
bool DisplaySolidScrPolys( RENDEROBJECT *renderObject );
bool DisplayNonSolidScrPolys( RENDEROBJECT *renderObject );
bool ScrPolyDispSolid( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextScrPoly );
bool ScrPolyDispNonSolid( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextScrPoly );
void FreeAllLastAFrameScrPolys( void );
void LoadTimeForLevel( void );
FILE * SaveScreenPolys( FILE * fp );
FILE * LoadScreenPolys( FILE * fp );
void ShowFlagOnShip( u_int16_t Ship );
void ShowFlag( u_int16_t Ship );
void ShowBountyOnShip( u_int16_t Ship );
void ShowBounty( u_int16_t Ship );
void ShowCTFFlagsOnShip( u_int16_t Ship );
void ShowCTFFlags( u_int16_t Ship );

/* Defined in screenpolys.cpp; shared with the modules that use it. */
extern u_int32_t TotalScrPolysInUse;
extern u_int16_t FirstScrPolyUsed;
extern SCRPOLY ScrPolys[ MAXNUMOFSCRPOLYS ];
extern u_int16_t FlashScreenPoly;

/* The countdown clock (one instance, Countdown, defined in screenpolys.cpp):
 * whether this game runs against it, the time left (in hundredths of a
 * second) and the timer that measures each frame's share of it, whether a
 * player who joined mid-game still waits for the host's time, and whether
 * running out of time (or the thermometer) has already killed the local
 * player; then its digits and their animation: the ScrPolys each digit's
 * segments use, the font, and the position, scale and fade as the clock
 * slides from the corner to the centre in the last ten seconds. Switched on
 * and off by the game set-up and the time-limit trigger, reset by
 * StartCountDown and ResetCountDownBombTag, moved each frame by
 * UpdateCountdownDigits while on, its digits made and freed with the scene,
 * and stored by the saved game. */
class CountdownClock {
	public:
	/* Always inlined and defined in screenpolys.cpp, so the per-frame
	 * UpdateCountdownDigits keeps its body rather than becoming a tail call.
	 * Other files call UpdateCountdownDigits. */
	[[gnu::always_inline]] inline void update();
	void create_digits();
	void delete_digits();
	void clear_buffers();
	void start( int16_t Minutes, int16_t Seconds );
	void reset_bomb_tag( float Amount );
	void load_for_level();
	void save( FILE * fp );
	void restore( FILE * fp );

	// the game loop and level set-up: does this game run against the clock?
	bool on() const { return CountDownOn; }
	// game set-up: this game is, or is not, played against the clock
	void switch_on() { CountDownOn = true; }
	void switch_off() { CountDownOn = false; }
	// the time-limit trigger and a restored game: the clock runs, and running out may kill the player again
	void rearm() { CountDownOn = true; KilledPlayer = false; }
	// the level's single-player time limit, in minutes
	int level_time_limit() const { return SinglePlayerTimeLimit; }
	// a player who joined mid-game waits for the host's time; the others keep their own
	void await_host_time() { IllegalTime = true; }
	void trust_own_time() { IllegalTime = false; }
	// MSG_SETTIME: take the host's time if still waiting for it
	void take_host_time( float Time ) { if( IllegalTime ) { Countdown_Float = Time; IllegalTime = false; } }
	// MSG_REQTIME: the time left, for the host to send
	float time_left() const { return Countdown_Float; }
	// after a saved game loads: the load's time does not count against the clock
	void restart_timer() { timer_clear( &countdown_timer ); }
	// the thermometer: has the player already been killed by heat or time?
	bool player_killed() const { return KilledPlayer; }
	void note_player_killed() { KilledPlayer = true; }

	private:
	float     Countdown_Float = 3000.0F;	// 30 Seconds
	px_timer_t countdown_timer = {};
	bool      CountDownOn = false;
	bool      IllegalTime = false;
	bool      KilledPlayer = false;
	int       SinglePlayerTimeLimit = 0;
	int16_t   CurrentCountDownFont = 1;
	bool      Toggle = false;
	float     ScaleInc = 0.0F;
	float     Interp_Time = 0.0F;
	float     CountDown_X = TIMERSTARTSCREENX;
	float     CountDown_Y = TIMERSTARTSCREENY;
	float     CountDown_Xoff = 0.0F;
	float     CountDown_Yoff = 0.0F;
	float     CountDown_Wanted_X = TIMERSTARTSCREENX;
	float     CountDown_Wanted_Y = TIMERSTARTSCREENY;
	float     CountDown_Scale = TIMERSTARTSCALE;
	float     CountDown_Wanted_Scale = TIMERSTARTSCALE;
	float     CountDown_Col = 1.0F;
	float     Time_Diff = 0.0F;
	u_int16_t Min1Digit[ 7 ] = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
	u_int16_t Min0Digit[ 7 ] = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
	u_int16_t Bit0Digit[ 1 ] = { 0xffff };
	u_int16_t Sec1Digit[ 7 ] = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
	u_int16_t Sec0Digit[ 7 ] = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
	u_int16_t Bit1Digit[ 1 ] = { 0xffff };
	u_int16_t Hun1Digit[ 7 ] = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
	u_int16_t Hun0Digit[ 7 ] = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
};

extern CountdownClock Countdown;
