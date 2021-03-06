//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// JMS 2009: -Added maximum number of races into InitSIS function to facilitate Androsynth into the game.
// It is very important to remember to update this number of races here when adding races to the game, otherwise
// the races won't show up in the game AND the defines and enums in races.h will have quirky side-effects.
//
// Originally the max number of races was defined by KOHR_AH_ID and some constant numbers.
// Max num of races is now defined by ISD_ID plus the constant numbers because of the added new races.
//
// JMS 2010: -Added SET_GAME_STATE (STARBASE_AVAILABLE, 1); to InitSIS so starbase is available right
// from the start of the game. Kind of hack, maybe we should just remove the whole starbase_available variable...
//
//			 -Changed the starting number of thrusters and turning jets for Chmmr Explorer
//			 -Game now starts at Procyon
//			 -Show human and Chmmr SoIs right from the start
//			 -Utwig ships not available for buying at starbases anymore
// 
// JMS 2011: -The locations of known races' home planets is shown right from the start of the game (but not the whole SoIs).

#include "globdata.h"

#include "coderes.h"
#include "encount.h"
#include "master.h"
#include "setup.h"
#include "units.h"
#include "hyper.h"
#include "resinst.h"
#include "nameref.h"
#include "build.h"
#include "hyper.h"
#include "state.h"
#include "grpinfo.h"
#include "gamestr.h"

#include <stdlib.h>
#include "libs/log.h"

#include "libs/mathlib.h"

#include <glib.h>


static void CreateRadar (void);

CONTEXT RadarContext;
FRAME PlayFrame;

GLOBDATA GlobData;

static BOOLEAN initedGameStructs = FALSE;


BYTE
getGameState (int startBit, int endBit)
{
	return (BYTE) (((startBit >> 3) == (endBit >> 3)
			? (GLOBAL (GameState[startBit >> 3]) >> (startBit & 7))
			: ((GLOBAL (GameState[startBit >> 3]) >> (startBit & 7))
			  | (GLOBAL (GameState[endBit >> 3])
			  << (endBit - startBit - (endBit & 7)))))
			& ((1 << (endBit - startBit + 1)) - 1));
}

void
setGameState (int startBit, int endBit, BYTE val
#ifdef STATE_DEBUG
		, const char *name
#endif
)
{
	GLOBAL (GameState[startBit >> 3]) =
			(GLOBAL (GameState[startBit >> 3])
			& (BYTE) ~(((1 << (endBit - startBit + 1)) - 1) << (startBit & 7)))
			| (BYTE)((val) << (startBit & 7));

	if ((startBit >> 3) < (endBit >> 3)) {
		GLOBAL (GameState[endBit >> 3]) =
				(GLOBAL (GameState[endBit >> 3])
				& (BYTE)~((1 << ((endBit & 7) + 1)) - 1))
				| (BYTE)((val) >> (endBit - startBit - (endBit & 7)));
	}
#ifdef STATE_DEBUG
	log_add (log_Debug, "State '%s' set to %d.", name, (int)val);
#endif
}

DWORD
getGameState32 (int startBit)
{
	DWORD v;
	int shift;

	for (v = 0, shift = 0; shift < 32; shift += 8, startBit += 8)
	{
		v |= getGameState (startBit, startBit + 7) << shift;
	}

	return v;
}

void
setGameState32 (int startBit, DWORD val
#ifdef STATE_DEBUG
		, const char *name
#endif
)
{
	DWORD v = val;
	int i;

	for (i = 0; i < 4; ++i, v >>= 8, startBit += 8)
	{
		setGameState (startBit, startBit + 7, v & 0xff
#ifdef STATE_DEBUG
				, "(ignored)"
#endif
				);
	}

#ifdef STATE_DEBUG
	log_add (log_Debug, "State '%s' set to %u.", name, (unsigned)val);
#endif
}

static void
CreateRadar (void)
{
	if (RadarContext == 0)
	{
		RECT r;
		CONTEXT OldContext;

		RadarContext = CreateContext ("RadarContext");
		OldContext = SetContext (RadarContext);
		SetContextFGFrame (Screen);
		r.corner.x = RADAR_X;
		r.corner.y = RADAR_Y;
		r.extent.width = RADAR_WIDTH;
		r.extent.height = RADAR_HEIGHT;
		SetContextClipRect (&r);
		SetContext (OldContext);
	}
}

BOOLEAN
LoadSC2Data (void)
{
	if (FlagStatFrame == 0)
	{
		FlagStatFrame = CaptureDrawable (
				LoadGraphic (FLAGSTAT_MASK_PMAP_ANIM));
		if (FlagStatFrame == NULL)
			return FALSE;

		MiscDataFrame = CaptureDrawable (
				LoadGraphic (MISCDATA_MASK_PMAP_ANIM));
		if (MiscDataFrame == NULL)
			return FALSE;

		FontGradFrame = CaptureDrawable (
				LoadGraphic (FONTGRAD_PMAP_ANIM));
	}

	CreateRadar ();

	if (LOBYTE (GLOBAL (CurrentActivity)) == IN_HYPERSPACE)
	{
		GLOBAL (ShipStamp.origin.x) =
				GLOBAL (ShipStamp.origin.y) = -1;
	}

	return TRUE;
}

static void
copyFleetInfo (FLEET_INFO *dst, SHIP_INFO *src, FLEET_STUFF *fleet)
{
	// other leading fields are irrelevant
	dst->crew_level = src->crew_level;
	dst->max_crew = src->max_crew;
	dst->max_energy = src->max_energy;

	dst->race_strings = src->race_strings;
	dst->icons = src->icons;
	dst->melee_icon = src->melee_icon;

	dst->actual_strength = fleet->strength;
	dst->known_loc = fleet->known_loc;
}

BOOLEAN
InitGameStructures (void)
{
	COUNT i;

	InitGlobData ();

	PlayFrame = CaptureDrawable (LoadGraphic (PLAYMENU_ANIM));
	
	{
		COUNT num_ships;
		SPECIES_ID s_id = ARILOU_ID;

		num_ships = LAST_MELEE_ID - s_id + 2; /* Yehat Rebels and Transport ship */
		
		InitQueue (&GLOBAL (avail_race_q), num_ships, sizeof (FLEET_INFO));
		for (i = 0; i < num_ships; ++i)
		{
			SPECIES_ID ship_ref;
			HFLEETINFO hFleet;
			FLEET_INFO *FleetPtr;

			if (i < num_ships - 2)
				ship_ref = s_id++;
			else if (i == num_ships - 2)
				ship_ref = YEHAT_ID;
			else  /* (i == num_ships - 1) */
				ship_ref = TRANSPORT_ID;
			
			hFleet = AllocLink (&GLOBAL (avail_race_q));
			if (!hFleet)
				continue;
			FleetPtr = LockFleetInfo (&GLOBAL (avail_race_q), hFleet);
			FleetPtr->SpeciesID = ship_ref;

			if (i < num_ships - 1)
			{
				HMASTERSHIP hMasterShip;
				MASTER_SHIP_INFO *MasterPtr;
				
				hMasterShip = FindMasterShip (ship_ref);
				MasterPtr = LockMasterShip (&master_q, hMasterShip);
				// Grab a copy of loaded icons and strings (not owned)
				copyFleetInfo (FleetPtr, &MasterPtr->ShipInfo,
						&MasterPtr->Fleet);
				UnlockMasterShip (&master_q, hMasterShip);
			}
			else
			{
				// Transport ship.
				RACE_DESC *RDPtr = load_ship (FleetPtr->SpeciesID,
						FALSE);
				if (RDPtr)
				{	// Grab a copy of loaded icons and strings
					copyFleetInfo (FleetPtr, &RDPtr->ship_info,
							&RDPtr->fleet);
					// avail_race_q owns these resources now
					free_ship (RDPtr, FALSE, FALSE);
				}
			}
			FleetPtr->allied_state = BAD_GUY;
			FleetPtr->known_strength = 0;
			FleetPtr->loc = FleetPtr->known_loc;
			// XXX: Hack: Rebel special case 
			if (i == YEHAT_REBEL_SHIP)
				FleetPtr->actual_strength = 0;
			FleetPtr->growth = 0;
			FleetPtr->growth_fract = 0;
			FleetPtr->growth_err_term = 255 >> 1;
			FleetPtr->days_left = 0;
			FleetPtr->func_index = ~0;

			UnlockFleetInfo (&GLOBAL (avail_race_q), hFleet);
			PutQueue (&GLOBAL (avail_race_q), hFleet);
		}
	}

	InitSISContexts ();
	LoadSC2Data ();

	InitPlanetInfo ();
	InitGroupInfo (TRUE);

	GLOBAL (glob_flags) = 0;

	GLOBAL (ElementWorth[COMMON]) = 1;
	GLOBAL_SIS (ElementAmounts[COMMON]) = 0;
	GLOBAL (ElementWorth[CORROSIVE]) = 2;
	GLOBAL_SIS (ElementAmounts[CORROSIVE]) = 0;
	GLOBAL (ElementWorth[BASE_METAL]) = 3;
	GLOBAL_SIS (ElementAmounts[BASE_METAL]) = 0;
	GLOBAL (ElementWorth[NOBLE]) = 4;
	GLOBAL_SIS (ElementAmounts[NOBLE]) = 0;
	GLOBAL (ElementWorth[RARE_EARTH]) = 5;
	GLOBAL_SIS (ElementAmounts[RARE_EARTH]) = 0;
	GLOBAL (ElementWorth[PRECIOUS]) = 6;
	GLOBAL_SIS (ElementAmounts[PRECIOUS]) = 0;
	GLOBAL (ElementWorth[RADIOACTIVE]) = 8;
	GLOBAL_SIS (ElementAmounts[RADIOACTIVE]) = 0;
	GLOBAL (ElementWorth[EXOTIC]) = 25;
	GLOBAL_SIS (ElementAmounts[EXOTIC]) = 0;

	for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
		GLOBAL_SIS (DriveSlots[i]) = EMPTY_SLOT + 0;
	GLOBAL_SIS (DriveSlots[0]) =  // JMS
	GLOBAL_SIS (DriveSlots[1]) =  // JMS
	GLOBAL_SIS (DriveSlots[2]) =  // JMS
	GLOBAL_SIS (DriveSlots[3]) =
	GLOBAL_SIS (DriveSlots[4]) = // JMS
	GLOBAL_SIS (DriveSlots[5]) = // JMS
	GLOBAL_SIS (DriveSlots[6]) = // JMS
			GLOBAL_SIS (DriveSlots[7]) = FUSION_THRUSTER;
	for (i = 0; i < NUM_JET_SLOTS; ++i)
		GLOBAL_SIS (JetSlots[i]) = EMPTY_SLOT + 1;
	GLOBAL_SIS (JetSlots[0]) =
	GLOBAL_SIS (JetSlots[1]) = // JMS
	GLOBAL_SIS (JetSlots[2]) = // JMS
	GLOBAL_SIS (JetSlots[3]) = // JMS
	GLOBAL_SIS (JetSlots[4]) = // JMS
	GLOBAL_SIS (JetSlots[5]) = // JMS
	GLOBAL_SIS (JetSlots[6]) = // JMS
		GLOBAL_SIS (JetSlots[7]) = TURNING_JETS;

	if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) != CHMMR_EXPLORER_SHIP)
		for (i = 0; i < NUM_MODULE_SLOTS; ++i)
			GLOBAL_SIS (ModuleSlots[i]) = EMPTY_SLOT + 2;
	
	/*GLOBAL_SIS (ModuleSlots[15]) = GUN_WEAPON;
	GLOBAL_SIS (ModuleSlots[2]) = CREW_POD;
	GLOBAL_SIS (ModuleSlots[1]) = FUEL_TANK;*/
	
	GLOBAL_SIS (ModuleSlots[0]) = STORAGE_BAY; // BW: no storage if commented
	GLOBAL_SIS (CrewEnlisted) = EXPLORER_CREW_CAPACITY;
	GLOBAL_SIS (FuelOnBoard) = EXPLORER_FUEL_CAPACITY;

	InitQueue (&GLOBAL (built_ship_q),
			MAX_BUILT_SHIPS, sizeof (SHIP_FRAGMENT));
	InitQueue (&GLOBAL (npc_built_ship_q), MAX_SHIPS_PER_SIDE,
			sizeof (SHIP_FRAGMENT));
	InitQueue (&GLOBAL (ip_group_q), MAX_BATTLE_GROUPS,
			sizeof (IP_GROUP));
	InitQueue (&GLOBAL (encounter_q), MAX_ENCOUNTERS, sizeof (ENCOUNTER));

	// DN 27FEB11 INITIALIZE BETA_NAOS FLAG 
	SET_GAME_STATE (PLAYER_VISITED_BETA_NAOS, 0);
	
	// JMS: Starbase is available right from the start!
	SET_GAME_STATE (STARBASE_AVAILABLE, 1);
	// BW: Lander is fully shielded from the start
	SET_GAME_STATE (LANDER_SHIELDS,
			(1 << EARTHQUAKE_DISASTER) |
			(1 << BIOLOGICAL_DISASTER) |
			(1 << LIGHTNING_DISASTER) |
			(1 << LAVASPOT_DISASTER));
	
	// JMS: Lander currently has "stronger" shot from the beginning. 
	// Currently it does nothing else than enables killing the critters marked with INVULNERABLE_TO_BASIC_WEAPON.
	SET_GAME_STATE (STRONGER_LANDER_SHOT, 1);
	
	SET_GAME_STATE (IMPROVED_LANDER_SHOT, 1);
	SET_GAME_STATE (IMPROVED_LANDER_SPEED, 1);
	
	GLOBAL (CurrentActivity) = IN_INTERPLANETARY | START_INTERPLANETARY;

	GLOBAL_SIS (ResUnits) = 6500;
	GLOBAL (CrewCost) = 5;
	GLOBAL (FuelCost) = 20;
	GLOBAL (ModuleCost[PLANET_LANDER]) = 500 / MODULE_COST_SCALE;
	GLOBAL (ModuleCost[FUSION_THRUSTER]) = 500 / MODULE_COST_SCALE;
	GLOBAL (ModuleCost[TURNING_JETS]) = 500 / MODULE_COST_SCALE;
	GLOBAL (ModuleCost[CREW_POD]) = 2000 / MODULE_COST_SCALE;
	GLOBAL (ModuleCost[STORAGE_BAY]) = 750 / MODULE_COST_SCALE;
	GLOBAL (ModuleCost[FUEL_TANK]) = 500 / MODULE_COST_SCALE;
	GLOBAL (ModuleCost[DYNAMO_UNIT]) = 2000 / MODULE_COST_SCALE;
	GLOBAL (ModuleCost[GUN_WEAPON]) = 2000 / MODULE_COST_SCALE;

	GLOBAL_SIS (NumLanders) = 1;

	utf8StringCopy (GLOBAL_SIS (ShipName), sizeof (GLOBAL_SIS (ShipName)),
			GAME_STRING (NAMING_STRING_BASE + 2));
	utf8StringCopy (GLOBAL_SIS (CommanderName),
			sizeof (GLOBAL_SIS (CommanderName)),
			GAME_STRING (NAMING_STRING_BASE + 3));

	// BW: all NAFS ships available but no ship allocated by default
	// JMS: Except Utwig. They're all pacifists now!
	ActivateStarShip (ZOQFOTPIK_SHIP, SET_ALLIED);
	ActivateStarShip (HUMAN_SHIP, SET_ALLIED);
	ActivateStarShip (SYREEN_SHIP, SET_ALLIED);
	ActivateStarShip (YEHAT_SHIP, SET_ALLIED);
	ActivateStarShip (SHOFIXTI_SHIP, SET_ALLIED);
	ActivateStarShip (CHMMR_SHIP, SET_ALLIED);
	ActivateStarShip (SUPOX_SHIP, SET_ALLIED);
	// ActivateStarShip (UTWIG_SHIP, SET_ALLIED);
	ActivateStarShip (ORZ_SHIP, SET_ALLIED);
	//	CloneShipFragment (HUMAN_SHIP, &GLOBAL (built_ship_q), 0);
	
	// JMS: Show human and Chmmr SoIs right from the start
	ActivateStarShip (CHMMR_SHIP, SPHERE_TRACKING);
	ActivateStarShip (HUMAN_SHIP, SPHERE_TRACKING);
	
	// JMS: Show where other known races' home planets are, not the whole SoIs.
	ActivateStarShip (ORZ_SHIP, SPHERE_INITIAL);
	ActivateStarShip (SHOFIXTI_SHIP, SPHERE_INITIAL);
	ActivateStarShip (SPATHI_SHIP, SPHERE_INITIAL);
	ActivateStarShip (SYREEN_SHIP, SPHERE_INITIAL);
	ActivateStarShip (VUX_SHIP, SPHERE_INITIAL);
	ActivateStarShip (YEHAT_SHIP, SPHERE_INITIAL);

	// JMS: Start at Procyon
	GLOBAL_SIS (log_x) = UNIVERSE_TO_LOGX (PROCYON_X);
	GLOBAL_SIS (log_y) = UNIVERSE_TO_LOGY (PROCYON_Y);
	CurStarDescPtr = 0;
	GLOBAL (autopilot.x) = ~0;
	GLOBAL (autopilot.y) = ~0;
	
	// JMS: Vary the possible location of the ones that left in hurry
	SET_GAME_STATE(HINT_WORLD_LOCATION, ((COUNT)TFB_Random () % 3));
	log_add (log_Debug, "Hint world location randomly set to %d.", GET_GAME_STATE(HINT_WORLD_LOCATION));

	/* In case the program is exited before the full game is terminated,
	 * make sure that the temporary files are deleted.
	 * This can be removed if we make sure if the full game is terminated
	 * before the game is exited.
	 * The initedSIS variable is added so the uninit won't happen more
	 * than once, as you can't remove the atexit function (when the full game
	 * ends).
	 */
	initedGameStructs = TRUE;
	atexit (UninitGameStructures);

	return (TRUE);
}

void
FreeSC2Data (void)
{
	DestroyContext (RadarContext);
	RadarContext = 0;
	DestroyDrawable (ReleaseDrawable (FontGradFrame));
	FontGradFrame = 0;
	DestroyDrawable (ReleaseDrawable (MiscDataFrame));
	MiscDataFrame = 0;
	DestroyDrawable (ReleaseDrawable (FlagStatFrame));
	FlagStatFrame = 0;
}

void
UninitGameStructures (void)
{
	HFLEETINFO hStarShip;

	if (!initedGameStructs)
		return;

	UninitQueue (&GLOBAL (encounter_q));
	UninitQueue (&GLOBAL (ip_group_q));
	UninitQueue (&GLOBAL (npc_built_ship_q));
	UninitQueue (&GLOBAL (built_ship_q));
	UninitGroupInfo ();
	UninitPlanetInfo ();

//    FreeSC2Data ();

	// The only resources avail_race_q owns are the Ur-Quan probe's
	// so free them now
	hStarShip = GetTailLink (&GLOBAL (avail_race_q));
	if (hStarShip)
	{
		FLEET_INFO *FleetPtr;

		FleetPtr = LockFleetInfo (&GLOBAL (avail_race_q), hStarShip);
		DestroyDrawable (ReleaseDrawable (FleetPtr->melee_icon));
		DestroyDrawable (ReleaseDrawable (FleetPtr->icons));
		DestroyStringTable (ReleaseStringTable (FleetPtr->race_strings));
		UnlockFleetInfo (&GLOBAL (avail_race_q), hStarShip);
	}

	UninitQueue (&GLOBAL (avail_race_q));
	
	DestroyDrawable (ReleaseDrawable (PlayFrame));
	PlayFrame = 0;
	initedGameStructs = FALSE;
}


// Name-based game state accessibility

//ENHANCE: allocate map_entries more efficiently, not one for every single bit position

struct map_entry
{
    char *stateName;
    int statePos;
    int stateLen;
};

static struct map_entry gameStateMap[NUM_GAME_STATE_BITS];

static GHashTable *gameStateNameMap = NULL;

void DumpAllGameStates(void)
{
    COUNT i;
    for (i = 0; i < NUM_GAME_STATE_BITS; i++)
    {
        if (gameStateMap[i].stateName)
        {
            BYTE val = getGameState(i, i+gameStateMap[i].stateLen);
            printf("%s = %d, ", gameStateMap[i].stateName, val);
        }
    }
    printf("\n");
}

void
InitGlobData (void)
{
	COUNT i;

	i = GLOBAL (glob_flags);
	memset (&GlobData, 0, sizeof (GlobData));
	GLOBAL (glob_flags) = (BYTE)i;

	GLOBAL (DisplayArray) = DisplayArray;
    
        /* This data is static, so really only needs to be initalised once. */
    if (gameStateNameMap == NULL)
    {
        gameStateNameMap = g_hash_table_new(g_str_hash, g_str_equal);
        
        memset (&gameStateMap, 0, sizeof (gameStateMap));
        
        i = 0;
#define ADD_GAME_STATE(name, len) \
        gameStateMap[i].stateName = #name; \
        gameStateMap[i].statePos = i; \
        gameStateMap[i].stateLen = len; \
        g_hash_table_insert(gameStateNameMap, gameStateMap[i].stateName, &gameStateMap[i]); \
        i += len;
#include "gamestate.inc"
    
    }
}

BYTE GetGameStateByName(const char *name)
{
    struct map_entry *e = g_hash_table_lookup(gameStateNameMap, name);
    BYTE val;
    
    if (!e)
    {
        log_add (log_Warning, "Cannot retrieve unknown game state '%s'; assuming 0!", name);
        return 0;
    }
    
    val = getGameState(e->statePos, e->statePos + e->stateLen);
    return val;
}

void SetGameStateByName(const char *name, BYTE val)
{
    struct map_entry *e = g_hash_table_lookup(gameStateNameMap, name);

    if (!e)
    {
        log_add (log_Warning, "Cannot set unknown game state '%s' !", name);
        return;
    }
    
    setGameState(e->statePos, e->statePos + e->stateLen, val);
}
