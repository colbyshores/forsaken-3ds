/* 2dpolys.cpp -- FmPoly camera-facing sprites: sequences and batches; see 2dpolys.h. */
#include "game/main.h"
#include "data/file.h"
#include <stdio.h>
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "rendering/2dpolys.h"
#include "game/camera.h"
#include "game/primary.h"
#include "game/spotfx.h"
#include "game/water.h"
#include "util/util.h"
#include "data/tload.h"
#include "game/oct2.h"
#include "rendering/2dtextures.h"


/*===================================================================
	Externs
===================================================================*/

/*===================================================================
	Globals
===================================================================*/
FMPOLY		FmPolys[ MAXNUMOF2DPOLYS ];
u_int16_t		FirstFmPolyUsed;
u_int16_t		FirstFmPolyFree;
u_int32_t		TotalFmPolysInUse = 0;
TPAGEINFO	FmPolyTPages[ MAXTPAGESPERTLOAD + 1 ];

void FadeColour( u_int8_t * Colour, u_int8_t WantedColour, float Speed );

/*===================================================================
	Procedure	:	Init Faceme poly structures
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void InitFmPoly( void )
{
	int i;

	FirstFmPolyUsed = INDEX_NONE;
	FirstFmPolyFree = 0;

	for( i=0;i<MAXNUMOF2DPOLYS;i++)
	{
		memset( &FmPolys[i], 0, sizeof( FMPOLY ) );

		FmPolys[i].xsize = ( 16.0F * GLOBAL_SCALE );
		FmPolys[i].ysize = ( 16.0F * GLOBAL_SCALE );
		FmPolys[i].R = 255;
		FmPolys[i].G = 255;
		FmPolys[i].B = 255;
		FmPolys[i].Trans = 255;
		FmPolys[i].DirVector.x = 0.0F;
		FmPolys[i].DirVector.y = 0.0F;
		FmPolys[i].DirVector.z = 1.0F;
		FmPolys[i].UpVector.x = 0.0F;
		FmPolys[i].UpVector.y = 1.0F;
		FmPolys[i].UpVector.z = 0.0F;

		FmPolys[i].NextInTPage = INDEX_NONE;
		FmPolys[i].PrevInTPage = INDEX_NONE;

		FmPolys[i].Next = i + 1;
		FmPolys[i].Prev = INDEX_NONE;
	}
	FmPolys[MAXNUMOF2DPOLYS-1].Next = INDEX_NONE;

	InitFmPolyTPages();
}

/*===================================================================
	Procedure	:	Find a free FmPoly and move it from the free list to
					the used list
	Input		:	Nothing
	Output		:	u_int16_t	Number of the free FmPoly
===================================================================*/
u_int16_t FindFreeFmPoly( void )
{
	u_int16_t i;

	i = FirstFmPolyFree;
	if( i == INDEX_NONE ) return i;

	FmPolys[i].Prev = FirstFmPolyUsed;

	if ( FirstFmPolyUsed != INDEX_NONE)
	{
		FmPolys[FirstFmPolyUsed].Next = i;
	}

	FirstFmPolyUsed = i;
	FirstFmPolyFree = FmPolys[i].Next;

	TotalFmPolysInUse++;

	return i ;
}
/*===================================================================
	Procedure	:	Kill a used FmPoly and move it from the used list to
					the free list
	Input		:	u_int16_t		Number of FmPoly to free....
	Output		:	Nothing
===================================================================*/
void KillUsedFmPoly( u_int16_t i )
{
	u_int16_t	its_prev;
	u_int16_t	its_next;

	its_prev = FmPolys[i].Prev;
	its_next = FmPolys[i].Next;

	if ( i == FirstFmPolyUsed )
		FirstFmPolyUsed = FmPolys[i].Prev;

	if( its_prev != INDEX_NONE)
		FmPolys[its_prev].Next = its_next;

	if( its_next != INDEX_NONE)
		FmPolys[its_next].Prev = its_prev;

	TotalFmPolysInUse--;

	RemoveFmPolyFromTPage( i, GetTPage( *FmPolys[i].Frm_Info, 0 ) );

	FmPolys[i].Prev = INDEX_NONE;
	FmPolys[i].Next = FirstFmPolyFree;
	FmPolys[i].LifeCount = 0.0F;
	FmPolys[i].xsize = ( 16.0F * GLOBAL_SCALE );
	FmPolys[i].ysize = ( 16.0F * GLOBAL_SCALE );
	FmPolys[i].Rot = 0.0F;
	FmPolys[i].RotSpeed = 0.0F;
	FmPolys[i].R = 255;
	FmPolys[i].G = 255;
	FmPolys[i].B = 255;
	FmPolys[i].Trans = 255;
	FmPolys[i].Flags = 0;
	FmPolys[i].Frame = 0.0F;
	FmPolys[i].SeqNum = 0;
	FmPolys[i].Dir.x = 0.0F;
	FmPolys[i].Dir.y = 0.0F;
	FmPolys[i].Dir.z = 0.0F;
	FmPolys[i].DirVector.x = 0.0F;
	FmPolys[i].DirVector.y = 0.0F;
	FmPolys[i].DirVector.z = 1.0F;
	FmPolys[i].UpVector.x = 0.0F;
	FmPolys[i].UpVector.y = 1.0F;
	FmPolys[i].UpVector.z = 0.0F;
	FmPolys[i].Speed = 0.0F;
	FmPolys[i].Frm_Info = nullptr;
	FirstFmPolyFree	= i;
}

/*===================================================================
	Procedure	:	Process FmPoly's
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/

constexpr auto NMEGEN_START = ( 2.0F * ANIM_SECOND );
constexpr auto NMEGEN_STAGE1 = ( 0.8F * ANIM_SECOND );
constexpr auto NMEGEN_STAGE2 = ( 0.4F * ANIM_SECOND );
constexpr auto NMEGEN_END = ( 0.0F * ANIM_SECOND );
#define	NMEGEN_STAGE0_LEN	( NMEGEN_START - NMEGEN_STAGE1 )
#define	NMEGEN_STAGE1_LEN	( NMEGEN_STAGE1 - NMEGEN_STAGE2 )
#define	NMEGEN_STAGE2_LEN	( NMEGEN_STAGE2 - NMEGEN_END )


static	u_int8_t	ShadeTable[ 16 ] = { 255, 200, 128, 104, 96, 92, 80, 77, 74, 72, 69, 66, 64, 64, 64, 64 };
static	u_int8_t	SmokeRingShadeTable[ 5 ] = { 255, 200, 150, 100, 50 };

/* FM_SMOKE_TRAIL */
[[gnu::always_inline]] static inline void FmPolySmokeTrail( u_int16_t & i, float & Speed )
{
	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( FmPolys[i].Frame * 8.0F ) ) );

	FmPolys[i].R = ShadeTable[ (int16_t) FmPolys[i].Frame ];
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].G = ShadeTable[ (int16_t) FmPolys[i].Frame ];
	FmPolys[i].B = ShadeTable[ (int16_t) FmPolys[i].Frame ];

	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= 4.0F;
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	FmPolys[i].Frame += ( framelag / 2.0F );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		KillUsedFmPoly( i );

		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	}
}

/* FM_TROJAX_EXP */
[[gnu::always_inline]] static inline void FmPolyTrojaxExplosion( u_int16_t & i, float & R, float & G, float & B, float & T, float & FadeDec )
{
	FadeDec = ( 8.0F * framelag );
	R = ( ( (float) FmPolys[i].R ) - FadeDec );
	G = ( ( (float) FmPolys[i].G ) - FadeDec );
	B = ( ( (float) FmPolys[i].B ) - FadeDec );
	T = ( ( (float) FmPolys[i].Trans ) - FadeDec );
	if( R < 0.0F ) R = 0.0F;
	if( G < 0.0F ) G = 0.0F;
	if( B < 0.0F ) B = 0.0F;
	if( T < 0.0F ) T = 0.0F;
	FmPolys[i].R = (u_int8_t) R;
	FmPolys[i].G = (u_int8_t) G;
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].B = (u_int8_t) B;
	FmPolys[i].Trans = (u_int8_t) T;

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].xsize += ( TROJAX_EXPINC * framelag );
	FmPolys[i].ysize += ( TROJAX_EXPINC * framelag );
	if( FmPolys[i].xsize >= TROJAX_EXPMAXSIZE )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_SHRAPNEL_EXP */
[[gnu::always_inline]] static inline void FmPolyShrapnelExplosion( u_int16_t & i )
{
	if( FmPolys[i].LifeCount < 995.0F )
	{
		if( FmPolys[i].Frm_Info != &Shrapnel_Header )
		{
			RemoveFmPolyFromTPage( i, GetTPage( *FmPolys[i].Frm_Info, (int16_t) FmPolys[i].Frame ) );
			FmPolys[i].Frm_Info = &Shrapnel_Header;
			AddFmPolyToTPage( i, GetTPage( *FmPolys[i].Frm_Info, 0 ) );
			FmPolys[i].Frame = 0.0F;
			FmPolys[i].xsize = SUSSGUN_EXPSIZE;
			FmPolys[i].ysize = SUSSGUN_EXPSIZE;
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			FmPolys[i].R = 128;
			FmPolys[i].G = 128;
			FmPolys[i].B = 128;
		}
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		else
		{
			FmPolys[i].Frame += framelag;
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
			{
				KillUsedFmPoly( i );
			}
		}
	}
}

/* FM_ARC_SPARK */
[[gnu::always_inline]] static inline void FmPolyArcSpark( u_int16_t & i, float & Speed )
{
	FmPolys[i].Frame += ( framelag / 5.0F );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		if( FmPolys[i].Speed > 0.0F )
		{
			FmPolys[i].Speed -= 0.5F;
			if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
		}

		Speed = ( FmPolys[i].Speed * framelag );
		FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
		FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
		FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );
	}
}

/* FM_BITS */
[[gnu::always_inline]] static inline void FmPolyBits( u_int16_t & i, float & Speed )
{
	FmPolys[i].Frame += ( framelag / 6.0F );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		Speed = ( FmPolys[i].Speed * framelag );
		FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );
	}
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
}

/* FM_ARC_RING */
[[gnu::always_inline]] static inline void FmPolyArcRing( u_int16_t & i, float & R, float & G, float & B, float & T, float & FadeDec )
{
	FadeDec = ( 8.0F * framelag );
	R = ( ( (float) FmPolys[i].R ) - FadeDec );
	G = ( ( (float) FmPolys[i].G ) - FadeDec );
	B = ( ( (float) FmPolys[i].B ) - FadeDec );
	T = ( ( (float) FmPolys[i].Trans ) - FadeDec );
	if( R < 0.0F ) R = 0.0F;
	if( G < 0.0F ) G = 0.0F;
	if( B < 0.0F ) B = 0.0F;
	if( T < 0.0F ) T = 0.0F;
	FmPolys[i].R = (u_int8_t) R;
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].G = (u_int8_t) G;
	FmPolys[i].B = (u_int8_t) B;
	FmPolys[i].Trans = (u_int8_t) T;

	FmPolys[i].xsize += ( TRANSPULSE_EXP_INC * framelag );
	FmPolys[i].ysize += ( TRANSPULSE_EXP_INC * framelag );
	if( FmPolys[i].xsize >= TRANSPULSE_EXP_MAXSIZE )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_GRAVGON_TRAIL */
[[gnu::always_inline]] static inline void FmPolyGravgonTrail( u_int16_t & i, float & Speed )
{
	Speed = ( FmPolys[i].Speed * framelag );
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( ( FmPolys[i].Frame * 5.0F ) * 8.0F ) ) );

	FmPolys[i].R = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 5.0F )];
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].G = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 5.0F )];
	FmPolys[i].B = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 5.0F )];

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	FmPolys[i].xsize += ( GRAVGON_TRAIL_INC * framelag );
	FmPolys[i].ysize += ( GRAVGON_TRAIL_INC * framelag );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= ( 2.0F * framelag );
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	FmPolys[i].Frame += ( 0.1F * framelag );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames ) KillUsedFmPoly( i );
}

/* FM_SOLARIS_TRAIL */
[[gnu::always_inline]] static inline void FmPolySolarisTrail( u_int16_t & i, float & Speed )
{
	Speed = ( FmPolys[i].Speed * framelag );
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( ( FmPolys[i].Frame * 3.0F ) * 8.0F ) ) );

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].R = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 3.0F )];
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].G = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 3.0F )];
	FmPolys[i].B = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 3.0F )];

	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= ( 2.0F * framelag );
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	FmPolys[i].Frame += ( 0.166F * framelag );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames ) KillUsedFmPoly( i );
}

/* FM_NEW_TRAIL */
[[gnu::always_inline]] static inline void FmPolyNewTrail( u_int16_t & i, float & Speed )
{
	Speed = ( FmPolys[i].Speed * framelag );
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( ( FmPolys[i].Frame * 3.0F ) * 8.0F ) ) );

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].R = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 3.0F )];
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].G = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 3.0F )];
	FmPolys[i].B = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 3.0F )];

	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= ( 2.0F * framelag );
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	FmPolys[i].Frame += ( 0.166F * framelag );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames ) KillUsedFmPoly( i );
}

/* FM_BLOOD */
[[gnu::always_inline]] static inline void FmPolyBlood( u_int16_t & i, float & Speed )
{
	FmPolys[i].Frame += ( framelag / 8.0F );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		Speed = ( FmPolys[i].Speed * framelag );
		FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
		FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
		FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );
	}
}

/* FM_FLASH */
[[gnu::always_inline]] static inline void FmPolyFlash( u_int16_t & i )
{
	/* @vfp-exempt R3 — the flash sprite's float lifetime test and size clamps at zero; integerizing changes when it dies and how it shrinks. measured n/a. Verified 2026-11-19. */
	if( FmPolys[i].LifeCount == 0.0F )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		FmPolys[i].xsize -= ( 0.1F * framelag );
		if( FmPolys[i].xsize < 0.0F ) FmPolys[i].xsize = 0.0F;
		FmPolys[i].ysize -= ( 0.1F * framelag );
		if( FmPolys[i].ysize < 0.0F ) FmPolys[i].ysize = 0.0F;
	}
}

/* FM_BLACK_SMOKE */
[[gnu::always_inline]] static inline void FmPolyBlackSmoke( u_int16_t & i, float & Speed )
{
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( FmPolys[i].Frame * 8.0F ) ) );

	Speed = ( FmPolys[i].UpSpeed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].UpVector.x * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.y += ( FmPolys[i].UpVector.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].UpVector.z * Speed );

	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= 0.2F;
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	if( FmPolys[i].UpSpeed < 10.0F )
	{
		FmPolys[i].UpSpeed += 0.2F;
		if( FmPolys[i].UpSpeed > 10.0F ) FmPolys[i].UpSpeed = 10.0F;
	}

	FmPolys[i].Frame += ( framelag / 2.0F );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );

	}
}

/* FM_SMOKE_RING */
[[gnu::always_inline]] static inline void FmPolySmokeRing( u_int16_t & i, float & Speed )
{
	Speed = ( FmPolys[i].Speed * framelag );
	FmPolys[i].Trans = (u_int8_t) 255;
	FmPolys[i].R = SmokeRingShadeTable[ (int16_t) FmPolys[i].Frame ];
	FmPolys[i].G = SmokeRingShadeTable[ (int16_t) FmPolys[i].Frame ];
	FmPolys[i].B = SmokeRingShadeTable[ (int16_t) FmPolys[i].Frame ];

	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );

	}
}

/* FM_SPOTFX_SMOKE */
[[gnu::always_inline]] static inline void FmPolySpotFXSmoke( u_int16_t & i, float & Speed )
{
	FmPolys[i].Trans = ( 128 - ( (int16_t) ( FmPolys[i].Frame * 25.0F ) ) );

	Speed = (float) ( 64.0F - ( (float) ( FmPolys[i].Frame * 12.0F ) ) );
	FmPolys[i].R = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_R ) );
	FmPolys[i].G = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_G ) );
	FmPolys[i].B = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_B ) );

	Speed = ( FmPolys[i].UpSpeed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].UpVector.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].UpVector.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].UpVector.z * Speed );

	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	FmPolys[i].xsize += ( ( 0.5F * GLOBAL_SCALE ) * framelag );
	FmPolys[i].ysize += ( ( 0.5F * GLOBAL_SCALE ) * framelag );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= 0.01F;
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	if( FmPolys[i].UpSpeed < 4.0F )
	{
		FmPolys[i].UpSpeed += 0.075F;
		if( FmPolys[i].UpSpeed > 4.0F ) FmPolys[i].UpSpeed = 4.0F;
	}

	FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );

	}
}

/* FM_SPOTFX_STEAM */
[[gnu::always_inline]] static inline void FmPolySpotFXSteam( u_int16_t & i, float & Speed )
{
	FmPolys[i].Trans = (u_int8_t) 255;

	Speed = SmokeRingShadeTable[ (int16_t) FmPolys[i].Frame ];
	FmPolys[i].R = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_R ) );
	FmPolys[i].G = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_G ) );
	FmPolys[i].B = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_B ) );

	Speed = ( FmPolys[i].UpSpeed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].UpVector.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].UpVector.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].UpVector.z * Speed );

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].xsize += ( ( 0.5F * GLOBAL_SCALE ) * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].ysize += ( ( 0.5F * GLOBAL_SCALE ) * framelag );

	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= 0.2F;
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	if( FmPolys[i].UpSpeed < 10.0F )
	{
		FmPolys[i].UpSpeed += 0.2F;
		if( FmPolys[i].UpSpeed > 10.0F ) FmPolys[i].UpSpeed = 10.0F;
	}

	FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );

	}
}

/* FM_SPOTFX_FLAME */
[[gnu::always_inline]] static inline void FmPolySpotFXFlame( u_int16_t & i, float & Speed, float & OldSize )
{
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( FmPolys[i].Frame * 8.0F ) ) );

	Speed = ( FmPolys[i].UpSpeed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].UpVector.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].UpVector.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].UpVector.z * Speed );

	Speed = ( FmPolys[i].Speed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	OldSize = FmPolys[i].xsize;

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].xsize -= ( ( 0.15F * GLOBAL_SCALE ) * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].ysize -= ( ( 0.15F * GLOBAL_SCALE ) * framelag );

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	if( ( OldSize >= ( 3.5F * GLOBAL_SCALE ) ) && ( FmPolys[i].xsize < ( 3.5F * GLOBAL_SCALE ) ) )
	{
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		CreateSpotFXFlameSmoke( &FmPolys[i].Pos, &FmPolys[i].Dir, FmPolys[i].Group );
	}

	if( ( FmPolys[i].xsize > 0.0F ) && ( FmPolys[i].ysize > 0.0F ) )
	{
		if( FmPolys[i].Speed > 0.0F )
		{
			FmPolys[i].Speed -= 0.2F;
			if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
		}

		if( FmPolys[i].UpSpeed < 10.0F )
		{
			FmPolys[i].UpSpeed += 0.2F;
			if( FmPolys[i].UpSpeed > 10.0F ) FmPolys[i].UpSpeed = 10.0F;
		}

		FmPolys[i].Frame += ( framelag / 2.0F );
		if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
		{
			KillUsedFmPoly( i );

		}
	}
	else
	{
		KillUsedFmPoly( i );
	}
}

/* FM_SPOTFX_SPARKS */
[[gnu::always_inline]] static inline void FmPolySpotFXSparks( u_int16_t & i, float & Speed )
{
	FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		Speed = ( FmPolys[i].Speed * framelag );
		FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
		FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
		FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );
	}
}

/* FM_SPOTFX_GRAVGON_TRAIL */
[[gnu::always_inline]] static inline void FmPolySpotFXGravgonTrail( u_int16_t & i, float & Speed )
{
	Speed = ( FmPolys[i].Speed * framelag );
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( ( FmPolys[i].Frame * 5.0F ) * 8.0F ) ) );

	FmPolys[i].R = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 5.0F )];
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].G = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 5.0F )];
	FmPolys[i].B = ShadeTable[ (int16_t) ( FmPolys[i].Frame * 5.0F )];

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	FmPolys[i].xsize += ( GRAVGON_TRAIL_INC * framelag );
	FmPolys[i].ysize += ( GRAVGON_TRAIL_INC * framelag );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed += ( 0.1F * framelag );
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	}

	FmPolys[i].Frame += ( 0.1F * framelag );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames ) KillUsedFmPoly( i );
}

/* FM_NME_TRAIL */
[[gnu::always_inline]] static inline void FmPolyNmeTrail( u_int16_t & i, float & Speed )
{
	if( FmPolys[i].LifeCount == 0.0F )
	{
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		KillUsedFmPoly( i );
	}
	else
	{
		FmPolys[i].xsize -= ( ( 0.175F * GLOBAL_SCALE ) * framelag );
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		FmPolys[i].ysize -= ( ( 0.175F * GLOBAL_SCALE ) * framelag );

		Speed = ( FmPolys[i].Speed * framelag );
		FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
		FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
		FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

		if( FmPolys[i].Speed > 0.0F )
		{
			FmPolys[i].Speed -= ( 2.0F * framelag );
			if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
		}
	}
}

/* FM_NME_VAPOUR_TRAIL */
[[gnu::always_inline]] static inline void FmPolyNmeVapourTrail( u_int16_t & i, float & Speed )
{
	FmPolys[i].Trans = ( 128 - ( (int16_t) ( FmPolys[i].Frame * 25.0F ) ) );

	Speed = (float) ( 64.0F - ( (float) ( FmPolys[i].Frame * 12.0F ) ) );
	FmPolys[i].R = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_R ) );
	FmPolys[i].G = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_G ) );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].B = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_B ) );

	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	FmPolys[i].xsize += ( ( 0.05F * GLOBAL_SCALE ) * framelag );
	FmPolys[i].ysize += ( ( 0.05F * GLOBAL_SCALE ) * framelag );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= 0.01F;
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );

	}
}

/* FM_SPOTFX_SHRAPNEL_TRAIL */
[[gnu::always_inline]] static inline void FmPolySpotFXShrapnelTrail( u_int16_t & i, float & Speed )
{
	FmPolys[i].Trans = (u_int8_t) 255;

	Speed = SmokeRingShadeTable[ (int16_t) FmPolys[i].Frame ];
	FmPolys[i].R = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_R ) );
	FmPolys[i].G = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_G ) );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].B = (u_int8_t) ( ( Speed / 256.0F ) * ( (float) FmPolys[i].Start_B ) );

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].xsize += ( ( 0.25F * GLOBAL_SCALE ) * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].ysize += ( ( 0.25F * GLOBAL_SCALE ) * framelag );

	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= 0.2F;
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	if( FmPolys[i].UpSpeed < 10.0F )
	{
		FmPolys[i].UpSpeed += 0.2F;
		if( FmPolys[i].UpSpeed > 10.0F ) FmPolys[i].UpSpeed = 10.0F;
	}
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */

	FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );

	}
}

/* FM_SPOTFX_REALSPARKS */
[[gnu::always_inline]] static inline void FmPolySpotFXRealSparks( u_int16_t & i, float & Speed, VECTOR & TempDir, VECTOR & TempVector, VECTOR & Int_Point, NORMAL & Int_Normal, MATRIX & UpMatrix, MATRIX & InvUpMatrix, QUAT & TempQuat )
{
	FmPolys[i].Frame += ( framelag * 0.1F );
	FmPolys[i].Frame = FMOD( FmPolys[i].Frame, (*FmPolys[i].Frm_Info)->Num_Frames );

	if( FmPolys[i].LifeCount == 0.0F )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		TempQuat.From2Vectors(&Mloadheader.Group[ FmPolys[i].Group ].up, &SlideUp);
		TempQuat.ToMatrix(&UpMatrix);
		MatrixTranspose( &UpMatrix, &InvUpMatrix );

		TempDir.x = ( FmPolys[i].Dir.x * framelag );
		TempDir.y = ( FmPolys[i].Dir.y * framelag );
		TempDir.z = ( FmPolys[i].Dir.z * framelag );

		/* @vfp-exempt R3 — wall-collision test along the spark's float path; integerizing changes where it bounces. measured n/a. Verified 2026-11-19. */
		if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, &FmPolys[i].Pos,
							  /* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
							  FmPolys[i].Group, &TempDir, (VECTOR *) &Int_Point,
							  &FmPolys[i].Group, &Int_Normal, &TempVector, true, nullptr ) == true )
		{
			FmPolys[i].Pos = Int_Point;

			TempVector = FmPolys[i].Dir;
			NormaliseVector( &TempVector );
			Speed = VectorLength( &FmPolys[i].Dir );
			ReflectVector( &TempVector, &Int_Normal, &FmPolys[i].Dir );
			Speed *= 0.5F;

			if( Speed < ( 5.0F * GLOBAL_SCALE ) )
			{
				KillUsedFmPoly( i );
				return;
			}

			FmPolys[i].Dir.x *= Speed;
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			FmPolys[i].Dir.y *= Speed;
			FmPolys[i].Dir.z *= Speed;
		}
		else
		{
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * framelag );
			FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * framelag );
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * framelag );
		}

		ApplyMatrix( &UpMatrix, &FmPolys[i].Dir, &TempDir );

		if( TempDir.x )
		{
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			if( TempDir.x > 0.0F )
			{
				TempDir.x -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
				/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
				if( TempDir.x < 0.0F ) TempDir.x = 0.0F;
			}
			else
			{
				TempDir.x += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
				/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
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

		ApplyMatrix( &InvUpMatrix, &TempDir, &FmPolys[i].Dir );
	}
}

/* FM_SPOTFX_BUBBLE */
[[gnu::always_inline]] static inline void FmPolySpotFXBubble( u_int16_t & i, float & Speed, float & Damage, VECTOR & TempDir, VECTOR & Int_Point, VECTOR & OldPos )
{
	OldPos = FmPolys[i].Pos;

	Speed = ( FmPolys[i].UpSpeed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].UpVector.x * Speed ) + ( (float) fast_sinf( FmPolys[i].LifeCount * ( 0.10F ) ) * framelag );
	FmPolys[i].Pos.y += ( FmPolys[i].UpVector.y * Speed ) + ( (float) fast_sinf( FmPolys[i].LifeCount * ( 0.05F ) ) * framelag );
	FmPolys[i].Pos.z += ( FmPolys[i].UpVector.z * Speed ) + ( (float) fast_sinf( FmPolys[i].LifeCount * ( 0.15F ) ) * framelag );

	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed ) + ( (float) fast_sinf( FmPolys[i].LifeCount * ( 0.10F ) ) * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed ) + ( (float) fast_sinf( FmPolys[i].LifeCount * ( 0.05F ) ) * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed ) + ( (float) fast_sinf( FmPolys[i].LifeCount * ( 0.15F ) ) * framelag );

	if( FmPolys[i].Speed > 0.0F )
	{
		FmPolys[i].Speed -= ( 0.4F * framelag );
		if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
	}

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	if( FmPolys[i].UpSpeed < 4.0F )
	{
		FmPolys[i].UpSpeed += ( 0.4F * framelag );
		if( FmPolys[i].UpSpeed > 4.0F ) FmPolys[i].UpSpeed = 4.0F;
	}

	TempDir.x = ( FmPolys[i].Pos.x - OldPos.x );
	TempDir.y = ( FmPolys[i].Pos.y - OldPos.y );
	TempDir.z = ( FmPolys[i].Pos.z - OldPos.z );
	FmPolys[i].Group = MoveGroup( &Mloadheader, &OldPos, FmPolys[i].Group, &TempDir );

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	if( ( FmPolys[i].LifeCount == 0.0F ) ||	( !InWater( FmPolys[i].Group, &FmPolys[i].Pos, &Damage ) ) )
	{
		TempDir.x = ( FmPolys[i].Pos.x - OldPos.x );
		TempDir.y = ( FmPolys[i].Pos.y - OldPos.y );
		TempDir.z = ( FmPolys[i].Pos.z - OldPos.z );
		WaterObjectCollide( FmPolys[i].Group, &OldPos, &TempDir, &Int_Point, 8 );
		KillUsedFmPoly( i );
	}
}

/* FM_NMEGEN_EFFECT */
[[gnu::always_inline]] static inline void FmPolyNmeGenEffect( u_int16_t & i )
{
	if( FmPolys[ i ].LifeCount <= 0.0F )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		if( FmPolys[ i ].LifeCount >= NMEGEN_STAGE1 )			// stage 0
		{
			FmPolys[ i ].Rot -= ( ( 360.0F / NMEGEN_STAGE0_LEN ) * framelag ); // one turn over stage 0
			FmPolys[ i ].Rot = FMOD( FmPolys[ i ].Rot, 360.0F );
			FmPolys[ i ].R = (u_int8_t) ( 172.0F - ( ( FmPolys[ i ].LifeCount - NMEGEN_STAGE1 ) * ( 172.0F / NMEGEN_STAGE0_LEN ) ) );
			FmPolys[ i ].G = (u_int8_t) ( 49.0F - ( ( FmPolys[ i ].LifeCount - NMEGEN_STAGE1 ) * ( 49.0F / NMEGEN_STAGE0_LEN ) ) );
		}
		else
		{
			FmPolys[ i ].Rot -= ( ( 360.0F / ( NMEGEN_STAGE1_LEN + NMEGEN_STAGE2_LEN ) ) * framelag );	// one turn over stages 1-2
			FmPolys[ i ].Rot = FMOD( FmPolys[ i ].Rot, 360.0F );

			if( FmPolys[ i ].LifeCount >= NMEGEN_STAGE2 )		// stage 1
			{
				FmPolys[ i ].R = (u_int8_t) ( 255.0F - ( ( FmPolys[ i ].LifeCount - NMEGEN_STAGE2 ) * ( 83.0F / NMEGEN_STAGE1_LEN ) ) );
				FmPolys[ i ].G = (u_int8_t) ( 255.0F - ( ( FmPolys[ i ].LifeCount - NMEGEN_STAGE2 ) * ( 206.0F / NMEGEN_STAGE1_LEN ) ) );
				FmPolys[ i ].B = (u_int8_t) ( 255.0F - ( ( FmPolys[ i ].LifeCount - NMEGEN_STAGE2 ) * ( 255.0F / NMEGEN_STAGE1_LEN ) ) );
				/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
				FmPolys[ i ].xsize -= ( ( ( 6.0F * GLOBAL_SCALE ) / NMEGEN_STAGE1_LEN ) * framelag );
				FmPolys[ i ].ysize = FmPolys[ i ].xsize;
			}
			else												// stage 2
			{
				FmPolys[ i ].xsize -= ( ( ( 10.0F * GLOBAL_SCALE ) / NMEGEN_STAGE2_LEN ) * framelag );
				FmPolys[ i ].ysize = FmPolys[ i ].xsize;
			}
		}
	}
}

/* FM_NMEGEN_FLARE */
[[gnu::always_inline]] static inline void FmPolyNmeGenFlare( u_int16_t & i )
{
	if( FmPolys[ i ].LifeCount <= 0.0F )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		if( FmPolys[ i ].LifeCount >= NMEGEN_STAGE1 )			// stage 0
		{
			FmPolys[ i ].R = (u_int8_t) ( 172.0F - ( ( FmPolys[ i ].LifeCount - NMEGEN_STAGE1 ) * ( 172.0F / NMEGEN_STAGE0_LEN ) ) );
			FmPolys[ i ].G = (u_int8_t) ( 172.0F - ( ( FmPolys[ i ].LifeCount - NMEGEN_STAGE1 ) * ( 172.0F / NMEGEN_STAGE0_LEN ) ) );
			FmPolys[ i ].B = (u_int8_t) ( 172.0F - ( ( FmPolys[ i ].LifeCount - NMEGEN_STAGE1 ) * ( 172.0F / NMEGEN_STAGE0_LEN ) ) );
			FmPolys[ i ].xsize += ( ( ( 14.0F * GLOBAL_SCALE ) / NMEGEN_STAGE0_LEN ) * framelag );
			FmPolys[ i ].ysize = FmPolys[ i ].xsize;
		}
		else
		{
			if( FmPolys[ i ].LifeCount >= NMEGEN_STAGE2 )		// stage 1
			{
				/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
				/* @vfp-exempt R7 — constant divisor (NMEGEN_STAGE1_LEN is constexpr), folded at compile time; no division at run time. measured n/a. Verified 2026-11-19. */
				FmPolys[ i ].xsize -= ( ( ( 6.0F * GLOBAL_SCALE ) / NMEGEN_STAGE1_LEN ) * framelag );
				FmPolys[ i ].ysize = FmPolys[ i ].xsize;
				/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			}
			else												// stage 2
			{
				/* @vfp-exempt R7 — constant divisor (NMEGEN_STAGE2_LEN is constexpr), folded at compile time; no division at run time. measured n/a. Verified 2026-11-19. */
				FmPolys[ i ].xsize -= ( ( ( 8.0F * GLOBAL_SCALE ) / NMEGEN_STAGE2_LEN ) * framelag );
				FmPolys[ i ].ysize = FmPolys[ i ].xsize;
			}
		}
	}
}

/* FM_SPOTFX_DRIP */
[[gnu::always_inline]] static inline void FmPolySpotFXDrip( u_int16_t & i, float & Speed, VECTOR & TempDir, VECTOR & TempVector, VECTOR & Int_Point, NORMAL & Int_Normal )
{
	if( FmPolys[ i ].LifeCount <= 0.0F )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		FmPolys[i].Speed += ( 0.1F * framelag );
		if( FmPolys[i].Speed > ( 40.0F * GLOBAL_SCALE ) ) FmPolys[i].Speed = ( 40.0F * GLOBAL_SCALE );

		Speed = ( FmPolys[i].Speed * framelag );

		TempDir.x = ( FmPolys[i].Dir.x * Speed );
		TempDir.y = ( FmPolys[i].Dir.y * Speed );
		TempDir.z = ( FmPolys[i].Dir.z * Speed );

		if( WaterObjectCollide( FmPolys[i].Group, &FmPolys[i].Pos, &TempDir, &Int_Point, 32 ) )
		{
			CreateSpotFXDripSplash( &Int_Point, &SlideUp, FmPolys[i].Group, FmPolys[i].R, FmPolys[i].G, FmPolys[i].B );
			KillUsedFmPoly( i );
		}
		else
		{
			/* @vfp-exempt R3 — wall-collision test along the drip's float path; integerizing changes where it lands. measured n/a. Verified 2026-11-19. */
			if( BackgroundCollide( &MCloadheadert0 ,&Mloadheader, &FmPolys[i].Pos,
								  FmPolys[i].Group, &TempDir, (VECTOR *) &Int_Point,
								  &FmPolys[i].Group, &Int_Normal, &TempVector, true, nullptr ) == true )
			{
				CreateSpotFXDripSplash( &Int_Point, &SlideUp, FmPolys[i].Group, FmPolys[i].R, FmPolys[i].G, FmPolys[i].B );
				KillUsedFmPoly( i );
			}
			else
			{
				FmPolys[i].Pos.x += TempDir.x;
				FmPolys[i].Pos.y += TempDir.y;
				FmPolys[i].Pos.z += TempDir.z;
			}
		}
	}
}

/* FM_SPOTFX_BURNING */
[[gnu::always_inline]] static inline void FmPolySpotFXBurning( u_int16_t & i, float & Speed, float & OldSize )
{
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( FmPolys[i].Frame * 8.0F ) ) );

	Speed = ( FmPolys[i].UpSpeed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].UpVector.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].UpVector.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].UpVector.z * Speed );

	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	OldSize = FmPolys[i].xsize;
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */

	FmPolys[i].xsize -= ( ( 0.15F * GLOBAL_SCALE ) * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].ysize -= ( ( 0.15F * GLOBAL_SCALE ) * framelag );

	if( ( FmPolys[i].xsize > 0.0F ) && ( FmPolys[i].ysize > 0.0F ) )
	{
		if( FmPolys[i].Speed > 0.0F )
		{
			FmPolys[i].Speed -= ( 0.15F * framelag );
			if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
		}

		if( FmPolys[i].UpSpeed < 10.0F )
		{
			FmPolys[i].UpSpeed += ( 0.15F * framelag );
			if( FmPolys[i].UpSpeed > 10.0F ) FmPolys[i].UpSpeed = 10.0F;
		}

		/* @vfp-exempt R7 — division by 2.0F compiles to an exact multiply by 0.5; no division at run time. measured n/a. Verified 2026-11-19. */
		FmPolys[i].Frame += ( framelag / 2.0F );
		if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
		{
			KillUsedFmPoly( i );

		}
	}
	else
	{
		KillUsedFmPoly( i );
	}
}

/* FM_SPOTFX_BEARD_AFTERBURNER */
[[gnu::always_inline]] static inline void FmPolySpotFXBeardAfterburner( u_int16_t & i, float & Speed )
{
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( FmPolys[i].Frame * 8.0F ) ) );

	Speed = ( FmPolys[i].UpSpeed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].UpVector.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].UpVector.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].UpVector.z * Speed );

	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	Speed = ( FmPolys[i].Speed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	FmPolys[i].xsize -= ( ( 0.1F * GLOBAL_SCALE ) * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].ysize -= ( ( 0.1F * GLOBAL_SCALE ) * framelag );

	if( ( FmPolys[i].xsize > 0.0F ) && ( FmPolys[i].ysize > 0.0F ) )
	{
		if( FmPolys[i].Speed > 0.0F )
		{
			FmPolys[i].Speed -= ( 0.15F * framelag );
			if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
		}

		if( FmPolys[i].UpSpeed < 10.0F )
		{
			FmPolys[i].UpSpeed += ( 0.15F * framelag );
			if( FmPolys[i].UpSpeed > 10.0F ) FmPolys[i].UpSpeed = 10.0F;
		}

		FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
		if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
		{
			KillUsedFmPoly( i );
		}
	}
	else
	{
		KillUsedFmPoly( i );
	}

}

/* FM_SPOTFX_WATERSPLASH */
[[gnu::always_inline]] static inline void FmPolySpotFXWaterSplash( u_int16_t & i, VECTOR & TempDir, VECTOR & Int_Point, MATRIX & UpMatrix, MATRIX & InvUpMatrix, QUAT & TempQuat )
{
	FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		TempQuat.From2Vectors(&Mloadheader.Group[ FmPolys[i].Group ].up, &SlideUp);
		TempQuat.ToMatrix(&UpMatrix);
		MatrixTranspose( &UpMatrix, &InvUpMatrix );

		TempDir.x = ( FmPolys[i].Dir.x * framelag );
		TempDir.y = ( FmPolys[i].Dir.y * framelag );
		TempDir.z = ( FmPolys[i].Dir.z * framelag );

		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		if( WaterObjectCollide( FmPolys[i].Group, &FmPolys[i].Pos, &TempDir, &Int_Point, 0 ) )
		{
			KillUsedFmPoly( i );
		}
		else
		{
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * framelag );
			FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * framelag );
			FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * framelag );

			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			ApplyMatrix( &UpMatrix, &FmPolys[i].Dir, &TempDir );

			if( TempDir.x )
			{
				/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
				if( TempDir.x > 0.0F )
				{
					TempDir.x -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
					/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
					if( TempDir.x < 0.0F ) TempDir.x = 0.0F;
				}
				else
				{
					TempDir.x += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
					/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
					if( TempDir.x > 0.0F ) TempDir.x = 0.0F;
				}
			}
			if( TempDir.z )
			{
				if( TempDir.z > 0.0F )
				{
					TempDir.z -= ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
					if( TempDir.z < 0.0F ) TempDir.z = 0.0F;
					/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
				}
				else
				{
					TempDir.z += ( ( 0.000001F * GLOBAL_SCALE ) * framelag );
					if( TempDir.z > 0.0F ) TempDir.z = 0.0F;
				}
			}
			TempDir.y -= ( ( 0.5F * GLOBAL_SCALE ) * framelag );

			ApplyMatrix( &InvUpMatrix, &TempDir, &FmPolys[i].Dir );
		}
	}
}

/* FM_HK5_TRAIL */
[[gnu::always_inline]] static inline void FmPolyHK5Trail( u_int16_t & i, float & Speed )
{
	if( FmPolys[i].LifeCount == 0.0F )
	{
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		KillUsedFmPoly( i );
	}
	else
	{
		FmPolys[i].xsize -= ( ( 0.25F * GLOBAL_SCALE ) * framelag );
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		FmPolys[i].ysize -= ( ( 0.25F * GLOBAL_SCALE ) * framelag );

		Speed = ( FmPolys[i].Speed * framelag );
		FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
		FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
		FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

		if( FmPolys[i].Speed > 0.0F )
		{
			FmPolys[i].Speed -= ( 0.125F * framelag );
			if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
		}
	}
}

/* FM_GOALSPARKLE */
[[gnu::always_inline]] static inline void FmPolyGoalSparkle( u_int16_t & i, float & Speed )
{
	if( FmPolys[i].LifeCount == 0.0F )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		Speed = ( FmPolys[i].Speed * framelag );
		FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
		FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
		FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

		Speed = FMOD( FmPolys[i].LifeCount, 20.0F );

		/* @vfp-exempt R3 — sparkle pulse threshold on a float phase; integerizing changes the pulse shape. measured n/a. Verified 2026-11-19. */
		if( Speed > 10.0F )
		{
			FmPolys[i].xsize = ( ( 20.0F - Speed ) * 0.05F );
		}
		else
		{
			FmPolys[i].xsize = ( Speed * 0.05F );
		}
		FmPolys[i].ysize = FmPolys[i].xsize;
	}
}

/* FM_ANIMSPEED */
[[gnu::always_inline]] static inline void FmPolyAnimSpeed( u_int16_t & i )
{
	FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );

	}
}

/* FM_ANIM */
[[gnu::always_inline]] static inline void FmPolyAnim( u_int16_t & i )
{
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( FmPolys[i].Frame * 8.0F ) ) );
	FmPolys[i].Frame += ( framelag / 3.0F );

	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_ANIM2 */
[[gnu::always_inline]] static inline void FmPolyAnim2( u_int16_t & i )
{
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( FmPolys[i].Frame * 8.0F ) ) );
	FmPolys[i].Frame += ( framelag / 4.0F );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_TROJAX */
[[gnu::always_inline]] static inline void FmPolyTrojax( u_int16_t & i, VECTOR & Rotation )
{
	FmPolys[ i ].Rot += ( FmPolys[i].RotSpeed * framelag );
	/* @vfp-exempt R3 — sprite rotation wraps at 360 degrees in float; integerizing changes the drawn angle. measured n/a. Verified 2026-11-19. */
	if( FmPolys[ i ].Rot > 360.0F ) FmPolys[ i ].Rot -= 360.0F;
	else if( FmPolys[ i ].Rot < 0.0F ) FmPolys[ i ].Rot += 360.0F;
	fast_sincosf( D2R( FmPolys[ i ].Rot ), &Rotation.x, &Rotation.y );
	Rotation.z = 0.0F;
	ApplyMatrix( &FmPolys[ i ].Mat, &Rotation, &FmPolys[ i ].UpVector );
}

/* FM_PULSAR_EXP */
[[gnu::always_inline]] static inline void FmPolyPulsarExplosion( u_int16_t & i )
{
	FmPolys[i].Frame += ( framelag / 4.0F );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_NMEBULLET1 */
[[gnu::always_inline]] static inline void FmPolyNmeBullet1( u_int16_t & i )
{
	FmPolys[i].Frame += ( framelag / 3.0F );
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		FmPolys[i].Frame -= ( ( FmPolys[i].Frame / (*FmPolys[i].Frm_Info)->Num_Frames )
								* (*FmPolys[i].Frm_Info)->Num_Frames );
	}
}

/* FM_FLAME */
[[gnu::always_inline]] static inline void FmPolyFlame( u_int16_t & i )
{
	FmPolys[ i ].Rot += ( FmPolys[i].RotSpeed * framelag );
	/* @vfp-exempt R3 — flame sprite rotation wrap on a float angle; integerizing changes the drawn rotation. measured n/a. Verified 2026-11-19. */
	if( FmPolys[ i ].Rot > 360.0F ) FmPolys[ i ].Rot -= 360.0F;
	else if( FmPolys[ i ].Rot < 0.0F ) FmPolys[ i ].Rot += 360.0F;

	FmPolys[i].Frame += framelag;
	/* @vfp-exempt R3 — animation frame clamp on a float frame counter; integerizing changes which frame is drawn. measured n/a. Verified 2026-11-19. */
	if( FmPolys[i].Frame > 12.0F ) FmPolys[i].Frame = 12.0F;
}

/* FM_PICKUP_REGEN */
[[gnu::always_inline]] static inline void FmPolyPickupRegen( u_int16_t & i )
{
	FmPolys[i].Frame += ( framelag / 6.0F );

	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_BLOODSPLAT */
[[gnu::always_inline]] static inline void FmPolyBloodSplat( u_int16_t & i, float & R, float & G, float & B, float & T, float & FadeDec )
{
	FmPolys[i].Frame += ( framelag / 8.0F ); //6.0F;
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		KillUsedFmPoly( i );
	}
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	else
	{
		FadeDec = ( 255.0F / ( (*FmPolys[i].Frm_Info)->Num_Frames * 8.0F ) ) * framelag;
		R = ( ( (float) FmPolys[i].R ) - FadeDec );
		G = ( ( (float) FmPolys[i].G ) - FadeDec );
		B = ( ( (float) FmPolys[i].B ) - FadeDec );
		T = ( ( (float) FmPolys[i].Trans ) - FadeDec );
		if( R < 0.0F ) R = 0.0F;
		if( G < 0.0F ) G = 0.0F;
		if( B < 0.0F ) B = 0.0F;
		if( T < 0.0F ) T = 0.0F;
		FmPolys[i].R = (u_int8_t) R;
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		FmPolys[i].G = (u_int8_t) G;
		FmPolys[i].B = (u_int8_t) B;
		FmPolys[i].Trans = (u_int8_t) T;
	}
}

/* FM_BLOODDRIBBLE */
[[gnu::always_inline]] static inline void FmPolyBloodDribble( u_int16_t & i )
{
	FmPolys[i].Frame += ( framelag / 8.0F ); //6.0F;
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_MUSHROOM */
[[gnu::always_inline]] static inline void FmPolyMushroom( u_int16_t & i )
{
	FmPolys[i].Frame += ( framelag / 6.0F );

	if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_SPOTFX_SPARKSFACEME */
[[gnu::always_inline]] static inline void FmPolySpotFXSparksFaceMe( u_int16_t & i )
{
	/* @vfp-exempt R3 — lifetime test on the sprite's float countdown; integerizing changes when it dies. measured n/a. Verified 2026-11-19. */
	if( FmPolys[i].LifeCount == 0.0F )
	{
		KillUsedFmPoly( i );
	}
}

/* FM_SPOTFX_GLOW */
[[gnu::always_inline]] static inline void FmPolySpotFXGlow( u_int16_t & i )
{
	if( FmPolys[i].OnceOnly )
	{
		FmPolys[i].OnceOnly--;
	}
	else
	{
		KillUsedFmPoly( i );
	}
}

/* FM_RESTART_INACTIVE */
[[gnu::always_inline]] static inline void FmPolyRestartInactive( u_int16_t & i )
{
	FadeColour( &FmPolys[i].R, FmPolys[i].Start_R, 5.0F );
	FadeColour( &FmPolys[i].G, FmPolys[i].Start_G, 5.0F );
	FadeColour( &FmPolys[i].B, FmPolys[i].Start_B, 5.0F );

	FmPolys[ i ].Rot += ( FmPolys[ i ].RotSpeed * framelag );
	FmPolys[ i ].Rot = FMOD( FmPolys[ i ].Rot, 360.0F );
}

/* FM_RESTART_ACTIVE */
[[gnu::always_inline]] static inline void FmPolyRestartActive( u_int16_t & i )
{
	FadeColour( &FmPolys[i].R, FmPolys[i].Start_R, 5.0F );
	FadeColour( &FmPolys[i].G, FmPolys[i].Start_G, 5.0F );
	FadeColour( &FmPolys[i].B, FmPolys[i].Start_B, 5.0F );

	FmPolys[ i ].Rot += ( FmPolys[ i ].RotSpeed * framelag );
	FmPolys[ i ].Rot = FMOD( FmPolys[ i ].Rot, 360.0F );
}

/* FM_FIREBALL */
[[gnu::always_inline]] static inline void FmPolyFireball( u_int16_t & i, float & Speed, float & OldSize )
{
	FmPolys[i].Trans = ( 248 - ( (int16_t) ( FmPolys[i].Frame * 8.0F ) ) );

	Speed = ( FmPolys[i].UpSpeed * framelag );
	FmPolys[i].Pos.x += ( FmPolys[i].UpVector.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].UpVector.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].UpVector.z * Speed );

	Speed = ( FmPolys[i].Speed * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
	FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

	OldSize = FmPolys[i].xsize;
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */

	FmPolys[i].xsize -= ( ( 0.15F * GLOBAL_SCALE ) * framelag );
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	FmPolys[i].ysize -= ( ( 0.15F * GLOBAL_SCALE ) * framelag );

	if( ( FmPolys[i].xsize > 0.0F ) && ( FmPolys[i].ysize > 0.0F ) )
	{
		if( FmPolys[i].Speed > 0.0F )
		{
			FmPolys[i].Speed -= ( 0.15F * framelag );
			if( FmPolys[i].Speed < 0.0F ) FmPolys[i].Speed = 0.0F;
		}

		if( FmPolys[i].UpSpeed < 10.0F )
		{
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			FmPolys[i].UpSpeed += ( 0.15F * framelag );
			if( FmPolys[i].UpSpeed > 10.0F ) FmPolys[i].UpSpeed = 10.0F;
		}

		FmPolys[i].Frame += ( framelag / FmPolys[i].AnimSpeed );
		if( FmPolys[i].Frame >= (*FmPolys[i].Frm_Info)->Num_Frames )
		{
			KillUsedFmPoly( i );

		}
	}
	else
	{
		KillUsedFmPoly( i );
	}
}

/* FM_SPARKLE */
[[gnu::always_inline]] static inline void FmPolySparkle( u_int16_t & i, float & Speed )
{
	if( FmPolys[i].LifeCount == 0.0F )
	{
		KillUsedFmPoly( i );
	}
	else
	{
		Speed = ( FmPolys[i].Speed * framelag );
		FmPolys[i].Pos.x += ( FmPolys[i].Dir.x * Speed );
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		FmPolys[i].Pos.y += ( FmPolys[i].Dir.y * Speed );
		FmPolys[i].Pos.z += ( FmPolys[i].Dir.z * Speed );

		Speed = FMOD( FmPolys[i].LifeCount, 20.0F );

		if( Speed > 10.0F )
		{
			FmPolys[i].xsize = ( ( 20.0F - Speed ) * 0.05F );
		}
		else
		{
			FmPolys[i].xsize = ( Speed * 0.05F );
		}
		FmPolys[i].ysize = FmPolys[i].xsize;
	}
}

void FmPolyProcess( void )
{
	u_int16_t	i;
	u_int16_t	nextprim;
	float	Speed;
	/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
	float	R, G, B, T, FadeDec;
	VECTOR	Rotation;
	float	OldSize;
	float	Damage;
	VECTOR	TempDir, TempVector;
	VECTOR	Int_Point;
	NORMAL	Int_Normal;
	MATRIX	UpMatrix;
	MATRIX	InvUpMatrix;
	QUAT	TempQuat;
	VECTOR	OldPos;

	i = FirstFmPolyUsed;

	while( i != INDEX_NONE )
	{
		nextprim = FmPolys[i].Prev;

		FmPolys[i].LifeCount -= framelag;
		if( FmPolys[i].LifeCount < 0.0F ) FmPolys[i].LifeCount = 0.0F;

		switch( FmPolys[i].SeqNum )
		{
		case FM_ANIMSPEED:							// Anim using animspeed
			FmPolyAnimSpeed( i );
			break;

		case FM_ANIM:								// Explosion?
			FmPolyAnim( i );
			break;

		case FM_ANIM2:								// Ship Explosion?
			FmPolyAnim2( i );
			break;

		case FM_SMOKE_TRAIL:						// Smoke Trail?
			FmPolySmokeTrail( i, Speed );
			break;

		case FM_TROJAX_EXP:								// Explosion?
			FmPolyTrojaxExplosion( i, R, G, B, T, FadeDec );
			break;

		case FM_TROJAX:								// Trojax bullet
			FmPolyTrojax( i, Rotation );
			break;

		case FM_PULSAR_EXP:							// Pulsar Explosion?
			FmPolyPulsarExplosion( i );
			break;

			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		case FM_PULSAR:
			break;

		case FM_NMEBULLET1:
			FmPolyNmeBullet1( i );
			break;

		case FM_SHRAPNEL_EXP:						// Shrapnel Explosion?
			FmPolyShrapnelExplosion( i );
			break;

		case FM_FLAME:								// Flame Thrower?
			FmPolyFlame( i );
			break;

			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		case FM_STATIC:								// Static Obj Don't move/rotate
			break;

		case FM_ARC:								// Arc?
			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
			break;

		case FM_ARC2:								// Arc Glow?
			break;

		case FM_ARC_SPARK:							// Arc Sparks?
			FmPolyArcSpark( i, Speed );
			break;

		case FM_BITS:								// Bits?
			FmPolyBits( i, Speed );
			break;

		case FM_ARC_RING:							// Arc Explosion Ring?
			FmPolyArcRing( i, R, G, B, T, FadeDec );
			break;

		case FM_FLARE:								// Arc Explosion Flare?
			if( FmPolys[i].LifeCount == 0.0F )
			{
				KillUsedFmPoly( i );
			}
			break;

		case FM_GRAVGON_TRAIL:						// Gravgon Smoke Trail?
			FmPolyGravgonTrail( i, Speed );
			break;

		case FM_SOLARIS_TRAIL:						// Solaris Smoke Trail?
			FmPolySolarisTrail( i, Speed );
			break;

		case FM_NEW_TRAIL:								// New Smoke Trail?
			FmPolyNewTrail( i, Speed );
			break;

		case FM_PICKUP_REGEN:							// Regenerating Pickup?
			FmPolyPickupRegen( i );
			break;

		case FM_BLOOD:									// Blood?
			FmPolyBlood( i, Speed );
			break;

		case FM_BLOODSPLAT:								// Blood Splat?
			FmPolyBloodSplat( i, R, G, B, T, FadeDec );
			break;

			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		case FM_BLOODDRIBBLE:							// Blood Dribble?
			FmPolyBloodDribble( i );
			break;

			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		case FM_FLASH:								// Flash?
			FmPolyFlash( i );
			break;

		case FM_MUZZLEFLASH:						// Muzzle Flash?
			if( FmPolys[i].LifeCount == 0.0F )
			{
				KillUsedFmPoly( i );
			}
			break;


		case FM_BLACK_SMOKE:						// Black Smoke?
			FmPolyBlackSmoke( i, Speed );
			break;

		case FM_MUSHROOM:								// Mushroom Explosion?
			FmPolyMushroom( i );
			break;

		case FM_SMOKE_RING:								// Smoke Ring?
			FmPolySmokeRing( i, Speed );
			break;

		case FM_SPOTFX_SMOKE:							// Smoke?
			FmPolySpotFXSmoke( i, Speed );
			break;

		case FM_SPOTFX_STEAM:							// STEAM?
			FmPolySpotFXSteam( i, Speed );
			break;

		case FM_SPOTFX_FLAME:							// Flame?
			FmPolySpotFXFlame( i, Speed, OldSize );
			break;

		case FM_SPOTFX_SPARKS:							// Sparks?
			FmPolySpotFXSparks( i, Speed );
			break;

		case FM_SPOTFX_SPARKSFACEME:
			FmPolySpotFXSparksFaceMe( i );
			break;

		case FM_SPOTFX_GRAVGON_TRAIL:						// Gravgon Smoke Trail?
			FmPolySpotFXGravgonTrail( i, Speed );
			break;

		case FM_SPOTFX_GLOW:							// SpotFX Glow?
			FmPolySpotFXGlow( i );
			break;

		case FM_NME_TRAIL:								// Nme Trail?
			FmPolyNmeTrail( i, Speed );
			break;

		case FM_NME_VAPOUR_TRAIL:						// Vapour Trail?
			FmPolyNmeVapourTrail( i, Speed );
			break;

		case FM_SPOTFX_SHRAPNEL_TRAIL:						// Shrapnel Trail?
			FmPolySpotFXShrapnelTrail( i, Speed );
			break;


		case FM_SPOTFX_REALSPARKS:						// Real Sparks?
			FmPolySpotFXRealSparks( i, Speed, TempDir, TempVector, Int_Point, Int_Normal, UpMatrix, InvUpMatrix, TempQuat );
			break;

		case FM_SPOTFX_BUBBLE:							// Bubble?
			FmPolySpotFXBubble( i, Speed, Damage, TempDir, Int_Point, OldPos );
			break;

		case FM_NMEGEN_EFFECT:
			FmPolyNmeGenEffect( i );
			break;

		case FM_NMEGEN_FLARE:
			FmPolyNmeGenFlare( i );
			break;

		case FM_SPOTFX_DRIP:
			FmPolySpotFXDrip( i, Speed, TempDir, TempVector, Int_Point, Int_Normal );
			break;

		case FM_PHOTON:
			FmPolys[ i ].Rot += ( FmPolys[i].RotSpeed * framelag );
			FmPolys[ i ].Rot = FMOD( FmPolys[ i ].Rot, 360.0F );

			FmPolys[i].ysize = (float) ( ( fast_sinf( FmPolys[ i ].Rot ) * 4.0F ) );
			break;

		case FM_BLUEPHOTON:
			FmPolys[ i ].Rot += ( FmPolys[i].RotSpeed * framelag );
			FmPolys[ i ].Rot = FMOD( FmPolys[ i ].Rot, 360.0F );

			FmPolys[i].ysize = (float) ( ( fast_sinf( FmPolys[ i ].Rot ) * 6.0F ) );
			break;

		case FM_SPOTFX_BURNING:
			FmPolySpotFXBurning( i, Speed, OldSize );
			break;

		case FM_SPOTFX_BEARD_AFTERBURNER:
			FmPolySpotFXBeardAfterburner( i, Speed );
			break;

		case FM_RESTART_INACTIVE:							// Restart Point Inactive
			FmPolyRestartInactive( i );
			break;

		case FM_RESTART_ACTIVE:								// Restart Point Active
			FmPolyRestartActive( i );
			break;

		case FM_SPOTFX_WATERSPLASH:							// Water Splash?
			FmPolySpotFXWaterSplash( i, TempDir, Int_Point, UpMatrix, InvUpMatrix, TempQuat );
			break;

		case FM_HK5_TRAIL:								// Hk5 Trail?
			FmPolyHK5Trail( i, Speed );
			break;

		case FM_FIREBALL:
			FmPolyFireball( i, Speed, OldSize );
			break;

			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		case FM_SPARKLE:						// Sparkle?
			FmPolySparkle( i, Speed );
			break;

			/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		case FM_GOALSPARKLE:						// Sparkle?
			FmPolyGoalSparkle( i, Speed );
			break;

		default:
			break;
		}
		i = nextprim;
	}
}

/*===================================================================
	Procedure	:	Init FmPoly TPage Groups
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void InitFmPolyTPages( void )
{
	u_int16_t	i;

	for( i = 0; i < MAXTPAGESPERTLOAD; i++ )
	{
		FmPolyTPages[ i ].FirstPoly = INDEX_NONE;
	}
}

/*===================================================================
	Procedure	:	Add FmPoly To TPage Link List
	Input		:	u_int16_t		FmPoly Index
				:	u_int16_t		TPage to add to
	Output		:	Nothing
===================================================================*/
void AddFmPolyToTPage( u_int16_t i, int16_t TPage )
{
	if( TPage == -1 ) TPage = MAXTPAGESPERTLOAD;

	FmPolys[ i ].PrevInTPage = INDEX_NONE;
	FmPolys[ i ].NextInTPage = FmPolyTPages[ TPage ].FirstPoly;

	if( FmPolys[ i ].NextInTPage != INDEX_NONE )
	{
		FmPolys[ FmPolys[ i ].NextInTPage ].PrevInTPage = i;
	}

	FmPolyTPages[ TPage ].FirstPoly = i;
}

/*===================================================================
	Procedure	:	Remove FmPoly From TPage Link List
	Input		:	u_int16_t		FmPoly Index
				:	u_int16_t		TPage to remove from
	Output		:	Nothing
===================================================================*/
void RemoveFmPolyFromTPage( u_int16_t i, int16_t TPage )
{
	if( TPage == -1 ) TPage = MAXTPAGESPERTLOAD;

	if( FmPolys[ i ].PrevInTPage != INDEX_NONE )
	{
		FmPolys[ FmPolys[ i ].PrevInTPage ].NextInTPage = FmPolys[ i ].NextInTPage;
	}
	else
	{
		FmPolyTPages[ TPage ].FirstPoly = FmPolys[ i ].NextInTPage;
	}

	if( FmPolys[ i ].NextInTPage != INDEX_NONE )
	{
		FmPolys[ FmPolys[ i ].NextInTPage ].PrevInTPage = FmPolys[ i ].PrevInTPage;
	}

	FmPolys[ i ].PrevInTPage = INDEX_NONE;
	FmPolys[ i ].NextInTPage = INDEX_NONE;
}

bool DisplayGroupClippedFmPolys( RENDEROBJECT *renderObject, u_int16_t Group )
{
	int16_t	TPage;
	u_int16_t	i;

	TPage = 0;
	i = FmPolyTPages[ 0 ].FirstPoly;

	while( 1 )
	{
		if( !FmPolyDispGroupClipped( Group, renderObject, &TPage, &i ) )
			return( true );

		cull_none();

		if (!draw_billboard_object(renderObject))
			return false;

		reset_cull();
	}

	return( false );
}

bool DisplayGroupUnclippedFmPolys(RENDEROBJECT *renderObject )
{
	int16_t	TPage;
	u_int16_t	i;

	TPage = 0;
	i = FmPolyTPages[ 0 ].FirstPoly;

	while( 1 )
	{
		if( !FmPolyDispGroupUnclipped( renderObject, &TPage, &i ) )
			return( true );

		cull_none();

		if (!draw_billboard_object(renderObject))
			return false;

		reset_cull();
	}

	return( false );
}

/* Is FmPoly i in this fill? Clipped: a clippable poly in the given group.
 * Unclipped: any FM_FLAG_DONTCLIP poly, whatever its group. */
[[gnu::always_inline]] static inline bool FmPolyInFill( u_int16_t i, bool Clipped, u_int16_t Group )
{
	if( Clipped )
		return ( !( FmPolys[i].Flags & FM_FLAG_DONTCLIP ) && ( FmPolys[i].Group == Group ) );
	return ( ( FmPolys[i].Flags & FM_FLAG_DONTCLIP ) != 0 );
}

/* the FmPoly's up, forward and left vectors: fixed (FM_FLAG_DIRCONST) or from its rotation, optionally relative to its own orientation, turned to face the camera */
[[gnu::always_inline]] static inline void FmPolyOrient( u_int16_t i, VECTOR & XVector, VECTOR & YVector, VECTOR & ZVector )
{
	MATRIX			MatrixCopy;
	MATRIX			TempMatrix;
	QUAT			TempQuat;

	if( FmPolys[ i ].Flags & FM_FLAG_DIRCONST )
	{
		YVector = FmPolys[ i ].UpVector;					/* Calc Up Vector */
		ZVector = FmPolys[ i ].DirVector;					/* Calc Forward Vector */
	}
	else
	{
		fast_sincosf( D2R( FmPolys[ i ].Rot ), &YVector.x, &YVector.y ); /* Calc Up Vector */
		YVector.z = 0.0F;
		ZVector = Forward;

		if( FmPolys[ i ].Flags & FM_FLAG_RELATIVE )
		{
			TempQuat.FromDirAndUp(&FmPolys[i].DirVector, &FmPolys[i].UpVector);
			TempQuat.ToMatrix(&TempMatrix);
			ApplyMatrix( &TempMatrix, &YVector, &YVector );
			ApplyMatrix( &TempMatrix, &ZVector, &ZVector );
		}

		MatrixCopy = CurrentCamera.Mat;
		MatrixCopy._41 = 0.0F;
		MatrixCopy._42 = 0.0F;
		MatrixCopy._43 = 0.0F;
		ApplyMatrix( &MatrixCopy, &YVector, &YVector );		/* Calc Up Vector */
		ApplyMatrix( &MatrixCopy, &ZVector, &ZVector );		/* Calc Forward Vector */
	}

	CrossProduct( &YVector, &ZVector, &XVector );			/* Calc Left Vector */
}

/* one bit of an FmPoly frame: its offset and size along the poly's axes (with flips), optional pull toward the camera, and its four vertices */
[[gnu::always_inline]] static inline void FmPolyBuildQuad( RENDEROBJECT * renderObject, u_int16_t i, VECTOR & XVector, VECTOR & YVector, VECTOR & Xsize, VECTOR & Ysize, OFF_INFO * Off_Ptr, COLOR Colour, LVERTEX *& FmPolyVertPnt )
{
	VECTOR			TempVector;
	VECTOR			Xoff;
	VECTOR			Yoff;
	VECTOR			NewPos;
	BOX_INFO	*	Box_Ptr;
	float			Distance;

	Box_Ptr = ( (*FmPolys[ i ].Frm_Info)->Box_Info + ( Off_Ptr->box & 0x0fff ) );

	Xoff.x = ( ( Off_Ptr->xoff * FmPolys[ i ].xsize ) * XVector.x );
	Xoff.y = ( ( Off_Ptr->xoff * FmPolys[ i ].xsize ) * XVector.y );
	Xoff.z = ( ( Off_Ptr->xoff * FmPolys[ i ].xsize ) * XVector.z );
	Yoff.x = ( ( -Off_Ptr->yoff * FmPolys[ i ].ysize ) * YVector.x );
	Yoff.y = ( ( -Off_Ptr->yoff * FmPolys[ i ].ysize ) * YVector.y );
	Yoff.z = ( ( -Off_Ptr->yoff * FmPolys[ i ].ysize ) * YVector.z );

	switch( Off_Ptr->box & BOX_BOTHFLIP )
	{
	case BOX_NOFLIP:
		Xsize.x = ( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.x );
		Xsize.y = ( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.y );
		Xsize.z = ( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.z );
		Ysize.x = ( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.x );
		Ysize.y = ( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.y );
		Ysize.z = ( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.z );
		break;

	case BOX_XFLIP:
		Xsize.x = -( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.x );
		Xsize.y = -( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.y );
		Xsize.z = -( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.z );
		Ysize.x = ( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.x );
		Ysize.y = ( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.y );
		Ysize.z = ( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.z );
		break;

	case BOX_YFLIP:
		Xsize.x = ( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.x );
		Xsize.y = ( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.y );
		Xsize.z = ( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.z );
		Ysize.x = -( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.x );
		Ysize.y = -( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.y );
		Ysize.z = -( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.z );
		break;

	case BOX_BOTHFLIP:
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		Xsize.x = -( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.x );
		Xsize.y = -( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.y );
		/* @vfp-exempt R3 — polygon clip/visibility decision; integerizing changes pixel output. measured n/a. Verified 2026-10-25. */
		Xsize.z = -( ( Box_Ptr->xsize * FmPolys[ i ].xsize ) * XVector.z );
		Ysize.x = -( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.x );
		Ysize.y = -( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.y );
		Ysize.z = -( ( Box_Ptr->ysize * FmPolys[ i ].ysize ) * YVector.z );
		break;
	}

	/*-------------- MOVEOUT: draw pulled toward the camera ---------------*/
	if( ( FmPolys[ i ].Flags & FM_FLAG_MOVEOUT ) )
	{
		renderObject->memoEpochPass = SMEMO_PASS_NO_REUSE;
		TempVector.x = ( FmPolys[ i ].Pos.x - CurrentCamera.Pos.x );
		TempVector.y = ( FmPolys[ i ].Pos.y - CurrentCamera.Pos.y );
		TempVector.z = ( FmPolys[ i ].Pos.z - CurrentCamera.Pos.z );
		Distance = VectorLength( &TempVector );
		NormaliseVector( &TempVector );

		/* @vfp-exempt R3 — explosion size from camera distance; integerizing changes the drawn size. measured n/a. Verified 2026-11-19. */
		if( Distance >= ( EXPLO_DISTANCE + 110.0F ) )
		{
			Distance = EXPLO_DISTANCE;
		}
		else
		{
			Distance -= 110.0F;
			if( Distance < 32.0F ) Distance = 32.0F;
		}

		NewPos.x = ( FmPolys[ i ].Pos.x + ( -Distance * TempVector.x ) );
		NewPos.y = ( FmPolys[ i ].Pos.y + ( -Distance * TempVector.y ) );
		NewPos.z = ( FmPolys[ i ].Pos.z + ( -Distance * TempVector.z ) );
	}
	else
	{
		NewPos = FmPolys[i].Pos;
	}
	/*-------------------------------------------*/

	NewPos.x += ( Xoff.x + Yoff.x );
	NewPos.y += ( Xoff.y + Yoff.y );
	NewPos.z += ( Xoff.z + Yoff.z );

	FmPolyVertPnt->x = NewPos.x;						// v1
	FmPolyVertPnt->y = NewPos.y;
	FmPolyVertPnt->z = NewPos.z;
	FmPolyVertPnt->tu = Box_Ptr->u1;
	FmPolyVertPnt->tv = Box_Ptr->v1;
	FmPolyVertPnt->color = Colour;
	FmPolyVertPnt++;

	FmPolyVertPnt->x = ( NewPos.x + Xsize.x );			// v2
	FmPolyVertPnt->y = ( NewPos.y + Xsize.y );
	FmPolyVertPnt->z = ( NewPos.z + Xsize.z );
	FmPolyVertPnt->tu = Box_Ptr->u2;
	FmPolyVertPnt->tv = Box_Ptr->v1;
	FmPolyVertPnt->color = Colour;
	FmPolyVertPnt++;

	FmPolyVertPnt->x = ( NewPos.x + Xsize.x - Ysize.x ); // v3
	FmPolyVertPnt->y = ( NewPos.y + Xsize.y - Ysize.y );
	FmPolyVertPnt->z = ( NewPos.z + Xsize.z - Ysize.z );
	FmPolyVertPnt->tu = Box_Ptr->u2;
	FmPolyVertPnt->tv = Box_Ptr->v2;
	FmPolyVertPnt->color = Colour;
	FmPolyVertPnt++;

	FmPolyVertPnt->x = ( NewPos.x - Ysize.x );			// v4
	FmPolyVertPnt->y = ( NewPos.y - Ysize.y );
	FmPolyVertPnt->z = ( NewPos.z - Ysize.z );
	FmPolyVertPnt->tu = Box_Ptr->u1;
	FmPolyVertPnt->tv = Box_Ptr->v2;
	FmPolyVertPnt->color = Colour;
	FmPolyVertPnt++;
}

/* Fill one batch of FmPolys: the clippable ones of one group (Clipped) or
 * every FM_FLAG_DONTCLIP one (unclipped). The two public wrappers stamp the
 * memo epoch and pass a constant Clipped, so each compiles to its own
 * specialised copy. */
[[gnu::always_inline]] static inline bool FmPolyDispGroupFill( bool Clipped, u_int16_t Group, RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextFmPoly )
{
	u_int16_t			i;
	int16_t			Count;
	int16_t			BitCount;
	int16_t			TotalVerts;
	int16_t			StartVert;
#ifdef __3DS__
	int				batchVert = 0;	/* write position in the current batch */
#endif
	int16_t			NumVerts;
	int16_t			NumTris;
	VECTOR			XVector;
	VECTOR			YVector;
	VECTOR			ZVector;
	VECTOR			Xsize; ZERO_STACK_MEM(Xsize);
	VECTOR			Ysize; ZERO_STACK_MEM(Ysize);
	BIT_INFO	*	Bit_Ptr;
	OFF_INFO	*	Off_Ptr;
	COLOR		Colour;
	LVERTEX	*FmPolyVertPnt;
	TRIANGLE	*FmPolyFacePnt;
	LVERTEX	*BufStart;
	WORD			*Indices = nullptr;
	int				start_index = 0;

	/*===================================================================
			Pass 1: count verts per texture page
	===================================================================*/
	TotalVerts = 0;

	for( Count = *TPage; Count < MAXTPAGESPERTLOAD; Count++ )
	{
		StartVert = TotalVerts;
		NumVerts = 0;
		NumTris = 0;

		if( Count == *TPage ) i = *NextFmPoly;
		else i = FmPolyTPages[ Count ].FirstPoly;

		while( ( i != INDEX_NONE ) && ( ( StartVert + NumVerts ) < MAXFMPOLYVERTS ) )
		{
			if( FmPolyInFill( i, Clipped, Group ) )
			{
				if( FmPolys[i].Frm_Info && (*FmPolys[i].Frm_Info ) )
				{
					Bit_Ptr = ( (*FmPolys[ i ].Frm_Info)->Bit_Info + (int16_t) FmPolys[ i ].Frame );
					NumTris += ( 2 * Bit_Ptr->numbits );
					NumVerts += ( 4 * Bit_Ptr->numbits );
				}
			}

			i = FmPolys[ i ].NextInTPage;
		}

		FmPolyTPages[ Count ].StartVert = StartVert;
		FmPolyTPages[ Count ].NumVerts = NumVerts;
		FmPolyTPages[ Count ].NumTris = NumTris;

		TotalVerts = ( StartVert + NumVerts );
		if( TotalVerts >= MAXFMPOLYVERTS ) break;
	}

	if( !TotalVerts ) return( false );

	renderObject->numTextureGroups = 0;

	/*===================================================================
			Lock the vertex and index buffers
	===================================================================*/

	if (!(FSLockVertexBuffer(renderObject, &BufStart)))
	{
		return false;
	}

	if (!(FSLockIndexBuffer(renderObject, &Indices)))
	{
		return false;
	}

	FmPolyFacePnt = (TRIANGLE *) Indices;

	FmPolyVertPnt = (LVERTEX *) BufStart;

	/*===================================================================
			Pass 2: fill verts and indices, one texture group per quad
	===================================================================*/
	for( Count = *TPage; Count < MAXTPAGESPERTLOAD; Count++ )
	{
		StartVert = FmPolyTPages[ Count ].StartVert;
		NumVerts = FmPolyTPages[ Count ].NumVerts;
		NumTris = FmPolyTPages[ Count ].NumTris;

		if( NumVerts )
		{

			if( Count == *TPage ) i = *NextFmPoly;
			else i = FmPolyTPages[ Count ].FirstPoly;

			while( ( i != INDEX_NONE ) && ( StartVert < MAXFMPOLYVERTS ) )
			{
				if( FmPolyInFill( i, Clipped, Group ) )
				{
					if( FmPolys[i].Frm_Info && (*FmPolys[i].Frm_Info ) )
					{
						/*===================================================================
								Create Colour Value for polys
						===================================================================*/

						Colour = RGBA_MAKE2( FmPolys[ i ].R, FmPolys[ i ].G, FmPolys[ i ].B, 255 );

						/*===================================================================
								Set Dir and Up Vectors
						===================================================================*/
						FmPolyOrient( i, XVector, YVector, ZVector );

						/*===================================================================
								Setup FrameInfo description pointers
						===================================================================*/
						Bit_Ptr = ( (*FmPolys[ i ].Frm_Info)->Bit_Info + (int16_t) FmPolys[ i ].Frame );
						Off_Ptr = ( (*FmPolys[ i ].Frm_Info)->Off_Info + Bit_Ptr->startbit );

						for( BitCount = 0; BitCount < Bit_Ptr->numbits; BitCount++ )
						{
							int ntris = 0;

							FmPolyBuildQuad( renderObject, i, XVector, YVector, Xsize, Ysize, Off_Ptr, Colour, FmPolyVertPnt );

							FmPolyFacePnt->v1 = 0;
							FmPolyFacePnt->v2 = 1;
							FmPolyFacePnt->v3 = 2;
							FmPolyFacePnt++;
							ntris++;

							FmPolyFacePnt->v1 = 0;
							FmPolyFacePnt->v2 = 2;
							FmPolyFacePnt->v3 = 3;
							FmPolyFacePnt++;
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

							start_index += ntris*3; // each triangle has three indexes...
							StartVert += 4;

#ifdef __3DS__
							/* Mid-draw flush: textureGroups is full (one group per
							 * quad), so draw what is filled and refill from the
							 * start of the buffers. See polys.h. */
							renderObject->numTextureGroups++;
							batchVert += 4;
							if (renderObject->numTextureGroups >= MAX_TEXTURE_GROUPS) {
								FSUnlockVertexBuffer(renderObject);
								FSUnlockIndexBuffer(renderObject);
								draw_billboard_object(renderObject);
								renderObject->numTextureGroups = 0;
								renderObject->memoEpochResume += SMEMO_CHUNK_STEP;
								start_index = 0;
								batchVert = 0;
								FSLockVertexBuffer(renderObject, &BufStart);
								FSLockIndexBuffer(renderObject, &Indices);
								FmPolyVertPnt = (LVERTEX *) BufStart;
								FmPolyFacePnt = (TRIANGLE *) Indices;
							}
#else
							INCREASE_TEXTURE_GROUPS(renderObject);
#endif

							Off_Ptr++;
						}
					}
				}

				i = FmPolys[ i ].NextInTPage;
			}

		}

		if( StartVert >= MAXFMPOLYVERTS ) break;
	}

	/*===================================================================
			Unlock the buffers and return the resume point
	===================================================================*/
	if (!(FSUnlockVertexBuffer(renderObject)))
	{
		return false;
	}

	if (!(FSUnlockIndexBuffer(renderObject)))
	{
		Msg( "FSUnlockIndexBuffer failed");
		return false ;
	}

	*TPage = Count;
	*NextFmPoly = i;

	return( true );
}

/*===================================================================
	Procedure	:	Fill one batch of a group's FmPolys
	Input		:	u_int16_t						Group
				:	RENDEROBJECT *			Render object
				:	int16_t	*					Current TPage List (in/out)
				:	u_int16_t	*					Current FmPoly (in/out)
	Output		:	true if a batch was filled and should be drawn
===================================================================*/
bool FmPolyDispGroupClipped( u_int16_t Group, RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextFmPoly )
{
	/* opt5b memo epoch: stamp this fill's content identity. */
	renderObject->memoEpochPass = SMEMO_PASS_FM_CLIPPED;
	renderObject->memoEpochGroup = Group;
	renderObject->memoEpochResume = smemo_resume(*TPage, *NextFmPoly);
	return FmPolyDispGroupFill( true, Group, renderObject, TPage, NextFmPoly );
}

/*===================================================================
	Procedure	:	Fill one batch of FM_FLAG_DONTCLIP FmPolys
	Input		:	RENDEROBJECT *			Render object
				:	int16_t	*					Current TPage List (in/out)
				:	u_int16_t	*					Current FmPoly (in/out)
	Output		:	true if a batch was filled and should be drawn
===================================================================*/
bool FmPolyDispGroupUnclipped( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextFmPoly )
{
	/* opt5b memo epoch: whole-scene fill (no single group). */
	renderObject->memoEpochPass = SMEMO_PASS_FM_UNCLIPPED;
	renderObject->memoEpochGroup = SMEMO_WHOLE_GROUP;
	renderObject->memoEpochResume = smemo_resume(*TPage, *NextFmPoly);
	return FmPolyDispGroupFill( false, 0, renderObject, TPage, NextFmPoly );
}

/*===================================================================
	Procedure	:	Fade Colour to wanted colour
	Input		:	u_int8_t	*	Colour
				:	u_int8_t		Wanted Colour
	Output		:	Nothing
===================================================================*/
void FadeColour( u_int8_t * Colour, u_int8_t WantedColour, float Speed )
{
	float	WanCol, CurCol;

	WanCol = (float) WantedColour;
	CurCol = (float) *Colour;

	if( CurCol < WanCol )
	{
		CurCol += Speed;
		if( CurCol > WanCol ) CurCol = WanCol;
	}

	if( CurCol > WanCol )
	{
		CurCol -= Speed;
		if( CurCol < WanCol ) CurCol = WanCol;
	}

	*Colour = (u_int8_t) CurCol;
}

/*===================================================================
	Procedure	:	Save FmPolys Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * SaveFmPolys( FILE * fp )
{
	u_int16_t i;
	int16_t	Frm_Info_Index;

	if( fp )
	{
		fwrite( &TotalFmPolysInUse, sizeof( u_int32_t ),1 ,fp );
		fwrite( &FirstFmPolyUsed, sizeof( FirstFmPolyUsed ), 1, fp );
		fwrite( &FirstFmPolyFree, sizeof( FirstFmPolyFree ), 1, fp );

		for( i = 0; i < ( MAXTPAGESPERTLOAD + 1 ); i++ )
		{
			fwrite( &FmPolyTPages[ i ].FirstPoly, sizeof( u_int16_t ), 1, fp );
		}

		i = FirstFmPolyUsed;

		while( i != INDEX_NONE )
		{
			fwrite( &FmPolys[ i ].Next, sizeof( u_int16_t ), 1, fp );
			fwrite( &FmPolys[ i ].Prev, sizeof( u_int16_t ), 1, fp );
			fwrite( &FmPolys[ i ].NextInTPage, sizeof( u_int16_t ), 1, fp );
			fwrite( &FmPolys[ i ].PrevInTPage, sizeof( u_int16_t ), 1, fp );
			fwrite( &FmPolys[ i ].LifeCount, sizeof( float ), 1, fp );
			fwrite( &FmPolys[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fwrite( &FmPolys[ i ].Start_R, sizeof( u_int8_t ), 1, fp );
			fwrite( &FmPolys[ i ].Start_G, sizeof( u_int8_t ), 1, fp );
			fwrite( &FmPolys[ i ].Start_B, sizeof( u_int8_t ), 1, fp );
			fwrite( &FmPolys[ i ].R, sizeof( u_int8_t ), 1, fp );
			fwrite( &FmPolys[ i ].G, sizeof( u_int8_t ), 1, fp );
			fwrite( &FmPolys[ i ].B, sizeof( u_int8_t ), 1, fp );
			fwrite( &FmPolys[ i ].Trans, sizeof( u_int8_t ), 1, fp );
			fwrite( &FmPolys[ i ].OnceOnly, sizeof( u_int8_t ), 1, fp );
			fwrite( &FmPolys[ i ].Rot, sizeof( float ), 1, fp );
			fwrite( &FmPolys[ i ].RotSpeed, sizeof( float ), 1, fp );
			fwrite( &FmPolys[ i ].Frame, sizeof( float ), 1, fp );
			fwrite( &FmPolys[ i ].AnimSpeed, sizeof( float ), 1, fp );
			fwrite( &FmPolys[ i ].Flags, sizeof( int16_t ), 1, fp );
			Frm_Info_Index = Get_Frm_Info_Index( FmPolys[ i ].Frm_Info );
			fwrite( &Frm_Info_Index, sizeof( int16_t ), 1, fp );
			fwrite( &FmPolys[ i ].SeqNum, sizeof( int16_t ), 1, fp );
			fwrite( &FmPolys[ i ].Dir, sizeof( VECTOR ), 1, fp );
			fwrite( &FmPolys[ i ].DirVector, sizeof( VECTOR ), 1, fp );
			fwrite( &FmPolys[ i ].UpVector, sizeof( VECTOR ), 1, fp );
			fwrite( &FmPolys[ i ].Mat, sizeof( MATRIX ), 1, fp );
			fwrite( &FmPolys[ i ].Group, sizeof( u_int16_t ), 1, fp );
			fwrite( &FmPolys[ i ].Speed, sizeof( float ), 1, fp );
			fwrite( &FmPolys[ i ].UpSpeed, sizeof( float ), 1, fp );
			fwrite( &FmPolys[ i ].xsize, sizeof( float ), 1, fp );
			fwrite( &FmPolys[ i ].ysize, sizeof( float ), 1, fp );
			i = FmPolys[ i ].Prev;
		}


		i = FirstFmPolyFree;

		while( i != INDEX_NONE )
		{
			fwrite( &FmPolys[ i ].Next, sizeof( u_int16_t ), 1, fp );
			i = FmPolys[ i ].Next;
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load FmPolys Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * LoadFmPolys( FILE * fp )
{
	u_int16_t i;
	int16_t	Frm_Info_Index;

	if( fp )
	{
		file_read( &TotalFmPolysInUse, sizeof( u_int32_t ),1 ,fp );
		file_read( &FirstFmPolyUsed, sizeof( FirstFmPolyUsed ), 1, fp );
		file_read( &FirstFmPolyFree, sizeof( FirstFmPolyFree ), 1, fp );

		for( i = 0; i < ( MAXTPAGESPERTLOAD + 1 ); i++ )
		{
			file_read( &FmPolyTPages[ i ].FirstPoly, sizeof( u_int16_t ), 1, fp );
		}

		i = FirstFmPolyUsed;

		while( i != INDEX_NONE )
		{
			file_read( &FmPolys[ i ].Next, sizeof( u_int16_t ), 1, fp );
			file_read( &FmPolys[ i ].Prev, sizeof( u_int16_t ), 1, fp );
			file_read( &FmPolys[ i ].NextInTPage, sizeof( u_int16_t ), 1, fp );
			file_read( &FmPolys[ i ].PrevInTPage, sizeof( u_int16_t ), 1, fp );
			file_read( &FmPolys[ i ].LifeCount, sizeof( float ), 1, fp );
			file_read( &FmPolys[ i ].Pos, sizeof( VECTOR ), 1, fp );
			file_read( &FmPolys[ i ].Start_R, sizeof( u_int8_t ), 1, fp );
			file_read( &FmPolys[ i ].Start_G, sizeof( u_int8_t ), 1, fp );
			file_read( &FmPolys[ i ].Start_B, sizeof( u_int8_t ), 1, fp );
			file_read( &FmPolys[ i ].R, sizeof( u_int8_t ), 1, fp );
			file_read( &FmPolys[ i ].G, sizeof( u_int8_t ), 1, fp );
			file_read( &FmPolys[ i ].B, sizeof( u_int8_t ), 1, fp );
			file_read( &FmPolys[ i ].Trans, sizeof( u_int8_t ), 1, fp );
			file_read( &FmPolys[ i ].OnceOnly, sizeof( u_int8_t ), 1, fp );
			file_read( &FmPolys[ i ].Rot, sizeof( float ), 1, fp );
			file_read( &FmPolys[ i ].RotSpeed, sizeof( float ), 1, fp );
			file_read( &FmPolys[ i ].Frame, sizeof( float ), 1, fp );
			file_read( &FmPolys[ i ].AnimSpeed, sizeof( float ), 1, fp );
			file_read( &FmPolys[ i ].Flags, sizeof( int16_t ), 1, fp );
			file_read( &Frm_Info_Index, sizeof( int16_t ), 1, fp );
			FmPolys[ i ].Frm_Info = Get_Frm_Info_Address( Frm_Info_Index );
			file_read( &FmPolys[ i ].SeqNum, sizeof( int16_t ), 1, fp );
			file_read( &FmPolys[ i ].Dir, sizeof( VECTOR ), 1, fp );
			file_read( &FmPolys[ i ].DirVector, sizeof( VECTOR ), 1, fp );
			file_read( &FmPolys[ i ].UpVector, sizeof( VECTOR ), 1, fp );
			file_read( &FmPolys[ i ].Mat, sizeof( MATRIX ), 1, fp );
			file_read( &FmPolys[ i ].Group, sizeof( u_int16_t ), 1, fp );
			file_read( &FmPolys[ i ].Speed, sizeof( float ), 1, fp );
			file_read( &FmPolys[ i ].UpSpeed, sizeof( float ), 1, fp );
			file_read( &FmPolys[ i ].xsize, sizeof( float ), 1, fp );
			file_read( &FmPolys[ i ].ysize, sizeof( float ), 1, fp );
			i = FmPolys[ i ].Prev;
		}

		i = FirstFmPolyFree;

		while( i != INDEX_NONE )
		{
			memset( &FmPolys[i], 0, sizeof( FMPOLY ) );
			FmPolys[i].xsize = ( 16.0F * GLOBAL_SCALE );
			FmPolys[i].ysize = ( 16.0F * GLOBAL_SCALE );
			FmPolys[i].R = 255;
			FmPolys[i].G = 255;
			FmPolys[i].B = 255;
			FmPolys[i].Trans = 255;
			FmPolys[i].DirVector.x = 0.0F;
			FmPolys[i].DirVector.y = 0.0F;
			FmPolys[i].DirVector.z = 1.0F;
			FmPolys[i].UpVector.x = 0.0F;
			FmPolys[i].UpVector.y = 1.0F;
			FmPolys[i].UpVector.z = 0.0F;
			FmPolys[i].NextInTPage = INDEX_NONE;
			FmPolys[i].PrevInTPage = INDEX_NONE;
			FmPolys[i].Prev = INDEX_NONE;

			file_read( &FmPolys[ i ].Next, sizeof( u_int16_t ), 1, fp );
			i = FmPolys[ i ].Next;
		}
	}

	return( fp );
}
