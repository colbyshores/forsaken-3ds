#ifndef TEXT2D_INCLUDED
#define TEXT2D_INCLUDED
#include "game/main.h"

/*===================================================================
	Defines
===================================================================*/
#define	MAX_NAMES		8

#define	DO_LOAD			true
#define	DONT_LOAD		false

#define	BOX_NOFLIP		0x0000
#define	BOX_XFLIP		0x2000
#define	BOX_YFLIP		0x8000
#define	BOX_BOTHFLIP	0xA000

/*===================================================================
	Structures for 2D Texture info
===================================================================*/
typedef struct BOX_INFO {

	int16_t	tpage;
	float	u1;
	float	v1;
	float	u2;
	float	v2;
	int16_t	xsize;
	int16_t	ysize;

} BOX_INFO;

typedef struct OFF_INFO {

	int16_t	xoff;
	int16_t	yoff;
	u_int16_t	box;

} OFF_INFO;

typedef struct BIT_INFO {

	int16_t	startbit;
	int16_t	numbits;

} BIT_INFO;

typedef struct FRAME_INFO {

	int16_t			Num_Frames;
	int8_t		*	File_Addr;
	BOX_INFO	*	Box_Info;
	OFF_INFO	*	Off_Info;
	BIT_INFO	*	Bit_Info;
	char			tpage_name[32];		// only valid if only one t-page is associated with box file
	int16_t			vid_tpage_index;		//					---"---
	int16_t			sys_tpage_index;		//					---"---
	int16_t			NumBoxes;

} FRAME_INFO;

typedef struct OFF_FILES {

	int8_t		*	Filename;
	FRAME_INFO	**	InfoPtrAddr;
	bool			LoadEnable;
	bool			Scale;
	int				LoadTPages;
	int16_t			xsize;
	int16_t			ysize;
} OFF_FILES;

/*===================================================================
	Prototypes
===================================================================*/
FRAME_INFO * Load_Off_File( int8_t * Filename, bool Scale, int LoadTPages, int16_t *last_tpage, int16_t xsize, int16_t ysize, bool placeholder );
bool Load_All_Off_Files( OFF_FILES * FileInfo );
void Free_All_Off_Files( OFF_FILES * FileInfo );
int16_t GetTPage( FRAME_INFO * FrameInfo, int16_t Frame );
int16_t Get_Frm_Info_Index( FRAME_INFO ** Frame_Info );
FRAME_INFO ** Get_Frm_Info_Address( int16_t Index );
void EnableOffFile( OFF_FILES * FileInfo, int8_t * Filename );
void DisableOffFile( OFF_FILES * FileInfo, int8_t * Filename );
void EnableRelevantOffFiles( OFF_FILES * FileInfo );

/* Defined in 2dtextures.cpp; shared with the modules that use it. */
extern FRAME_INFO * Exp_Header;
extern FRAME_INFO * GreyFlare_Header;
extern FRAME_INFO * Flare_Header;
extern OFF_FILES OffsetFiles[];
extern OFF_FILES Title_OffsetFiles[];
extern OFF_FILES Splash_OffsetFiles[];
extern FRAME_INFO * Circle_Header;
extern FRAME_INFO * PickupRegen_Header;
extern FRAME_INFO * Pulsar_Header;
extern FRAME_INFO * Pulsar_Trail_Header;
extern FRAME_INFO * Pulsar_Exp_Header;
extern FRAME_INFO * Trojax1_Header;
extern FRAME_INFO * Trojax2_Header;
extern FRAME_INFO * Trojax_Exp_Header;
extern FRAME_INFO * Exp2_Header;
extern FRAME_INFO * Shrapnel_Header;
extern FRAME_INFO * Arc_Header;
extern FRAME_INFO * Laser_Header;
extern FRAME_INFO * Bits_Header;
extern FRAME_INFO * Multiple_Header;
extern FRAME_INFO * NMEBullet_Header;
extern FRAME_INFO * Mushroom_Header;
extern FRAME_INFO * BigExp1_Header;
extern FRAME_INFO * Restart_Header;
extern FRAME_INFO * GravgonTrail_Header;
extern FRAME_INFO * SolarisTrail_Header;
extern FRAME_INFO * NewTrail_Header;
extern FRAME_INFO * Blood_Header;
extern FRAME_INFO * BloodSplat_Header;
extern FRAME_INFO * BloodDribble_Header;
extern FRAME_INFO * ScatterTrail_Header;
extern FRAME_INFO * BigFlash_Header;
extern FRAME_INFO * Target_Header;
extern FRAME_INFO * LensFlare_Header;
extern FRAME_INFO * BlueSpark_Header;
extern FRAME_INFO * Tentacle_Header;
extern FRAME_INFO * Bubble_Header;
extern FRAME_INFO * EnemyGen_Header;
extern FRAME_INFO * Drip_Header;
extern FRAME_INFO * AfterBurner_Header;
extern FRAME_INFO * Numbers_Header;
extern FRAME_INFO * Numbers2_Header;
extern FRAME_INFO * Text512b_Header;
extern FRAME_INFO * Text512_Header;
extern FRAME_INFO * Orb_Header;
extern FRAME_INFO * Solid_Header;
extern FRAME_INFO * Thermo_Header;
extern FRAME_INFO * Logos_Header;
extern FRAME_INFO * Flag_Header;
extern FRAME_INFO * Flags_Header;
extern FRAME_INFO * Title_LevelPics_Header;
extern FRAME_INFO * Title_TVFrame_Header;
extern float VduScaleX;
extern float VduScaleY;
extern FRAME_INFO * Title_Fonts_Header;
extern FRAME_INFO * Title_Chars1_Header;
extern FRAME_INFO * Title_Chars2_Header;

#endif
