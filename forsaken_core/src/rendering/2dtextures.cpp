/*===================================================================
	Includes
===================================================================*/
#include <stdio.h>

#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "util/util.h"

#include	<string.h>
#include	"game/main.h"
#include	"data/mload.h"
#include	"data/tload.h"
#include	"rendering/2dtextures.h"
#include	"game/triggers.h"
#include	"game/pickups.h"
#include	"game/restart.h"
#include "game/oct2.h"
#include "ui/title.h"

/*===================================================================
	Globals
===================================================================*/
#define	DO_SCALE		true
#define	DONT_SCALE		false

int8_t	*	OffsetPath = (int8_t*)"data\\offsets\\";
int8_t  * FontBigFile = (int8_t*)"fontbig.off";
int8_t	*	Font512File = (int8_t*)"font512.off";
int8_t	*	VduFont512	= (int8_t*)"f512x384.off";
int8_t		FontFile[ 16 ];
int8_t    FontFileBig [ 16 ];
int8_t		TitleFontFile[ 16 ];

float VduScaleX, VduScaleY;


	FRAME_INFO	*	Arc_Header = NULL;
	FRAME_INFO	*	Bits_Header = NULL;
	FRAME_INFO	*	Exp2_Header = NULL;
	FRAME_INFO	*	Flare_Header = NULL;
	FRAME_INFO	*	Laser_Header = NULL;
	FRAME_INFO	*	Pulsar_Header = NULL;
	FRAME_INFO	*	Pulsar_Exp_Header = NULL;
	FRAME_INFO	*	Pulsar_Trail_Header = NULL;
	FRAME_INFO	*	Shrapnel_Header = NULL;
	FRAME_INFO	*	Exp_Header = NULL;
	FRAME_INFO	*	Trojax1_Header = NULL;
	FRAME_INFO	*	Trojax2_Header = NULL;
	FRAME_INFO	*	Trojax_Exp_Header = NULL;
	FRAME_INFO	*	Circle_Header = NULL;
	FRAME_INFO	*	Blood_Header = NULL;
	FRAME_INFO	*	LensFlare_Header = NULL;
	FRAME_INFO	*	AfterBurner_Header = NULL;
	FRAME_INFO	*	GravgonTrail_Header = NULL;
	FRAME_INFO	*	SolarisTrail_Header = NULL;
	FRAME_INFO	*	NewTrail_Header = NULL;
	FRAME_INFO	*	PickupRegen_Header = NULL;
	FRAME_INFO	*	ScatterTrail_Header = NULL;
	FRAME_INFO	*	GreyFlare_Header = NULL;
	FRAME_INFO	*	BigFlash_Header = NULL;
	FRAME_INFO	*	BloodSplat_Header = NULL;
	FRAME_INFO	*	BloodDribble_Header = NULL;
	FRAME_INFO	*	Target_Header = NULL;
	FRAME_INFO	*	Numbers_Header = NULL;
	FRAME_INFO	*	Numbers2_Header = NULL;
	FRAME_INFO	*	Text512_Header = NULL;
	FRAME_INFO  * Text512b_Header = NULL;
	FRAME_INFO	*	Orb_Header = NULL;
	FRAME_INFO	*	Multiple_Header = NULL;
	FRAME_INFO	*	Solid_Header = NULL;
	FRAME_INFO	*	Thermo_Header = NULL;
	FRAME_INFO	*	NMEBullet_Header = NULL;
	FRAME_INFO	*	BigExp1_Header = NULL;
	FRAME_INFO	*	Mushroom_Header = NULL;
	FRAME_INFO	*	Bubble_Header = NULL;
	FRAME_INFO	*	EnemyGen_Header = NULL;
	FRAME_INFO	*	Drip_Header = NULL;
	FRAME_INFO	*	Restart_Header = NULL;
	FRAME_INFO	*	BlueSpark_Header = NULL;
	FRAME_INFO	*	Tentacle_Header = NULL;
	FRAME_INFO	*	Flag_Header = NULL;
	FRAME_INFO	*	Flags_Header = NULL;
	FRAME_INFO	*	CreditsFont_Header = NULL;

OFF_FILES OffsetFiles[] = {

	{ (int8_t*)"Credits.off",	&CreditsFont_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Credits Font
	{ (int8_t*)"Num2.off",		&Numbers_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Countdown Numbers
	{ (int8_t*)"Num3.off",		&Numbers2_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Countdown Numbers
	{ (int8_t*)"arc.off",		&Arc_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Arc
	{ (int8_t*)"bits.off",		&Bits_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Bits
	{ (int8_t*)"expl.off",		&Exp2_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Gregs Explosion
	{ (int8_t*)"flare.off",		&Flare_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Flare
	{ (int8_t*)"laser.off",		&Laser_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Laser
	{ (int8_t*)"pulsar.off",		&Pulsar_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Pulsar
	{ (int8_t*)"puls_exp.off",	&Pulsar_Exp_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Pulsar Explosion
	{ (int8_t*)"pultrail.off",	&Pulsar_Trail_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Pulsar Trail
	{ (int8_t*)"shrapnel.off",	&Shrapnel_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Shrapnel Explosion
	{ (int8_t*)"smlexpl.off",	&Exp_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// My Explosion
	{ (int8_t*)"Trojax1.off",	&Trojax1_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Trojax 1
	{ (int8_t*)"Trojax2.off",	&Trojax2_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Trojax 2
	{ (int8_t*)"Troj_exp.off",	&Trojax_Exp_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Trojax Explosion
	{ (int8_t*)"Circle.off",		&Circle_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Pickup Circle
	{ (int8_t*)"Blood.off",		&Blood_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Blood
	{ (int8_t*)"Lensflar.off",	&LensFlare_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Lensflare
	{ (int8_t*)"Afterbrn.off",	&AfterBurner_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Afterburner
	{ (int8_t*)"Grvtrail.off",	&GravgonTrail_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Gravgon Trail
	{ (int8_t*)"Soltrail.off",	&SolarisTrail_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Solaris Trail
	{ (int8_t*)"Newtrail.off",	&NewTrail_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// New Trail
	{ (int8_t*)"Pikgen.off",		&PickupRegen_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Pickup Regeneration
    { (int8_t*)"Scttrail.off",	&ScatterTrail_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Scatter Trail
	{ (int8_t*)"Greyflar.off",	&GreyFlare_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Grey Flare Trail
	{ (int8_t*)"BigFlash.off",	&BigFlash_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Grey Flare Trail
	{ (int8_t*)"Splat.off",		&BloodSplat_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Blood Splat
	{ (int8_t*)"SpltDrib.off",	&BloodDribble_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Blood Splat & Dribble
	{ (int8_t*)"Target.off",		&Target_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Target
	{ (int8_t*)"Orb.off",		&Orb_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Orb
	{ (int8_t*)"Solid.off",		&Solid_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Solid 8x8
	{ (int8_t*)"Multiple.off",	&Multiple_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Multiple
	{ (int8_t*)"Thermo.off",		&Thermo_Header,			DONT_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Thermo Meter
	{ (int8_t*)"Nmebuits.off",	&NMEBullet_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// NME Bullets
	{ (int8_t*)"Bigexp1.off",	&BigExp1_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Big Explosion 1
	{ (int8_t*)"Mushroom.off",	&Mushroom_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Mushroom Explosion
	{ (int8_t*)"Bubble.off",		&Bubble_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Bubble Header
	{ (int8_t*)"Nmegen.off",		&EnemyGen_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Enemy Generation
	{ (int8_t*)"Bubble.off",		&Drip_Header,			DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Drip
	{ (int8_t*)"Restart.off",	&Restart_Header,		DONT_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Restart
	{ (int8_t*)"BlueSprk.off",	&BlueSpark_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Blue Spark
	{ (int8_t*)"Tentacle.off",	&Tentacle_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Tentacle
	{ (int8_t*)"Flag.off",		&Flag_Header,			DONT_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Flag
	{ (int8_t*)"Flags.off",		&Flags_Header,			DONT_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Flags
	{ &FontFile[ 0 ],	&Text512_Header,		DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// 512x284 Font/Text
	{ &FontFileBig[0], &Text512b_Header, DONT_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 }, // bigger Font/text
	{ NULL,				NULL,					DO_LOAD,	DO_SCALE,	LOAD_TPAGES_FINISH, 0, 0 },

};

FRAME_INFO	*	Title_Chars1_Header;
FRAME_INFO	*	Title_Chars2_Header;
FRAME_INFO	*	Title_Fonts_Header;
FRAME_INFO	*	Title_LevelPics_Header;
FRAME_INFO	*	Title_TVFrame_Header;

OFF_FILES Title_OffsetFiles[] = {

	{ (int8_t*)"chars1.off",			&Title_Chars1_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 },		
	{ (int8_t*)"chars2.off",			&Title_Chars2_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 },		
	{ (int8_t*)"",					&Title_LevelPics_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES_PLACEHOLDER | LOAD_TPAGES, 256, 128 },		
	{ (int8_t*)"",					&Title_TVFrame_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES_PLACEHOLDER | LOAD_TPAGES, 128, 128 },		
	{ &TitleFontFile[ 0 ],	&Title_Fonts_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 },	// vdu font page
	{ &FontFile[ 0 ],		&Text512_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 },	// 512x284 Font/Text
  { &FontFileBig[ 0 ], &Text512b_Header, DONT_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 }, // bigger font/text
	{ NULL,					NULL, DO_LOAD, DO_SCALE, LOAD_TPAGES_FINISH, 0, 0 },			  
};

OFF_FILES Splash_OffsetFiles[] = {
	{ (int8_t*)"Credits.off", &CreditsFont_Header,	DO_LOAD,	DONT_SCALE, LOAD_TPAGES, 0, 0 },	// Credits Font
	{ (int8_t*)"Bubble.off", &Drip_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 },				// Drip
	{ (int8_t*)"bits.off", &Bits_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 },				// Bits
	{ (int8_t*)"flare.off", &Flare_Header, DO_LOAD, DONT_SCALE, LOAD_TPAGES, 0, 0 },				// Flare
	{ NULL, NULL, DO_SCALE, DO_SCALE, LOAD_TPAGES_FINISH, 0, 0 },			  
};

typedef struct
{
	BOX_INFO box_info;
	OFF_INFO off_info;
	BIT_INFO bit_info;
} OFF_BUFFER;

/*===================================================================
	Procedure	:	Load Texture Pages, Offsets and Sizes
				:	for 2D Faceme polygons
	Input		:	int8_t		*	Filename of offset file
				:	bool			Scale
				:	bool			LoadTPages
	Output		:	FRAME_INFO	*	Structure for offset file
===================================================================*/
FRAME_INFO * Load_Off_File( int8_t * Filename, bool Scale, int LoadTPages, int16_t *last_tpage, int16_t xsize, int16_t ysize, bool placeholder )
{
	int16_t			Name_Index[ MAX_NAMES ];
	int8_t		*	Off_Buffer;
	u_int32_t			File_Size;
	u_int32_t			Read_Size;
	int8_t		*	Char_Ptr;
	int16_t		*	Short_Ptr;
	BOX_INFO	*	Box_Info;
	int16_t			Num_Boxes;
	BIT_INFO	*	Bit_Info;
	int16_t			Num_Frames;
	OFF_INFO	*	Off_Info;
	int16_t			Num_Offs;
	FRAME_INFO	*	Frm_Info = NULL;
	int16_t			Num_TPages;
	int16_t			Count;
	char			tpagename[32];
	int8_t			TempFilename[ 256 ];
	OFF_BUFFER		*Placeholder_Off_Buffer;
	
	Frm_Info = (FRAME_INFO*)malloc( sizeof( FRAME_INFO ) );							// Allocate memory for return structure
	if( Frm_Info == NULL ) return( NULL );

	if( placeholder )
	{
		Placeholder_Off_Buffer = (OFF_BUFFER *)malloc( sizeof( OFF_BUFFER ) );									// Allocate memory 
		if( Placeholder_Off_Buffer == NULL )
		{
			free( Frm_Info );
			return( NULL );
		}

		Frm_Info->vid_tpage_index = AddTexture( &Tloadheader, "", true , Scale , false, xsize, ysize );	// add dummy texture
		if( Frm_Info->vid_tpage_index == -1 )
		{
			free( Frm_Info );
			free( Placeholder_Off_Buffer );
			return ( NULL );
		}

		Frm_Info->tpage_name[ 0 ] = 0;
		Frm_Info->sys_tpage_index = -1;

		Box_Info = &Placeholder_Off_Buffer->box_info;
		Off_Info = &Placeholder_Off_Buffer->off_info;
		Bit_Info = &Placeholder_Off_Buffer->bit_info;

		Box_Info->tpage = Frm_Info->vid_tpage_index; 
		Box_Info->u1 = 0.0F;
		Box_Info->v1 = 0.0F;
		Box_Info->u2 = 1.0F;
		Box_Info->v2 = 1.0F;
		Box_Info->xsize = xsize;
		Box_Info->ysize = ysize;

		Off_Info->xoff = 0;
		Off_Info->yoff = 0;
		Off_Info->box = 0;

		Bit_Info->startbit = 0;
		Bit_Info->numbits = 1;

		Frm_Info->Num_Frames = 1;
		Frm_Info->File_Addr = ( int8_t *)Placeholder_Off_Buffer;
		Frm_Info->Box_Info = Box_Info;
		Frm_Info->Off_Info = Off_Info;
		Frm_Info->Bit_Info = Bit_Info;
		Frm_Info->NumBoxes = 1;

	}
	else
	{

		File_Size = Get_File_Size( (char*)Filename );								// Get size of file to load
		if( File_Size == 0 )
		{
			free( Frm_Info );
			return( NULL );
		}
																			
		/* +8 bytes headroom: we may need to memmove the post-filenames
		   section forward by up to 3 bytes to make BOX_INFO* 4-byte
		   aligned (BOX_INFO has float fields, and ARM's vldr requires
		   4-byte alignment). See alignment shim below. */
		Off_Buffer = (int8_t*)malloc( File_Size + 8 );
		if( Off_Buffer == NULL )
		{
			free( Frm_Info );
			return( NULL );
		}
																			
		Read_Size = Read_File( (char*)Filename, (char*)Off_Buffer, File_Size );			// Read file
		if( Read_Size != File_Size )
		{
			free( Frm_Info );
			free( Off_Buffer );
			return( NULL );
		}
																			
		Short_Ptr = (int16_t *) Off_Buffer;									
																			
		Num_TPages = *Short_Ptr++;											// Number of possible new texture pages

		Char_Ptr = (int8_t *) Short_Ptr;

		for( Count = 0; Count < Num_TPages; Count++ )
		{
			sprintf( (char*)&TempFilename[ 0 ], "data\\textures\\%s", Char_Ptr );

			if( LoadTPages & LOAD_TPAGES)
			{
				Name_Index[ Count ] = AddTexture( &Tloadheader, (char*)&TempFilename[ 0 ], true , Scale , false, 0, 0 );
				strcpy ( tpagename, (char*)Char_Ptr );
				*last_tpage = Name_Index[ Count ];
				if( Name_Index[ Count ] == -1 )
				{
		   			free( Frm_Info );
					free( Off_Buffer );
					return( NULL );
				}
			}

			Char_Ptr += ( strlen( (char*)Char_Ptr ) + 1 );
		}

		// copy t-page info into Frm_Info - this is only valid if only one t-page is used!
		strcpy ( Frm_Info->tpage_name, tpagename );
		Frm_Info->vid_tpage_index = Name_Index[ 0 ];
		Frm_Info->sys_tpage_index = -1;

		Short_Ptr = (int16_t *) Char_Ptr;
		Num_Boxes = *Short_Ptr++;											// Number of Boxes

		/* BOX_INFO contains floats; ARM's vldr requires 4-byte alignment.
		   Short_Ptr here has skidded off 4-byte alignment because of the
		   variable-length filename section above. Shift the remaining
		   bytes forward just enough to land Short_Ptr on a 4-byte boundary
		   (Off_Buffer was malloc'd with +8 headroom for this). */
		if( ((uintptr_t)Short_Ptr) & 3 )
		{
			size_t pad = (4 - (((uintptr_t)Short_Ptr) & 3)) & 3;
			size_t used = (size_t)( (int8_t*)Short_Ptr - Off_Buffer );
			size_t remaining = (size_t)File_Size - used;
			memmove( (int8_t*)Short_Ptr + pad, Short_Ptr, remaining );
			Short_Ptr = (int16_t *)( (int8_t*)Short_Ptr + pad );
		}

		Box_Info = (BOX_INFO *) Short_Ptr;

		Short_Ptr = (int16_t *) ( Box_Info + Num_Boxes );
		Num_Frames = *Short_Ptr++;											// Number of Frames
		Bit_Info = (BIT_INFO *) Short_Ptr;

		Short_Ptr = (int16_t *) ( Bit_Info + Num_Frames );
		Num_Offs = *Short_Ptr++;											// Number of Offsets
		Off_Info = (OFF_INFO *) Short_Ptr;

		for( Count = 0; Count < Num_Boxes; Count++ )
		{
			if (LoadTPages & LOAD_TPAGES)
				Box_Info[ Count ].tpage = Name_Index[ Box_Info[ Count ].tpage ];	// Update TPages
		}

		Frm_Info->Num_Frames = Num_Frames;
		Frm_Info->File_Addr = Off_Buffer;
		Frm_Info->Box_Info = Box_Info;
		Frm_Info->Off_Info = Off_Info;
		Frm_Info->Bit_Info = Bit_Info;
		Frm_Info->NumBoxes = Num_Boxes;
	}

	return( Frm_Info );
}

/*===================================================================
	Procedure	:	Load All Offset Files
	Input		:	Nothing
	Output		:	bool	True/False
===================================================================*/
bool Load_All_Off_Files( OFF_FILES * FileInfo )
{
	int8_t			TempFilename[ 256 ];
	int16_t			last_tpage;

	if( !FileInfo ) return( false );

	EnableRelevantOffFiles( FileInfo );

	// polytext is default now
	strcpy( (char*)&FontFile[ 0 ], (char*)Font512File );
	strcpy( (char*)&FontFileBig [ 0 ], (char*)FontBigFile );

	// this still uses the old blitted font file
	strcpy ( (char*)&TitleFontFile[ 0 ], (char*)VduFont512 );

	//
#ifdef __3DS__
	/* 3DS: 320x240 is smaller than the 512x384 font atlas, so the
	   floorf() used on PC (for crisp integer-pixel scaling at 640x480+)
	   would truncate the ratio to 0, making all text invisible.
	   Use the raw ratio instead — text will be slightly downscaled. */
	VduScaleX = (float)render_info.window_size.cx / 512.0F;
	VduScaleY = (float)render_info.window_size.cy / 384.0F;
#else
	VduScaleX = (float)floorf((double)((float)render_info.window_size.cx / 512.0F));
	VduScaleY = (float)floorf((double)((float)render_info.window_size.cy / 384.0F));
#endif

	last_tpage = 0;

	while (!( FileInfo->LoadTPages & LOAD_TPAGES_FINISH ) )
	{
		if( FileInfo->LoadEnable )
		{
			if ( strcmp((char*)&FileInfo->Filename[ 0 ], (char*)&TitleFontFile[ 0 ] ) == 0 )
			{
				FileInfo->LoadTPages = LOAD_TPAGES;
			}

			if ( ( !FileInfo->Filename[0] ) && (!( FileInfo->LoadTPages & LOAD_TPAGES_PLACEHOLDER ) ) )
			{
				FileInfo++;
				continue;
			}

			sprintf((char*)TempFilename,"%s%s",OffsetPath,FileInfo->Filename);

			*FileInfo->InfoPtrAddr = Load_Off_File( &TempFilename[ 0 ], FileInfo->Scale, FileInfo->LoadTPages, &last_tpage, FileInfo->xsize, FileInfo->ysize, ( FileInfo->LoadTPages & LOAD_TPAGES_PLACEHOLDER ) );
			if( *FileInfo->InfoPtrAddr == NULL )
			{
				Msg( "offset file missing or too many tpages\n%s\n", &TempFilename[ 0 ] );
				return false;
			}
		}
		FileInfo++;
	}

	return true;
}

/*===================================================================
	Procedure	:	Free All Offset Files
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void Free_All_Off_Files( OFF_FILES * FileInfo )
{
	if( !FileInfo ) return;

	while( FileInfo->InfoPtrAddr != NULL )
	{
		if( *FileInfo->InfoPtrAddr != NULL )
		{
			free( (*FileInfo->InfoPtrAddr)->File_Addr );
			free( *FileInfo->InfoPtrAddr );
			*FileInfo->InfoPtrAddr = NULL;
		}
		FileInfo++;
	}
}

/*===================================================================
	Procedure	:	Get TPage
	Input		:	FRAME_INFO	*	FrameInfo
				:	int16_t			Frame
	Output		:	int16_t			TPage (-1 is non specified)
===================================================================*/
int16_t GetTPage( FRAME_INFO * FrameInfo, int16_t Frame )
{
	BIT_INFO	*	Bit_Ptr;
	BOX_INFO	*	Box_Ptr;
	OFF_INFO	*	Off_Ptr;

	if( FrameInfo )
	{
	   	Bit_Ptr = ( FrameInfo->Bit_Info + Frame );
	   	Off_Ptr = ( FrameInfo->Off_Info + Bit_Ptr->startbit );
	   	Box_Ptr = ( FrameInfo->Box_Info + ( Off_Ptr->box & 0x0fff ) );
		return ( Box_Ptr->tpage );
	}
	return( (u_int16_t) -1 );
}

/*===================================================================
	Procedure	:	Calc FrameInfo Ptr Index
	Input		:	FRM_INFO	**	FrameInfoPtr Address
	Output		:	int16_t			Index
===================================================================*/
int16_t Get_Frm_Info_Index( FRAME_INFO ** Frame_Info )
{
	OFF_FILES * FileInfo;
	int16_t		Count;

	FileInfo = &OffsetFiles[ 0 ];
	Count = 0;

	while( FileInfo->InfoPtrAddr != NULL )
	{
		if( FileInfo->InfoPtrAddr == Frame_Info )
		{
			return( Count );
		}

		Count++;
		FileInfo++;
	}

	return( -1 );
}

/*===================================================================
	Procedure	:	Calc FrameInfo Ptr Index
	Input		:	int16_t			Index
	Output		:	FRM_INFO	**	FraneInfoPtr Address
===================================================================*/
FRAME_INFO ** Get_Frm_Info_Address( int16_t Index )
{
	if( Index == -1 ) return( NULL );

	return( OffsetFiles[ Index ].InfoPtrAddr );
}

/*===================================================================
	Procedure	:	Enable all relevant off files for
				:	level / game type
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/

// in case they don't have the new font file we just default to use old one
void EnableRelevantFontOffFiles( void )
{
	int8_t path[256];
	sprintf((char*)path,"%s%s",OffsetPath,FontBigFile);
	if( Get_File_Size( (char*)path ) == 0 )
	{
		DisableOffFile( &OffsetFiles[ 0 ], FontBigFile );
		DisableOffFile( &Title_OffsetFiles[ 0 ], FontBigFile );
	}
	else
	{
		EnableOffFile(  &OffsetFiles[ 0 ], FontBigFile );
		EnableOffFile(  &Title_OffsetFiles[ 0 ], FontBigFile );
	}
}

void EnableRelevantOffFiles( OFF_FILES * FileInfo )
{
	EnableRelevantFontOffFiles();

	if( !FileInfo ) return;
	if( FileInfo != &OffsetFiles[ 0 ] ) return;

	if( ( ChangeLevel_MyGameStatus == STATUS_SinglePlayer ) ||
		( ChangeLevel_MyGameStatus == STATUS_PostStartingSinglePlayer ) ||
		( ChangeLevel_MyGameStatus == STATUS_TitleLoadGamePostStartingSinglePlayer) )
	{
		if( !DecreaseTemperature ) DisableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Thermo.off" );
		else EnableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Thermo.off" );

		DisableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Restart.off" );
		DisableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Flag.off" );
		DisableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Flags.off" );

	}
	else
	{
		if( !DecreaseTemperature ) DisableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Thermo.off" );
		else EnableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Thermo.off" );

		if( !ENABLE_RESTART ) DisableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Restart.off" );
		else EnableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Restart.off" );

		if( !CaptureTheFlag && !BountyHunt ) DisableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Flag.off" );
		else EnableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Flag.off" );

		if( !CTF ) DisableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Flags.off" );
		else EnableOffFile( &OffsetFiles[ 0 ], (int8_t*)"Flags.off" );
	}
}

/*===================================================================
	Procedure	:	Enable Specific off files for loading
	Input		:	OFF_FILES	*	Ptr to first Offset filename
				:	int8_t		*	File to enable
	Output		:	Nothing
===================================================================*/
void EnableOffFile( OFF_FILES * FileInfo, int8_t * Filename )
{
	while( FileInfo->InfoPtrAddr != NULL )
	{
		if( !strcasecmp( (char*)&FileInfo->Filename[ 0 ], (char*)Filename ) )
		{
			FileInfo->LoadEnable = true;
			return;
		}

		FileInfo++;
	}
}

/*===================================================================
	Procedure	:	Disable Specific off files for loading
	Input		:	OFF_FILES	*	Ptr to first Offset filename
				:	int8_t		*	File to disable
	Output		:	Nothing
===================================================================*/
void DisableOffFile( OFF_FILES * FileInfo, int8_t * Filename )
{
	while( FileInfo->InfoPtrAddr != NULL )
	{
		if( !strcasecmp( (char*)&FileInfo->Filename[ 0 ], (char*)Filename ) )
		{
			FileInfo->LoadEnable = false;
			return;
		}

		FileInfo++;
	}
}
