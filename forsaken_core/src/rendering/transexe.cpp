
/*===================================================================
		Include File...	
===================================================================*/
#include "game/main.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
/* OWNER_BGOBJECT (= 3) — defined in primary.h, but that header pulls
 * in FRAME_INFO etc.; inlined to avoid the include cascade. */
#ifndef OWNER_BGOBJECT
#define OWNER_BGOBJECT 3
#endif

#include "rendering/transexe.h"
#include "game/models.h"
#include "game/oct2.h"

/*===================================================================
		Globals...
===================================================================*/
int16_t	NumOfTransExe = 0;
TRANSEXE TransExe[MAXTRANSEXE];


/*===================================================================
	Add Translucent Execute Buffer...
	Input	: RENDERMATRIX *Matrix...Only needed if ya need to do a compunded view Matrix
			  renderObject... the render object to draw later (copied)
			  int UseIdentity....set to 0 for normal background...1 for Bikes etc..
===================================================================*/
void AddTransExe( RENDERMATRIX *Matrix , RENDEROBJECT *renderObject , int UseIdentity, u_int16_t Model, u_int16_t group, int16_t NumVerts )
{
	if( NumOfTransExe < MAXTRANSEXE	)
	{
		TransExe[NumOfTransExe].UseIdentity = UseIdentity;
		
		if( UseIdentity == 0 )
		{
			TransExe[NumOfTransExe].Matrix = *Matrix;
		}
		/* Plain shallow struct copy. The textureGroups field is now a
		 * pointer to a TAG_LEVEL hunk allocation rather than an inline
		 * MAX_TEXTURE_GROUPS array, so copying the struct just copies
		 * the pointer — both src and dst reference the same array,
		 * which is fine because TransExe entries are short-lived
		 * (per-frame) and the hunk allocation lives until level
		 * transition. RENDEROBJECT and LEVELRENDEROBJECT are now
		 * structurally identical, so the LEVELRENDEROBJECT* cast to
		 * RENDEROBJECT* that used to overread the source is no
		 * longer a concern (same size, same layout). */
		TransExe[NumOfTransExe].renderObject = (*renderObject);
		/* The copy is drawn from its slot, and slots are refilled every
		 * camera pass in visibility order, so the slot address does not
		 * identify the content: keep it out of the stereo memo and the
		 * persistent level cache (both key on the renderObject). */
		TransExe[NumOfTransExe].renderObject.memoEpochPass = SMEMO_PASS_NO_REUSE;
		TransExe[NumOfTransExe].renderObject.vertOwner = VERT_OWNER_OTHER;
		TransExe[NumOfTransExe].Model = Model;
		TransExe[NumOfTransExe].NumVerts = NumVerts;
		TransExe[NumOfTransExe].group = group;


		NumOfTransExe++;
	}
}


/*===================================================================
		Execute Translucent Execute Buffers for specific group...
===================================================================*/
void ExecuteTransExe( u_int16_t group )
{
	int16_t	i;
	u_int16_t	Model;
	bool	Display;

	for( i = 0 ; i < NumOfTransExe ; i++ )
	{
		if ( TransExe[i].group == group )
		{
			Display = true;


			if( TransExe[i].Model != (u_int16_t) -1 )
			{
				Model = TransExe[i].Model;
				if( !( Models[ Model ].Flags & MODFLAG_Clip ) )
					continue; // do not display fields clipped

				if( ( Models[ Model ].Flags & MODFLAG_Light ) )
				{
					if( !LightModel2( Models[ Model ].ModelNum, &Models[ Model ].Pos ) ) Display = false;
				}

				switch( Models[ Model ].Func )
				{
					case MODFUNC_Explode:
						ProcessModelExec( /*TransExe[i].lpExBuf*/&TransExe[i].renderObject, TransExe[i].NumVerts, Models[ Model ].Scale, Models[ Model ].MaxScale, 1, 0, 0 );
						break;
		
					case MODFUNC_Regen:
						ProcessModelExec( /*TransExe[i].lpExBuf*/&TransExe[i].renderObject, TransExe[i].NumVerts, Models[ Model ].Scale, Models[ Model ].MaxScale, 0, 1, 0 );
						break;
					case MODFUNC_Scale:
					case MODFUNC_Scale2:
						ProcessModelExec( /*TransExe[i].lpExBuf*/&TransExe[i].renderObject, TransExe[i].NumVerts, Models[ Model ].Scale, Models[ Model ].MaxScale, 0, 0, 1 );
						break;

					case MODFUNC_SphereZone:
						ProcessSphereZoneModelExec( /*TransExe[i].lpExBuf*/&TransExe[i].renderObject, TransExe[i].NumVerts, (u_int8_t) Models[ Model ].Red, (u_int8_t) Models[ Model ].Green, (u_int8_t) Models[ Model ].Blue );
						break;

					case MODFUNC_OrbitPulsar:
						if( ( Ships[ Models[ Model ].Ship ].Object.Flags & SHIP_Stealth ) )
						{
							if( !LightModel( Model, &Models[ Model ].Pos ) ) Display = false;
						}
						break;

					case MODFUNC_ScaleDonut:
						break;
					
					case MODFUNC_Nothing:
					default:
						break;
				}
			}

			if( TransExe[i].UseIdentity	== 1 )
			{
				FSSetWorld(&identity);
			}
			else
			{
				FSSetWorld(&TransExe[i].Matrix);
			}

			if( Display )
			{
#ifdef RENDERER_C3D
				/* Same eligibility check as models.c: BGOBJECTs
				 * (doors, vents, animated decorations) shouldn't
				 * get Phong shine. The TransExe path renders
				 * animating/transparent geometry — moving doors go
				 * here, so without this gate they'd flicker shine
				 * on while opening/closing. TransExe[i].Model is
				 * the Models[] index; check OwnerType. */
				{
					/* Same eligibility check as models.c: BGOBJECT
					 * (doors, vents, animated decorations) gets no
					 * Phong shine. TransExe[i].Model is the Models[]
					 * index; can be -1 (= 0xFFFF) for animated mxa
					 * draws with no associated Models entry, in
					 * which case default to eligible. */
					u_int16_t mi = TransExe[i].Model;
					bool eligible = !(mi < MAXNUMOFMODELS &&
					                  Models[mi].OwnerType == OWNER_BGOBJECT);
					c3d_set_object_shine_eligible(eligible);
				}
#endif
				draw_object(&TransExe[i].renderObject);
#ifdef RENDERER_C3D
				{ c3d_set_object_shine_eligible(true); }
#endif
			}

		}
	}
	FSSetWorld(&identity);
}


void ExecuteTransExeUnclipped( u_int16_t group )
{
	int16_t	i;
	u_int16_t	Model;
	bool	Display;

	for( i = 0 ; i < NumOfTransExe ; i++ )
	{
		if ( TransExe[i].group == group )
		{
			Display = true;


			if( TransExe[i].Model != (u_int16_t) -1 )
			{
				Model = TransExe[i].Model;
				if( ( Models[ Model ].Flags & MODFLAG_Clip ) )
					continue; // do not display clipped models here

				if( ( Models[ Model ].Flags & MODFLAG_Light ) )
				{
					if( !LightModel2( Models[ Model ].ModelNum, &Models[ Model ].Pos ) ) Display = false;
				}

				switch( Models[ Model ].Func )
				{
					case MODFUNC_Explode:
						ProcessModelExec( /*TransExe[i].lpExBuf*/&TransExe[i].renderObject, TransExe[i].NumVerts, Models[ Model ].Scale, Models[ Model ].MaxScale, 1, 0, 0 );
						break;
					case MODFUNC_Regen:
						ProcessModelExec( /*TransExe[i].lpExBuf*/&TransExe[i].renderObject, TransExe[i].NumVerts, Models[ Model ].Scale, Models[ Model ].MaxScale, 0, 1, 0 );
						break;
		
					case MODFUNC_Scale:
					case MODFUNC_Scale2:
						ProcessModelExec( /*TransExe[i].lpExBuf*/&TransExe[i].renderObject, TransExe[i].NumVerts, Models[ Model ].Scale, Models[ Model ].MaxScale, 0, 0, 1 );
						break;

					case MODFUNC_SphereZone:
						ProcessSphereZoneModelExec( /*TransExe[i].lpExBuf*/&TransExe[i].renderObject, TransExe[i].NumVerts, (u_int8_t) Models[ Model ].Red, (u_int8_t) Models[ Model ].Green, (u_int8_t) Models[ Model ].Blue );
						break;

					case MODFUNC_OrbitPulsar:
						if( ( Ships[ Models[ Model ].Ship ].Object.Flags & SHIP_Stealth ) )
						{
							if( !LightModel( Model, &Models[ Model ].Pos ) ) Display = false;
						}
						break;

					case MODFUNC_ScaleDonut:
						break;
					
					case MODFUNC_Nothing:
					default:
						break;
				}
			}
			else
				continue; // do not display non-model translucencies

			if( TransExe[i].UseIdentity	== 1 )
			{
				FSSetWorld(&identity);
			}
			else
			{
				FSSetWorld(&TransExe[i].Matrix);
			}

			if( Display )
			{
#ifdef RENDERER_C3D
				{
					/* Same eligibility check as models.c: BGOBJECT
					 * (doors, vents, animated decorations) gets no
					 * Phong shine. TransExe[i].Model is the Models[]
					 * index; can be -1 (= 0xFFFF) for animated mxa
					 * draws with no associated Models entry, in
					 * which case default to eligible. */
					u_int16_t mi = TransExe[i].Model;
					bool eligible = !(mi < MAXNUMOFMODELS &&
					                  Models[mi].OwnerType == OWNER_BGOBJECT);
					c3d_set_object_shine_eligible(eligible);
				}
#endif
				draw_object(&TransExe[i].renderObject);
#ifdef RENDERER_C3D
				{ c3d_set_object_shine_eligible(true); }
#endif
			}

		}
	}
	FSSetWorld(&identity);
}
