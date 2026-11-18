#pragma once

/* compobjects.h -- component objects: animated model hierarchies (.cob)
 *
 * A .cob file describes a tree of COMP_OBJ components. Each component may own
 * one entry in Models[] (ID != -1, model BaseModel + ID), a list of timed
 * transforms (translate, rotate, morph frame, property: state marks, sounds,
 * destroy, spot-FX off), collision zones (sphere, box or convex hull of
 * ZONESIDE planes) and its own children. Background objects, componented
 * ships, enemies and the title-screen FlyGirl are all built from these.
 *
 * Loading is two-step. PreLoadCompObj appends the file's model names to
 * ModNames (the level or title model table) starting at *BaseModel and
 * advances it, before models are loaded; LoadCompObj later builds the tree,
 * allocating Models[] entries, and returns OverallTime and MidTime (the
 * "open" mark, or half of OverallTime) in ticks of ANIM_SECOND per second.
 * The tree is malloc'd and lives as long as its owner; FreeCompObjChildren
 * releases it and its Models[] entries but not the top-level array.
 *
 * UpdateCompObjChildren evaluates the tree at a time: every transform that
 * has started is applied, scaled by how far through it is, and each
 * component's display matrix / position (and its model) is updated. It keeps
 * the previous two poses so UndoCompObjAnim can step back one frame after a
 * blocked move. A component with UserControl set is posed from
 * UserAngle / UserAxis around UserAxisPoint instead of its authored rotates
 * (its authored translates still apply); enemies use this for turrets and
 * aiming parts, found with GetCompObjAddress by authored ID.
 *
 * GetCompObjBoundingBox / GetCompObjColBoundingBox sample the animation 100
 * times and leave the components' display matrices posed as at time 0.
 * Collision zone and side damage is held per tick; the file stores it per
 * second.
 */

#include "game/main.h"
#include <stdio.h>

/*===================================================================
	Defines
===================================================================*/
constexpr int TRANS_TRANSLATE = 0;
constexpr int TRANS_ROTATE = 1;
constexpr int TRANS_MORPH = 2;
constexpr int TRANS_PROPERTY = 3;

constexpr int PROPTYPE_Unknown = -1; // Unknown
constexpr int PROPTYPE_State = 0; // Property Types
constexpr int PROPTYPE_SFX = 1;

constexpr int PROPSTATE_Opening = 0; // States
constexpr int PROPSTATE_Open = 1;
constexpr int PROPSTATE_Closing = 2;
constexpr int PROPSTATE_Closed = 3;
constexpr int PROPSTATE_UserControl = 4;
constexpr int PROPSTATE_Invulnerable = 5;
constexpr int PROPSTATE_Destroy = 6;
constexpr int PROPSTATE_DisableSpotFX = 7;

constexpr int COMPFLAG_Enable = 1;

constexpr float ANIM_SECOND = 60.0F;

constexpr int MAX_USER_CONT_COMPS = 10;
/*===================================================================
	Structures
===================================================================*/
typedef struct ANIM_SEQ {

	float	StartTime;			// Start of the sequence, in ticks ( ANIM_SECOND per second )
	float	EndTime;			// End of the sequence; equal to StartTime holds one pose

} ANIM_SEQ;

typedef struct COMP_OBJ {

	int16_t		Flags;
	int16_t		ID;
	VECTOR		DisplayPos;
	VECTOR		OffPos;
	VECTOR		Dir;
	VECTOR		Up;
	VECTOR		RotPoint;
	VECTOR		RotAxis;
	float		RotAngle;
	MATRIX		InitMatrix;
	MATRIX		LocalMatrix;
	MATRIX		DisplayMatrix;
	MATRIX		InvDisplayMatrix;

	int16_t		Frame;
	int16_t		InterpFrame;
	float		InterpTime;

	float		OldOldTime;
	VECTOR		OldOldDisplayPos;
	MATRIX		OldOldDisplayMatrix;
	MATRIX		OldOldInvDisplayMatrix;

	float		OldTime;
	VECTOR		OldDisplayPos;
	MATRIX		OldDisplayMatrix;
	MATRIX		OldInvDisplayMatrix;

	bool		UserControl;

	VECTOR		UserAxis;
	VECTOR		UserAxisPoint;
	float		UserAngle;

	int16_t		NumChildren;
	u_int16_t		ModelIndex;
	struct COMP_OBJ	 * Children;
	int16_t		NumTrans;
	struct ANI_TRANS * Trans;

	int16_t		NumColZones;
	struct ANI_ZONES * ColZones;

} COMP_OBJ;

/*===================================================================
	Animation Types
===================================================================*/
typedef struct ANI_TRANS {

	int16_t	Type;
	float	TimeStart;
	float	TimeDuration;
	void *	Data;

} ANI_TRANS;

typedef struct ANI_TRANSLATE {

	VECTOR	Trans;
	int16_t	Local;

} ANI_TRANSLATE;

typedef struct ANI_ROTATE {

	VECTOR	Axis;
	VECTOR	Origin;
	float	Angle;
	int16_t	Local;

} ANI_ROTATE;

typedef struct ANI_MORPH {

	int16_t	Frame;

} ANI_MORPH;

typedef struct ANI_PROPERTY {

	int16_t	Type;
	int16_t	Value;

} ANI_PROPERTY;

/*===================================================================
	Collision Zone Types
===================================================================*/
typedef struct ZONESIDE {

	VECTOR	Normal;
	float	PlaneOffset;
	int16_t	Sensitive;
	float	Damage;

} ZONESIDE;

typedef struct ANI_ZONES {

	int16_t	Type;
	int16_t	Sensitive;
	float	Damage;
	void *	Data;

} ANI_ZONES;

typedef struct ANI_ZONE_SPHERE {

	VECTOR	Center;
	VECTOR	HalfSize;

} ANI_ZONE_SPHERE;

typedef struct ANI_ZONE_BOX {

	VECTOR	Center;
	VECTOR	HalfSize;
	u_int16_t	NumSides;
	ZONESIDE * Sides;

} ANI_ZONE_BOX;

typedef struct ANI_ZONE_POLYGONAL {

	VECTOR	Center;
	VECTOR	HalfSize;
	u_int16_t	NumSides;
	ZONESIDE * Sides;

} ANI_ZONE_POLYGONAL;

/*===================================================================
	Prototypes
===================================================================*/
bool PreLoadCompObj( int8_t * Filename, u_int16_t * BaseModel, bool LevelSpecific );
COMP_OBJ * LoadCompObj( int8_t * Filename, VECTOR * Pos, VECTOR * Dir, u_int16_t Group,
						float * OverallTime, float * MidTime, u_int16_t * BaseModel,
						u_int16_t OwnerType, u_int16_t OwnerID );
FILE * LoadCompObjChildren( FILE * fp, COMP_OBJ * Comp, int16_t NumComp,
							VECTOR * Pos, VECTOR * Dir, u_int16_t Group,
							float * Time, float * MidTime, u_int16_t * BaseModel, u_int16_t OwnerType, u_int16_t OwnerID );
void FreeCompObjChildren( COMP_OBJ * Children, int16_t NumChildren );
void UpdateCompObjChildren( COMP_OBJ * Children, int16_t NumChildren, MATRIX * ParentMatrix, VECTOR * ParentPos, float Time, u_int16_t Group, VECTOR * Pos );
COMP_OBJ * GetCompObjAddress( int16_t ID, int16_t NumChildren, COMP_OBJ * Children );
void SetCompObjModelsState( COMP_OBJ * Children, int16_t NumChildren, bool Visible );
void UndoCompObjAnim( COMP_OBJ * Children, int16_t NumChildren );
void ShowCompObjColZones( COMP_OBJ * Children, int16_t NumChildren, u_int16_t Group );
bool RayToColZone( VECTOR * StartPos, VECTOR * EndPos, ZONESIDE * StartSide, int16_t StartNumSides, VECTOR * IntPoint, ZONESIDE ** IntSide, float Radius );
bool PointInside( VECTOR * Pos, ZONESIDE * Sides, int16_t NumSides, float Radius, int16_t Side );
bool GetCompObjAxis( COMP_OBJ * Comp );
void GetCompObjBoundingBox( MATRIX * ParentMatrix, VECTOR * ParentPos,
						   COMP_OBJ * Children, int16_t NumChildren, float OverallTime, VECTOR * TopLeft,
						   VECTOR * BottomRight );
void GetCompObjBoundingBoxChildren( COMP_OBJ * Children, int16_t NumChildren, MATRIX * ParentMatrix,
									VECTOR * ParentPos, float Time, VECTOR * TopLeft, VECTOR * BottomRight );
void UpdateCompObjColours( COMP_OBJ * Children, int16_t NumChildren, int Red, int Green, int Blue );
void UpdateCompObjClipGroup( COMP_OBJ * Children, int16_t NumChildren, u_int16_t ClipGroup );
void MaximizeBoundingBox( VECTOR * TopLeft, VECTOR * BottomRight );
void SetCompObjRealLighting( COMP_OBJ * Children, int16_t NumChildren, float Radius );
bool AmIInvulnerable( COMP_OBJ * Children, int16_t NumChildren, float Time );
void UpdateCompObjFlags( COMP_OBJ * Children, int16_t NumChildren, u_int16_t Flags );
FILE * SaveAllCompObj( FILE * fp, COMP_OBJ * Children, int16_t NumChildren );
FILE * LoadAllCompObj( FILE * fp, COMP_OBJ * Children, int16_t NumChildren );

void GetCompObjColBoundingBox( MATRIX * ParentMatrix, VECTOR * ParentPos,
						   COMP_OBJ * Children, int16_t NumChildren, float OverallTime, VECTOR * TopLeft,
						   VECTOR * BottomRight );
void GetCompObjColBoundingBoxChildren( COMP_OBJ * Children, int16_t NumChildren, MATRIX * ParentMatrix,
									VECTOR * ParentPos, float Time, VECTOR * TopLeft, VECTOR * BottomRight );
void GetCompObjColZoneBoundingBox( COMP_OBJ * Children, MATRIX * DisplayMatrix, VECTOR * DisplayPos,
								   VECTOR * TopLeft, VECTOR * BottomRight );
void GetZoneBoundingBox( VECTOR * Center, VECTOR * HalfSize, MATRIX * Matrix, VECTOR * Pos,
						 VECTOR * TopLeft, VECTOR * BottomRight );
void AddVertToBoundingBox( VECTOR * Vert, MATRIX * Matrix, VECTOR * Pos, VECTOR * TopLeft, VECTOR * BottomRight );
void SetStealthOffset( COMP_OBJ * Children, int16_t NumChildren, u_int16_t Offset );
void SetCompObjStealth( COMP_OBJ * Children, int16_t NumChildren );
void SetCompObjNonStealth( COMP_OBJ * Children, int16_t NumChildren );
