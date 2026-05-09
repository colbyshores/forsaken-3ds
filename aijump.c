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
	/* Free-run the .cob's authored Trans data — Ramqan.cob carries 99
	 * Trans entries (mostly per-leg-joint ROT keys + body TRANS) that
	 * play continuously when CurAnimSeq=-1. The Mekton template's
	 * MektonTurretSeqs are all {Start==End} single-frame poses; using
	 * them via SetCurAnimSeq(N) freezes Object.Time and the legs go
	 * static. -1 routes ProcessEnemies into its free-run loop branch
	 * (Time += framelag*AnimSpeed, wraps at OverallTime). */
	Enemy->Object.CurAnimSeq = -1;
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
	/* Nodeload's downward floor-snap adds +75 to put the object pivot
	 * above the floor (calibrated for Mekton-sized enemies whose pivot
	 * is at body center). KEX's SetupJump lands at the raw raycast hit
	 * — no offset — so a Ramqan-sized rig with its pivot near the
	 * pelvis ends up hovering ~75 units off the pad. Subtract the
	 * Nodeload offset back out so the boss's feet kiss the floor. */
	Enemy->JumpTarget.y -= 75.0F;
	Enemy->JumpApexY   = target->Pos.y;        /* apex from raw node Y */

	Enemy->LastTNode = Enemy->TNode;
	Enemy->TNode     = target;
	Enemy->Object.NearestNode = target;

	/* Don't lock the rig to seq 2 ("jump pose") at takeoff. Sequences
	 * authored as a single-frame pose (StartTime == EndTime) freeze
	 * the per-frame anim time advance, so the multi-part rig stops
	 * animating mid-air — visually the boss looks frozen during the
	 * arc, which reads as broken. Letting the previous animation
	 * seq (idle / walk-cycle) continue produces a continuous body
	 * animation that's clearly "the boss is still alive and moving"
	 * even if it isn't a leg-tucked jump pose specifically.
	 *
	 * The frame-exact KEX behaviour is a per-component anim during
	 * the arc (legs cycle, body bobs); reproducing that requires
	 * decompiled per-component frame data we don't have. The 80-85%
	 * fidelity bar is "boss visibly animates during arc" — left in
	 * the previous seq, that holds. SetCurAnimSeq(4) at landing
	 * still fires for the land flourish. */
}

extern u_int16_t MoveGroup( MLOADHEADER * m, VECTOR * StartPos,
                            u_int16_t StartGroup, VECTOR * MoveOffset );
extern MLOADHEADER Mloadheader;
extern VECTOR Forward;

/* Body rotation deliberately removed.
 *
 * Per Ghidra decomp of kexForsakenAIBrain::AimAtTarget, KEX does
 * NOT rotate the body — it computes (target - boss) in body-local
 * space (via inverse body matrix), produces yaw + pitch angles, and
 * writes them to the turret comp. The body stays in spawn
 * orientation, the turret swivels. We follow the same shape: the
 * UserContComps[] wired in enemies.c carries the aim, body matrix
 * stays static. */
static void JumpFaceTarget( ENEMY * Enemy )
{
	(void) Enemy;
}

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
		/* Free-run the .cob's per-component anim track (see
		 * JumpEnterIdle for full rationale). Mekton template's
		 * AnimSeqs are single-frame poses — any SetCurAnimSeq(N)
		 * freezes time. CurAnimSeq=-1 routes ProcessEnemies into
		 * the free-run branch and the .cob's 99 ROT/TRANS keys
		 * (per-leg joint cycle, body bob, springs) play continuously. */
		Enemy->Object.CurAnimSeq = -1;
		/* Stationary fire phase: dwell long enough for AI_UPDATEGUNS
		 * to cycle through several cooldown periods. Per research,
		 * Boss_Ramqan in Remaster fires for ~3s between leaps before
		 * picking the next pad. Was 0.5s (just a hop dwell) which
		 * left him silent and motion-less the whole encounter. */
		Enemy->Timer = ONE_SECOND * 3.0F;
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

	/* Acquire a target before AI_THINK so the LoS / viewcone block
	 * inside AI_THINK can set AI_ICANSEEPLAYER. Without TShip set,
	 * AI_THINK skips that block and AI_UPDATEGUNS's gating
	 * (`AIFlags & AI_ICANSEEPLAYER`) silently filters out every
	 * shot — boss hops correctly but never fires. Mirrors the
	 * `SET_TARGET_PLAYERS` + `Tinfo->TObject` acquisition pattern
	 * in aifollow.c / aiscan.c. */
	Tinfo->Flags = 0;
	SET_TARGET_PLAYERS;
	AI_GetDistToNearestTarget( Enemy );
	if( Tinfo->TObject ) Enemy->TShip = Tinfo->TObject;

	/* Rotate body toward target BEFORE AI_THINK so AI_THINK's
	 * viewcone check sees the boss facing the player. */
	JumpFaceTarget( Enemy );

	AI_THINK( Enemy, false, false );

	/* Boss_Ramqan is a single-encounter boss — KEX keeps him locked
	 * onto the player at all times.  Force AI_ANYPLAYERINRANGE so
	 * (a) AI_UPDATEGUNS keeps firing through the entire encounter
	 * and (b) the per-frame SetTurretVector loop in ProcessEnemies
	 * runs every tick, keeping the head/cannons aimed at the player
	 * (without it the head visibly sags between AI_THINK refreshes
	 * because UserAngle stops updating). */
	if( Enemy->TShip )
		Enemy->AIFlags |= AI_ANYPLAYERINRANGE | AI_ICANSEEPLAYER;

	/* Per Ghidra decomp of kexForsakenAIBrainJump::MODE_FollowPath:
	 * KEX calls UpdateGuns unconditionally every frame, including
	 * during the arc — boss fires while airborne, just like he does
	 * during dwell. The aifire.c:130 gate has been extended to
	 * bypass AI_ICANSEEPLAYER for JUMP_AI (mirroring the SPLINE
	 * bypass) so the per-gun BurstAngle check is what gates fire. */
	AI_UPDATEGUNS( Enemy );

	if( Enemy->JumpInAir )
	{
		JumpDoMovement( Enemy );
		return;
	}

	if( !(Enemy->AIFlags & AI_ANYPLAYERINRANGE) ) return;

	/* Dwell timer between leaps. AI_UPDATEGUNS already ran above
	 * (KEX-faithful unconditional call) — this loop just waits out
	 * the post-landing pause before picking the next pad. */

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
