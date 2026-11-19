/* aijump.cpp -- AI_JUMP, Boss_Ramqan's hopping brain (ENEMY_CONTROLTYPE_JUMP_AI)
 *
 * A port of Forsaken Remastered's kexForsakenAIBrainJump; Boss_Ramqan is its
 * only user, and the whole file is EDITION_REMASTER only. In
 * AIMODE_FOLLOWPATH the boss dwells, picks a linked node, and hops to it on
 * a parametric parabolic arc that writes Object.Pos directly (no
 * Speed-driven movement), firing through AI_UPDATEGUNS throughout. IDLE and
 * SCAN reuse the crawl handlers.
 *
 * Constants from the Remaster binary (.rodata): duration 75 ticks (~1.25 s
 * at 60 Hz), arc scale 4.0; JumpT advances by framelag. KEX's SetupJump
 * raycasts 1000 units down from the target node and uses
 *   landing    = floor hit point
 *   apex_y     = authored node Y (sits above the floor)
 *   arc_height = (apex_y - landing_y) * 4.0
 * so the boss arcs up toward the node altitude, then drops onto the pad.
 * Here landing is the node's SolidPos (Nodeload's floor snap, floor + 75)
 * with the 75 taken back off, and apex is Pos.y. When that arc is lower than
 * 30% of the horizontal distance (nodes placed flat at pad level), the 30%
 * arc is used so flat-pad rooms still show a hop.
 *
 * State lives in 5 ENEMY fields (JumpT, JumpInAir, JumpStart, JumpTarget,
 * JumpApexY) under EDITION_REMASTER.
 */

#include <stdio.h>
#include <math.h>

#include "game/main.h"
#include "math/new3d.h"
#include "data/mload.h"
#include "game/object.h"
#include "world/node.h"
#include "game/enemies.h"
#include "ai/ai.h"
#include "game/oct2.h"

/* Boss_Ramqan's jump AI is Remaster-only content: the ENEMY state
 * fields it drives exist only under EDITION_REMASTER (enemies.h), and
 * the dispatch slot is never selected in the 1998 build. The whole
 * body is compiled out of the original edition. */
#ifdef EDITION_REMASTER

constexpr float JUMP_DURATION_TICKS = 75.0F;
constexpr float JUMP_ARC_SCALE = 4.0F;

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
	/* Seq 0 (idle, degenerate 0→0): standing rest pose between hops.
	 * SetCurAnimSeq sets Object.Time = StartTime = 0, clamped there
	 * each frame — the rig holds the authored standing pose. */
	SetCurAnimSeq( 0, &Enemy->Object );
}

/* Snapshot start/target, switch into airborne sub-state.
 *
 * KEX matches `arc = (node[+0x10] - target.z) * 4` where target.z is
 * the post-snap floor hit from a downward raycast in SetupJump and
 * +0x10 is the un-snapped (authored) node Y. Translated to 1998:
 *   landing  = node->SolidPos - 75  (Nodeload's floor + 75, offset removed)
 *   apex_y   = node->Pos.y          (original authored, sits above floor)
 *   arc_height = (apex_y - landing.y) * JUMP_ARC_SCALE  (in JumpDoMovement)
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

	/* Seq 2 (jump takeoff, 60→78 ticks): KEX calls SetAnimSeq(2) inside
	 * SetupJump itself (our JumpBegin equivalent). Sets Object.Time = 60
	 * and clamps to 78 — the rig plays the authored takeoff keyframes
	 * over the start of the arc, then JumpDoMovement switches to seq 3
	 * (frozen mid-air hold) once Object.Time reaches 78. */
	SetCurAnimSeq( 2, &Enemy->Object );
}

u_int16_t MoveGroup( MLOADHEADER * m, VECTOR * StartPos,
							u_int16_t StartGroup, VECTOR * MoveOffset );

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
		/* Seq 4 (land, 78→120 ticks): touchdown animation. KEX calls
		 * SetAnimSeq(4) at landing. SetCurAnimSeq advances time from
		 * StartTime (78) to EndTime (120) across subsequent frames then
		 * holds — the rig plays the authored landing flourish and settles,
		 * and keeps that pose through the dwell below. When the dwell
		 * ends, AI_JUMP_FOLLOWPATH starts the next hop (seq 2); only if
		 * no node can be picked does JumpEnterIdle switch to seq 0. */
		SetCurAnimSeq( 4, &Enemy->Object );
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

		/* Once the takeoff anim (seq 2, 60→78) has played through
		 * (Object.Time reaches 1.3 s = 78 ticks), switch to seq 3 (jump
		 * idle, degenerate 78→78): KEX holds the rig frozen at the
		 * mid-air pose for the rest of the arc, which keeps the
		 * backward-lean cycle from playing mid-hop. */
		if( Enemy->Object.CurAnimSeq == 2 &&
			Enemy->Object.Time >= 1.3F * ANIM_SECOND )
			SetCurAnimSeq( 3, &Enemy->Object );

		/* arc_height = (apex_y - landing_y) * 4 — mirrors KEX's
		 * `(node[+0x10] - target.z) * _UNK_00b1cb1c`. Landing is
		 * JumpTarget.y (SolidPos.y - 75) and apex is the authored node Y.
		 * The larger of that and 30%-of-horizontal is used, so nodes
		 * authored at floor level (KEX-style nodes with no apex
		 * offset) still produce a visible hop. */
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
void AI_JUMP_FOLLOWPATH( ENEMY * Enemy )
{
	NODE * pick;

	/* Acquire a target before AI_THINK so the LoS / viewcone block
	 * inside AI_THINK can set AI_ICANSEEPLAYER. Without TShip set,
	 * AI_THINK skips that block and AI_UPDATEGUNS's gating
	 * (`AIFlags & AI_ICANSEEPLAYER`) silently filters out every
	 * shot — boss hops correctly but never fires. Mirrors the
	 * `SET_TARGET_PLAYERS` + `Tinfo->TObject` acquisition pattern
	 * in aifollow.cpp / aiscan.cpp. */
	Tinfo->Flags = 0;
	SET_TARGET_PLAYERS;
	AI_GetDistToNearestTarget( Enemy );
	if( Tinfo->TObject ) Enemy->TShip = Tinfo->TObject;

	/* The body is not turned toward the target. Per Ghidra decomp of
	 * kexForsakenAIBrain::AimAtTarget, KEX computes (target - boss) in
	 * body-local space and writes yaw + pitch to the turret comp; the
	 * body stays in spawn orientation. Here the UserContComps[] wired
	 * in enemies.cpp carry the aim. */

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
	 * during dwell. The aifire.cpp gate has been extended to
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

void AI_JUMP( ENEMY * Enemy )
{
	if( AI_JUMP_Mode[ Enemy->Object.AI_Mode ] )
		( * AI_JUMP_Mode[ Enemy->Object.AI_Mode ] )( Enemy );
}
#endif /* EDITION_REMASTER */
