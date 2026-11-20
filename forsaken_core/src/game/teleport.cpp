

/*===================================================================
		Include Files...
===================================================================*/
#include <stdio.h>

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
#include "game/teleport.h"

#include "audio/sfx.h"
#include "game/ships.h"
#include "util/util.h"
#include "game/oct2.h"
#include "game/skin.h"
#include "util/file_reader.h"

/*===================================================================
		Defines
===================================================================*/
constexpr int TELEPORTS_VERSION_NUMBER = 2;

/*===================================================================
		Globals ...
===================================================================*/
TELEPORT * Teleports = nullptr;

namespace {

/* The level's teleport bookkeeping (one instance, g_teleporters): how many
 * entries Teleports[] holds and each group's chain through it. Loaded with
 * the level, walked when objects move and in the zone display, and saved
 * with the game. Teleports[] itself stays global (triggers index it). */
class Teleporters {
	public:
	bool  load( char * Filename );
	void  release();
	bool  area_check( VECTOR * NewPos, VECTOR * OldPos, u_int16_t Group, OBJECT *obj );
	void  display_in_group( u_int16_t Group );
	FILE *save( FILE * fp );
	FILE *restore( FILE * fp );

	private:
	TELEPORT * TeleportsGroupLink[MAXGROUPS] = {};
	int16_t    NumOfTeleports = 0;
};

} // namespace

/*===================================================================
	Procedure	:	Teleports load...
	Input		:	char * filename....
	Output		:	bool
===================================================================*/
bool Teleporters::load( char * Filename )
{
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	char		*	OrgBuffer;
	TELEPORT * TPpnt;
	int i,j,e;
	TRIGGER_ZONE * ZonePnt;
	u_int32_t			MagicNumber;
	u_int32_t			VersionNumber;


	NumOfTeleports = 0;
	for( i = 0 ; i < MAXGROUPS ; i++ )
	{
		TeleportsGroupLink[i] = nullptr;
	}


	File_Size = Get_File_Size( Filename );
	if( !File_Size )
	{
		// dont need Teleports..
		return true;
	}
	Buffer = (char*)malloc( File_Size );
	OrgBuffer = Buffer;

	if( !Buffer )
	{
		Msg( "Teleports Load : Unable to allocate file buffer %s\n", Filename );
		return( false );
	}
	Read_Size = Read_File( Filename, Buffer, File_Size );
	if( Read_Size != File_Size )
	{
		Msg( "Teleports Load Error reading %s\n", Filename );
		return( false );
	}


	FileReader in( Buffer );
	MagicNumber = in.u32();
	VersionNumber = in.u32();

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != TELEPORTS_VERSION_NUMBER  ) )
	{
		Msg( "Teleportload() Incompatible Teleports( .tel ) file %s", Filename );
		return( false );
	}

	NumOfTeleports = in.u16();

	Teleports	= (TELEPORT *) malloc( NumOfTeleports * sizeof(TELEPORT) );
	TPpnt = Teleports;
	if( !TPpnt )
	{
		Msg( "Teleports : cant allocate buffer %s\n", Filename );
		return( false );
	}

	for( i = 0 ; i < NumOfTeleports ; i++ )
	{
		TPpnt->Group = in.u16();
		TPpnt->Status = in.u16();
		if( TeleportsGroupLink[TPpnt->Group] )
		{
			TPpnt->NextInGroup = TeleportsGroupLink[TPpnt->Group];
			TeleportsGroupLink[TPpnt->Group] = TPpnt;
		}else{
			TeleportsGroupLink[TPpnt->Group] = TPpnt;
			TPpnt->NextInGroup = nullptr;
		}

		TPpnt->num_links = in.u16();
		if( TPpnt->num_links > MAXTELEPORTLINKS )
		{
			Msg( "Teleports : To many Links in %s\n", Filename );
			return( false );
		}

		for( e = 0 ; e < TPpnt->num_links ; e++ )
		{
			TPpnt->Links[e] = in.u16();
		}

		TPpnt->Pos = in.vec();
		TPpnt->Dir = in.vec();
		TPpnt->Up = in.vec();
		TPpnt->half_size = in.vec();

		TPpnt->zone_type = in.u16();

		if( TPpnt->zone_type != ZONE_Sphere )
		{
			// convex hull...
			TPpnt->num_sides = in.u16();

			TPpnt->Zone = (TRIGGER_ZONE*) malloc( TPpnt->num_sides * sizeof( TRIGGER_ZONE ) );
			ZonePnt = TPpnt->Zone;

			if( !ZonePnt )
			{
				Msg( "Teleport maloc Error with %s\n", Filename );
				return( false );
			}
			for( j = 0 ; j < TPpnt->num_sides ; j++ )
			{
				ZonePnt->normal = in.vec();
				ZonePnt->offset = in.f32();
				ZonePnt++;
			}
		}
		else
		{
			TPpnt->Zone = nullptr;
		}

		TPpnt++;
	}
	free(OrgBuffer);

	return true;
}
/*===================================================================
	Procedure	:	Release Forces load...
	Input		:	void
	Output		:	void
===================================================================*/
void Teleporters::release()
{
	int i;
	TELEPORT * TPpnt;
	TPpnt = Teleports;

	if( TPpnt )
	{
		for( i = 0 ; i < NumOfTeleports ; i++ )
		{
			if( TPpnt->Zone )
			{
				free( TPpnt->Zone );
			}
			TPpnt++;
		}
		free(Teleports);
		Teleports = nullptr;
	}
	NumOfTeleports = 0;
}

/*===================================================================
	Procedure	:	Check if im in an Active Teleport....
	Input		:	void
	Output		:	true/false
===================================================================*/
bool Teleporters::area_check( VECTOR * NewPos , VECTOR * OldPos ,u_int16_t Group, OBJECT *obj )
{
	TELEPORT * TPpnt;
	TELEPORT * newTPpnt;

	TPpnt = TeleportsGroupLink[Group];

	while( TPpnt )
	{
		if( TPpnt->Status == TELEPORTACTIVE )
		{
			TeleportsZoneCheck( OldPos , NewPos , TPpnt );
			if( Entry )
			{

				PlaySfx( SFX_Teleport, 1.0F );

				// Were in the Zone....
				newTPpnt = Teleports;
				newTPpnt += TPpnt->Links[Random_Range(TPpnt->num_links)];
				obj->Group = newTPpnt->Group;
				obj->Pos = newTPpnt->Pos;
				obj->Quat.FromDirAndUp(&newTPpnt->Dir, &newTPpnt->Up);
				obj->Quat.ToMatrix(&obj->Mat);
				return true;
			}
		}
		TPpnt = TPpnt->NextInGroup;
	}
	return false;
}

/*===================================================================
	Procedure	:	Display Teleports in group
	Input		:	u_int16_t	Group
	Output		:	Nothing
===================================================================*/
void Teleporters::display_in_group( u_int16_t Group )
{
	TELEPORT * TPpnt;

	if( !ShowTeleports ) return;

	TPpnt = TeleportsGroupLink[Group];

	while( TPpnt )
	{
		if( TPpnt->Status == TELEPORTACTIVE )
		{
			if( TPpnt->zone_type == ZONE_Sphere )
			{
				DisplaySphereZone( &TPpnt->Pos, TPpnt->half_size.x, Group, 0, 64, 0, 128 );
			}
			else
			{
				DisplayTriggerZone( TPpnt->Zone, TPpnt->num_sides, Group, 0, 64, 0, 128 );
			}
		}
		else
		{
			if( TPpnt->zone_type == ZONE_Sphere )
			{
				DisplaySphereZone( &TPpnt->Pos, TPpnt->half_size.x, Group, 16, 16, 16, 128 );
			}
			else
			{
				DisplayTriggerZone( TPpnt->Zone, TPpnt->num_sides, Group, 16, 16, 16, 128 );
			}
		}
		DisplayDirAndUp( &TPpnt->Pos, &TPpnt->Dir, &TPpnt->Up, TPpnt->Group );

		TPpnt = TPpnt->NextInGroup;
	}
}


/*===================================================================
	Procedure	:	Check if im in a Teleport Zone....
	Input		:	void
	Output		:	void
===================================================================*/
void TeleportsZoneCheck( VECTOR * OldPos , VECTOR * NewPos , TELEPORT * TPpnt )
{

	bool	OldIn;
	bool	NewIn;
	Entry = false;
	Exit = false;
	In = false;
	if( TPpnt->zone_type == ZONE_Sphere )
	{
		OldIn = DistanceVector2Vector( &TPpnt->Pos , OldPos ) <= TPpnt->half_size.x;
		NewIn = DistanceVector2Vector( &TPpnt->Pos , NewPos ) <= TPpnt->half_size.x;
		if( !OldIn && NewIn )
			Entry = true;
		if( OldIn && !NewIn )
			Exit = true;
		if( OldIn && NewIn )
			In = true;
	}else{
		RayToHull( TPpnt->Zone , OldPos , NewPos , TPpnt->num_sides );
	}
}


/*===================================================================
	Procedure	:	Start a Teleport..
	Input		:	void
	Output		:	void
===================================================================*/
void StartTeleport( u_int16_t * Data )
{
	TELEPORT * TPpnt;
	TPpnt = Teleports;
	if( !TPpnt )
		return;
	TPpnt += *Data;
	TPpnt->Status = TELEPORTACTIVE;
}
/*===================================================================
	Procedure	:	Stop a Teleport..
	Input		:	void
	Output		:	void
===================================================================*/
void StopTeleport( u_int16_t * Data )
{
	TELEPORT * TPpnt;
	TPpnt = Teleports;
	if( !TPpnt )
		return;
	TPpnt += *Data;
	TPpnt->Status = TELEPORTINACTIVE;
}


/*===================================================================
	Procedure	:	Save Teleports arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * Teleporters::save( FILE * fp )
{
	int				i;
	TELEPORT	*	TeleportPtr;

	if( fp )
	{
		fwrite( &NumOfTeleports, sizeof( int16_t ), 1, fp );

		TeleportPtr = Teleports;

		for( i = 0; i < NumOfTeleports; i++ )
		{
			fwrite( &TeleportPtr->Status, sizeof( u_int16_t ), 1, fp );
			TeleportPtr++;
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load Teleports Arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * Teleporters::restore( FILE * fp )
{
	int				i;
	TELEPORT	*	TeleportPtr;
	int16_t			NumTeleports;

	if( fp )
	{
		file_read( &NumTeleports, sizeof( int16_t ), 1, fp );

		if( NumTeleports != NumOfTeleports )
		{
			fclose( fp );
			return( nullptr );
		}

		TeleportPtr = Teleports;

		for( i = 0; i < NumOfTeleports; i++ )
		{
			file_read( &TeleportPtr->Status, sizeof( u_int16_t ), 1, fp );
			TeleportPtr++;
		}
	}

	return( fp );
}

namespace { Teleporters g_teleporters; }

bool TeleportsLoad( char * Filename ) { return g_teleporters.load( Filename ); }
void ReleaseTeleports( void ) { g_teleporters.release(); }
bool TeleportsAreaCheck( VECTOR * NewPos , VECTOR * OldPos ,u_int16_t Group, OBJECT *obj ) { return g_teleporters.area_check( NewPos, OldPos, Group, obj ); }
void DisplayTeleportsInGroup( u_int16_t Group ) { g_teleporters.display_in_group( Group ); }
FILE * SaveTeleports( FILE * fp ) { return g_teleporters.save( fp ); }
FILE * LoadTeleports( FILE * fp ) { return g_teleporters.restore( fp ); }
