/*===================================================================
	AI_JUMP — port of Forsaken Remastered's kexForsakenAIBrainJump.

	The KEX brain hops along the AI node graph with a parametric
	parabolic arc instead of crawling. Boss_Ramqan is the only user
	in the Remaster level set.

	Constants extracted from the Remaster binary (.rodata):
		duration  = 75 ticks (~1.25 s at 60 Hz)
		arc scale = 4.0
		dt        = 1.0 per tick

	The KEX brain's SetupJump runs a 1000-unit downward raycast from
	the target node to find the floor underneath, then uses:
		landing  = floor_hit_point   (snapped Y)
		apex_y   = original_node_Y   (un-snapped, sits above floor)
		arc_height = (apex_y - landing_y) * 4.0
	So the boss arcs UP toward the authored node altitude, then drops
	onto the pad below. Translated to 1998 NODE struct: landing is
	`SolidPos` (already computed by Nodeload's downward snap = floor +
	75) and apex is `Pos.y` (original authored). When the two agree
	(KEX-authored nodes placed flat at pad level), a 30%-of-horizontal
	fallback arc kicks in so flat-pad rooms still produce a visible
	hop instead of sliding.

	State lives in 5 ENEMY-struct fields under EDITION_REMASTER.
===================================================================*/

#include <stdio.h>
#include <math.h>

#include "main.h"
#include "new3d.h"
#include "mload.h"
#include "object.h"
#include "node.h"
#include "enemies.h"
#include "ai.h"

extern ENEMY_TYPES	EnemyTypes[];
extern float		framelag;

#define JUMP_DURATION_TICKS	75.0F
#define JUMP_ARC_SCALE		4.0F

/* Walk the current node's neighbor list, pick a random valid one
 * (excluding prev/cur). Mirrors KEX's biased reservoir sample —
 * first match wins; later matches replace with prob (n-1)/n where
 * n = NumOfLinks/2.
 *
 * Two-pass: first try forward links (non-prev, non-cur). If no
 * forward link exists (dead-end pad — the only neighbor IS prev),
 * fall back to allowing the back-link so the boss oscillates
 * between the dead-end and its parent rather than idling forever. */
static NODE * JumpChooseNextNode( ENEMY * Enemy )
{
	NODE * cur  = (NODE*) Enemy->TNode;
	NODE * prev = (NODE*) Enemy->LastTNode;
	NODE * pick = NULL;
	int    pass, i;

	if( !cur ) return NULL;

	for( pass = 0; pass < 2 && !pick; pass++ )
	{
		bool allow_prev = (pass == 1);
		for( i = 0; i < cur->NumOfLinks; i++ )
		{
			NODE * link = cur->NodeLink[i];
			if( !link ) continue;
			if( link == cur ) continue;
			if( !allow_prev && link == prev ) continue;
			if( !(link->NetMask & Enemy->Object.NodeNetwork) ) continue;

			if( !pick )
			{
				pick = link;
			}
			else
			{
				u_int16_t span = (u_int16_t)(cur->NumOfLinks >> 1);
				if( span < 2 ) span = 2;
				if( Random_Range(span) ) pick = link;
			}
		}
	}
	return pick;
}

/* Park the boss in idle for a beat between hops. */
static void JumpEnterIdle( ENEMY * Enemy )
{
	float idle = EnemyTypes[ Enemy->Type ].Behave.IdleTime;
	if( idle < ONE_SECOND ) idle = ONE_SECOND;

	Enemy->Timer = idle + (float) Random_Range( (u_int16_t) idle );
	Enemy->JumpInAir = 0;
	SetCurAnimSeq( 0, &Enemy->Object );
}

/* Snapshot start/target, switch into airborne sub-state.
 *
 * KEX matches `arc = (node[+0x10] - target.z) * 4` where target.z is
 * the post-snap floor hit from a downward raycast in SetupJump and
 * +0x10 is the un-snapped (authored) node Y. Translated to 1998:
 *   landing  = node->SolidPos  (= floor + 75 from Nodeload's snap)
 *   apex_y   = node->Pos.y     (original authored, sits above floor)
 *   arc_height = (apex_y - SolidPos.y) * JUMP_ARC_SCALE
 * The ramqan boss room's pads are placed with nodes well above the
 * pad surface, so the arc has real altitude. */
static void JumpBegin( ENEMY * Enemy, NODE * target )
{
	Enemy->JumpInAir   = 1;
	Enemy->JumpT       = 0.0F;
	Enemy->JumpStart   = Enemy->Object.Pos;
	Enemy->JumpTarget  = target->SolidPos;     /* land on the pad */
	Enemy->JumpApexY   = target->Pos.y;        /* apex from raw node Y */

	Enemy->LastTNode = Enemy->TNode;
	Enemy->TNode     = target;
	Enemy->Object.NearestNode = target;

	SetCurAnimSeq( 2, &Enemy->Object );
}

extern u_int16_t MoveGroup( MLOADHEADER * m, VECTOR * StartPos,
                            u_int16_t StartGroup, VECTOR * MoveOffset );
extern MLOADHEADER Mloadheader;

/* Per-frame parametric arc — pure lerp in xz, parabolic in y.
 * Updates Object.Group via MoveGroup so the engine's per-frame
 * AI_THINK / SoundInfo lookups work after a multi-room jump.
 * Mirrors KEX's DoMovement which calls MoveGroup + SetAndLinkGroup. */
static void JumpDoMovement( ENEMY * Enemy )
{
	float t, omt, arc;
	VECTOR * s = &Enemy->JumpStart;
	VECTOR * d = &Enemy->JumpTarget;
	VECTOR   StartPos = Enemy->Object.Pos;
	VECTOR   MoveOff;
	u_int16_t OldGroup = Enemy->Object.Group;

	Enemy->JumpT += framelag;

	if( Enemy->JumpT >= JUMP_DURATION_TICKS )
	{
		Enemy->Object.Pos = *d;
		Enemy->JumpInAir  = 0;
		SetCurAnimSeq( 4, &Enemy->Object );
		Enemy->Timer = ONE_SECOND * 0.5F;
	}
	else
	{
		float dx = d->x - s->x;
		float dy = d->y - s->y;
		float dz = d->z - s->z;
		float kex_arc, fallback_arc, horiz;

		t   = Enemy->JumpT / JUMP_DURATION_TICKS;
		omt = t - (t * t);

		/* arc_height = (apex_y - landing_y) * 4 — mirrors KEX's
		 * `(node[+0x10] - target.z) * _UNK_00b1cb1c`. Landing is
		 * SolidPos.y (floor+75) and apex is the authored node Y.
		 * If the node was authored at floor level (KEX-style nodes
		 * with no apex offset, or pads where the raycast snap landed
		 * on the same Y as the authored Pos), fall back to
		 * 30%-of-horizontal so flat-pad rooms still produce a visible
		 * hop. */
		kex_arc = (Enemy->JumpApexY - d->y) * JUMP_ARC_SCALE;
		horiz = sqrtf( dx * dx + dz * dz );
		fallback_arc = horiz * 0.30F;
		arc = (kex_arc > fallback_arc) ? kex_arc : fallback_arc;

		Enemy->Object.Pos.x = s->x + dx * t;
		Enemy->Object.Pos.y = s->y + dy * t + arc * omt;
		Enemy->Object.Pos.z = s->z + dz * t;
	}

	MoveOff.x = Enemy->Object.Pos.x - StartPos.x;
	MoveOff.y = Enemy->Object.Pos.y - StartPos.y;
	MoveOff.z = Enemy->Object.Pos.z - StartPos.z;
	Enemy->Object.Group = MoveGroup( &Mloadheader, &StartPos,
	                                 OldGroup, &MoveOff );
}

/*===================================================================
	AI_JUMP_FOLLOWPATH — main per-frame entry from AI_JUMP_Mode[].
===================================================================*/
void AI_JUMP_FOLLOWPATH( register ENEMY * Enemy )
{
	NODE * pick;

	AI_THINK( Enemy, false, false );

	if( Enemy->JumpInAir )
	{
		JumpDoMovement( Enemy );
		return;
	}

	if( !(Enemy->AIFlags & AI_ANYPLAYERINRANGE) ) return;

	Enemy->Timer -= framelag;
	if( Enemy->Timer > 0.0F ) return;
	Enemy->Timer = 0.0F;

	if( !Enemy->TNode )
	{
		Enemy->TNode = Enemy->Object.NearestNode;
		if( !Enemy->TNode )
		{
			JumpEnterIdle( Enemy );
			return;
		}
	}

	pick = JumpChooseNextNode( Enemy );
	if( pick )
		JumpBegin( Enemy, pick );
	else
		JumpEnterIdle( Enemy );
}

/* Reuse 1998 IDLE/SCAN handlers — the KEX brain doesn't differentiate
 * between scan and idle; both just tick down before the next hop
 * decision. Wire to AI_CRAWL_IDLE/SCAN to avoid duplicating logic. */
extern void AI_CRAWL_IDLE( register ENEMY * Enemy );
extern void AI_CRAWL_SCAN( register ENEMY * Enemy );

void (* AI_JUMP_Mode[ ])( ENEMY * Enemy ) = {
	NULL,
	AI_JUMP_FOLLOWPATH,	/* AIMODE_FOLLOWPATH */
	AI_CRAWL_IDLE,		/* AIMODE_IDLE */
	AI_CRAWL_SCAN,		/* AIMODE_SCAN */
	NULL,			/* MOVETOTARGET */
	NULL,			/* FIREATTARGET */
	NULL,			/* DOGFIGHT */
	NULL,			/* KILLMINE */
	NULL,			/* RETREAT */
	NULL,			/* FORMATION */
	NULL,			/* DEATH_CRASHNBURN */
	NULL,			/* DEATH_PINGOFF */
};

void AI_JUMP( register ENEMY * Enemy )
{
	if( AI_JUMP_Mode[ Enemy->Object.AI_Mode ] )
		( * AI_JUMP_Mode[ Enemy->Object.AI_Mode ] )( Enemy );
}
