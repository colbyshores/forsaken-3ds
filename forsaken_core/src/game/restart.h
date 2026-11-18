/*===================================================================
	File	:	Restart.h
	Content	:	Restart.c include file
===================================================================*/
#pragma once

/*===================================================================
	Defines
===================================================================*/
#define	ENABLE_RESTART		0

constexpr int RESTARTTYPE_MODEL = 0;
constexpr int RESTARTTYPE_FACEME = 1;

constexpr int MAXRESTARTPOINTS = 128;

constexpr int RESTARTSEQ_Inactive = 0;
constexpr int RESTARTSEQ_Activating = 1;
constexpr int RESTARTSEQ_Active = 2;
constexpr int RESTARTSEQ_Deactivating = 3;

/*===================================================================
	Structures
===================================================================*/
typedef struct RESTART {
	
	u_int16_t		Sequence;
	u_int16_t		Index;
	u_int16_t		Group;
	VECTOR		Pos;
	VECTOR		DirVector;
	VECTOR		UpVector;
	u_int16_t		FmPoly;
	MATRIX		Matrix;
	float		Time;
	float		OverallTime;
	COMP_OBJ		*	Components;
	struct	RESTART	*	PrevUsed;
	struct	RESTART	*	NextUsed;
	struct	RESTART	*	PrevFree;
	struct	RESTART	*	NextFree;
	struct	RESTART	*	NextInGroup;
	struct	RESTART	*	PrevInGroup;

} RESTART;

/*===================================================================
	Prototypes
===================================================================*/
void InitRestartPoints( void );
RESTART * FindFreeRestartPoint( void );
void KillUsedRestartPoint( RESTART * Object );
void ReleaseAllRestartPoints( void );
bool LoadStartPoints( void );
bool PreLoadRestartPoints( void );
bool LoadRestartPoints( void );
RESTART * InitOneFaceMeRestartPoint( u_int16_t Group, VECTOR * Pos, VECTOR * DirVector, VECTOR * UpVector );
RESTART * InitOneRestartPoint( u_int16_t Group, VECTOR * Pos, VECTOR * DirVector, VECTOR * UpVector,
							  COMP_OBJ * CompObj, float OverallTime );
void ProcessRestartPoints( void );
void SetupRestartPointGroups( void );
void RemoveRestartPointFromGroup( RESTART * Object, u_int16_t Group );
void ActivateRestartPoint( u_int16_t NewStartPos, u_int16_t OldStartPos );
FILE * SaveStartRestartPoints( FILE * fp );
FILE * LoadStartRestartPoints( FILE * fp );
void DisplayStartPoints( void );

/* Defined in restart.cpp; shared with the modules that use it. */
extern bool ShowStartPoints;
extern RESTART * FirstRestartUsed;
