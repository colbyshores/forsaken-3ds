

/*===================================================================
		Include Files...
===================================================================*/
#include <stdio.h>
#include "game/debugview.h"

#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "game/main.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "data/file.h"
#include "game/triggers.h"
#include "game/pickups.h"
#include "game/enemies.h"
#include "data/mload.h"
#include "game/water.h"
#include "game/spotfx.h"
#include "game/trigarea.h"
#include "game/extforce.h"

#include "game/ships.h"
#include "game/skin.h"
#include "util/util.h"
#include "game/oct2.h"
#include "ui/title.h"
#include "util/file_reader.h"

/*===================================================================
		Defines
===================================================================*/
constexpr int EXTERNALFORCES_VERSION_NUMBER = 2;

/*===================================================================
		Globals ...
===================================================================*/
EXTERNALFORCE * ExternalForces = nullptr;

namespace {

/* The level's external-force bookkeeping (one instance, g_extforces): how
 * many entries ExternalForces[] holds and each group's chain through it.
 * Loaded with the level, walked when ships move and in the zone display,
 * and saved with the game. ExternalForces[] itself stays global (triggers
 * index it). */
class ExternalForceSet {
	public:
	bool  load( char * Filename );
	void  release();
	bool  area_check( VECTOR * OldPos, VECTOR * NewPos, u_int16_t Group, VECTOR * ExtForce, float * Shield );
	void  display_in_group( u_int16_t Group );
	FILE *save( FILE * fp );
	FILE *restore( FILE * fp );

	private:
	EXTERNALFORCE * ExternalForcesGroupLink[MAXGROUPS] = {};
	int32_t         NumOfExternalForces = 0;
};

} // namespace

/*===================================================================
	Procedure	:	External Forces load...
	Input		:	char * filename....
	Output		:	bool
===================================================================*/
bool ExternalForceSet::load( char * Filename )
{
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	EXTERNALFORCE * EFpnt;
	int i,j;
	TRIGGER_ZONE * ZonePnt;
	u_int32_t			MagicNumber;
	u_int32_t			VersionNumber;
	char		*	OrgBuffer;

	DebugPrintf("You might crash right here... Let methods know... That external forces is fucked...\n");

	NumOfExternalForces = 0;
	for( i = 0 ; i < MAXGROUPS ; i++ )
	{
		ExternalForcesGroupLink[i] = nullptr;
	}

	File_Size = Get_File_Size( Filename );
	if( !File_Size )
	{
		// dont need External Forces..
		return true;
	}
	Buffer = (char*)malloc( File_Size );
	OrgBuffer = Buffer;

	if( !Buffer )
	{
		Msg( "External Forces Load : Unable to allocate file buffer %s\n", Filename );
		return( false );
	}
	Read_Size = Read_File( Filename, Buffer, File_Size );
	if( Read_Size != File_Size )
	{
		Msg( "External Forces Load Error reading %s\n", Filename );
		return( false );
	}
	FileReader in( Buffer );
	MagicNumber = in.u32();
	VersionNumber = in.u32();

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != EXTERNALFORCES_VERSION_NUMBER  ) )
	{
		Msg( "ExternalForcesLoad() Incompatible ( .efc ) file %s", Filename );
		return( false );
	}

	NumOfExternalForces = in.i32();

	ExternalForces	= (EXTERNALFORCE *) malloc( NumOfExternalForces * sizeof(EXTERNALFORCE) );
	EFpnt = ExternalForces;
	if( !EFpnt )
	{
		Msg( "External Forces : cant allocate buffer %s\n", Filename );
		return( false );
	}

	for( i = 0 ; i < NumOfExternalForces ; i++ )
	{
		EFpnt->Status = in.u16();
		EFpnt->ForceType = in.u16();
		EFpnt->Group = in.u16();

		if( ExternalForcesGroupLink[EFpnt->Group] )
		{
			EFpnt->NextInGroup = ExternalForcesGroupLink[EFpnt->Group];
			ExternalForcesGroupLink[EFpnt->Group] = EFpnt;
		}else{
			ExternalForcesGroupLink[EFpnt->Group] = EFpnt;
			EFpnt->NextInGroup = nullptr;
		}

		EFpnt->Origin = in.vec();
		EFpnt->Dir = in.vec();
		EFpnt->Up = in.vec();
		EFpnt->MinForce = in.f32();
		EFpnt->MaxForce = in.f32();
		EFpnt->Width = in.f32();
		EFpnt->Height = in.f32();
		EFpnt->Range = in.f32();
		EFpnt->Range=1.0f/EFpnt->Range;

		EFpnt->Type = in.u16();

		EFpnt->Pos = in.vec();
		EFpnt->half_size = in.vec();

		if( EFpnt->Type != ZONE_Sphere )
		{
			// convex hull...
			EFpnt->num_sides = in.u16();

			EFpnt->Zone = (TRIGGER_ZONE*) malloc( EFpnt->num_sides * sizeof( TRIGGER_ZONE ) );
			ZonePnt = EFpnt->Zone;

			if( !ZonePnt )
			{
				Msg( "External Forces maloc Error with %s\n", Filename );
				return( false );
			}
			for( j = 0 ; j < EFpnt->num_sides ; j++ )
			{
				ZonePnt->normal = in.vec();
				ZonePnt->offset = in.f32();
				ZonePnt++;
			}
		}

		EFpnt++;
	}
	free( OrgBuffer );

	return true;
}
/*===================================================================
	Procedure	:	Release Forces load...
	Input		:	void
	Output		:	void
===================================================================*/
void ExternalForceSet::release()
{
	int i;
	EXTERNALFORCE * EFpnt;
	EFpnt = ExternalForces;

	if( EFpnt )
	{
		for( i = 0 ; i < NumOfExternalForces ; i++ )
		{
			if( EFpnt->Zone )
			{
				free( EFpnt->Zone );
			}
			EFpnt++;
		}
		free(ExternalForces);
		ExternalForces = nullptr;
	}
	NumOfExternalForces = 0;
}

/*===================================================================
	Procedure	:	Check if im in an Active External Force....
	Input		:	void
	Output		:	true/false
===================================================================*/
bool ExternalForceSet::area_check( VECTOR * OldPos , VECTOR * NewPos , u_int16_t Group , VECTOR * ExtForce , float * Shield )
{
	EXTERNALFORCE * EFpnt;
	float	Distance;
	float	Power;
	float	Force;
	bool	OnehasEffectedMe = false;

	EFpnt = ExternalForcesGroupLink[Group];

	while( EFpnt )
	{
		if( EFpnt->Status == EXTERNALFORCEACTIVE )
		{
			ExternalForcesZoneCheck( OldPos , NewPos , EFpnt );
			if( In | Entry | Exit )
			{
				// Were in the Zone....
				OnehasEffectedMe = true;

				Distance = DistanceVector2Vector( &EFpnt->Origin , NewPos );
				Power =  1.0F - (Distance * EFpnt->Range );		// how much of the force is needed...
				if ( Power < 0.0F )
					Power = 0.0F;
				Force = EFpnt->MaxForce - EFpnt->MinForce;	// how much of the range of Force is needed
				Force *= Power;
				Force += EFpnt->MinForce;					// the minimum needs to be added back in...

				switch( EFpnt->ForceType )
				{
				case EXTERNALFORCETYPE_Shield:
					if( !GodMode )
					{
						*Shield += (Force * framelag);
					}
					break;
				case EXTERNALFORCETYPE_Move:
					ExtForce->x += (EFpnt->Dir.x * Force) * framelag;
					ExtForce->y += (EFpnt->Dir.y * Force) * framelag;
					ExtForce->z += (EFpnt->Dir.z * Force) * framelag;
					break;
				case EXTERNALFORCETYPE_Shake:
					StartShipScreenShake( Force );
					break;
				}
			}
		}
		EFpnt = EFpnt->NextInGroup;
	}
	return OnehasEffectedMe;

}

/*===================================================================
	Procedure	:	Display External Forces
	Input		:	u_int16_t	Group
	Output		:	Nothing
===================================================================*/
void ExternalForceSet::display_in_group( u_int16_t Group )
{
	EXTERNALFORCE * EFpnt;

	if( !DebugViews.ShowEFZones ) return;

	EFpnt = ExternalForcesGroupLink[Group];

	while( EFpnt )
	{
		if( EFpnt->Status == EXTERNALFORCEACTIVE )
		{
			switch( EFpnt->ForceType )
			{
			case EXTERNALFORCETYPE_Shield:
			case EXTERNALFORCETYPE_Shake:
				break;
			case EXTERNALFORCETYPE_Move:
				DisplayZoneDirection( &EFpnt->Origin, &EFpnt->Dir, Group );
				break;
			}

			if( EFpnt->Type == ZONE_Sphere )
			{
				DisplaySphereZone( &EFpnt->Pos, EFpnt->half_size.x, Group, 0, 64, 0, 128 );
			}
			else
			{
				DisplayTriggerZone( EFpnt->Zone, EFpnt->num_sides, Group, 0, 64, 0, 128 );
			}
		}
		else
		{
			if( EFpnt->Type == ZONE_Sphere )
			{
				DisplaySphereZone( &EFpnt->Pos, EFpnt->half_size.x, Group, 0, 64, 0, 128 );
			}
			else
			{
				DisplayTriggerZone( EFpnt->Zone, EFpnt->num_sides, Group, 0, 64, 0, 128 );
			}
		}
		EFpnt = EFpnt->NextInGroup;
	}
}


/*===================================================================
	Procedure	:	Check if im in a External Force Zone....
	Input		:	void
	Output		:	void
===================================================================*/
void ExternalForcesZoneCheck( VECTOR * OldPos , VECTOR * NewPos , EXTERNALFORCE * EFpnt )
{

	bool	OldIn;
	bool	NewIn;
	Entry = false;
	Exit = false;
	In = false;
	if( EFpnt->Type == ZONE_Sphere )
	{
		OldIn = DistanceVector2Vector( &EFpnt->Pos , OldPos ) < EFpnt->half_size.x;
		NewIn = DistanceVector2Vector( &EFpnt->Pos , NewPos ) < EFpnt->half_size.x;
		if( !OldIn && NewIn )
			Entry = true;
		if( OldIn && !NewIn )
			Exit = true;
		if( OldIn && NewIn )
		{
			In = true;
		}
	}else{
		RayToHull( EFpnt->Zone , OldPos , NewPos , EFpnt->num_sides );
	}
}


/*===================================================================
	Procedure	:	Start an external force..
	Input		:	void
	Output		:	void
===================================================================*/
void StartExternalForce( u_int16_t * Data )
{
	EXTERNALFORCE * EFpnt;
	EFpnt = ExternalForces;
	if( !EFpnt )
		return;
	EFpnt += *Data;
	EFpnt->Status = EXTERNALFORCEACTIVE;
}
/*===================================================================
	Procedure	:	Stop an external force..
	Input		:	void
	Output		:	void
===================================================================*/
void StopExternalForce( u_int16_t * Data )
{
	EXTERNALFORCE * EFpnt;
	EFpnt = ExternalForces;
	if( !EFpnt )
		return;
	EFpnt += *Data;
	EFpnt->Status = EXTERNALFORCEINACTIVE;
}

/*===================================================================
	Procedure	:	Save ExtForses arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * ExternalForceSet::save( FILE * fp )
{
	int				i;
	EXTERNALFORCE *	ExtForcesPtr;

	if( fp )
	{
		fwrite( &NumOfExternalForces, sizeof( int16_t ), 1, fp );

		ExtForcesPtr = ExternalForces;

		for( i = 0; i < NumOfExternalForces; i++ )
		{
			fwrite( &ExtForcesPtr->Status, sizeof( u_int16_t ), 1, fp );
			ExtForcesPtr++;
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load ExtForces Arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * ExternalForceSet::restore( FILE * fp )
{
	int				i;
	EXTERNALFORCE *	ExtForcesPtr;
	int16_t			NumExtForces;

	if( fp )
	{
		file_read( &NumExtForces, sizeof( int16_t ), 1, fp );

		if( NumExtForces != NumOfExternalForces )
		{
			fclose( fp );
			return( nullptr );
		}

		ExtForcesPtr = ExternalForces;

		for( i = 0; i < NumOfExternalForces; i++ )
		{
			file_read( &ExtForcesPtr->Status, sizeof( u_int16_t ), 1, fp );
			ExtForcesPtr++;
		}
	}

	return( fp );
}

namespace { ExternalForceSet g_extforces; }

bool ExternalForcesLoad( char * Filename ) { return g_extforces.load( Filename ); }
void ReleaseExternalForces( void ) { g_extforces.release(); }
bool ExternalForcesAreaCheck( VECTOR * OldPos , VECTOR * NewPos , u_int16_t Group , VECTOR * ExtForce , float * Shield ) { return g_extforces.area_check( OldPos, NewPos, Group, ExtForce, Shield ); }
void DisplayExternalForcesInGroup( u_int16_t Group ) { g_extforces.display_in_group( Group ); }
FILE * SaveExternalForces( FILE * fp ) { return g_extforces.save( fp ); }
FILE * LoadExternalForces( FILE * fp ) { return g_extforces.restore( fp ); }
