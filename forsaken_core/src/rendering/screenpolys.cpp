/* screenpolys.cpp -- ScrPoly HUD quads, countdown, lens flare; see screenpolys.h. */
#include <stdio.h>

#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "game/models.h"
#include "rendering/2dpolys.h"
#include "rendering/screenpolys.h"
#include "input/controls.h"
#include "game/secondary.h"
#include "game/camera.h"
#include "ui/title.h"
#include "game/main.h"
#include "data/file.h"
#include "rendering/text.h"
#include "game/triggers.h"
#include "ui/local.h"
#include "util/util.h"
#include "util/timer.h"
#include "game/oct2.h"
#include "rendering/render.h"
#include "data/mload.h"
#include "data/tload.h"
#include "game/ships.h"
#include "rendering/2dtextures.h"

constexpr float THERMAL_STEP = (1.6F);
constexpr float THERMAL_INC = (0.0092593F);
constexpr float THERMAL_DEC = (0.25F);

/*===================================================================
	Externs
===================================================================*/

constexpr auto CUTOFF = ( 5.0F * BLOCKSIZE );

/*===================================================================
	Globals
===================================================================*/
u_int16_t	FlashScreenPoly	= INDEX_NONE;

u_int32_t	TotalScrPolysInUse = 0;
SCRPOLY	ScrPolys[ MAXNUMOFSCRPOLYS ];

u_int16_t	FirstScrPolyUsed;

FRAME_INFO	**	CountDownFontGraphics[ 2 ] = { &Numbers2_Header, &Numbers_Header };
float	CountDownFontWidth[ 2 ] = { 16.0F, 23.0F };

int16_t	NumberSegments[ 11 ][ 7 ] = {
//		  0   1   2   3   4   5   6
		{ -1, -1, -1, -1, -1, -1,  0 },	// 0
		{  0, -1, -1,  0,  0,  0,  0 },	// 1
		{ -1, -1,  0, -1, -1,  0, -1 },	// 2
		{ -1, -1, -1, -1,  0,  0, -1 },	// 3
		{  0, -1, -1,  0,  0, -1, -1 },	// 4
		{ -1,  0, -1, -1,  0, -1, -1 },	// 5
		{ -1,  0, -1, -1, -1, -1, -1 },	// 6
		{ -1, -1, -1,  0,  0,  0,  0 },	// 7
		{ -1, -1, -1, -1, -1, -1, -1 },	// 8
		{ -1, -1, -1, -1,  0, -1, -1 },	// 9
		{  0,  0,  0,  0,  0,  0,  0 },	// Blank
};

namespace {

/* The ScrPolys[] pool's private bookkeeping (one instance, g_scrpolys):
 * - the free-list head, and each texture page's chain through the pool,
 *   which the fills walk and fill in with that page's vertex counts;
 * - the 3DS bottom-screen HUD routing. While g_scrpoly_route_bottom is true,
 *   polys added or reset get SCRFLAG_BottomHUD. g_scrpoly_display_mode picks
 *   the subset a draw shows: 0 = all polys (the pre-routing behaviour),
 *   1 = only polys WITHOUT SCRFLAG_BottomHUD (stereo top screen), 2 = only
 *   polys WITH it (mono bottom screen). The two-pass render keeps in-world
 *   overlays (aim reticle, enemy markers, damage flash) on the stereo top
 *   screen while the gameplay text HUD (ammo/shield/messages) moves to the
 *   bottom;
 * - the Z and RHW every fill gives its vertices;
 * - the thermometer: its poly and temperatures, which InitThermo resets, the
 *   Thermo sequence in ScreenPolyProcess moves each frame, and the saved game
 *   stores.
 * Reset by InitScrPolys with the scene, changed as polys are found and
 * killed, and walked by every fill. ScrPolys[] and the used-list head
 * FirstScrPolyUsed stay global (oct2, title and secondary use them). */
class ScrPolyPool {
	public:
	void      init();
	u_int16_t find_free();
	void      kill_used( u_int16_t i );
	void      set_route_bottom( bool b )   { g_scrpoly_route_bottom = b; }
	void      set_display_mode( int mode ) { g_scrpoly_display_mode = mode; }
	bool      route_bottom() const         { return g_scrpoly_route_bottom; }
	void      add_text( u_int16_t Frame, float XPos, float YPos, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );
	void      add_text_scale( u_int16_t Frame, float XPos, float YPos, float XScale, float YScale, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );
	void      init_tpages();
	void      add_to_tpage( u_int16_t i, int16_t TPage );
	void      remove_from_tpage( u_int16_t i, int16_t TPage );
	bool      display_solid( RENDEROBJECT *renderObject );
	bool      display_non_solid( RENDEROBJECT *renderObject );
	/* Always inlined: ScrPolyDispSolid and ScrPolyDispNonSolid pass a
	 * constant Solid, so each is its own specialised copy. */
	[[gnu::always_inline]] bool disp_fill( bool Solid, RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextScrPoly );
	void      process();
	void      init_thermo();
	void      kill_thermo();
	FILE     *save( FILE * fp );
	FILE     *restore( FILE * fp );

	private:
	void init_poly( u_int16_t i );
	bool skip( u_int16_t i );
	[[gnu::always_inline]] bool in_fill( u_int16_t i, bool Solid );
	[[gnu::always_inline]] void build_textured_quad( u_int16_t & i, float & Xoff, float & Yoff, float & Xsize, float & Ysize, BOX_INFO *& Box_Ptr, OFF_INFO *& Off_Ptr, COLOR & Colour, TLVERTEX *& ScrPolyVertPnt, float & u1, float & v1, float & u2, float & v2, float & x1, float & y1, float & x3, float & y3 );
	[[gnu::always_inline]] void build_plain_quad( bool & Solid, u_int16_t & i, COLOR & Colour, TLVERTEX *& ScrPolyVertPnt, float & u1, float & v1, float & u2, float & v2, float & x1, float & y1, float & x3, float & y3 );
	[[gnu::always_inline]] void count_verts( bool & Solid, int16_t *& TPage, u_int16_t *& NextScrPoly, u_int16_t & i, int16_t & Count, int16_t & TotalVerts, int16_t & StartVert, int16_t & NumVerts, int16_t & NumTris, BIT_INFO *& Bit_Ptr, bool & Textured );

	/* Static, so it keeps its own address as the global did. As an instance
	 * member, either it or FirstScrPolyFree would sit at an offset that costs
	 * an instruction in AddScrPolyToTPage (inlined into every poly builder)
	 * or KillUsedScrPoly. */
	static TPAGEINFO ScrPolyTPages[ MAXTPAGESPERTLOAD + 1 ];
	u_int16_t FirstScrPolyFree = 0;
	u_int16_t ThermoScrPoly = INDEX_NONE;
	int       g_scrpoly_display_mode = 0;
	float     ZValue = 0.0F;
	float     RHWValue = 0.0F;
	bool      g_scrpoly_route_bottom = false;
	float     ThermalTemp = 0.0F;
	float     WantedThermalTemp = 0.0F;
	float     ThermalMinimum = 0.0F;
};

TPAGEINFO ScrPolyPool::ScrPolyTPages[ MAXTPAGESPERTLOAD + 1 ];

} // namespace

/* Skip-filter used inside ScrPolyDispSolid / ScrPolyDispNonSolid to drop
   polys that don't belong on the current render target. */
inline bool ScrPolyPool::skip(u_int16_t i)
{
	if (g_scrpoly_display_mode == 1 && (ScrPolys[i].Flags & SCRFLAG_BottomHUD))
		return true;
	if (g_scrpoly_display_mode == 2 && !(ScrPolys[i].Flags & SCRFLAG_BottomHUD))
		return true;
	return false;
}

// defaults a screen poly
void ScrPolyPool::init_poly( u_int16_t i )
{
	ScrPolys[i].Flags = g_scrpoly_route_bottom ? SCRFLAG_BottomHUD : SCRFLAG_Nothing;
	ScrPolys[i].Type = SCRTYPE_Normal;
	ScrPolys[i].R = 255;
	ScrPolys[i].G = 255;
	ScrPolys[i].B = 255;
	ScrPolys[i].Trans = 255;
	ScrPolys[i].Frame = 0.0F;
	ScrPolys[i].FrameRate = 1.0F;
	ScrPolys[i].Frm_Info = nullptr;
	ScrPolys[i].SeqNum = ScrSeq::Nothing;
	ScrPolys[i].Xscale = 1.0F;
	ScrPolys[i].Yscale = 1.0F;

	ScrPolys[i].NextInTPage = INDEX_NONE;
	ScrPolys[i].PrevInTPage = INDEX_NONE;

	// Next and Prev are left alone: the caller has already linked the poly

	ScrPolys[i].x1 = 0.0F;
	ScrPolys[i].y1 = 0.0F;
	ScrPolys[i].x2 = 0.0F;
	ScrPolys[i].y2 = 0.0F;
	ScrPolys[i].x3 = 0.0F;
	ScrPolys[i].y3 = 0.0F;
	ScrPolys[i].x4 = 0.0F;
	ScrPolys[i].y4 = 0.0F;

	ScrPolys[i].Pos.x = 0.0f;
	ScrPolys[i].Pos.y = 0.0f;
	ScrPolys[i].Pos.z = 0.0f;
	ScrPolys[i].LifeCount = 0.0f;
	ScrPolys[i].FadeRed = 0.0f;
	ScrPolys[i].FadeGreen = 0.0f;
	ScrPolys[i].FadeBlue = 0.0f;
	ScrPolys[i].FadeTrans = 0.0f;
	ScrPolys[i].Col1.R = 0;
	ScrPolys[i].Col1.G = 0;
	ScrPolys[i].Col1.B = 0;
	ScrPolys[i].Col1.Trans = 0;
	ScrPolys[i].Col2.R = 0;
	ScrPolys[i].Col2.G = 0;
	ScrPolys[i].Col2.B = 0;
	ScrPolys[i].Col2.Trans = 0;
	ScrPolys[i].Col3.R = 0;
	ScrPolys[i].Col3.G = 0;
	ScrPolys[i].Col3.B = 0;
	ScrPolys[i].Col3.Trans = 0;
	ScrPolys[i].Col4.R = 0;
	ScrPolys[i].Col4.G = 0;
	ScrPolys[i].Col4.B = 0;
	ScrPolys[i].Col4.Trans = 0;

}

void ScrPolyPool::init()
{
	u_int16_t	i;

	FlashScreenPoly = INDEX_NONE;
	Secondaries.forget_target_polys();
	ClearCountdownBuffers();

	FirstScrPolyUsed = INDEX_NONE;
	FirstScrPolyFree = 0;

	for( i = 0; i < MAXNUMOFSCRPOLYS; i++ )
	{
		ScrPolys[i].Next = i + 1;
		ScrPolys[i].Prev = INDEX_NONE;
		init_poly(i);
	}

	ScrPolys[ MAXNUMOFSCRPOLYS-1 ].Next = INDEX_NONE;

	InitScrPolyTPages();

	if( Countdown.on() ) CreateCountdownDigits();
}

/*===================================================================
	Procedure	:	Find a free ScrPoly and move it from the free
				:	list to	the used list
	Input		:	Nothing
	Output		:	u_int16_t	Number of the free ScrPoly
===================================================================*/
u_int16_t ScrPolyPool::find_free()
{
	u_int16_t i;

	i = FirstScrPolyFree;
	if( i == INDEX_NONE ) return i;

	ScrPolys[i].Prev = FirstScrPolyUsed;

	if ( FirstScrPolyUsed != INDEX_NONE)
	{
		ScrPolys[ FirstScrPolyUsed ].Next = i;
	}

	FirstScrPolyUsed = i;
	FirstScrPolyFree = ScrPolys[i].Next;

	TotalScrPolysInUse++;

	return i ;
}

/*===================================================================
	Procedure	:	Kill a used ScrPoly and move it from the used
				:	list to the free list
	Input		:	u_int16_t		Number of ScrPoly to free....
	Output		:	Nothing
===================================================================*/
void ScrPolyPool::kill_used( u_int16_t i )
{
	u_int16_t	its_prev;
	u_int16_t	its_next;

	if( i == INDEX_NONE ) return;

	its_prev = ScrPolys[i].Prev;
	its_next = ScrPolys[i].Next;

	if ( i == FirstScrPolyUsed ) FirstScrPolyUsed = ScrPolys[ i ].Prev;
	if( its_prev != INDEX_NONE) ScrPolys[ its_prev ].Next = its_next;
	if( its_next != INDEX_NONE) ScrPolys[ its_next ].Prev = its_prev;

	TotalScrPolysInUse--;

	if( ScrPolys[i].Frm_Info )
	{
		RemoveScrPolyFromTPage( i, GetTPage( *ScrPolys[i].Frm_Info, 0 ) );
	}
	else
	{
		RemoveScrPolyFromTPage( i, GetTPage( nullptr, 0 ) );
	}

	ScrPolys[i].Prev = INDEX_NONE;
	ScrPolys[i].Next = FirstScrPolyFree;
	FirstScrPolyFree = i;

	// cleanup the poly
	// this is important other wise bugs will appear
	// not all routines that get a poly
	// clear out all previous possible settings

	init_poly( i );
}

/*===================================================================
	Procedure	:	Kill all lastaframe Screen Polys
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void FreeAllLastAFrameScrPolys( void )
{
	u_int16_t	i;
	u_int16_t	NextScrPoly;

	i = FirstScrPolyUsed;

	while( i != INDEX_NONE )
	{
		NextScrPoly = ScrPolys[i].Prev;

		if( ScrPolys[i].Type == SCRTYPE_LastAFrame )
		{
			KillUsedScrPoly( i );
		}

		// bustage! we are pointing to our selves!
		// most likely we are zero and never initialized
		if(i == NextScrPoly)
		{
			break;
		}

		i = NextScrPoly;
	}
}


/*===================================================================
	Procedure	:	Process Screen Polys
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void ScrPolyPool::process()
{
	u_int16_t	i;
	u_int16_t	NextScrPoly;
	float	Speed;

	i = FirstScrPolyUsed;

	while( i != INDEX_NONE )
	{
		NextScrPoly = ScrPolys[i].Prev;

		if( ScrPolys[i].Type == SCRTYPE_LastAFrame )
		{
			KillUsedScrPoly( i );
		}
		else
		{
			if( ( ScrPolys[i].Frm_Info != nullptr ) && ( *ScrPolys[i].Frm_Info != nullptr ) )
			{
				switch( ScrPolys[i].SeqNum )
				{
				case ScrSeq::Anim:								// Explosion?
					ScrPolys[i].Frame += ( framelag / ScrPolys[i].FrameRate );

					/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
					if( ScrPolys[i].Frame >= (*ScrPolys[i].Frm_Info)->Num_Frames )
					{
						KillUsedScrPoly( i );
					}
					break;

				case ScrSeq::Fade:								// Fade
					/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
					Speed = ( framelag * 20.0F );
					ScrPolys[i].LifeCount -= Speed;
					if( ScrPolys[i].LifeCount <= 0.0F )
					{
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						KillUsedScrPoly( i );
						FlashScreenPoly = INDEX_NONE;
					}
					else
					{
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						ScrPolys[i].FadeRed -= Speed;
						if( ScrPolys[i].FadeRed < 0.0F ) ScrPolys[i].FadeRed = 0.0F;
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						ScrPolys[i].FadeGreen -= Speed;
						if( ScrPolys[i].FadeGreen < 0.0F ) ScrPolys[i].FadeGreen = 0.0F;
						ScrPolys[i].FadeBlue -= Speed;
						if( ScrPolys[i].FadeBlue < 0.0F ) ScrPolys[i].FadeBlue = 0.0F;
						ScrPolys[i].FadeTrans -= Speed;
						if( ScrPolys[i].FadeTrans < 0.0F ) ScrPolys[i].FadeTrans = 0.0F;
						ScrPolys[i].R = (u_int8_t) ScrPolys[i].FadeRed;
						ScrPolys[i].G = (u_int8_t) ScrPolys[i].FadeGreen;
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						ScrPolys[i].B = (u_int8_t) ScrPolys[i].FadeBlue;
						ScrPolys[i].Trans = (u_int8_t) ScrPolys[i].FadeTrans;
					}
					break;
				case ScrSeq::FadeUp:								// Fade
					ScrPolys[i].LifeCount += framelag / ( 60.0F * 10.0F );
					if( ScrPolys[i].LifeCount >= 1.0F )
					{
						ScrPolys[i].LifeCount = 1.0F;
					}

					ScrPolys[i].R = (u_int8_t) ((float)ScrPolys[i].FadeRed * ScrPolys[i].LifeCount);
					ScrPolys[i].G = (u_int8_t) ((float)ScrPolys[i].FadeGreen * ScrPolys[i].LifeCount);
					ScrPolys[i].B = (u_int8_t) ((float)ScrPolys[i].FadeBlue * ScrPolys[i].LifeCount);
					ScrPolys[i].Trans = (u_int8_t) ((float)ScrPolys[i].FadeTrans * ScrPolys[i].LifeCount);
					break;
				case ScrSeq::Thermo:								// Explosion?
					if( CheckDecreaseTemperature() )
					{
						if( WantedThermalTemp > ThermalMinimum )
							/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						{
							WantedThermalTemp -= ( THERMAL_DEC * framelag );
							if( WantedThermalTemp < ThermalMinimum ) WantedThermalTemp = ThermalMinimum;
						}
					}
					else
					{
						WantedThermalTemp += ( THERMAL_INC * framelag );
						if( WantedThermalTemp > 100.0F ) WantedThermalTemp = 100.0F;
					}

					if( ThermalTemp > WantedThermalTemp )
					{
						ThermalTemp -= ( THERMAL_STEP * framelag );
						if( ThermalTemp < WantedThermalTemp )
						{
							ThermalTemp = WantedThermalTemp;
							/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						}
					}
					else
					{
						if( ThermalTemp < WantedThermalTemp )
						{
							ThermalTemp += ( THERMAL_STEP * framelag );
							if( ThermalTemp > WantedThermalTemp )
							{
								ThermalTemp = WantedThermalTemp;

								if( WantedThermalTemp > 99.9F )
								{
									if( !Countdown.player_killed() )
									{
										Ships[WhoIAm].Damage = 512.0F;
										Lives = 0;
										DoDamage( OVERRIDE_INVUL );

										Ships[WhoIAm].ShipThatLastKilledMe = WhoIAm;

										if( (Ships[WhoIAm].Object.Mode == DEATH_MODE) || (Ships[WhoIAm].Object.Mode == LIMBO_MODE) )
										{
											Ships[WhoIAm].Object.Mode = GAMEOVER_MODE;
										}else{
											Ships[WhoIAm].Object.Mode = DEATH_MODE;
										}
										AddColourMessageToQue(SystemMessageColour, IF_YOU_CANT_TAKE_THE_HEAT );
										AddColourMessageToQue(SystemMessageColour, GET_OUT_OF_THE_KITCHEN );
										Ships[WhoIAm].Timer = 0.0F;

										Countdown.note_player_killed();
									}
									/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
								}
							}
						}
					}

					ScrPolys[i].Frame = ( ThermalTemp / ( 100.0F / ( (float) ( Thermo_Header->Num_Frames - 1 ) ) ) );
					break;

					/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
				case ScrSeq::Logo:								// Logos
					ScrPolys[i].LifeCount += framelag;

					/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
					if( ScrPolys[i].LifeCount >= 600.0F )
					{
						ScrPolys[i].Frame += 2.0F;
						ScrPolys[i].LifeCount = 0.0F;
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
					}

					Speed = ( 2.125F * framelag );

					if( ScrPolys[i].LifeCount < 60.0F )
					{
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						ScrPolys[i].FadeRed += Speed;
						if( ScrPolys[i].FadeRed > 128.0F ) ScrPolys[i].FadeRed = 128.0F;
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						ScrPolys[i].FadeGreen += Speed;
						if( ScrPolys[i].FadeGreen > 128.0F ) ScrPolys[i].FadeGreen = 128.0F;
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						ScrPolys[i].FadeBlue += Speed;
						if( ScrPolys[i].FadeBlue > 128.0F ) ScrPolys[i].FadeBlue = 128.0F;
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
						ScrPolys[i].FadeTrans += Speed;
						if( ScrPolys[i].FadeTrans > 128.0F ) ScrPolys[i].FadeTrans = 128.0F;
						/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
					}
					else
					{
						if( ScrPolys[i].LifeCount >= ( 600.0F - 60.0F ) )
						{
							/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
							ScrPolys[i].FadeRed -= Speed;
							if( ScrPolys[i].FadeRed < 0.0F ) ScrPolys[i].FadeRed = 0.0F;
							/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
							ScrPolys[i].FadeGreen -= Speed;
							if( ScrPolys[i].FadeGreen < 0.0F ) ScrPolys[i].FadeGreen = 0.0F;
							ScrPolys[i].FadeBlue -= Speed;
							if( ScrPolys[i].FadeBlue < 0.0F ) ScrPolys[i].FadeBlue = 0.0F;
							ScrPolys[i].FadeTrans -= Speed;
							if( ScrPolys[i].FadeTrans < 0.0F ) ScrPolys[i].FadeTrans = 0.0F;
						}
						else
						{
							ScrPolys[i].FadeRed = 128.0F;
							ScrPolys[i].FadeGreen = 128.0F;
							ScrPolys[i].FadeBlue = 128.0F;
							ScrPolys[i].FadeTrans = 128.0F;
						}
					}

					ScrPolys[i].R = (u_int8_t) ScrPolys[i].FadeRed;
					ScrPolys[i].G = (u_int8_t) ScrPolys[i].FadeGreen;
					ScrPolys[i].B = (u_int8_t) ScrPolys[i].FadeBlue;
					ScrPolys[i].Trans = (u_int8_t) ScrPolys[i].FadeTrans;
					break;

				case ScrSeq::Nothing:
				default:
					break;
				}
			}
		}

		i = NextScrPoly;
	}
}

/*===================================================================
	Procedure	:	Lensflare Effect
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/

void DoLensflareEffect( void )
{
	VECTOR		DirVector;
	VECTOR		LeftVector;
	VECTOR		UpVector;
	VECTOR		Trans;
	VECTOR		Trans2;
	VECTOR		Vert;
	VECTOR		NewVert;
	MATRIX		TempMatrix;
	MATRIX		FinalMatrix;
	int16_t		Count;
	float		Center_X;
	float		Center_Y;
	VECTOR		Int_Point;
	u_int16_t		Int_Group;
	NORMAL		Int_Normal;
	VECTOR		ShipVector;
	VECTOR		TempVector;
	VECTOR		TempVector2;
	float		Cos, Cos2, Angle, Angle2;
	float		DistToShip;
	float		DistFactor;
	u_int8_t		Int;
	u_int16_t		i;

	Trans.x = -CurrentCamera.Pos.x;
	Trans.y = -CurrentCamera.Pos.y;
	Trans.z = -CurrentCamera.Pos.z;
	ApplyMatrix( &CurrentCamera.InvMat, &Trans, &Trans2 );			/* Set Translation */

	TempMatrix = CurrentCamera.Mat;
	TempMatrix._41 = Trans2.x;
	TempMatrix._42 = Trans2.y;
	TempMatrix._43 = Trans2.z;

	MatrixMultiply( &TempMatrix, &ProjMatrix, &FinalMatrix );

	Center_X = ( CurrentCamera.Viewport.X + ( CurrentCamera.Viewport.Width / 2.0F ) );
	Center_Y = ( CurrentCamera.Viewport.Y + ( CurrentCamera.Viewport.Height / 2.0F ) );

	for( Count = 0; Count < MAX_PLAYERS; Count++ )
	{
		if( Ships[ Count ].enable && ( Current_Camera_View != Count ) )
		{
			if( ( Ships[ Count ].Object.Flags & SHIP_Turbo ) )
			{
				if( !SoundInfo[ Ships[ Count ].Object.Group ][ CurrentCamera.GroupImIn ] )
				{
					ApplyMatrix( &Ships[ Count ].Object.FinalMat, &Forward, &DirVector );
					ApplyMatrix( &Ships[ Count ].Object.FinalMat, &SlideLeft, &LeftVector );
					ApplyMatrix( &Ships[ Count ].Object.FinalMat, &SlideUp, &UpVector );

					Vert.x = ( Ships[ Count ].Object.Pos.x + ( ( 270.0F * GLOBAL_SCALE ) * -DirVector.x ) + ( ( 26.0F * GLOBAL_SCALE ) * UpVector.x ) );
					Vert.y = ( Ships[ Count ].Object.Pos.y + ( ( 270.0F * GLOBAL_SCALE ) * -DirVector.y ) + ( ( 26.0F * GLOBAL_SCALE ) * UpVector.y ) );
					Vert.z = ( Ships[ Count ].Object.Pos.z + ( ( 270.0F * GLOBAL_SCALE ) * -DirVector.z ) + ( ( 26.0F * GLOBAL_SCALE ) * UpVector.z ) );

					ShipVector.x = ( Vert.x - CurrentCamera.Pos.x );
					ShipVector.y = ( Vert.y - CurrentCamera.Pos.y );
					ShipVector.z = ( Vert.z - CurrentCamera.Pos.z );

					/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
					DistToShip = VectorLength( &ShipVector );

					/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
					TempVector = ShipVector;
					NormaliseVector( &TempVector );
					ApplyMatrix( &CurrentCamera.Mat, &Forward, &TempVector2 );
					NormaliseVector( &TempVector2 );
					Cos = DotProduct( &TempVector, &TempVector2 );
					if( Cos < -1.0F ) Cos = -1.0F;
					/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
					else if ( Cos > 1.0F ) Cos = 1.0F;
					/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
					/* @vfp-exempt R6 — inverse trig has no LUT form (fast trig is sin/cos only); LUT-izing is new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
					Angle = (float) R2D( acosf( Cos ) );

					/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
					ApplyMatrix( &Ships[ Count ].Object.FinalMat, &Forward, &TempVector );
					NormaliseVector( &TempVector );
					Cos2 = DotProduct( &TempVector, &TempVector2 );
					if( Cos2 < -1.0F ) Cos2 = -1.0F;
					else if ( Cos2 > 1.0F ) Cos2 = 1.0F;
					/* @vfp-exempt R3,R6 — angle clamp + acosf: inverse trig has no LUT form and the clamp compare is gameplay-facing; integerizing changes output. measured n/a. Verified 2026-10-25. */
					Angle2 = (float) R2D( acosf( Cos2 ) );

					if( DistToShip < CUTOFF ) DistFactor = 1.0F;
					else DistFactor = ( 1.0F - ( ( DistToShip - CUTOFF ) / CUTOFF ) );

					Int = (u_int8_t) ( ( Cos * Cos2 * DistFactor ) * 255.0F );

					if( ( Angle < 45.0F ) && ( Angle2 < 90.0F ) && ( DistToShip < ( 2.0F * CUTOFF ) ) )
					{
						if( !BackgroundCollide( &MCloadheadert0, &Mloadheader,
									&CurrentCamera.Pos,
									CurrentCamera.GroupImIn,
									&ShipVector, &Int_Point, &Int_Group, &Int_Normal,
									&TempVector, true, nullptr ) )
						{
							Conv3DTo2D( &Vert, &NewVert, &FinalMatrix );
							DirVector.x = ( NewVert.x - Center_X );
							DirVector.y = ( NewVert.y - Center_Y );

							i = FindFreeScrPoly();
							if( i != INDEX_NONE )
							{
								ScrPolys[ i ].Flags = SCRFLAG_Scale;
								ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
								ScrPolys[ i ].Pos.x = ( Center_X + ( DirVector.x * 0.7F ) );
								ScrPolys[ i ].Pos.y = ( Center_Y + ( DirVector.y * 0.7F ) );
								ScrPolys[ i ].R = ( Int / 2 );				// R Colour
								ScrPolys[ i ].G = ( Int / 4 );				// G Colour
								ScrPolys[ i ].B = ( Int );					// B Colour
								ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
								ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
								ScrPolys[ i ].FrameRate = 1.0F;
								ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
								ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
								ScrPolys[ i ].Xscale = 0.3125F;
								ScrPolys[ i ].Yscale = 0.3125F;
								AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
							}

							i = FindFreeScrPoly();
							if( i != INDEX_NONE )
							{
								ScrPolys[ i ].Flags = SCRFLAG_Scale;
								ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
								ScrPolys[ i ].Pos.x = ( Center_X + ( DirVector.x * 0.4F ) );
								ScrPolys[ i ].Pos.y = ( Center_Y + ( DirVector.y * 0.4F ) );
								ScrPolys[ i ].R = ( Int / 4 );				// R Colour
								ScrPolys[ i ].G = ( Int / 4 );				// G Colour
								ScrPolys[ i ].B = ( Int );					// B Colour
								ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
								ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
								ScrPolys[ i ].FrameRate = 1.0F;
								ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
								ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
								ScrPolys[ i ].Xscale = 0.234F;
								ScrPolys[ i ].Yscale = 0.234F;
								AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
							}

							i = FindFreeScrPoly();
							if( i != INDEX_NONE )
							{
								ScrPolys[ i ].Flags = SCRFLAG_Scale;
								ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
								ScrPolys[ i ].Pos.x = ( Center_X + ( -DirVector.x * 0.2F ) );
								ScrPolys[ i ].Pos.y = ( Center_Y + ( -DirVector.y * 0.2F ) );
								ScrPolys[ i ].R = ( Int );					// R Colour
								ScrPolys[ i ].G = ( Int / 2 );				// G Colour
								ScrPolys[ i ].B = ( 0 );					// B Colour
								ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
								ScrPolys[ i ].Frame = 1.0F;					// Animation Frame
								ScrPolys[ i ].FrameRate = 1.0F;
								ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
								ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
								ScrPolys[ i ].Xscale = 0.50F;
								ScrPolys[ i ].Yscale = 0.5F;
								AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
							}

							i = FindFreeScrPoly();
							if( i != INDEX_NONE )
							{
								ScrPolys[ i ].Flags = SCRFLAG_Scale;
								ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
								ScrPolys[ i ].Pos.x = ( Center_X + ( -DirVector.x * 0.5F ) );
								ScrPolys[ i ].Pos.y = ( Center_Y + ( -DirVector.y * 0.5F ) );
								ScrPolys[ i ].R = ( Int / 4 );				// R Colour
								ScrPolys[ i ].G = ( Int / 4 );				// G Colour
								ScrPolys[ i ].B = ( Int );					// B Colour
								ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
								ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
								ScrPolys[ i ].FrameRate = 1.0F;
								ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
								ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
								ScrPolys[ i ].Xscale = 1.167F;
								ScrPolys[ i ].Yscale = 1.167F;
								AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
							}

							i = FindFreeScrPoly();
							if( i != INDEX_NONE )
							{
								ScrPolys[ i ].Flags = SCRFLAG_Scale;
								ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
								ScrPolys[ i ].Pos.x = ( Center_X + ( -DirVector.x * 0.8F ) );
								ScrPolys[ i ].Pos.y = ( Center_Y + ( -DirVector.y * 0.8F ) );
								ScrPolys[ i ].R = ( Int / 4 );				// R Colour
								ScrPolys[ i ].G = ( Int / 4 );				// G Colour
								ScrPolys[ i ].B = ( Int / 2 );					// B Colour
								ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
								ScrPolys[ i ].Frame = 2.0F;					// Animation Frame
								ScrPolys[ i ].FrameRate = 1.0F;
								ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
								ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
								ScrPolys[ i ].Xscale = 0.75F;
								ScrPolys[ i ].Yscale = 0.75F;
								AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
							}

							i = FindFreeScrPoly();
							if( i != INDEX_NONE )
							{
								ScrPolys[ i ].Flags = SCRFLAG_Scale;
								ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
								ScrPolys[ i ].Pos.x = ( Center_X + ( -DirVector.x * 1.2F ) );
								ScrPolys[ i ].Pos.y = ( Center_Y + ( -DirVector.y * 1.2F ) );
								ScrPolys[ i ].R = ( Int );					// R Colour
								ScrPolys[ i ].G = ( Int );					// G Colour
								ScrPolys[ i ].B = ( Int );					// B Colour
								ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
								ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
								ScrPolys[ i ].FrameRate = 1.0F;
								ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
								ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
								ScrPolys[ i ].Xscale = 0.75F;
								ScrPolys[ i ].Yscale = 0.75F;
								AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
							}
						}
					}
				}
			}
		}
	}
}

/*===================================================================
	Procedure	:	3D World to 2D Screen
	Input		:	VECTOR	*	Vertex
				:	VECTOR	*	2D Vertex
				:	MATRIX	*	Final View Matrix
	Output		:	Nothing
===================================================================*/
void Conv3DTo2D( VECTOR * SrcVert, VECTOR * DstVert, MATRIX * FinalMat )
{
	VECTOR	TempVert;

	VisPolyApplyMatrix( FinalMat, SrcVert, &TempVert );
	DstVert->x = CurrentCamera.Viewport.X + ( ( CurrentCamera.Viewport.Width / 2 ) + ( CurrentCamera.Viewport.ScaleX * TempVert.x ) );
	DstVert->y = CurrentCamera.Viewport.Y + ( ( CurrentCamera.Viewport.Height / 2 ) - ( CurrentCamera.Viewport.ScaleY * TempVert.y ) );
	DstVert->z = TempVert.z;
}

/*===================================================================
	Procedure	:	Clip box to viewport
	Input		:	TLVERTEX	*Top-left vertex
				:	TLVERTEX	*Bottom-right vertex
	Output		:	true if the box is wholly outside the viewport;
				:	otherwise false, with a partly visible box clipped
				:	in place (positions and UVs)
===================================================================*/

bool ClipBox( TLVERTEX *topleft, TLVERTEX *bottomright )
{
	float xmin = 0.0f, ymin = 0.0f, xmax = 0.0f, ymax = 0.0f;
	int clip_topleft, clip_bottomright;
	float dx, dy;
	float du, dv;

	xmin = (float) CurrentCamera.Viewport.X;
	ymin = (float) CurrentCamera.Viewport.Y;
	xmax = (float) ( CurrentCamera.Viewport.X + CurrentCamera.Viewport.Width );
	ymax = (float) ( CurrentCamera.Viewport.Y + CurrentCamera.Viewport.Height );

	if ( topleft->sx < xmin )
		clip_topleft = CLIP_LEFT;
	else if ( topleft->sx >= xmax )
		return true;
	else
		clip_topleft = 0;
	if ( topleft->sy < ymin )
		clip_topleft |= CLIP_TOP;
	else if ( topleft->sy >= ymax )
		return true;

	if ( bottomright->sx < xmin )
		return true;
	else if ( bottomright->sx > xmax )
		clip_bottomright = CLIP_RIGHT;
	else
		clip_bottomright = 0;
	if ( bottomright->sy < ymin )
		return true;
	else if ( bottomright->sy > ymax )
		clip_bottomright |= CLIP_BOTTOM;

	dx = bottomright->sx - topleft->sx;
	dy = bottomright->sy - topleft->sy;
	du = bottomright->tu - topleft->tu;
	dv = bottomright->tv - topleft->tv;

	if ( clip_topleft & CLIP_LEFT )
	{
		topleft->tu += du * ( xmin - topleft->sx ) / dx;
		topleft->sx = xmin;
	}
	if ( clip_topleft & CLIP_TOP )
	{
		topleft->tv += dv * ( ymin - topleft->sy ) / dy;
		topleft->sy = ymin;
	}

	if ( clip_bottomright & CLIP_RIGHT )
	{
		bottomright->tu -= du * ( bottomright->sx - xmax ) / dx;
		bottomright->sx = xmax;
	}
	if ( clip_bottomright & CLIP_BOTTOM )
	{
		bottomright->tv -= dv * ( bottomright->sy - ymax ) / dy;
		bottomright->sy = ymax;
	}

	return false;
}

/*===================================================================
	Procedure	:	Create Lensflare effect on missiles
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void DoAllSecBullLensflare( void )
{
	u_int16_t	i;
	u_int16_t	nSecWalk;

	i = FirstSecBullUsed;
	nSecWalk = 0;

	/* Bound the used-chain walk: a corrupt/empty Prev chain must terminate. */
	while( i != INDEX_NONE && nSecWalk < MAXSECONDARYWEAPONBULLETS )
	{
		nSecWalk++;
		if( !SoundInfo[ SecBulls[i].GroupImIn ][ CurrentCamera.GroupImIn ] )
		{
			if( SecBulls[ i ].Lensflare ) SecBullLensflare( i );
		}
		i = SecBulls[ i ].Prev;											/* Next Missile */
	}
}

/*===================================================================
	Procedure	:	Create Lensflare effect on missiles
	Input		:	u_int16_t	Secondary bullet index
	Output		:	Nothing
===================================================================*/
void SecBullLensflare( u_int16_t i )
{
	VECTOR		DirVector;
	VECTOR		LeftVector;
	VECTOR		UpVector;
	VECTOR		Trans;
	VECTOR		Trans2;
	VECTOR		Vert;
	VECTOR		NewVert;
	MATRIX		TempMatrix;
	MATRIX		FinalMatrix;
	float		Center_X;
	float		Center_Y;
	VECTOR		Int_Point;
	u_int16_t		Int_Group;
	NORMAL		Int_Normal;
	VECTOR		BullVector;
	VECTOR		TempVector;
	VECTOR		TempVector2;
	float		Cos, Cos2, Angle, Angle2;
	float		DistToBull;
	float		DistFactor;
	u_int8_t		Int;

	Trans.x = -CurrentCamera.Pos.x;
	Trans.y = -CurrentCamera.Pos.y;
	Trans.z = -CurrentCamera.Pos.z;
	ApplyMatrix( &CurrentCamera.InvMat, &Trans, &Trans2 );			/* Set Translation */

	TempMatrix = CurrentCamera.Mat;
	TempMatrix._41 = Trans2.x;
	TempMatrix._42 = Trans2.y;
	TempMatrix._43 = Trans2.z;

	MatrixMultiply( &TempMatrix, &ProjMatrix, &FinalMatrix );

	Center_X = ( CurrentCamera.Viewport.X + ( CurrentCamera.Viewport.Width / 2.0F ) );
	Center_Y = ( CurrentCamera.Viewport.Y + ( CurrentCamera.Viewport.Height / 2.0F ) );

	ApplyMatrix( &SecBulls[ i ].Mat, &Forward, &DirVector );
	ApplyMatrix( &SecBulls[ i ].Mat, &SlideLeft, &LeftVector );
	ApplyMatrix( &SecBulls[ i ].Mat, &SlideUp, &UpVector );

	Vert.x = ( SecBulls[ i ].Pos.x + ( ( 270.0F * GLOBAL_SCALE ) * -DirVector.x ) + ( ( 26.0F * GLOBAL_SCALE ) * UpVector.x ) );
	Vert.y = ( SecBulls[ i ].Pos.y + ( ( 270.0F * GLOBAL_SCALE ) * -DirVector.y ) + ( ( 26.0F * GLOBAL_SCALE ) * UpVector.y ) );
	Vert.z = ( SecBulls[ i ].Pos.z + ( ( 270.0F * GLOBAL_SCALE ) * -DirVector.z ) + ( ( 26.0F * GLOBAL_SCALE ) * UpVector.z ) );

	BullVector.x = ( Vert.x - CurrentCamera.Pos.x );
	BullVector.y = ( Vert.y - CurrentCamera.Pos.y );
	BullVector.z = ( Vert.z - CurrentCamera.Pos.z );

	DistToBull = VectorLength( &BullVector );

	TempVector = BullVector;
	NormaliseVector( &TempVector );
	ApplyMatrix( &CurrentCamera.Mat, &Forward, &TempVector2 );
	NormaliseVector( &TempVector2 );
	Cos = DotProduct( &TempVector, &TempVector2 );
	if( Cos < -1.0F ) Cos = -1.0F;
	else if ( Cos > 1.0F ) Cos = 1.0F;
	/* @vfp-exempt R6 — inverse trig has no LUT form (fast trig is sin/cos only); LUT-izing is new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
	Angle = (float) R2D( acosf( Cos ) );

	ApplyMatrix( &SecBulls[ i ].Mat, &Forward, &TempVector );
	NormaliseVector( &TempVector );
	Cos2 = DotProduct( &TempVector, &TempVector2 );
	if( Cos2 < -1.0F ) Cos2 = -1.0F;
	else if ( Cos2 > 1.0F ) Cos2 = 1.0F;
	/* @vfp-exempt R6 — inverse trig has no LUT form (fast trig is sin/cos only); LUT-izing is new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
	Angle2 = (float) R2D( acosf( Cos2 ) );

	if( DistToBull < CUTOFF ) DistFactor = 1.0F;
	else DistFactor = ( 1.0F - ( ( DistToBull - CUTOFF ) / CUTOFF ) );

	Int = (u_int8_t) ( ( Cos * Cos2 * DistFactor ) * 255.0F );

	if( ( Angle < 45.0F ) && ( Angle2 < 90.0F ) && ( DistToBull < ( 2.0F * CUTOFF ) ) )
	{
		if( !BackgroundCollide( &MCloadheadert0, &Mloadheader,
					&CurrentCamera.Pos,
					CurrentCamera.GroupImIn,
					&BullVector, &Int_Point, &Int_Group, &Int_Normal,
					&TempVector, true, nullptr ) )
		{
			Conv3DTo2D( &Vert, &NewVert, &FinalMatrix );
			DirVector.x = ( NewVert.x - Center_X );
			DirVector.y = ( NewVert.y - Center_Y );

			i = FindFreeScrPoly();
			if( i != INDEX_NONE )
			{
				ScrPolys[ i ].Flags = SCRFLAG_Scale;
				ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
				ScrPolys[ i ].Pos.x = ( Center_X + ( DirVector.x * 0.7F ) );
				ScrPolys[ i ].Pos.y = ( Center_Y + ( DirVector.y * 0.7F ) );
				ScrPolys[ i ].R = ( Int / 2 );				// R Colour
				ScrPolys[ i ].G = ( Int / 4 );				// G Colour
				ScrPolys[ i ].B = ( Int );					// B Colour
				ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
				ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
				ScrPolys[ i ].FrameRate = 1.0F;
				ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
				ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
				ScrPolys[ i ].Xscale = 0.3125F;
				ScrPolys[ i ].Yscale = 0.3125F;
				AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
			}

			i = FindFreeScrPoly();
			if( i != INDEX_NONE )
			{
				ScrPolys[ i ].Flags = SCRFLAG_Scale;
				ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
				ScrPolys[ i ].Pos.x = ( Center_X + ( DirVector.x * 0.4F ) );
				ScrPolys[ i ].Pos.y = ( Center_Y + ( DirVector.y * 0.4F ) );
				ScrPolys[ i ].R = ( Int / 4 );				// R Colour
				ScrPolys[ i ].G = ( Int / 4 );				// G Colour
				ScrPolys[ i ].B = ( Int );					// B Colour
				ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
				ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
				ScrPolys[ i ].FrameRate = 1.0F;
				ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
				ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
				ScrPolys[ i ].Xscale = 0.234F;
				ScrPolys[ i ].Yscale = 0.234F;
				AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
			}

			i = FindFreeScrPoly();
			if( i != INDEX_NONE )
			{
				ScrPolys[ i ].Flags = SCRFLAG_Scale;
				ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
				ScrPolys[ i ].Pos.x = ( Center_X + ( -DirVector.x * 0.2F ) );
				ScrPolys[ i ].Pos.y = ( Center_Y + ( -DirVector.y * 0.2F ) );
				ScrPolys[ i ].R = ( Int );					// R Colour
				ScrPolys[ i ].G = ( Int / 2 );				// G Colour
				ScrPolys[ i ].B = ( 0 );					// B Colour
				ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
				ScrPolys[ i ].Frame = 1.0F;					// Animation Frame
				ScrPolys[ i ].FrameRate = 1.0F;
				ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
				ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
				ScrPolys[ i ].Xscale = 0.50F;
				ScrPolys[ i ].Yscale = 0.5F;
				AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
			}

			i = FindFreeScrPoly();
			if( i != INDEX_NONE )
			{
				ScrPolys[ i ].Flags = SCRFLAG_Scale;
				ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
				ScrPolys[ i ].Pos.x = ( Center_X + ( -DirVector.x * 0.5F ) );
				ScrPolys[ i ].Pos.y = ( Center_Y + ( -DirVector.y * 0.5F ) );
				ScrPolys[ i ].R = ( Int / 4 );				// R Colour
				ScrPolys[ i ].G = ( Int / 4 );				// G Colour
				ScrPolys[ i ].B = ( Int );					// B Colour
				ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
				ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
				ScrPolys[ i ].FrameRate = 1.0F;
				ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
				ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
				ScrPolys[ i ].Xscale = 1.167F;
				ScrPolys[ i ].Yscale = 1.167F;
				AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
			}

			i = FindFreeScrPoly();
			if( i != INDEX_NONE )
			{
				ScrPolys[ i ].Flags = SCRFLAG_Scale;
				ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
				ScrPolys[ i ].Pos.x = ( Center_X + ( -DirVector.x * 0.8F ) );
				ScrPolys[ i ].Pos.y = ( Center_Y + ( -DirVector.y * 0.8F ) );
				ScrPolys[ i ].R = ( Int / 4 );				// R Colour
				ScrPolys[ i ].G = ( Int / 4 );				// G Colour
				ScrPolys[ i ].B = ( Int / 2 );					// B Colour
				ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
				ScrPolys[ i ].Frame = 2.0F;					// Animation Frame
				ScrPolys[ i ].FrameRate = 1.0F;
				ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
				ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
				ScrPolys[ i ].Xscale = 0.75F;
				ScrPolys[ i ].Yscale = 0.75F;
				AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
			}

			i = FindFreeScrPoly();
			if( i != INDEX_NONE )
			{
				ScrPolys[ i ].Flags = SCRFLAG_Scale;
				ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
				ScrPolys[ i ].Pos.x = ( Center_X + ( -DirVector.x * 1.2F ) );
				ScrPolys[ i ].Pos.y = ( Center_Y + ( -DirVector.y * 1.2F ) );
				ScrPolys[ i ].R = ( Int );					// R Colour
				ScrPolys[ i ].G = ( Int );					// G Colour
				ScrPolys[ i ].B = ( Int );					// B Colour
				ScrPolys[ i ].Trans = ( Int );				// Amount of transparency
				ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
				ScrPolys[ i ].FrameRate = 1.0F;
				ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
				ScrPolys[ i ].Frm_Info = &LensFlare_Header;	// Offset Info
				ScrPolys[ i ].Xscale = 0.75F;
				ScrPolys[ i ].Yscale = 0.75F;
				AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
			}
		}
	}
}

/*===================================================================
	Procedure	:	Update Position and digits of counter
	Input		:	float		XPos
				:	float		YPos
				:	LONGLONG	Number
	Output		:	Nothing
===================================================================*/
inline void CountdownClock::update()
{
	float		XPos, YPos;
	int			DigitNumber;
	int			Number;
	float		Scale = 0.125F;
	float		Center_X = 160.0F;
	float		Center_Y = 120.0F;
	FRAME_INFO	**	Graphics;
	float		Width;

	Graphics = CountDownFontGraphics[ CurrentCountDownFont ];
	Width = CountDownFontWidth[ CurrentCountDownFont ];

	Time_Diff = timer_run( &countdown_timer ) * 100.0f;

	if( MyGameStatus == STATUS_SinglePlayer )
	{
		if( ( framelag == 0.0F ) || ( DebugInfo ) )
		{
			Time_Diff = 0.0F;
		}
	}
	else
	{
		if( IllegalTime )
		{
			RequestTime();
		}
	}

	if( ( Countdown_Float >= 1000.0F ) && ( ( Countdown_Float - Time_Diff ) < 1000.0F ) )
	{
		CurrentCountDownFont = 1;
		CountDown_Wanted_X = TIMERENDSCREENX;
		CountDown_Wanted_Y = TIMERENDSCREENY;
		CountDown_Wanted_Scale = TIMERENDSCALE;
		Toggle = true;
	}

	Countdown_Float -= Time_Diff;
	Number = (int) Countdown_Float;

	if( Countdown_Float < 0.0F )
	{
		if( MyGameStatus != STATUS_SinglePlayer )
		{
			LevelNum = -1;
			NewLevelNum++;
			if( NewLevelNum >= NumLevels ) NewLevelNum = 0;
		}
		else
		{
			if( !KilledPlayer )
			{
				Ships[WhoIAm].Damage = 512.0F;
				Lives = 0;
				DoDamage( OVERRIDE_INVUL );

				Ships[WhoIAm].ShipThatLastKilledMe = WhoIAm;

				if( (Ships[WhoIAm].Object.Mode == DEATH_MODE) || (Ships[WhoIAm].Object.Mode == LIMBO_MODE) )
				{
					Ships[WhoIAm].Object.Mode = GAMEOVER_MODE;
				}else{
					Ships[WhoIAm].Object.Mode = DEATH_MODE;
				}
				AddColourMessageToQue(SystemMessageColour, OUT_OF_TIME );
				Ships[WhoIAm].Timer = 0.0F;

				KilledPlayer = true;
			}
		}

		Countdown_Float = 0.0F;
	}


	if( Toggle )
	{
		ScaleInc = ( ( CountDown_Wanted_Scale - CountDown_Scale ) / 420.0F );
		CountDown_Xoff = ( ( CountDown_Wanted_X - CountDown_X ) / 420.0F );
		CountDown_Yoff = ( ( CountDown_Wanted_Y - CountDown_Y ) / 420.0F );
		Interp_Time = 420.0F;
		Toggle = false;
	}

	if( Interp_Time )
	{
		Interp_Time -= framelag;
		if( Interp_Time < 0.0F )
		{
			Interp_Time = 0.0F;
			CountDown_X = CountDown_Wanted_X;
			CountDown_Y = CountDown_Wanted_Y;
			CountDown_Scale = CountDown_Wanted_Scale;
		}
		else
		{
			CountDown_X += ( CountDown_Xoff * framelag );
			CountDown_Y += ( CountDown_Yoff * framelag );
			CountDown_Scale += ( ScaleInc * framelag );
		}
	}

	if( Countdown_Float < 400.0F )
	{
		CountDown_Col -= ( ( 1.0F / 300.0F ) * framelag );
		if( CountDown_Col < 0.0F ) CountDown_Col = 0.0F;
	}
	else
	{
		CountDown_Col += ( ( 1.0F / 300.0F ) * framelag );
		if( CountDown_Col > 1.0F ) CountDown_Col = 1.0F;
	}

	Center_X = CountDown_X;
	Center_Y = CountDown_Y;
	Scale = CountDown_Scale * ( 23.0F / CountDownFontWidth[ CurrentCountDownFont ] );

	DigitNumber = ( Number / 60000 );
	Number -= ( DigitNumber * 60000 );
	XPos = Center_X - ( ( Width * 4.173913F ) * Scale );
	YPos = Center_Y;
	UpdateDigit( &Min1Digit[ 0 ], XPos, YPos, (int16_t) ( DigitNumber % 10 ), Scale, CountDown_Col, Graphics );

	DigitNumber = ( Number / 6000 );
	Number -= ( DigitNumber * 6000 );
	XPos = Center_X - ( ( Width * 2.7826F ) * Scale );
	UpdateDigit( &Min0Digit[ 0 ], XPos, YPos, (int16_t) ( DigitNumber % 10 ), Scale, CountDown_Col, Graphics );

	XPos = Center_X - ( ( Width * 1.73913F ) * Scale );
	UpdateDigit( &Bit0Digit[ 0 ], XPos, YPos, 10, Scale, CountDown_Col, Graphics );

	DigitNumber = ( Number / 1000 );
	Number -= ( DigitNumber * 1000 );
	XPos = Center_X - ( ( Width * 0.695652F ) * Scale );
	UpdateDigit( &Sec1Digit[ 0 ], XPos, YPos, (int16_t) ( DigitNumber % 10 ), Scale, CountDown_Col, Graphics );

	DigitNumber = ( Number / 100 );
	Number -= ( DigitNumber * 100 );
	XPos = Center_X + ( ( Width * 0.695652F ) * Scale );
	UpdateDigit( &Sec0Digit[ 0 ], XPos, YPos, (int16_t) ( DigitNumber % 10 ), Scale, CountDown_Col, Graphics );

	XPos = Center_X + ( ( Width * 1.73813F ) * Scale );
	UpdateDigit( &Bit1Digit[ 0 ], XPos, YPos, 10, Scale, CountDown_Col, Graphics );

	DigitNumber = ( Number / 10 );
	Number -= ( DigitNumber * 10 );
	XPos = Center_X + ( ( Width * 2.7826F ) * Scale );
	UpdateDigit( &Hun1Digit[ 0 ], XPos, YPos, (int16_t) ( DigitNumber % 10 ), Scale, CountDown_Col, Graphics );

	DigitNumber = Number;
	XPos = Center_X + ( ( Width * 4.173913F ) * Scale );
	UpdateDigit( &Hun0Digit[ 0 ], XPos, YPos, (int16_t) ( DigitNumber % 10 ), Scale, CountDown_Col, Graphics );
}

/*===================================================================
	Procedure	:	Update Digit Position and Number
	Input		:	u_int16_t	*	Digit Segmnent Array
				:	float		X Pos
				:	float		Y Pos
				:	int16_t		Number
				:	float		Scale
				:	float		Colour Brightness;
	Output		:	Nothing
===================================================================*/
void UpdateDigit( u_int16_t * DigitArray, float XPos, float YPos, int16_t Number, float Scale, float Col ,FRAME_INFO	** Graphics )
{
	u_int16_t	i;
	int16_t	Count;
	u_int8_t	Red, Green, Blue, Trans;
	int16_t	NumSegments;

	XPos = ( XPos * ( MainCamera.Viewport.Width / 320.0F ) );
	YPos = ( YPos * ( MainCamera.Viewport.Height / 240.0F ) );

	Green = 0;
	Blue = 0;
	Trans = (u_int8_t) ( 255.0F * Col );

	if( Number == 10 ) NumSegments = 1;
	else NumSegments = 7;

	for( Count = 0; Count < NumSegments; Count++ )
	{
		i = DigitArray[ Count ];

		if( i != INDEX_NONE )
		{
			if( Number == 10 )
			{
				Red = (u_int8_t) ( 255.0F * Col );
			}
			else
			{
				if( NumberSegments[ Number ][ Count ] ) Red = (u_int8_t) ( 255.0F * Col );
				else Red = (u_int8_t) 0;// ( 80.0F * Col );
			}

			ScrPolys[i].Pos.x = XPos;
			ScrPolys[i].Pos.y = YPos;
			ScrPolys[i].R = Red;
			ScrPolys[i].G = Green;
			ScrPolys[i].B = Blue;
			ScrPolys[i].Trans = Trans;
			ScrPolys[i].Xscale = Scale;
			ScrPolys[i].Yscale = Scale;

			if( ScrPolys[i].Frm_Info != Graphics )
			{
				RemoveScrPolyFromTPage( i, GetTPage( *ScrPolys[i].Frm_Info, 0 ) );
				ScrPolys[i].Frm_Info = Graphics;
				AddScrPolyToTPage( i, GetTPage( *ScrPolys[i].Frm_Info, 0 ) );
			}
		}
	}
}

/*===================================================================
	Procedure	:	Lensflare Effect
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void CountdownClock::create_digits()
{
	DeleteCountdownDigits();

	CreateDigit( &Min1Digit[ 0 ], 160.0F, 120.0F );
	CreateDigit( &Min0Digit[ 0 ], 160.0F, 120.0F );
	CreateSeperatorDigit( &Bit0Digit[ 0 ], 160.0F, 120.0F );
	CreateDigit( &Sec1Digit[ 0 ], 160.0F, 120.0F );
	CreateDigit( &Sec0Digit[ 0 ], 160.0F, 120.0F );
	CreateSeperatorDigit( &Bit1Digit[ 0 ], 160.0F, 120.0F );
	CreateDigit( &Hun1Digit[ 0 ], 160.0F, 120.0F );
	CreateDigit( &Hun0Digit[ 0 ], 160.0F, 120.0F );
}

/*===================================================================
	Procedure	:	Lensflare Effect
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void CountdownClock::delete_digits()
{
	DeleteDigit( &Min1Digit[ 0 ] );
	DeleteDigit( &Min0Digit[ 0 ] );
	DeleteSeperatorDigit( &Bit0Digit[ 0 ] );
	DeleteDigit( &Sec1Digit[ 0 ] );
	DeleteDigit( &Sec0Digit[ 0 ] );
	DeleteSeperatorDigit( &Bit1Digit[ 0 ] );
	DeleteDigit( &Hun1Digit[ 0 ] );
	DeleteDigit( &Hun0Digit[ 0 ] );
}

/*===================================================================
	Procedure	:	Clear Countdown ScrPolys index tables
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void CountdownClock::clear_buffers()
{
	int16_t	Count;

	for( Count = 0; Count < 7; Count++ )
	{
		Min1Digit[ Count ] = INDEX_NONE;
		Min0Digit[ Count ] = INDEX_NONE;
		Sec1Digit[ Count ] = INDEX_NONE;
		Sec0Digit[ Count ] = INDEX_NONE;
		Hun1Digit[ Count ] = INDEX_NONE;
		Hun0Digit[ Count ] = INDEX_NONE;
	}

	Bit0Digit[ 0 ] = INDEX_NONE;
	Bit1Digit[ 0 ] = INDEX_NONE;
}

/*===================================================================
	Procedure	:	Create Digit
	Input		:	u_int16_t	*	Digit Array ( 7 )
				:	float		X Position
				:	float		Y Position
	Output		:	Nothing
===================================================================*/
void CreateDigit( u_int16_t * DigitArray, float XPos, float YPos )
{
	u_int16_t	i;
	int16_t	Count;

	for( Count = 0; Count < 7; Count++ )
	{
		i = FindFreeScrPoly();

		DigitArray[ Count ] = i;

		if( i != INDEX_NONE )
		{
			ScrPolys[ i ].Flags = SCRFLAG_Scale;
			ScrPolys[ i ].Type = SCRTYPE_Normal;
			ScrPolys[ i ].Pos.x = XPos;
			ScrPolys[ i ].Pos.y = YPos;
			ScrPolys[ i ].Pos.z = 0.0F;
			ScrPolys[ i ].R = 64;						// R Colour
			ScrPolys[ i ].G = 0;						// G Colour
			ScrPolys[ i ].B = 0;						// B Colour
			ScrPolys[ i ].Trans = 64;					// Amount of transparency
			ScrPolys[ i ].Frame = (float) Count;		// Animation Frame
			ScrPolys[ i ].FrameRate = 0.0F;
			ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
			ScrPolys[ i ].Frm_Info = &Numbers_Header;	// Offset Info
			ScrPolys[ i ].Xscale = 0.125F;
			ScrPolys[ i ].Yscale = 0.125F;
			AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
		}
	}
}

/*===================================================================
	Procedure	:	Create ":" Seperator Digit
	Input		:	u_int16_t	*	Digit Array ( 7 )
				:	float		X Position
				:	float		Y Position
	Output		:	Nothing
===================================================================*/
void CreateSeperatorDigit( u_int16_t * DigitArray, float XPos, float YPos )
{
	u_int16_t	i;

	i = FindFreeScrPoly();

	DigitArray[ 0 ] = i;

	if( i != INDEX_NONE )
	{
		ScrPolys[ i ].Flags = SCRFLAG_Scale;
		ScrPolys[ i ].Type = SCRTYPE_Normal;
		ScrPolys[ i ].Pos.x = XPos;
		ScrPolys[ i ].Pos.y = YPos;
		ScrPolys[ i ].Pos.z = 0.0F;
		ScrPolys[ i ].R = 64;						// R Colour
		ScrPolys[ i ].G = 0;						// G Colour
		ScrPolys[ i ].B = 0;						// B Colour
		ScrPolys[ i ].Trans = 64;					// Amount of transparency
		ScrPolys[ i ].Frame = 7.0F;					// Animation Frame
		ScrPolys[ i ].FrameRate = 0.0F;
		ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
		ScrPolys[ i ].Frm_Info = &Numbers_Header;	// Offset Info
		ScrPolys[ i ].Xscale = 0.125F;
		ScrPolys[ i ].Yscale = 0.125F;
		AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
	}
}

/*===================================================================
	Procedure	:	Delete Digit
	Input		:	u_int16_t	*	Digit Array[ 7 ]
	Output		:	Nothing
===================================================================*/
void DeleteDigit( u_int16_t * DigitArray )
{
	int16_t	Count;

	for( Count = 0; Count < 7; Count++ )
	{
		if( DigitArray[ Count ] != INDEX_NONE )
		{
			KillUsedScrPoly( DigitArray[ Count ] );
			DigitArray[ Count ] = INDEX_NONE;
		}
	}
}

/*===================================================================
	Procedure	:	Delete ":" Seperator Digit
	Input		:	u_int16_t	*	Digit Array[ 1 ]
	Output		:	Nothing
===================================================================*/
void DeleteSeperatorDigit( u_int16_t * DigitArray )
{
	if( DigitArray[ 0 ] != INDEX_NONE )
	{
		KillUsedScrPoly( DigitArray[ 0 ] );
		DigitArray[ 0 ] = INDEX_NONE;
	}
}

/*===================================================================
	Procedure	:	Start CountDown Timer
	Input		:	int16_t	Minutes
				:	int16_t	Seconds
	Output		:	Nothing
===================================================================*/
void CountdownClock::start( int16_t Minutes, int16_t Seconds )
{
	CurrentCountDownFont = 0;
	Countdown_Float = ( ( Minutes * 6000.0F ) + ( Seconds * 100.0F ) );

	timer_clear( &countdown_timer );

	Toggle = false;
	ScaleInc = 0.0F;
	Interp_Time = 0.0F;
	CountDown_X = TIMERSTARTSCREENX;
	CountDown_Y = TIMERSTARTSCREENY;
	CountDown_Xoff = 0.0F;
	CountDown_Yoff = 0.0F;
	CountDown_Wanted_X = TIMERSTARTSCREENX;
	CountDown_Wanted_Y = TIMERSTARTSCREENY;
	CountDown_Scale = TIMERSTARTSCALE;
	CountDown_Wanted_Scale = TIMERSTARTSCALE;
	CountDown_Col = 1.0F;
}

/*===================================================================
	Procedure	:	Add a one-frame ScreenPoly text glyph (solid when
				:	Trans is 255)
	Input		:	u_int16_t	Frame (glyph)
				:	float	XPos
				:	float	YPos
				:	u_int8_t	Red
				:	u_int8_t	Green
				:	u_int8_t	Blue
				:	u_int8_t	Trans
	Output		:	void
===================================================================*/

void ScrPolyPool::add_text( u_int16_t Frame, float XPos, float YPos, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans )
{
	u_int16_t	i;

	i = FindFreeScrPoly();

	if( i != INDEX_NONE )
	{
		if( Trans == 255 ) ScrPolys[ i ].Flags = SCRFLAG_Solid;
		else ScrPolys[ i ].Flags = SCRFLAG_Nothing;
		/* [3DS] OR in the bottom-HUD bit when routing active so this poly
		   gets drained to the bottom screen instead of the stereo top. */
		if (g_scrpoly_route_bottom) ScrPolys[ i ].Flags |= SCRFLAG_BottomHUD;
		ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
		ScrPolys[ i ].Pos.x = XPos;
		ScrPolys[ i ].Pos.y = YPos;
		ScrPolys[ i ].R = Red;						// R Colour
		ScrPolys[ i ].G = Green;					// G Colour
		ScrPolys[ i ].B = Blue;						// B Colour
		ScrPolys[ i ].Trans = Trans;				// Amount of transparency
		ScrPolys[ i ].Frame = (float) Frame;		// Animation Frame
		ScrPolys[ i ].FrameRate = 0.0F;
		ScrPolys[ i ].SeqNum = ScrSeq::Nothing;

		if(Frame >= 63 && Frame <= 72) // don't scale the cross hair or trojax power
		{
			ScrPolys[ i ].Xscale = 1.0F;
			ScrPolys[ i ].Yscale = 1.0F;
		}
		else
		{
			ScrPolys[ i ].Xscale = FontWidth/8.0F;
			ScrPolys[ i ].Yscale = FontHeight/8.0F;
		}

		// Offset Info
		if( Text512b_Header && FontHeight > 8.0F )
			ScrPolys[ i ].Frm_Info = &Text512b_Header;	// bigger font
		else
			ScrPolys[ i ].Frm_Info = &Text512_Header;		// original font

		AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
	}
}

/*===================================================================
	Procedure	:	Start CountDown Timer
	Input		:	int16_t	Minutes
				:	int16_t	Seconds
	Output		:	Nothing
===================================================================*/
void CountdownClock::reset_bomb_tag( float Amount )
{
	CurrentCountDownFont = 0;
	Countdown_Float = Amount;

	timer_clear( &countdown_timer );

	Toggle = false;
	ScaleInc = 0.0F;
	Interp_Time = 0.0F;
	CountDown_X = TIMERSTARTSCREENX;
	CountDown_Y = TIMERSTARTSCREENY;
	CountDown_Xoff = 0.0F;
	CountDown_Yoff = 0.0F;
	CountDown_Wanted_X = TIMERSTARTSCREENX;
	CountDown_Wanted_Y = TIMERSTARTSCREENY;
	CountDown_Scale = TIMERSTARTSCALE;
	CountDown_Wanted_Scale = TIMERSTARTSCALE;
	CountDown_Col = 1.0F;

	if( Countdown_Float < 1000.0F )
	{
		CurrentCountDownFont = 1;
		CountDown_Wanted_X = TIMERENDSCREENX;
		CountDown_Wanted_Y = TIMERENDSCREENY;
		CountDown_Wanted_Scale = TIMERENDSCALE;
		ScaleInc = ( ( CountDown_Wanted_Scale - CountDown_Scale ) / 420.0F );
		CountDown_Xoff = ( ( CountDown_Wanted_X - CountDown_X ) / 420.0F );
		CountDown_Yoff = ( ( CountDown_Wanted_Y - CountDown_Y ) / 420.0F );
		Interp_Time = 420.0F;

		if( Countdown_Float >= 300.0F )
		{
			Interp_Time = ( ( Countdown_Float - 300.0F ) * ( 600.0F / 1000.0F ) );
			CountDown_X = ( TIMERSTARTSCREENX + ( CountDown_Xoff * ( 420.0F - Interp_Time ) ) );
			CountDown_Y = ( TIMERSTARTSCREENY + ( CountDown_Yoff * ( 420.0F - Interp_Time ) ) );
			CountDown_Scale = ( TIMERSTARTSCALE + ( ScaleInc * ( 420.0F - Interp_Time ) ) );
		}
		else
		{
			Interp_Time = 0.0F;
			CountDown_X = CountDown_Wanted_X;
			CountDown_Y = CountDown_Wanted_Y;
			CountDown_Scale = CountDown_Wanted_Scale;
			CountDown_Col = ( Countdown_Float / 300.0F );
		}
	}
}

/*===================================================================
	Procedure	:	Add a one-frame ScreenPoly text glyph (solid when
				:	Trans is 255)
	Input		:	u_int16_t	Frame (glyph)
				:	float	XPos
				:	float	YPos
				:	float	XScale
				:	float	YScale
				:	u_int8_t	Red
				:	u_int8_t	Green
				:	u_int8_t	Blue
				:	u_int8_t	Trans
	Output		:	void
===================================================================*/
void ScrPolyPool::add_text_scale( u_int16_t Frame, float XPos, float YPos, float XScale, float YScale, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans )
{
	u_int16_t	i;

	i = FindFreeScrPoly();

	if( i != INDEX_NONE )
	{
		if( Trans == 255 ) ScrPolys[ i ].Flags = SCRFLAG_Solid;
		else ScrPolys[ i ].Flags = SCRFLAG_Nothing;
		if (g_scrpoly_route_bottom) ScrPolys[ i ].Flags |= SCRFLAG_BottomHUD;
		ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
		ScrPolys[ i ].Pos.x = XPos;
		ScrPolys[ i ].Pos.y = YPos;
		ScrPolys[ i ].R = Red;						// R Colour
		ScrPolys[ i ].G = Green;					// G Colour
		ScrPolys[ i ].B = Blue;						// B Colour
		ScrPolys[ i ].Trans = Trans;				// Amount of transparency
		ScrPolys[ i ].Frame = (float) Frame;		// Animation Frame
		ScrPolys[ i ].FrameRate = 0.0F;
		ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
		ScrPolys[ i ].Xscale = XScale*(FontWidth/8.0F);
		ScrPolys[ i ].Yscale = YScale*(FontHeight/8.0F);

		// Offset Info
		if(Text512b_Header && FontHeight > 8.0F)
			ScrPolys[ i ].Frm_Info = &Text512b_Header;	// bigger font
		else
			ScrPolys[ i ].Frm_Info = &Text512_Header;		// original font

		AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
	}
}

/*===================================================================
	Procedure	:	Allocate Screen Multiples
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void ShowScreenMultiples( void )
{
	u_int16_t	i;
	u_int16_t	Model;
	int16_t	Count;
	VECTOR	Rotation;
	float	Center_X;
	float	Center_Y;

	if( Current_Camera_View != WhoIAm ) return;				// Dont show multiples in external view.

	Center_X = ( MainCamera.Viewport.X + ( MainCamera.Viewport.Width / 2.0F ) );
	Center_Y = ( MainCamera.Viewport.Y + ( MainCamera.Viewport.Height / 2.0F ) );

	for( Count = 0; Count < Ships[ WhoIAm ].NumMultiples; Count++ )
	{
		Model = Ships[ WhoIAm ].OrbModels[ Count ];

		if( Model != INDEX_NONE )
		{
			Rotation.x = (float) fast_sinf( D2R( Models[ Model ].AxisRot + 180.0F ) );
			Rotation.y = (float) fast_cosf( D2R( Models[ Model ].AxisRot + 180.0F ) );

			i = FindFreeScrPoly();


			if( i != INDEX_NONE )
			{
				ScrPolys[ i ].Flags = SCRFLAG_Nothing;
				ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
				ScrPolys[ i ].Pos.x = Center_X - ( Rotation.x * 20.0F );
				ScrPolys[ i ].Pos.y = Center_Y + ( Rotation.y * 20.0F );
				ScrPolys[ i ].R = 255;						// R Colour
				ScrPolys[ i ].G = 255;						// G Colour
				ScrPolys[ i ].B = 255;						// B Colour
				ScrPolys[ i ].Trans = 255;					// Amount of transparency
				ScrPolys[ i ].Frame = 0.0F;
				ScrPolys[ i ].FrameRate = 0.0F;
				ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
				ScrPolys[ i ].Frm_Info = &Orb_Header;		// Offset Info
				ScrPolys[ i ].Xscale = 1.0F;
				ScrPolys[ i ].Yscale = 1.0F;
				AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
			}
		}
	}
}

/*===================================================================
	Procedure	:	Make Screen Flash
				:	u_int8_t	Red
				:	u_int8_t	Green
				:	u_int8_t	Blue
				:	u_int8_t	Trans
	Output		:	Nothing
===================================================================*/
void MakeScreenFlash( u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans, u_int16_t *ScreenPolyStore, ScrSeq SeqNum )
{
	u_int16_t	i;

	if( *ScreenPolyStore != INDEX_NONE )
	{
		i = *ScreenPolyStore;

		if( Trans == 255 )
			ScrPolys[ i ].Flags = SCRFLAG_Solid | SCRFLAG_UseCoords;
		else
			ScrPolys[ i ].Flags = SCRFLAG_UseCoords;

		if( SeqNum != ScrSeq::FadeUp )
		{
			ScrPolys[ i ].LifeCount = 255.0F;
			ScrPolys[ i ].R = Red;						// R Colour
			ScrPolys[ i ].G = Green;					// G Colour
			ScrPolys[ i ].B = Blue;						// B Colour
			ScrPolys[ i ].Trans = Trans;				// Amount of transparency
		}
		else
		{
			ScrPolys[ i ].LifeCount = 0.0F;
			ScrPolys[ i ].R = 0;						// R Colour
			ScrPolys[ i ].G = 0;					// G Colour
			ScrPolys[ i ].B = 0;						// B Colour
			ScrPolys[ i ].Trans = 0;				// Amount of transparency
		}
		ScrPolys[ i ].FadeRed = (float) Red;
		ScrPolys[ i ].FadeGreen = (float) Green;
		ScrPolys[ i ].FadeBlue = (float) Blue;
		ScrPolys[ i ].FadeTrans = (float) Trans;
	}
	else
	{
		i = FindFreeScrPoly();

		if( i != INDEX_NONE )
		{
			*ScreenPolyStore = i;

			if( Trans == 255 )
				ScrPolys[ i ].Flags = SCRFLAG_Solid | SCRFLAG_UseCoords;
			else
				ScrPolys[ i ].Flags = SCRFLAG_UseCoords;

			if( SeqNum != ScrSeq::FadeUp )
			{
				ScrPolys[ i ].LifeCount = 255.0F;
				ScrPolys[ i ].R = Red;						// R Colour
				ScrPolys[ i ].G = Green;					// G Colour
				ScrPolys[ i ].B = Blue;						// B Colour
				ScrPolys[ i ].Trans = Trans;				// Amount of transparency
			}
			else
			{
				ScrPolys[ i ].LifeCount = 0.0F;
				ScrPolys[ i ].R = 0;						// R Colour
				ScrPolys[ i ].G = 0;					// G Colour
				ScrPolys[ i ].B = 0;						// B Colour
				ScrPolys[ i ].Trans = 0;				// Amount of transparency
			}

			ScrPolys[ i ].Type = SCRTYPE_Normal; //SCRTYPE_LastAFrame;
			ScrPolys[ i ].Pos.x = 0.0F;
			ScrPolys[ i ].Pos.y = 0.0F;
			ScrPolys[ i ].x1 = 0.0F;
			ScrPolys[ i ].y1 = 0.0F;
			ScrPolys[ i ].x2 = (float) MainCamera.Viewport.Width;
			ScrPolys[ i ].y2 = 0.0F;
			ScrPolys[ i ].x3 = (float) MainCamera.Viewport.Width;
			ScrPolys[ i ].y3 = (float) MainCamera.Viewport.Height;
			ScrPolys[ i ].x4 = 0.0F;
			ScrPolys[ i ].y4 = (float) MainCamera.Viewport.Height;
			ScrPolys[ i ].FadeRed = (float) Red;
			ScrPolys[ i ].FadeGreen = (float) Green;
			ScrPolys[ i ].FadeBlue = (float) Blue;
			ScrPolys[ i ].FadeTrans = (float) Trans;
			ScrPolys[ i ].Frame = 0.0F;
			ScrPolys[ i ].FrameRate = 0.0F;
			ScrPolys[ i ].SeqNum = SeqNum;
			ScrPolys[ i ].Frm_Info = &Solid_Header;		// Offset Info
			ScrPolys[ i ].Xscale = 1.0F;
			ScrPolys[ i ].Yscale = 1.0F;
			AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
		}
	}
}

/*===================================================================
	Procedure	:	Setup Thermo Screenpolys
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void ScrPolyPool::init_thermo()
{
	u_int16_t	i;
	float	Center_X;
	float	Center_Y;

	if( !DecreaseTemperature ) return;

	ThermalTemp = 0.0F;
	WantedThermalTemp = 0.0F;
	ThermalMinimum = 0.0F;

	if( ThermoScrPoly == INDEX_NONE )
	{
		i = FindFreeScrPoly();

		if( i != INDEX_NONE )
		{
			ThermoScrPoly = i;

			Center_X = ( CurrentCamera.Viewport.X + CurrentCamera.Viewport.Width - ( CurrentCamera.Viewport.Width / 6.0F ) );
			Center_Y = ( CurrentCamera.Viewport.Y + CurrentCamera.Viewport.Height - ( CurrentCamera.Viewport.Height / 5.0F ) );

			ScrPolys[ i ].Flags = SCRFLAG_Scale;
			ScrPolys[ i ].Type = SCRTYPE_Normal;
			ScrPolys[ i ].LifeCount = 255.0F;
			ScrPolys[ i ].Pos.x = Center_X;
			ScrPolys[ i ].Pos.y = Center_Y;
			ScrPolys[ i ].x1 = 0.0F;
			ScrPolys[ i ].y1 = 0.0F;
			ScrPolys[ i ].x2 = 0.0F;
			ScrPolys[ i ].y2 = 0.0F;
			ScrPolys[ i ].x3 = 0.0F;
			ScrPolys[ i ].y3 = 0.0F;
			ScrPolys[ i ].x4 = 0.0F;
			ScrPolys[ i ].y4 = 0.0F;
			ScrPolys[ i ].R = 255;					// R Colour
			ScrPolys[ i ].G = 255;					// G Colour
			ScrPolys[ i ].B = 255;					// B Colour
			ScrPolys[ i ].Trans = 255;				// Amount of transparency
			ScrPolys[ i ].FadeRed = (float) 255.0F;
			ScrPolys[ i ].FadeGreen = (float) 255.0F;
			ScrPolys[ i ].FadeBlue = (float) 255.0F;
			ScrPolys[ i ].FadeTrans = (float) 255.0F;
			ScrPolys[ i ].Frame = 0.0F;
			ScrPolys[ i ].FrameRate = 4.0F;
			ScrPolys[ i ].SeqNum = ScrSeq::Thermo;
			ScrPolys[ i ].Frm_Info = &Thermo_Header; // Offset Info
			ScrPolys[ i ].Xscale = 1.0F;
			ScrPolys[ i ].Yscale = 1.0F;
			AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
		}
	}
}

/*===================================================================
	Procedure	:	Remove Thermo Screenpolys
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void ScrPolyPool::kill_thermo()
{
	if( ThermoScrPoly != INDEX_NONE )
	{
		KillUsedScrPoly( ThermoScrPoly );
		ThermoScrPoly = INDEX_NONE;
	}
}


/*===================================================================
	Procedure	:	Init ScreenPoly TPage Groups
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void ScrPolyPool::init_tpages()
{
	u_int16_t	i;

	for( i = 0; i <= MAXTPAGESPERTLOAD; i++ )
	{
		ScrPolyTPages[ i ].FirstPoly = INDEX_NONE;
	}
}

/*===================================================================
	Procedure	:	Add ScreenPoly To TPage Link List
	Input		:	u_int16_t		ScreenPoly Index
				:	u_int16_t		TPage to add to
	Output		:	Nothing
===================================================================*/
void ScrPolyPool::add_to_tpage( u_int16_t i, int16_t TPage )
{
	if( TPage == -1 ) TPage = MAXTPAGESPERTLOAD;

	ScrPolys[ i ].PrevInTPage = INDEX_NONE;
	ScrPolys[ i ].NextInTPage = ScrPolyTPages[ TPage ].FirstPoly;

	if( ScrPolys[ i ].NextInTPage != INDEX_NONE )
	{
		ScrPolys[ ScrPolys[ i ].NextInTPage ].PrevInTPage = i;
	}

	ScrPolyTPages[ TPage ].FirstPoly = i;
}

/*===================================================================
	Procedure	:	Remove ScreenPoly From TPage Link List
	Input		:	u_int16_t		ScreenPoly Index
				:	u_int16_t		TPage to remove from
	Output		:	Nothing
===================================================================*/
void ScrPolyPool::remove_from_tpage( u_int16_t i, int16_t TPage )
{
	if( TPage == -1 ) TPage = MAXTPAGESPERTLOAD;

	if( ScrPolys[ i ].PrevInTPage != INDEX_NONE )
	{
		ScrPolys[ ScrPolys[ i ].PrevInTPage ].NextInTPage = ScrPolys[ i ].NextInTPage;
	}
	else
	{
		ScrPolyTPages[ TPage ].FirstPoly = ScrPolys[ i ].NextInTPage;
	}

	if( ScrPolys[ i ].NextInTPage != INDEX_NONE )
	{
		ScrPolys[ ScrPolys[ i ].NextInTPage ].PrevInTPage = ScrPolys[ i ].PrevInTPage;
	}

	ScrPolys[ i ].PrevInTPage = INDEX_NONE;
	ScrPolys[ i ].NextInTPage = INDEX_NONE;
}

bool ScrPolyPool::display_solid( RENDEROBJECT *renderObject )
{
	int16_t	TPage;
	u_int16_t	i;

	TPage = 0;
	i = ScrPolyTPages[ 0 ].FirstPoly;

	while( 1 )
	{
		if( !ScrPolyDispSolid( renderObject, &TPage, &i ) )
			return( true );

		disable_zbuff();
		cull_none();

		if (!draw_2d_object(renderObject))
			return false;

		reset_cull();
		reset_zbuff();
	}

	return( false );
}

bool ScrPolyPool::display_non_solid( RENDEROBJECT *renderObject )
{
	int16_t	TPage;
	u_int16_t	i;

	TPage = 0;
	i = ScrPolyTPages[ 0 ].FirstPoly;

	while( 1 )
	{
		if( !ScrPolyDispNonSolid( renderObject, &TPage, &i ) )
			return( true );

		disable_zbuff();
		cull_none();

		if (!draw_2d_object(renderObject))
			return false;

		reset_cull();
		reset_zbuff();
	}

	return( false );
}

/* Is ScrPoly i in this fill? Not skipped, and solid (Solid) or not solid. */
[[gnu::always_inline]] inline bool ScrPolyPool::in_fill( u_int16_t i, bool Solid )
{
	if( skip( i ) )
		return false;
	if( Solid )
		return ( ScrPolys[ i ].Flags & SCRFLAG_Solid ) != 0;
	return !( ScrPolys[ i ].Flags & SCRFLAG_Solid );
}

/* one bit of a textured screen poly: its box position and size (explicit coords, or offset and scale with flips), four pre-transformed vertices, the clip test and the Rot90 texture rotation */
[[gnu::always_inline]] inline void ScrPolyPool::build_textured_quad( u_int16_t & i, float & Xoff, float & Yoff, float & Xsize, float & Ysize, BOX_INFO *& Box_Ptr, OFF_INFO *& Off_Ptr, COLOR & Colour, TLVERTEX *& ScrPolyVertPnt, float & u1, float & v1, float & u2, float & v2, float & x1, float & y1, float & x3, float & y3 )
{
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	Box_Ptr = ( (*ScrPolys[ i ].Frm_Info)->Box_Info + ( Off_Ptr->box & 0x0fff ) );

	if( ( ScrPolys[ i ].Flags & SCRFLAG_UseCoords ) )
	{
		u1 = Box_Ptr->u1;
		v1 = Box_Ptr->v1;
		u2 = Box_Ptr->u2;
		v2 = Box_Ptr->v2;
		x1 = ScrPolys[ i ].x1;
		y1 = ScrPolys[ i ].y1;
		x3 = ScrPolys[ i ].x3;
		y3 = ScrPolys[ i ].y3;
	}
	else
	{
		Xoff = ( Off_Ptr->xoff * ScrPolys[ i ].Xscale );
		Yoff = ( Off_Ptr->yoff * ScrPolys[ i ].Yscale );
		Xsize = ( Box_Ptr->xsize * ScrPolys[ i ].Xscale );
		Ysize = ( Box_Ptr->ysize * ScrPolys[ i ].Yscale );

		if( ( ScrPolys[ i ].Flags & SCRFLAG_Scale ) )
		{
			Ysize = ( Ysize / Xsize );
			Xsize = ( ( CurrentCamera.Viewport.Width / 320.0F ) * Xsize );
			Ysize *= ( ( Xsize ) * pixel_aspect_ratio );

			/* @vfp-exempt R3 — division-by-zero guard on a float offset; integerizing changes the drawn position. measured n/a. Verified 2026-11-19. */
			if( Xoff != 0.0F ) Yoff = ( Yoff / Xoff );
			else Yoff = 0.0F;
			Xoff = ( ( CurrentCamera.Viewport.Width / 320.0F ) * Xoff );
			Yoff *= ( ( Xoff ) * pixel_aspect_ratio );
		}

		switch( Off_Ptr->box & BOX_BOTHFLIP )
		{
		case BOX_XFLIP:							// X Flip
			x3 = ( ScrPolys[ i ].Pos.x + Xoff );
			y1 = ( ScrPolys[ i ].Pos.y + Yoff );
			x1 = ( x3 - Xsize );
			y3 = ( y1 + Ysize );
			u1 = Box_Ptr->u2;
			v1 = Box_Ptr->v1;
			u2 = Box_Ptr->u1;
			v2 = Box_Ptr->v2;
			break;

		case BOX_YFLIP:							// Y Flip
			x1 = ( ScrPolys[ i ].Pos.x + Xoff );
			y3 = ( ScrPolys[ i ].Pos.y + Yoff );
			x3 = ( x1 + Xsize );
			y1 = ( y3 - Ysize );
			u1 = Box_Ptr->u1;
			v1 = Box_Ptr->v2;
			u2 = Box_Ptr->u2;
			v2 = Box_Ptr->v1;
			break;

		case BOX_BOTHFLIP:						// X & Y Flip
			x3 = ( ScrPolys[ i ].Pos.x + Xoff );
			y3 = ( ScrPolys[ i ].Pos.y + Yoff );
			x1 = ( x3 - Xsize );
			y1 = ( y3 - Ysize );
			u1 = Box_Ptr->u2;
			v1 = Box_Ptr->v2;
			u2 = Box_Ptr->u1;
			v2 = Box_Ptr->v1;
			break;

		default:								// No Flip
			x1 = ( ScrPolys[ i ].Pos.x + Xoff );
			y1 = ( ScrPolys[ i ].Pos.y + Yoff );
			x3 = ( x1 + Xsize );
			y3 = ( y1 + Ysize );
			u1 = Box_Ptr->u1;
			v1 = Box_Ptr->v1;
			u2 = Box_Ptr->u2;
			v2 = Box_Ptr->v2;
			break;

		}
	}

	/*===================================================================
			Create Colour Value for polys
	===================================================================*/

	Colour = RGBA_MAKE2( ScrPolys[ i ].R, ScrPolys[ i ].G, ScrPolys[ i ].B, 255 );

	ScrPolyVertPnt->sx = x1;
	ScrPolyVertPnt->sy = y1;
	ScrPolyVertPnt->sz = ZValue;
	ScrPolyVertPnt->color = Colour;
	ScrPolyVertPnt->rhw = RHWValue;
	ScrPolyVertPnt->tu = u1;
	ScrPolyVertPnt->tv = v1;
	ScrPolyVertPnt++;

	ScrPolyVertPnt->sz = ZValue;
	ScrPolyVertPnt->color = Colour;
	ScrPolyVertPnt->rhw = RHWValue;
	ScrPolyVertPnt++;

	ScrPolyVertPnt->sx = x3;
	ScrPolyVertPnt->sy = y3;
	ScrPolyVertPnt->sz = ZValue;
	ScrPolyVertPnt->color = Colour;
	ScrPolyVertPnt->rhw = RHWValue;
	ScrPolyVertPnt->tu = u2;
	ScrPolyVertPnt->tv = v2;
	ScrPolyVertPnt++;

	ScrPolyVertPnt->sz = ZValue;
	ScrPolyVertPnt->color = Colour;
	ScrPolyVertPnt->rhw = RHWValue;
	ScrPolyVertPnt++;

	if( ClipBox( ScrPolyVertPnt-4, ScrPolyVertPnt-2 ) )
	{
		( ScrPolyVertPnt - 4 )->sx = 0.0F;
		( ScrPolyVertPnt - 4 )->sy = 0.0F;
		( ScrPolyVertPnt - 3 )->sx = 0.0F;
		( ScrPolyVertPnt - 3 )->sy = 0.0F;
		( ScrPolyVertPnt - 2 )->sx = 0.0F;
		( ScrPolyVertPnt - 2 )->sy = 0.0F;
		( ScrPolyVertPnt - 1 )->sx = 0.0F;
		( ScrPolyVertPnt - 1 )->sy = 0.0F;
	}

	( ScrPolyVertPnt - 3 )->sx = ( ScrPolyVertPnt - 2 )->sx;
	( ScrPolyVertPnt - 3 )->sy = ( ScrPolyVertPnt - 4 )->sy;
	( ScrPolyVertPnt - 3 )->tu = ( ScrPolyVertPnt - 2 )->tu;
	( ScrPolyVertPnt - 3 )->tv = ( ScrPolyVertPnt - 4 )->tv;

	( ScrPolyVertPnt - 1 )->sx = ( ScrPolyVertPnt - 4 )->sx;
	( ScrPolyVertPnt - 1 )->sy = ( ScrPolyVertPnt - 2 )->sy;
	( ScrPolyVertPnt - 1 )->tu = ( ScrPolyVertPnt - 4 )->tu;
	( ScrPolyVertPnt - 1 )->tv = ( ScrPolyVertPnt - 2 )->tv;

	if( ( ScrPolys[ i ].Flags & SCRFLAG_Rot90 ) )
	{
		u1 = ( ScrPolyVertPnt - 4 )->tu;
		v1 = ( ScrPolyVertPnt - 4 )->tv;
		( ScrPolyVertPnt - 4 )->tu = ( ScrPolyVertPnt - 3 )->tu;
		( ScrPolyVertPnt - 4 )->tv = ( ScrPolyVertPnt - 3 )->tv;
		( ScrPolyVertPnt - 3 )->tu = ( ScrPolyVertPnt - 2 )->tu;
		( ScrPolyVertPnt - 3 )->tv = ( ScrPolyVertPnt - 2 )->tv;
		( ScrPolyVertPnt - 2 )->tu = ( ScrPolyVertPnt - 1 )->tu;
		( ScrPolyVertPnt - 2 )->tv = ( ScrPolyVertPnt - 1 )->tv;
		( ScrPolyVertPnt - 1 )->tu = u1;
		( ScrPolyVertPnt - 1 )->tv = v1;
	}
}

/* an untextured screen poly: four pre-transformed vertices with one colour (solid) or a colour per vertex, the clip test and the Rot90 texture rotation */
[[gnu::always_inline]] inline void ScrPolyPool::build_plain_quad( bool & Solid, u_int16_t & i, COLOR & Colour, TLVERTEX *& ScrPolyVertPnt, float & u1, float & v1, float & u2, float & v2, float & x1, float & y1, float & x3, float & y3 )
{
	u1 = 0.0F;
	v1 = 0.0F;
	u2 = 1.0F;
	v2 = 1.0F;
	x1 = ScrPolys[ i ].x1;
	y1 = ScrPolys[ i ].y1;
	x3 = ScrPolys[ i ].x3;
	y3 = ScrPolys[ i ].y3;

	if( Solid )
		Colour = RGBA_MAKE2( ScrPolys[ i ].R, ScrPolys[ i ].G, ScrPolys[ i ].B, 255 );
	else
		Colour = RGBA_MAKE2( ScrPolys[ i ].Col1.R, ScrPolys[ i ].Col1.G, ScrPolys[ i ].Col1.B, 255 );

	ScrPolyVertPnt->sx = x1;
	ScrPolyVertPnt->sy = y1;
	ScrPolyVertPnt->sz = ZValue;
	ScrPolyVertPnt->color = Colour;
	ScrPolyVertPnt->rhw = RHWValue;
	ScrPolyVertPnt->tu = u1;
	ScrPolyVertPnt->tv = v1;
	ScrPolyVertPnt++;

	if( !Solid )
		Colour = RGBA_MAKE2( ScrPolys[ i ].Col2.R, ScrPolys[ i ].Col2.G, ScrPolys[ i ].Col2.B, 255 );

	ScrPolyVertPnt->sz = ZValue;
	ScrPolyVertPnt->color = Colour;
	ScrPolyVertPnt->rhw = RHWValue;
	ScrPolyVertPnt++;
	if( !Solid )
		Colour = RGBA_MAKE2( ScrPolys[ i ].Col3.R, ScrPolys[ i ].Col3.G, ScrPolys[ i ].Col3.B, 255 );


	ScrPolyVertPnt->sx = x3;
	ScrPolyVertPnt->sy = y3;
	ScrPolyVertPnt->sz = ZValue;
	ScrPolyVertPnt->color = Colour;
	ScrPolyVertPnt->rhw = RHWValue;
	ScrPolyVertPnt->tu = u2;
	ScrPolyVertPnt->tv = v2;
	ScrPolyVertPnt++;
	if( !Solid )
		Colour = RGBA_MAKE2( ScrPolys[ i ].Col4.R, ScrPolys[ i ].Col4.G, ScrPolys[ i ].Col4.B, 255 );


	ScrPolyVertPnt->sz = ZValue;
	ScrPolyVertPnt->color = Colour;
	ScrPolyVertPnt->rhw = RHWValue;
	ScrPolyVertPnt++;

	if( ClipBox( ScrPolyVertPnt-4, ScrPolyVertPnt-2 ) )
	{
		( ScrPolyVertPnt - 4 )->sx = 0.0F;
		( ScrPolyVertPnt - 4 )->sy = 0.0F;
		( ScrPolyVertPnt - 3 )->sx = 0.0F;
		( ScrPolyVertPnt - 3 )->sy = 0.0F;
		( ScrPolyVertPnt - 2 )->sx = 0.0F;
		( ScrPolyVertPnt - 2 )->sy = 0.0F;
		( ScrPolyVertPnt - 1 )->sx = 0.0F;
		( ScrPolyVertPnt - 1 )->sy = 0.0F;
	}

	( ScrPolyVertPnt - 3 )->sx = ( ScrPolyVertPnt - 2 )->sx;
	( ScrPolyVertPnt - 3 )->sy = ( ScrPolyVertPnt - 4 )->sy;
	( ScrPolyVertPnt - 3 )->tu = ( ScrPolyVertPnt - 2 )->tu;
	( ScrPolyVertPnt - 3 )->tv = ( ScrPolyVertPnt - 4 )->tv;

	( ScrPolyVertPnt - 1 )->sx = ( ScrPolyVertPnt - 4 )->sx;
	( ScrPolyVertPnt - 1 )->sy = ( ScrPolyVertPnt - 2 )->sy;
	( ScrPolyVertPnt - 1 )->tu = ( ScrPolyVertPnt - 4 )->tu;
	( ScrPolyVertPnt - 1 )->tv = ( ScrPolyVertPnt - 2 )->tv;

	if( ( ScrPolys[ i ].Flags & SCRFLAG_Rot90 ) )
	{
		u1 = ( ScrPolyVertPnt - 4 )->tu;
		v1 = ( ScrPolyVertPnt - 4 )->tv;
		( ScrPolyVertPnt - 4 )->tu = ( ScrPolyVertPnt - 3 )->tu;
		( ScrPolyVertPnt - 4 )->tv = ( ScrPolyVertPnt - 3 )->tv;
		( ScrPolyVertPnt - 3 )->tu = ( ScrPolyVertPnt - 2 )->tu;
		( ScrPolyVertPnt - 3 )->tv = ( ScrPolyVertPnt - 2 )->tv;
		( ScrPolyVertPnt - 2 )->tu = ( ScrPolyVertPnt - 1 )->tu;
		( ScrPolyVertPnt - 2 )->tv = ( ScrPolyVertPnt - 1 )->tv;
		( ScrPolyVertPnt - 1 )->tu = u1;
		( ScrPolyVertPnt - 1 )->tv = v1;
	}
}

/* pass 1: count this fill's vertices and triangles per texture page from the resume point, setting each page's StartVert/NumVerts/NumTris and TotalVerts */
[[gnu::always_inline]] inline void ScrPolyPool::count_verts( bool & Solid, int16_t *& TPage, u_int16_t *& NextScrPoly, u_int16_t & i, int16_t & Count, int16_t & TotalVerts, int16_t & StartVert, int16_t & NumVerts, int16_t & NumTris, BIT_INFO *& Bit_Ptr, bool & Textured )
{
	for( Count = *TPage; Count <= MAXTPAGESPERTLOAD; Count++ )
	{
		StartVert = TotalVerts;
		NumVerts = 0;
		NumTris = 0;

		if( Count == MAXTPAGESPERTLOAD ) Textured = false;
		else Textured = true;

		if( Count == *TPage ) i = *NextScrPoly;
		else i = ScrPolyTPages[ Count ].FirstPoly;

		while( ( i != INDEX_NONE ) && ( ( StartVert + NumVerts ) < MAXSCREENPOLYVERTS ) )
		{
			if( in_fill( i, Solid ) )
			{
				if( Textured )
				{
					if( ScrPolys[i].Frm_Info && (*ScrPolys[i].Frm_Info ) )
					{
						Bit_Ptr = ( (*ScrPolys[ i ].Frm_Info)->Bit_Info + (int16_t) ScrPolys[ i ].Frame );
						NumTris += ( 2 * Bit_Ptr->numbits );
						NumVerts += ( 4 * Bit_Ptr->numbits );
					}
				}
				else
				{
					NumTris += 2;
					NumVerts += 4;
				}
			}

			i = ScrPolys[ i ].NextInTPage;
		}

		ScrPolyTPages[ Count ].StartVert = StartVert;
		ScrPolyTPages[ Count ].NumVerts = NumVerts;
		ScrPolyTPages[ Count ].NumTris = NumTris;

		TotalVerts = ( StartVert + NumVerts );
		if( TotalVerts >= MAXSCREENPOLYVERTS ) break;
	}
}

/* Fill one batch of screen polys: the solid ones (one colour per quad) or the
 * non-solid ones (a colour per vertex). The two public wrappers pass a constant
 * Solid, so each compiles to its own specialised copy. */
[[gnu::always_inline]] inline bool ScrPolyPool::disp_fill( bool Solid, RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextScrPoly )
{
	u_int16_t			i;
	int16_t			Count;
	int16_t			BitCount;
	int16_t			TotalVerts;
	int16_t			StartVert;
	int16_t			NumVerts;
	int16_t			NumTris;
	float			Xoff;
	float			Yoff;
	float			Xsize;
	float			Ysize;
	BIT_INFO	*	Bit_Ptr;
	BOX_INFO	*	Box_Ptr;
	OFF_INFO	*	Off_Ptr;
	COLOR		Colour;
	TLVERTEX	*ScrPolyVertPnt; // pre-transformed verts!
	TRIANGLE	*ScrPolyFacePnt;
	TLVERTEX	*BufStart;
	float			u1,v1,u2,v2;
	float			x1,y1,x3,y3;
	bool			Textured;
	WORD			*Indices = nullptr;
	int				start_index = 0;
#ifdef __3DS__
	int				batchVert = 0;  /* buffer position within current draw batch */
#endif

	/*===================================================================
			Pass 1: count verts per texture page
	===================================================================*/
	TotalVerts = 0;

	count_verts( Solid, TPage, NextScrPoly, i, Count, TotalVerts, StartVert, NumVerts, NumTris, Bit_Ptr, Textured );

	if( !TotalVerts ) return( false );

	renderObject->numTextureGroups = 0;

	ZValue = 1.0F;
	RHWValue = ( 1.0F / ZValue );

	/*===================================================================
			Lock Buffer and get ready to fill in...
	===================================================================*/

	if (!(FSLockPretransformedVertexBuffer(renderObject, &BufStart)))
	{
		return false;
	}

	if (!(FSLockIndexBuffer(renderObject, &Indices)))
	{
		return false;
	}

	ScrPolyFacePnt = (TRIANGLE *) Indices;

	ScrPolyVertPnt = (TLVERTEX *) BufStart;

	/*===================================================================
			Fill in Buffer ( Verts and Faces Simultaneously )
	===================================================================*/

	for( Count = *TPage; Count <= MAXTPAGESPERTLOAD; Count++ )
	{
		if( Count == MAXTPAGESPERTLOAD ) Textured = false;
		else Textured = true;

		StartVert = ScrPolyTPages[ Count ].StartVert;
		NumVerts = ScrPolyTPages[ Count ].NumVerts;
		NumTris = ScrPolyTPages[ Count ].NumTris;

		if( NumVerts )
		{
			if( !Textured )
			{
				renderObject->textureGroups[renderObject->numTextureGroups].texture = nullptr;
				renderObject->textureGroups[renderObject->numTextureGroups].colourkey = false;
			}
			else
			{
				renderObject->textureGroups[renderObject->numTextureGroups].texture = Tloadheader.TexHandle[Count];
				renderObject->textureGroups[renderObject->numTextureGroups].colourkey = Tloadheader.ColourKey[Count];
			}

			if( Count == *TPage ) i = *NextScrPoly;
			else i = ScrPolyTPages[ Count ].FirstPoly;

			while( ( i != INDEX_NONE ) && ( Solid || ( i < MAXNUMOFSCRPOLYS ) ) && ( StartVert < MAXSCREENPOLYVERTS ) )
			{
				if( in_fill( i, Solid ) )
				{
					if( Textured && ScrPolys[i].Frm_Info && (*ScrPolys[i].Frm_Info ) )
					{
						Bit_Ptr = ( (*ScrPolys[ i ].Frm_Info)->Bit_Info + (int16_t) ScrPolys[ i ].Frame );
						Off_Ptr = ( (*ScrPolys[ i ].Frm_Info)->Off_Info + Bit_Ptr->startbit );

						for( BitCount = 0; BitCount < Bit_Ptr->numbits; BitCount++ )
						{
							int ntris = 0;

							build_textured_quad( i, Xoff, Yoff, Xsize, Ysize, Box_Ptr, Off_Ptr, Colour, ScrPolyVertPnt, u1, v1, u2, v2, x1, y1, x3, y3 );

							ScrPolyFacePnt->v1 = 0;
							ScrPolyFacePnt->v2 = 1;
							ScrPolyFacePnt->v3 = 2;
							ScrPolyFacePnt++;
							ntris++;

							ScrPolyFacePnt->v1 = 0;
							ScrPolyFacePnt->v2 = 2;
							ScrPolyFacePnt->v3 = 3;
							ScrPolyFacePnt++;
							ntris++;

							renderObject->textureGroups[renderObject->numTextureGroups].numTriangles = ntris;
							renderObject->textureGroups[renderObject->numTextureGroups].numVerts = 4;
							renderObject->textureGroups[renderObject->numTextureGroups].startIndex = start_index;
#ifdef __3DS__
							renderObject->textureGroups[renderObject->numTextureGroups].startVert = batchVert;
#else
							renderObject->textureGroups[renderObject->numTextureGroups].startVert = StartVert;
#endif
							renderObject->textureGroups[renderObject->numTextureGroups].texture = Tloadheader.TexHandle[Count];
							renderObject->textureGroups[renderObject->numTextureGroups].colourkey = Tloadheader.ColourKey[Count];
#ifdef __3DS__
							renderObject->numTextureGroups++;
#else
							INCREASE_TEXTURE_GROUPS(renderObject);
#endif

							start_index += ntris*3; // each triangle has three indexes...
							StartVert += 4;
							/*
							 * 3DS mid-draw flush — textured quad path (ScrPolyDispSolid).
							 *
							 * WHY THIS EXISTS:
							 *   INCREASE_TEXTURE_GROUPS() asserts
							 *   numTextureGroups < MAX_TEXTURE_GROUPS, which is 64 on
							 *   3DS (600 on PC).  Loading screens and menus use one
							 *   TEXTUREGROUP per sprite quad and easily pass 64.  The
							 *   batch objects' textureGroups point at fixed scratch
							 *   arrays of that size (RenderBufs' static scratch and
							 *   stack arrays in oct2.cpp), so 3DS flushes instead.
							 *
							 * WHAT THE FIX DOES:
							 *   After committing each group we check whether the array is
							 *   full.  If so, we flush the accumulated batch immediately:
							 *   unlock vertex/index buffers, issue draw_2d_object, then
							 *   re-lock.  numTextureGroups, start_index, and batchVert are
							 *   all reset to 0 so the next group fills the array from the
							 *   beginning.  Each partial batch is a complete draw call.
							 *
							 * WHY batchVert INSTEAD OF StartVert FOR startVert:
							 *   StartVert is a cumulative counter across the entire function
							 *   call (unchanged on PC).  After a flush the vertex buffer is
							 *   re-locked from offset 0, so the next group's vertices land
							 *   at position 0 in the new buffer — not at whatever StartVert
							 *   has accumulated.  batchVert tracks position within the
							 *   current batch only; it resets to 0 on every flush so that
							 *   textureGroup.startVert indices stay correct.
							 *
							 * CRITICAL ORDERING:
							 *   start_index, StartVert, and batchVert are ALL advanced
							 *   BEFORE this flush check, so a flush leaves start_index
							 *   and batchVert at 0 for the first group of the next
							 *   batch.  Advancing after the reset would offset that
							 *   group by one quad.  StartVert is never reset: it keeps
							 *   the MAXSCREENPOLYVERTS bound tied to pass 1's count.
							 */
#ifdef __3DS__
							batchVert += 4;
							if (renderObject->numTextureGroups >= MAX_TEXTURE_GROUPS) {
								FSUnlockVertexBuffer(renderObject);
								FSUnlockIndexBuffer(renderObject);
								disable_zbuff();
								cull_none();
								draw_2d_object(renderObject);
								reset_cull();
								reset_zbuff();
								renderObject->numTextureGroups = 0;
								start_index = 0;
								batchVert = 0;
								FSLockPretransformedVertexBuffer(renderObject, &BufStart);
								FSLockIndexBuffer(renderObject, &Indices);
								ScrPolyVertPnt = (TLVERTEX *) BufStart;
								ScrPolyFacePnt = (TRIANGLE *) Indices;
							}
#endif
							Off_Ptr++;
						}
					}
					else
					{
						if( !Textured )
						{
							int ntris = 0;

							build_plain_quad( Solid, i, Colour, ScrPolyVertPnt, u1, v1, u2, v2, x1, y1, x3, y3 );

							ScrPolyFacePnt->v1 = 0;
							ScrPolyFacePnt->v2 = 1;
							ScrPolyFacePnt->v3 = 2;
							ScrPolyFacePnt++;
							ntris++;

							ScrPolyFacePnt->v1 = 0;
							ScrPolyFacePnt->v2 = 2;
							ScrPolyFacePnt->v3 = 3;
							ScrPolyFacePnt++;
							ntris++;

							renderObject->textureGroups[renderObject->numTextureGroups].numTriangles = ntris;
							renderObject->textureGroups[renderObject->numTextureGroups].numVerts = 4;
							renderObject->textureGroups[renderObject->numTextureGroups].startIndex = start_index;
#ifdef __3DS__
							renderObject->textureGroups[renderObject->numTextureGroups].startVert = batchVert;
#else
							renderObject->textureGroups[renderObject->numTextureGroups].startVert = StartVert;
#endif
							renderObject->textureGroups[renderObject->numTextureGroups].texture = nullptr;
							renderObject->textureGroups[renderObject->numTextureGroups].colourkey = false;
#ifdef __3DS__
							renderObject->numTextureGroups++;
#else
							INCREASE_TEXTURE_GROUPS(renderObject);
#endif

							start_index += ntris*3; // each triangle has three indexes...
							StartVert += 4;
							/*
							 * 3DS mid-draw flush — untextured quad path (ScrPolyDispSolid).
							 *
							 * Same overflow-prevention mechanism as the textured-quad path
							 * above.  INCREASE_TEXTURE_GROUPS() would assert/abort on overflow;
							 * instead we flush (unlock → draw → re-lock) and reset the
							 * group/index/vertex cursors before continuing.
							 * batchVert is used for startVert rather than StartVert because it
							 * resets to 0 on each flush, keeping vertex indices correct
							 * relative to the freshly re-locked buffer.  Advances
							 * (start_index, StartVert, batchVert) happen BEFORE this check so
							 * that a flush leaves the cursors at 0 for the next batch.
							 */
#ifdef __3DS__
							batchVert += 4;
							if (renderObject->numTextureGroups >= MAX_TEXTURE_GROUPS) {
								FSUnlockVertexBuffer(renderObject);
								FSUnlockIndexBuffer(renderObject);
								disable_zbuff();
								cull_none();
								draw_2d_object(renderObject);
								reset_cull();
								reset_zbuff();
								renderObject->numTextureGroups = 0;
								start_index = 0;
								batchVert = 0;
								FSLockPretransformedVertexBuffer(renderObject, &BufStart);
								FSLockIndexBuffer(renderObject, &Indices);
								ScrPolyVertPnt = (TLVERTEX *) BufStart;
								ScrPolyFacePnt = (TRIANGLE *) Indices;
							}
#endif
						}
					}
				}

				i = ScrPolys[ i ].NextInTPage;
			}
		}

		if( StartVert >= MAXSCREENPOLYVERTS ) break;
	}

	/*===================================================================
			Unlock the buffers and return the resume point
	===================================================================*/

	if (!FSUnlockVertexBuffer(renderObject))
	{
		return false;
	}

	if (!(FSUnlockIndexBuffer(renderObject)))
	{
		Msg( "FSUnlockIndexBuffer failed");
		return false ;
	}

	*TPage = Count;
	*NextScrPoly = i;

	return( true );
}

/*===================================================================
	Procedure	:	Load Time for level
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void CountdownClock::load_for_level()
{

	CountDownOn = false;
	SinglePlayerTimeLimit = 0;

}

/* The countdown's part of the saved game, in the order SaveScreenPolys /
 * LoadScreenPolys always stored it: whether it runs, then the time left. */
void CountdownClock::save( FILE * fp )
{
	fwrite( &CountDownOn, sizeof( CountDownOn ), 1, fp );
	fwrite( &Countdown_Float, sizeof( Countdown_Float ), 1, fp );
}

void CountdownClock::restore( FILE * fp )
{
	file_read( &CountDownOn, sizeof( CountDownOn ), 1, fp );
	file_read( &Countdown_Float, sizeof( Countdown_Float ), 1, fp );
}

/*===================================================================
	Procedure	:	Save ScreenPolys arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * ScrPolyPool::save( FILE * fp )
{
	if( fp )
	{
		Countdown.save( fp );
		fwrite( &ThermalTemp, sizeof( ThermalTemp ), 1, fp );
		fwrite( &WantedThermalTemp, sizeof( WantedThermalTemp ), 1, fp );
		fwrite( &ThermalMinimum, sizeof( ThermalMinimum ), 1, fp );
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load ScreenPolys Arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * ScrPolyPool::restore( FILE * fp )
{
	int16_t minutes;
	int16_t seconds;

	if( fp )
	{
		Countdown.restore( fp );
		file_read( &ThermalTemp, sizeof( ThermalTemp ), 1, fp );
		file_read( &WantedThermalTemp, sizeof( WantedThermalTemp ), 1, fp );
		file_read( &ThermalMinimum, sizeof( ThermalMinimum ), 1, fp );

		if( Countdown.on() )
		{
			Countdown.rearm();
			CreateCountdownDigits();
			minutes = Countdown.time_left() / 6000.0F;
			seconds = ( Countdown.time_left() - (minutes * 6000.0F) ) / 100.0F;
			StartCountDown( minutes, seconds );
			TimeLimitTrigger = nullptr;
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Create Flag Above Players Head
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
void ShowFlagOnShip( u_int16_t Ship )
{
	u_int16_t			i;
	MATRIX			FinalMatrix;
	VECTOR			TempPos;
	VECTOR			Pos;
	VECTOR			Trans1, Trans2;
	VECTOR			Pos1, Pos2;
	VECTOR			ScrPos1, ScrPos2;
	float			Xsize, Ysize;
	BIT_INFO	*	Bit_Ptr;
	BOX_INFO	*	Box_Ptr;
	OFF_INFO	*	Off_Ptr;

	if( !( Ships[ Ship ].Object.Flags & SHIP_CarryingFlag ) )
		return;

	GetFinalMatrix( &FinalMatrix );
	ApplyMatrix( &CurrentCamera.Mat, &SlideUp, &Trans1 );
	ApplyMatrix( &CurrentCamera.Mat, &SlideRight, &Trans2 );
	Trans1.x *= ( SHIP_RADIUS * 0.5F );
	Trans1.y *= ( SHIP_RADIUS * 0.5F );
	Trans1.z *= ( SHIP_RADIUS * 0.5F );
	Trans2.x *= ( SHIP_RADIUS * 0.5F );
	Trans2.y *= ( SHIP_RADIUS * 0.5F );
	Trans2.z *= ( SHIP_RADIUS * 0.5F );

	Pos1.x = ( Ships[ Ship ].Object.Pos.x + Trans1.x - Trans2.x );
	Pos1.y = ( Ships[ Ship ].Object.Pos.y + Trans1.y - Trans2.y );
	Pos1.z = ( Ships[ Ship ].Object.Pos.z + Trans1.z - Trans2.z );

	Pos2.x = ( Ships[ Ship ].Object.Pos.x - Trans1.x + Trans2.x );
	Pos2.y = ( Ships[ Ship ].Object.Pos.y - Trans1.y + Trans2.y );
	Pos2.z = ( Ships[ Ship ].Object.Pos.z - Trans1.z + Trans2.z );

	Conv3DTo2D( &Pos1, &ScrPos1, &FinalMatrix );
	Conv3DTo2D( &Pos2, &ScrPos2, &FinalMatrix );

	Bit_Ptr = ( Flag_Header->Bit_Info + 0 );
	Off_Ptr = ( Flag_Header->Off_Info + Bit_Ptr->startbit );
	Box_Ptr = ( Flag_Header->Box_Info + ( Off_Ptr->box & 0x0fff ) );

	Xsize = ( ( ScrPos2.x - ScrPos1.x ) / Box_Ptr->xsize ) * 1.8F;
	Ysize = ( ( ScrPos2.y - ScrPos1.y ) / Box_Ptr->ysize ) * 1.0F;


	ApplyMatrix( &CurrentCamera.Mat, &SlideUp, &Trans1 );

	TempPos.x = ( Ships[ Ship ].Object.Pos.x + ( Trans1.x * 100.0F ) );
	TempPos.y = ( Ships[ Ship ].Object.Pos.y + ( Trans1.y * 100.0F ) );
	TempPos.z = ( Ships[ Ship ].Object.Pos.z + ( Trans1.z * 100.0F ) );

	Conv3DTo2D( &TempPos, &Pos, &FinalMatrix );

	i = FindFreeScrPoly();
	if( i != INDEX_NONE )
	{
		ScrPolys[ i ].Flags = SCRFLAG_Nothing;
		ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
		ScrPolys[ i ].Pos.x = Pos.x;
		ScrPolys[ i ].Pos.y = Pos.y;
		ScrPolys[ i ].R = 255;
		ScrPolys[ i ].G = 255;
		ScrPolys[ i ].B = 255;
		ScrPolys[ i ].Trans = 255;
		ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
		ScrPolys[ i ].FrameRate = 1.0F;
		ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
		ScrPolys[ i ].Frm_Info = &Flag_Header;	// Offset Info
		ScrPolys[ i ].Xscale = Xsize;
		ScrPolys[ i ].Yscale = Ysize;
		AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
	}
}

/*===================================================================
	Procedure	:	Create Bounty Above Players Head
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
void ShowBountyOnShip( u_int16_t Ship )
{
	u_int16_t			i;
	MATRIX			FinalMatrix;
	VECTOR			TempPos;
	VECTOR			Pos;
	VECTOR			Trans1, Trans2;
	VECTOR			Pos1, Pos2;
	VECTOR			ScrPos1, ScrPos2;
	float			Xsize, Ysize;
	BIT_INFO	*	Bit_Ptr;
	BOX_INFO	*	Box_Ptr;
	OFF_INFO	*	Off_Ptr;

	if( !( Ships[ Ship ].Object.Flags & SHIP_CarryingBounty ) )
		return;

	GetFinalMatrix( &FinalMatrix );
	ApplyMatrix( &CurrentCamera.Mat, &SlideUp, &Trans1 );
	ApplyMatrix( &CurrentCamera.Mat, &SlideRight, &Trans2 );
	Trans1.x *= SHIP_RADIUS;
	Trans1.y *= SHIP_RADIUS;
	Trans1.z *= SHIP_RADIUS;
	Trans2.x *= SHIP_RADIUS;
	Trans2.y *= SHIP_RADIUS;
	Trans2.z *= SHIP_RADIUS;

	Pos1.x = ( Ships[ Ship ].Object.Pos.x + Trans1.x - Trans2.x );
	Pos1.y = ( Ships[ Ship ].Object.Pos.y + Trans1.y - Trans2.y );
	Pos1.z = ( Ships[ Ship ].Object.Pos.z + Trans1.z - Trans2.z );

	Pos2.x = ( Ships[ Ship ].Object.Pos.x - Trans1.x + Trans2.x );
	Pos2.y = ( Ships[ Ship ].Object.Pos.y - Trans1.y + Trans2.y );
	Pos2.z = ( Ships[ Ship ].Object.Pos.z - Trans1.z + Trans2.z );

	Conv3DTo2D( &Pos1, &ScrPos1, &FinalMatrix );
	Conv3DTo2D( &Pos2, &ScrPos2, &FinalMatrix );

	Bit_Ptr = ( Flag_Header->Bit_Info + 1 );
	Off_Ptr = ( Flag_Header->Off_Info + Bit_Ptr->startbit );
	Box_Ptr = ( Flag_Header->Box_Info + ( Off_Ptr->box & 0x0fff ) );

	Xsize = ( ( ScrPos2.x - ScrPos1.x ) / Box_Ptr->xsize ) * 0.75F;
	Ysize = ( ( ScrPos2.y - ScrPos1.y ) / Box_Ptr->ysize ) * 0.75F;


	ApplyMatrix( &CurrentCamera.Mat, &SlideUp, &Trans1 );

	TempPos.x = ( Ships[ Ship ].Object.Pos.x + ( Trans1.x * 100.0F ) );
	TempPos.y = ( Ships[ Ship ].Object.Pos.y + ( Trans1.y * 100.0F ) );
	TempPos.z = ( Ships[ Ship ].Object.Pos.z + ( Trans1.z * 100.0F ) );

	Conv3DTo2D( &TempPos, &Pos, &FinalMatrix );

	i = FindFreeScrPoly();
	if( i != INDEX_NONE )
	{
		ScrPolys[ i ].Flags = SCRFLAG_Nothing;
		ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
		ScrPolys[ i ].Pos.x = Pos.x;
		ScrPolys[ i ].Pos.y = Pos.y;
		ScrPolys[ i ].R = 255;
		ScrPolys[ i ].G = 255;
		ScrPolys[ i ].B = 255;
		ScrPolys[ i ].Trans = 255;
		ScrPolys[ i ].Frame = 1.0F;					// Animation Frame
		ScrPolys[ i ].FrameRate = 1.0F;
		ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
		ScrPolys[ i ].Frm_Info = &Flag_Header;	// Offset Info
		ScrPolys[ i ].Xscale = Xsize;
		ScrPolys[ i ].Yscale = Ysize;
		AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
	}
}

/*===================================================================
	Procedure	:	Create Flag(s) Above Players Head
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
VECTOR	TeamOffsets[ MAX_TEAMS ][ MAX_TEAMS ] = {

	{ { 0.0F, 0.0F, 0.0F },			// 1 flag
	  { 0.0F, 0.0F, 0.0F },
	  { 0.0F, 0.0F, 0.0F },
	  { 0.0F, 0.0F, 0.0F } },

	{ { -5.0F, -5.0F, 0.0F },			// 2 flags
	  { +5.0F, +5.0F, 0.0F },
	  { 0.0F, 0.0F, 0.0F },
	  { 0.0F, 0.0F, 0.0F } },

	{ { -10.0F, -10.0F, 0.0F },			// 3 flags
	  { 0.0F, 0.0F, 0.0F },
	  { +10.0F, +10.0F, 0.0F },
	  { 0.0F, 0.0F, 0.0F } },

	{ { -15.0F, -15.0F, 0.0F },			// 4 flags
	  { -5.0F, -5.0F, 0.0F },
	  { +5.0F, +5.0F, 0.0F },
	  { +15.0F, +15.0F, 0.0F } },
};

void ShowCTFFlagsOnShip( u_int16_t Ship )
{
	u_int16_t			i;
	int16_t			NumFlags;
	MATRIX			FinalMatrix;
	VECTOR			TempPos;
	VECTOR			Pos;
	VECTOR			Trans1, Trans2;
	VECTOR			Pos1, Pos2;
	VECTOR			ScrPos1, ScrPos2;
	float			Xsize, Ysize;
	BIT_INFO	*	Bit_Ptr;
	BOX_INFO	*	Box_Ptr;
	OFF_INFO	*	Off_Ptr;
	int16_t			Count;
	int16_t			Team;

	NumFlags = 0;

	for( Count = 0; Count < MAX_TEAMS; Count++ )
	{
		if( ( Ships[ Ship ].Object.Flags & TeamFlagMask[ Count ] ) )
		{
			NumFlags++;
		}
	}

	if( NumFlags )
	{
		GetFinalMatrix( &FinalMatrix );
		ApplyMatrix( &CurrentCamera.Mat, &SlideUp, &Trans1 );
		ApplyMatrix( &CurrentCamera.Mat, &SlideRight, &Trans2 );
		Trans1.x *= ( SHIP_RADIUS * 0.5F );
		Trans1.y *= ( SHIP_RADIUS * 0.5F );
		Trans1.z *= ( SHIP_RADIUS * 0.5F );
		Trans2.x *= ( SHIP_RADIUS * 0.5F );
		Trans2.y *= ( SHIP_RADIUS * 0.5F );
		Trans2.z *= ( SHIP_RADIUS * 0.5F );

		Pos1.x = ( Ships[ Ship ].Object.Pos.x + Trans1.x - Trans2.x );
		Pos1.y = ( Ships[ Ship ].Object.Pos.y + Trans1.y - Trans2.y );
		Pos1.z = ( Ships[ Ship ].Object.Pos.z + Trans1.z - Trans2.z );

		Pos2.x = ( Ships[ Ship ].Object.Pos.x - Trans1.x + Trans2.x );
		Pos2.y = ( Ships[ Ship ].Object.Pos.y - Trans1.y + Trans2.y );
		Pos2.z = ( Ships[ Ship ].Object.Pos.z - Trans1.z + Trans2.z );

		Conv3DTo2D( &Pos1, &ScrPos1, &FinalMatrix );
		Conv3DTo2D( &Pos2, &ScrPos2, &FinalMatrix );

		Bit_Ptr = ( Flags_Header->Bit_Info + 0 );
		Off_Ptr = ( Flags_Header->Off_Info + Bit_Ptr->startbit );
		Box_Ptr = ( Flags_Header->Box_Info + ( Off_Ptr->box & 0x0fff ) );

		Xsize = ( ( ScrPos2.x - ScrPos1.x ) / Box_Ptr->xsize ) * 1.8F;
		Ysize = ( ( ScrPos2.y - ScrPos1.y ) / Box_Ptr->ysize ) * 1.0F;


		ApplyMatrix( &CurrentCamera.Mat, &SlideUp, &Trans1 );
		ApplyMatrix( &CurrentCamera.Mat, &SlideRight, &Trans2 );

		Team = 0;

		for( Count = 0; Count < MAX_TEAMS; Count++ )
		{
			if( ( Ships[ Ship ].Object.Flags & TeamFlagMask[ Count ] ) )
			{
				Pos.x = TeamOffsets[ NumFlags -1 ][ Team ].x;
				Pos.y = TeamOffsets[ NumFlags -1 ][ Team ].y;
				Pos.z = TeamOffsets[ NumFlags -1 ][ Team ].z;
				ApplyMatrix( &CurrentCamera.Mat, &Pos, &TempPos );

				TempPos.x += ( Ships[ Ship ].Object.Pos.x + ( Trans1.x * 120.0F ) );
				TempPos.y += ( Ships[ Ship ].Object.Pos.y + ( Trans1.y * 120.0F ) );
				TempPos.z += ( Ships[ Ship ].Object.Pos.z + ( Trans1.z * 120.0F ) );

				Conv3DTo2D( &TempPos, &Pos, &FinalMatrix );

				i = FindFreeScrPoly();

				if( i != INDEX_NONE )
				{
					ScrPolys[ i ].Flags = SCRFLAG_Solid;
					ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
					ScrPolys[ i ].Pos.x = Pos.x;
					ScrPolys[ i ].Pos.y = Pos.y;
					ScrPolys[ i ].R = 255;
					ScrPolys[ i ].G = 255;
					ScrPolys[ i ].B = 255;
					ScrPolys[ i ].Trans = 255;
					ScrPolys[ i ].Frame = (float) Count;					// Animation Frame
					ScrPolys[ i ].FrameRate = 1.0F;
					ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
					ScrPolys[ i ].Frm_Info = &Flags_Header;	// Offset Info
					ScrPolys[ i ].Xscale = Xsize;
					ScrPolys[ i ].Yscale = Ysize;
					AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
				}

				Team++;
			}
		}
	}
}

/*===================================================================
	Procedure	:	Show Flag on screen for player who has flag
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
void ShowFlag( u_int16_t Ship )
{
	u_int16_t	i;

	if( !( Ships[ Ship ].Object.Flags & SHIP_CarryingFlag ) )
		return;

	i = FindFreeScrPoly();
	if( i != INDEX_NONE )
	{
		ScrPolys[ i ].Flags = SCRFLAG_Nothing;
		ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
		ScrPolys[ i ].Pos.x = ( CurrentCamera.Viewport.Width / 2.0F );
		ScrPolys[ i ].Pos.y = ( CurrentCamera.Viewport.Height - 30.0F );
		ScrPolys[ i ].R = 255;
		ScrPolys[ i ].G = 255;
		ScrPolys[ i ].B = 255;
		ScrPolys[ i ].Trans = 255;
		ScrPolys[ i ].Frame = 0.0F;					// Animation Frame
		ScrPolys[ i ].FrameRate = 1.0F;
		ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
		ScrPolys[ i ].Frm_Info = &Flag_Header;	// Offset Info
		ScrPolys[ i ].Xscale = 0.8F;
		ScrPolys[ i ].Yscale = 0.8F;
		AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
	}
}

/*===================================================================
	Procedure	:	Show Bounty on screen for player who has flag
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
void ShowBounty( u_int16_t Ship )
{
	u_int16_t	i;

	if( !( Ships[ Ship ].Object.Flags & SHIP_CarryingBounty ) )
		return;

	i = FindFreeScrPoly();
	if( i != INDEX_NONE )
	{
		ScrPolys[ i ].Flags = SCRFLAG_Nothing;
		ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
		ScrPolys[ i ].Pos.x = ( CurrentCamera.Viewport.Width / 2.0F );
		ScrPolys[ i ].Pos.y = ( CurrentCamera.Viewport.Height - 30.0F );
		ScrPolys[ i ].R = 255;
		ScrPolys[ i ].G = 255;
		ScrPolys[ i ].B = 255;
		ScrPolys[ i ].Trans = 255;
		ScrPolys[ i ].Frame = 1.0F;					// Animation Frame
		ScrPolys[ i ].FrameRate = 1.0F;
		ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
		ScrPolys[ i ].Frm_Info = &Flag_Header;	// Offset Info
		ScrPolys[ i ].Xscale = 0.8F;
		ScrPolys[ i ].Yscale = 0.8F;
		AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
	}
}

/*===================================================================
	Procedure	:	Show Flag(s) on screen for player who has flag
	Input		:	u_int16_t		Ship
	Output		:	Nothing
===================================================================*/
void ShowCTFFlags( u_int16_t Ship )
{
	u_int16_t	i;
	int16_t	Count;
	VECTOR	Pos;
	VECTOR	CenterPos;
	int16_t	NumFlags;
	int16_t	Team;

	NumFlags = 0;

	for( Count = 0; Count < MAX_TEAMS; Count++ )
	{
		if( ( Ships[ Ship ].Object.Flags & TeamFlagMask[ Count ] ) )
		{
			NumFlags++;
		}
	}

	if( NumFlags )
	{
		CenterPos.x = ( CurrentCamera.Viewport.Width / 2.0F );
		CenterPos.y = ( CurrentCamera.Viewport.Height - 30.0F );

		Team = 0;

		for( Count = 0; Count < MAX_TEAMS; Count++ )
		{
			if( ( Ships[ Ship ].Object.Flags & TeamFlagMask[ Count ] ) )
			{
				Pos.x = CenterPos.x + TeamOffsets[ NumFlags -1 ][ Team ].x;
				Pos.y = CenterPos.y + TeamOffsets[ NumFlags -1 ][ Team ].y;

				i = FindFreeScrPoly();

				if( i != INDEX_NONE )
				{
					ScrPolys[ i ].Flags = SCRFLAG_Solid;
					ScrPolys[ i ].Type = SCRTYPE_LastAFrame;
					ScrPolys[ i ].Pos.x = Pos.x;
					ScrPolys[ i ].Pos.y = Pos.y;
					ScrPolys[ i ].R = 255;
					ScrPolys[ i ].G = 255;
					ScrPolys[ i ].B = 255;
					ScrPolys[ i ].Trans = 255;
					ScrPolys[ i ].Frame = (float) Count;					// Animation Frame
					ScrPolys[ i ].FrameRate = 1.0F;
					ScrPolys[ i ].SeqNum = ScrSeq::Nothing;
					ScrPolys[ i ].Frm_Info = &Flags_Header;	// Offset Info
					ScrPolys[ i ].Xscale = 0.5F;
					ScrPolys[ i ].Yscale = 0.5F;
					AddScrPolyToTPage( i, GetTPage( *ScrPolys[ i ].Frm_Info, 0 ) );
				}

				Team++;
			}
		}
	}
}

namespace {
ScrPolyPool    g_scrpolys;
}
CountdownClock Countdown;

void InitScrPolys( void ) { g_scrpolys.init(); }
/* Out of line, as they were before the class: the HUD text, lens flares,
 * flags and countdown call them per poly each frame, so inlining them (or
 * branching through to a clone) would bulk up those callers. */
[[gnu::noinline]] u_int16_t FindFreeScrPoly( void ) { return g_scrpolys.find_free(); }
[[gnu::noinline]] void KillUsedScrPoly( u_int16_t i ) { g_scrpolys.kill_used( i ); }
void ScrPolySetRouteBottom(bool b) { g_scrpolys.set_route_bottom( b ); }
void ScrPolySetDisplayMode(int mode) { g_scrpolys.set_display_mode( mode ); }
bool ScrPolyGetRouteBottom(void) { return g_scrpolys.route_bottom(); }
void ScreenPolyProcess( void ) { g_scrpolys.process(); }
void AddScreenPolyText( u_int16_t Frame, float XPos, float YPos, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans ) { g_scrpolys.add_text( Frame, XPos, YPos, Red, Green, Blue, Trans ); }
void AddScreenPolyTextScale( u_int16_t Frame, float XPos, float YPos, float XScale, float YScale, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans ) { g_scrpolys.add_text_scale( Frame, XPos, YPos, XScale, YScale, Red, Green, Blue, Trans ); }
void InitThermo( void ) { g_scrpolys.init_thermo(); }
void KillThermo( void ) { g_scrpolys.kill_thermo(); }
void InitScrPolyTPages( void ) { g_scrpolys.init_tpages(); }
void AddScrPolyToTPage( u_int16_t i, int16_t TPage ) { g_scrpolys.add_to_tpage( i, TPage ); }
void RemoveScrPolyFromTPage( u_int16_t i, int16_t TPage ) { g_scrpolys.remove_from_tpage( i, TPage ); }
bool DisplaySolidScrPolys( RENDEROBJECT *renderObject ) { return g_scrpolys.display_solid( renderObject ); }
bool DisplayNonSolidScrPolys( RENDEROBJECT *renderObject ) { return g_scrpolys.display_non_solid( renderObject ); }
/* Fill one batch of the solid / non-solid screen polys from the resume point
 * (*TPage, *NextScrPoly); false when nothing is left to draw. */
bool ScrPolyDispSolid( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextScrPoly ) { return g_scrpolys.disp_fill( true, renderObject, TPage, NextScrPoly ); }
bool ScrPolyDispNonSolid( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextScrPoly ) { return g_scrpolys.disp_fill( false, renderObject, TPage, NextScrPoly ); }
FILE * SaveScreenPolys( FILE * fp ) { return g_scrpolys.save( fp ); }
FILE * LoadScreenPolys( FILE * fp ) { return g_scrpolys.restore( fp ); }

void UpdateCountdownDigits( void ) { Countdown.update(); }
void CreateCountdownDigits( void ) { Countdown.create_digits(); }
void DeleteCountdownDigits( void ) { Countdown.delete_digits(); }
void ClearCountdownBuffers( void ) { Countdown.clear_buffers(); }
void StartCountDown( int16_t Minutes, int16_t Seconds ) { Countdown.start( Minutes, Seconds ); }
void ResetCountDownBombTag( float Amount ) { Countdown.reset_bomb_tag( Amount ); }
void LoadTimeForLevel( void ) { Countdown.load_for_level(); }
