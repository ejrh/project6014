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

// JMS 2010: Chmmr Explorer and precursor vessel now have different graphics for ship picking before fight in full game.

// JMS_GFX 2011: Merged the resolution Factor stuff from UQM-HD.

#include "pickship.h"

#include "build.h"
#include "colors.h"
#include "controls.h"
#include "menustat.h"
#include "pickmele.h"
#include "battle.h"
#include "races.h"
#include "resinst.h"
#include "nameref.h"
#include "settings.h"
#include "setup.h"
#include "sounds.h"
#include "libs/mathlib.h"

#include "libs/log.h"

#define NUM_PICK_SHIP_ROWS 2
#define NUM_PICK_SHIP_COLUMNS 6

#define NUM_PICK_SHIP_EXPLORER_COLUMNS 4

#define ICON_WIDTH (16 << RESOLUTION_FACTOR)				// JMS_GFX
#define ICON_HEIGHT (16 << RESOLUTION_FACTOR)				// JMS_GFX

#define FLAGSHIP_X_OFFS (65 << RESOLUTION_FACTOR)			// JMS_GFX
#define FLAGSHIP_Y_OFFS (4 << RESOLUTION_FACTOR)			// JMS_GFX
#define FLAGSHIP_WIDTH (22 << RESOLUTION_FACTOR)			// JMS_GFX
#define FLAGSHIP_HEIGHT (48 << RESOLUTION_FACTOR)			// JMS_GFX

#define FLAGSHIP_EXPLORER_WIDTH (36 << RESOLUTION_FACTOR)	// JMS_GFX
#define FLAGSHIP_EXPLORER_HEIGHT (36 << RESOLUTION_FACTOR)	// JMS_GFX
#define FLAGSHIP_EXPLORER_X_OFFS (58 << RESOLUTION_FACTOR)	// JMS_GFX
#define FLAGSHIP_EXPLORER_Y_OFFS (16 << RESOLUTION_FACTOR)	// JMS_GFX

static BOOLEAN
DoPickBattleShip (MENU_STATE *pMS)
{
	if (GLOBAL (CurrentActivity) & CHECK_ABORT)
	{
		pMS->CurFrame = 0;
		return (FALSE);
	}

	SetMenuSounds (MENU_SOUND_NONE, MENU_SOUND_NONE);
	
	if (!pMS->Initialized)
	{
		pMS->Initialized = TRUE;
		pMS->InputFunc = DoPickBattleShip;

		LockMutex (GraphicsLock);

		goto ChangeSelection;
	}
	else if (PulsedInputState.menu[KEY_MENU_SELECT])
	{
		if ((HSTARSHIP)pMS->CurFrame)
		{
			PlayMenuSound (MENU_SOUND_SUCCESS);
			return (FALSE);
		}
	}
	else
	{
		COORD new_row, new_col;
		int dx = 0, dy = 0;
		if (PulsedInputState.menu[KEY_MENU_RIGHT]) dx = 1;
		if (PulsedInputState.menu[KEY_MENU_LEFT]) dx = -1;
		if (PulsedInputState.menu[KEY_MENU_UP]) dy = -1;
		if (PulsedInputState.menu[KEY_MENU_DOWN]) dy = 1;

		new_col = pMS->first_item.x + dx;
		new_row = pMS->first_item.y + dy;

		if (new_row != pMS->first_item.y
				|| new_col != pMS->first_item.x)
		{
			RECT r;
			TEXT t;
			COUNT crew_level, max_crew;
			COUNT ship_index;
			HSTARSHIP hBattleShip, hNextShip;
			STARSHIP *StarShipPtr;
			
			COUNT num_cols;
			
			// JMS: Chmmr Explorer gfx
			if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == CHMMR_EXPLORER_SHIP)
			{
				if (new_row == 0 && new_col < 0)
					new_col = NUM_PICK_SHIP_EXPLORER_COLUMNS;
				else if (new_row > 0 && new_col < 0)
					new_col = 1;
				else if ( (new_row == 0 && new_col > NUM_PICK_SHIP_EXPLORER_COLUMNS) || (new_row > 0 && new_col > 1 ) )
					new_col = 0;
				
				if (new_row == 0 && pMS->first_item.x == 1 && pMS->first_item.y == 1)
					new_col = 3;
				else if (new_row == 0 && pMS->first_item.x == 0 && pMS->first_item.y == 1)
					new_col = 1;
				
				if ((new_row == 1 || new_row < 0) && pMS->first_item.x < 2 && pMS->first_item.y == 0)
					new_col = 0;
				else if ((new_row == 1 || new_row < 0) && pMS->first_item.x > 2 && pMS->first_item.y == 0)
					new_col = 1;
	
				if (new_row < 0)
					new_row = NUM_PICK_SHIP_ROWS - 1;
				else if (new_row == NUM_PICK_SHIP_ROWS)
					new_row = 0;
			}
			// JMS: Precursor vessel gfx
			else if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == PRECURSOR_SERVICE_VEHICLE)
			{
				if (new_col < 0)
					new_col = NUM_PICK_SHIP_COLUMNS;
				else if (new_col > NUM_PICK_SHIP_COLUMNS)
					new_col = 0;

				if (new_row < 0)
					new_row = NUM_PICK_SHIP_ROWS - 1;
				else if (new_row == NUM_PICK_SHIP_ROWS)
					new_row = 0;
			}

			PlayMenuSound (MENU_SOUND_MOVE);

			LockMutex (GraphicsLock);

#ifdef NEVER
			SetContextForeGroundColor (
					BUILD_COLOR (MAKE_RGB15 (0x0A, 0x0A, 0x0A), 0x1D));
			DrawRectangle (&pMS->flash_rect0);
#endif /* NEVER */
			pMS->first_item.y = new_row;
			pMS->first_item.x = new_col;

ChangeSelection:
			// JMS
			if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == CHMMR_EXPLORER_SHIP)
				num_cols = NUM_PICK_SHIP_EXPLORER_COLUMNS;
			else if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == PRECURSOR_SERVICE_VEHICLE)
				num_cols = NUM_PICK_SHIP_COLUMNS;
			
			if (pMS->first_item.x == (num_cols >> 1))
			{
				// JMS: Chmmr Explorer graphics
				if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == CHMMR_EXPLORER_SHIP)
				{
					pMS->flash_rect0.corner.x =
						pMS->flash_rect1.corner.x - (2 << RESOLUTION_FACTOR) + FLAGSHIP_EXPLORER_X_OFFS; // JMS_GFX
					pMS->flash_rect0.corner.y =
						pMS->flash_rect1.corner.y - (2 << RESOLUTION_FACTOR) + 
						FLAGSHIP_EXPLORER_Y_OFFS + (2 << RESOLUTION_FACTOR);// - 1; // JMS_GFX
					pMS->flash_rect0.extent.width = FLAGSHIP_EXPLORER_WIDTH + (4 << RESOLUTION_FACTOR); // JMS_GFX
					pMS->flash_rect0.extent.height = FLAGSHIP_EXPLORER_HEIGHT + (4 << RESOLUTION_FACTOR) - 4 * RESOLUTION_FACTOR;
				}
				// JMS: Precursor vessel graphics
				else if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == PRECURSOR_SERVICE_VEHICLE)
				{
					pMS->flash_rect0.corner.x =
						pMS->flash_rect1.corner.x - (2 << RESOLUTION_FACTOR) + FLAGSHIP_X_OFFS; // JMS_GFX
					pMS->flash_rect0.corner.y =
						pMS->flash_rect1.corner.y - (2 << RESOLUTION_FACTOR) + FLAGSHIP_Y_OFFS; // JMS_GFX
					pMS->flash_rect0.extent.width = FLAGSHIP_WIDTH + (4 << RESOLUTION_FACTOR); // JMS_GFX
					pMS->flash_rect0.extent.height = FLAGSHIP_HEIGHT + 4;
				}

				hBattleShip = GetTailLink (&race_q[0]); /* Flagship */
			}
			// Escort ships
			else
			{
				// The Escort ships of Chmmr Explorer
				if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == CHMMR_EXPLORER_SHIP)
				{
					new_col = pMS->first_item.x;
					pMS->flash_rect0.corner.x = pMS->flash_rect1.corner.x + ((9 - 2) << RESOLUTION_FACTOR)
					+ ((ICON_WIDTH + (8 << RESOLUTION_FACTOR)) * new_col) + (RESOLUTION_FACTOR); // JMS_GFX
					
					if (new_col > (NUM_PICK_SHIP_EXPLORER_COLUMNS >> 1))
					{
						--new_col;
						pMS->flash_rect0.corner.x += FLAGSHIP_EXPLORER_WIDTH - ICON_WIDTH
						+ (2 << RESOLUTION_FACTOR);// + (RESOLUTION_FACTOR - 1); // JMS_GFX
					}
					if (new_row > 0)
					{
						pMS->flash_rect0.corner.x += (ICON_WIDTH + (8 << RESOLUTION_FACTOR)); // JMS_GFX
						pMS->flash_rect0.corner.x += (FLAGSHIP_EXPLORER_WIDTH + (10 << RESOLUTION_FACTOR)) * new_col; // JMS_GFX
					}
					
					pMS->flash_rect0.corner.y = pMS->flash_rect1.corner.y + ((16 - 2) << RESOLUTION_FACTOR)
					+ ((ICON_HEIGHT + (4 << RESOLUTION_FACTOR)) * pMS->first_item.y) + RESOLUTION_FACTOR; // JMS_GFX
					pMS->flash_rect0.extent.width = ICON_WIDTH + (4 << RESOLUTION_FACTOR) - 2 * RESOLUTION_FACTOR; // JMS_GFX
					pMS->flash_rect0.extent.height = ICON_HEIGHT + (4 << RESOLUTION_FACTOR) - 2 * RESOLUTION_FACTOR; // JMS_GFX
					
					ship_index = (pMS->first_item.y * NUM_PICK_SHIP_EXPLORER_COLUMNS)
					+ new_col;
				}
				
				// The Escort ships of Precursor vessel
				else if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == PRECURSOR_SERVICE_VEHICLE)
				{
					new_col = pMS->first_item.x;
					pMS->flash_rect0.corner.x = pMS->flash_rect1.corner.x + ((5 - 2) << RESOLUTION_FACTOR)
						+ ((ICON_WIDTH + (4 << RESOLUTION_FACTOR)) * new_col);
					
					if (new_col > (NUM_PICK_SHIP_COLUMNS >> 1))
					{
						--new_col;
						pMS->flash_rect0.corner.x += FLAGSHIP_WIDTH - ICON_WIDTH;
					}
					
					pMS->flash_rect0.corner.y = pMS->flash_rect1.corner.y + ((16 - 2) << RESOLUTION_FACTOR)
						+ ((ICON_HEIGHT + (4 << RESOLUTION_FACTOR)) * pMS->first_item.y); // JMS_GFX
					pMS->flash_rect0.extent.width = ICON_WIDTH + (4 << RESOLUTION_FACTOR); // JMS_GFX
					pMS->flash_rect0.extent.height = ICON_HEIGHT + (4 << RESOLUTION_FACTOR); // JMS_GFX

					ship_index = (pMS->first_item.y * NUM_PICK_SHIP_COLUMNS)
						+ new_col;
				}
				
				for (hBattleShip = GetHeadLink (&race_q[0]);
						hBattleShip != GetTailLink (&race_q[0]);
						hBattleShip = hNextShip)
				{
					StarShipPtr = LockStarShip (&race_q[0], hBattleShip);
					if (StarShipPtr->index == ship_index
							&& (StarShipPtr->SpeciesID != NO_ID))
					{
						UnlockStarShip (&race_q[0], hBattleShip);
						break;
					}

					hNextShip = _GetSuccLink (StarShipPtr);
					UnlockStarShip (&race_q[0], hBattleShip);
				}

				if (hBattleShip == GetTailLink (&race_q[0]))
					hBattleShip = 0;
			}

			pMS->CurFrame = (FRAME)hBattleShip;

			// Black box for ship's captain's name
			SetContextForeGroundColor (BLACK_COLOR);
			r.corner.x = pMS->flash_rect1.corner.x + (6 << RESOLUTION_FACTOR)- 2 * RESOLUTION_FACTOR; // JMS_GFX
			r.corner.y = pMS->flash_rect1.corner.y + (5 << RESOLUTION_FACTOR) - 1 * RESOLUTION_FACTOR; // JMS_GFX
			r.extent.width = ((ICON_WIDTH + (4 << RESOLUTION_FACTOR)) * 3) - (4 << RESOLUTION_FACTOR); // JMS_GFX
			r.extent.height = (7 << RESOLUTION_FACTOR) - 1 * RESOLUTION_FACTOR; // JMS_GFX
			DrawFilledRectangle (&r);

			if (hBattleShip == 0)
			{
				crew_level = 0;
				max_crew = 0;
						// Satisfy compiler.
			}
			else
			{
				SetContextFont (TinyFont);

				t.baseline.x = r.corner.x + (r.extent.width >> 1);
				t.baseline.y = r.corner.y + (r.extent.height - 1) - 2 * RESOLUTION_FACTOR; // JMS_GFX
				t.align = ALIGN_CENTER;

				StarShipPtr = LockStarShip (&race_q[0], hBattleShip);
				if (StarShipPtr->captains_name_index == 0)
				{
					t.pStr = GLOBAL_SIS (CommanderName);
					t.CharCount = (COUNT)~0;
					crew_level = GLOBAL_SIS (CrewEnlisted);
					max_crew = GetCrewPodCapacity ();
				}
				else
				{
					STRING locString;

					locString = SetAbsStringTableIndex (
							StarShipPtr->race_strings,
							StarShipPtr->captains_name_index);
					t.pStr = (UNICODE *)GetStringAddress (locString);
					t.CharCount = GetStringLength (locString);
					crew_level = StarShipPtr->crew_level;
					max_crew = StarShipPtr->max_crew;
				}
				UnlockStarShip (&race_q[0], hBattleShip);

				SetContextForeGroundColor (
						BUILD_COLOR (MAKE_RGB15 (0x14, 0x0A, 0x00), 0x0C));
				font_DrawText (&t);
				SetContextForeGroundColor (BLACK_COLOR);
			}

			// Black box for crew amount text
			r.corner.x += (ICON_WIDTH + (4 << RESOLUTION_FACTOR))
					* ((NUM_PICK_SHIP_COLUMNS >> 1) + 1)
					+ FLAGSHIP_WIDTH - ICON_WIDTH; // JMS_GFX
			DrawFilledRectangle (&r);

			if (crew_level)
			{
				char buf[80];

				t.baseline.x = r.corner.x + (r.extent.width >> 1);
				t.pStr = buf;
				t.CharCount = (COUNT)~0;
				if (crew_level >= max_crew)
					sprintf (buf, "%u", crew_level);
				else
					sprintf (buf, "%u/%u", crew_level, max_crew);
				SetContextForeGroundColor (
						BUILD_COLOR (MAKE_RGB15 (0x00, 0x14, 0x00), 0x02));
				font_DrawText (&t);
			}

			SetFlashRect (NULL);
			SetFlashRect (&pMS->flash_rect0);
			UnlockMutex (GraphicsLock);
		}
	}

	SleepThread (ONE_SECOND / 30);

	return (TRUE);
}

static HSTARSHIP
GetArmadaStarShip (void)
{
	RECT pick_r;
	CONTEXT OldContext;
	HSTARSHIP hBattleShip;
	
	if (battle_counter[1] == 0)
	{
		// No opponents left.
		return 0;
	}
	
//    MenuSounds = CaptureSound (LoadSound (MENU_SOUNDS));

	OldContext = SetContext (SpaceContext);
	DrawArmadaPickShip (FALSE, &pick_r);

	{
		MENU_STATE MenuState;

		MenuState.InputFunc = DoPickBattleShip;
		MenuState.Initialized = FALSE;
		if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == CHMMR_EXPLORER_SHIP)
			MenuState.first_item.x = NUM_PICK_SHIP_EXPLORER_COLUMNS >> 1;
		else
			MenuState.first_item.x = NUM_PICK_SHIP_COLUMNS >> 1;
		MenuState.first_item.y = 0;
		MenuState.flash_task = 0;
		MenuState.CurFrame = 0;
		MenuState.flash_rect1.corner = pick_r.corner;
		MenuState.flash_rect1.extent.width = 0;

		UnlockMutex (GraphicsLock);
		SetMenuSounds (MENU_SOUND_ARROWS, MENU_SOUND_SELECT);
		DoInput (&MenuState, FALSE);
		LockMutex (GraphicsLock);

		SetFlashRect (NULL);

		hBattleShip = (HSTARSHIP)MenuState.CurFrame;
	}

	if (hBattleShip)
	{
		if (hBattleShip == GetTailLink (&race_q[0]))
			battle_counter[0] = 1;

		WaitForSoundEnd (0);
	}

//    DestroySound (ReleaseSound (MenuSounds));
	
SetContext (OldContext);

	return (hBattleShip);
}

HSTARSHIP
GetEncounterStarShip (STARSHIP *LastStarShipPtr, COUNT which_player)
{
	HSTARSHIP hBattleShip;

	if (LOBYTE (GLOBAL (CurrentActivity)) == IN_HYPERSPACE)
		// Get the next ship from the battle group.
		hBattleShip = GetHeadLink (&race_q[which_player]);
	else if (LOBYTE (GLOBAL (CurrentActivity)) == SUPER_MELEE)
	{
		// Let the player chose his own ship. (May be a computer player).

		if (!(GLOBAL (CurrentActivity) & IN_BATTLE))
		{
			// XXX: This check is needed, because UninitShips() calls
			//      this function after the battle is over to record
			//      the crew left in the last ship standing.
			hBattleShip = 0;
		}
		else if (!MeleeShipDeath (LastStarShipPtr, which_player))
			hBattleShip = 0;
					// Game over.
		else
		{
			if (!GetNextMeleeStarShip (which_player, &hBattleShip))
				hBattleShip = 0;
		}
	}
	else
	{
		// Full game.
		HSHIPFRAG hStarShip;
		SHIP_FRAGMENT *FragPtr;
		
		if (LastStarShipPtr == 0)
		{
			// First time picking a ship.
			if (which_player == 0 && battle_counter[0] > 1)
			{
				// The player in a full game, still having a ship left.
				hBattleShip = GetArmadaStarShip ();
			}
			else
			{
				hBattleShip = GetHeadLink (&race_q[which_player]);
				if (which_player == 1)
				{
					// Select the next ship for the computer.
					hStarShip = GetHeadLink (&GLOBAL (npc_built_ship_q));
					FragPtr = LockShipFrag (&GLOBAL (npc_built_ship_q),
							hStarShip);
					if (FragPtr->crew_level == INFINITE_FLEET)
					{
						// Infinite number of ships.
						battle_counter[1]++;
					}
					UnlockShipFrag (&GLOBAL (npc_built_ship_q), hStarShip);
				}
			}
		}
		else
		{
			QUEUE *pQueue;
			HSHIPFRAG hNextShip;

			if (which_player == 0)
				pQueue = &GLOBAL (built_ship_q);
			else
				pQueue = &GLOBAL (npc_built_ship_q);

			hBattleShip = GetHeadLink (&race_q[which_player]);
			for (hStarShip = GetHeadLink (pQueue);
					hStarShip != 0; hStarShip = hNextShip)
			{
				STARSHIP *SPtr;

				SPtr = LockStarShip (&race_q[which_player], hBattleShip);
				hNextShip = _GetSuccLink (SPtr);
				UnlockStarShip (&race_q[which_player], hBattleShip);
				hBattleShip = hNextShip;
				
				FragPtr = LockShipFrag (pQueue, hStarShip);
				if (SPtr == LastStarShipPtr)
				{
					if (FragPtr->crew_level != INFINITE_FLEET)
					{
						/* Record crew left after the battle */
						FragPtr->crew_level = SPtr->crew_level;
						if (GLOBAL (CurrentActivity) & IN_BATTLE)
							SPtr->SpeciesID = NO_ID;
									// deactivates the ship
					}
					else /* if infinite ships */
					{
						hBattleShip = GetTailLink (&race_q[which_player]);
						/* XXX: Note that if Syreen had a homeworld you could
						 * fight, all Syreen ships there would be crewed to
						 * the maximum, instead of the normal level */
						SPtr->crew_level = FragPtr->max_crew;
						SPtr->playerNr = which_player;
						SPtr->captains_name_index = PickCaptainName ();

						battle_counter[1]++;
					}
					UnlockShipFrag (pQueue, hStarShip);
					break;
				}
				hNextShip = _GetSuccLink (FragPtr);
				UnlockShipFrag (pQueue, hStarShip);
			}

			if (which_player == 0)
			{
				// Player in a full game.
				if (battle_counter[0])
					hBattleShip = GetArmadaStarShip ();
				else /* last ship was flagship */
				{
#define RUN_AWAY_FUEL_COST (5 * FUEL_TANK_SCALE)
					hBattleShip = 0;
					if (LastStarShipPtr->crew_level == 0)
					{
						/* Died in the line of duty */
						GLOBAL_SIS (CrewEnlisted) = (COUNT)~0;
					}
					else
					{
						// Player ran away.
						if (GLOBAL_SIS (FuelOnBoard) > RUN_AWAY_FUEL_COST)
							GLOBAL_SIS (FuelOnBoard) -= RUN_AWAY_FUEL_COST;
						else
							GLOBAL_SIS (FuelOnBoard) = 0;
					}
				}
			}
		}
	}

	return (hBattleShip);
}

void
DrawArmadaPickShip (BOOLEAN draw_salvage_frame, RECT *pPickRect)
{
#define PICK_NAME_HEIGHT 6
	//COUNT i;
	HSTARSHIP hBattleShip, hNextShip;
	STARSHIP *StarShipPtr;
	RECT r, pick_r;
	STAMP s;
	TEXT t;
	CONTEXT OldContext;
	FRAME PickFrame;

	OldContext = SetContext (SpaceContext);

	PickFrame = CaptureDrawable (LoadGraphic (SC2_PICK_PMAP_ANIM));

	BatchGraphics ();

	s.frame = PickFrame;
	
	// JMS: Correct graphics for Chmmr Explorer (for ship picking in full game before fight.)
	if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == CHMMR_EXPLORER_SHIP)
		s.frame = IncFrameIndex (s.frame);
	
	SetFrameHot (s.frame, MAKE_HOT_SPOT (0, 0));
	GetFrameRect (s.frame, &pick_r);
	GetContextClipRect (&r);
	pick_r.corner.x = (r.extent.width >> 1) - (pick_r.extent.width >> 1);
	pick_r.corner.y = (r.extent.height >> 1) - (pick_r.extent.height >> 1);
	
	if (!draw_salvage_frame)
		*pPickRect = pick_r;
	else
	{
		s.origin.x = r.extent.width >> 1;
		s.frame = IncFrameIndex (s.frame);
		SetFrameHot (s.frame, MAKE_HOT_SPOT (0, 0));
		GetFrameRect (s.frame, &r);
		s.origin.x -= r.extent.width >> 1;
		s.origin.y = pick_r.corner.y - (r.extent.height >> 1);
		DrawStamp (&s);
		
		// JMS: Correct graphics for Precursor vessel (for ship picking in full game before fight)
		if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS)== PRECURSOR_SERVICE_VEHICLE)
			s.frame = DecFrameIndex (s.frame);
		
		s.frame = DecFrameIndex (s.frame);
		pick_r.corner.y = s.origin.y + r.extent.height;

		r.corner.x = pick_r.corner.x;
		r.corner.y = s.origin.y;
		*pPickRect = r;
	}
	s.origin = pick_r.corner;
	DrawStamp (&s);

	t.baseline.x = pick_r.corner.x + (pick_r.extent.width >> 1);
	t.baseline.y = pick_r.corner.y + pick_r.extent.height - (5 << RESOLUTION_FACTOR) - 2 * RESOLUTION_FACTOR; // JMS_GFX
	t.align = ALIGN_CENTER;
	t.pStr = GLOBAL_SIS (ShipName);
	t.CharCount = (COUNT)~0;
	SetContextForeGroundColor (
			BUILD_COLOR (MAKE_RGB15 (0x12, 0x12, 0x12), 0x17));
	SetContextFont (StarConFont);
	font_DrawText (&t);

	r.extent.width = ICON_WIDTH;
	r.extent.height = ICON_HEIGHT;
	for (hBattleShip = GetHeadLink (&race_q[0]);
			hBattleShip != 0; hBattleShip = hNextShip)
	{
		StarShipPtr = LockStarShip (&race_q[0], hBattleShip);

		if (StarShipPtr->captains_name_index)
		{	// Escort ship, not SIS
			COUNT ship_index;

			ship_index = StarShipPtr->index;

			// JMS: Chmmr explorer graphics
			if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == CHMMR_EXPLORER_SHIP)
			{
				s.origin.x = pick_r.corner.x
				+ ((9 << RESOLUTION_FACTOR) + ((ICON_WIDTH + (8 << RESOLUTION_FACTOR))
						* (ship_index % NUM_PICK_SHIP_EXPLORER_COLUMNS))); // JMS_GFX
				
				s.origin.y = pick_r.corner.y
				+ ((16 << RESOLUTION_FACTOR) + ((ICON_HEIGHT + (4 << RESOLUTION_FACTOR))
						 * (ship_index / NUM_PICK_SHIP_EXPLORER_COLUMNS))); // JMS_GFX
				
				if ((ship_index / NUM_PICK_SHIP_EXPLORER_COLUMNS)>0)
				{
					s.origin.x += ICON_WIDTH + (8 << RESOLUTION_FACTOR); // JMS_GFX
					
					if((ship_index % NUM_PICK_SHIP_EXPLORER_COLUMNS)>0)
						s.origin.x += FLAGSHIP_EXPLORER_WIDTH + (10 << RESOLUTION_FACTOR); // JMS_GFX
				}
				
				if ((ship_index % NUM_PICK_SHIP_EXPLORER_COLUMNS) >=
					(NUM_PICK_SHIP_EXPLORER_COLUMNS >> 1))
					s.origin.x += FLAGSHIP_EXPLORER_WIDTH + (10 << RESOLUTION_FACTOR); // JMS_GFX
			}
			// JMS: Precursor vessel graphics
			else if (GET_GAME_STATE(WHICH_SHIP_PLAYER_HAS) == PRECURSOR_SERVICE_VEHICLE)
			{
				s.origin.x = pick_r.corner.x
				+ ((5 << RESOLUTION_FACTOR) + ((ICON_WIDTH + (4 << RESOLUTION_FACTOR))
						* (ship_index % NUM_PICK_SHIP_COLUMNS))); // JMS_GFX
				if ((ship_index % NUM_PICK_SHIP_COLUMNS) >=
					(NUM_PICK_SHIP_COLUMNS >> 1))
					s.origin.x += FLAGSHIP_WIDTH + (4 << RESOLUTION_FACTOR); // JMS_GFX
				s.origin.y = pick_r.corner.y
				+ ((16 << RESOLUTION_FACTOR) + ((ICON_HEIGHT + (4 << RESOLUTION_FACTOR))
						 * (ship_index / NUM_PICK_SHIP_COLUMNS))); // JMS_GFX
			}
 
			s.frame = StarShipPtr->icons;
			r.corner = s.origin;
			SetContextForeGroundColor (BLACK_COLOR);
			DrawFilledRectangle (&r);
			
			if ((StarShipPtr->SpeciesID != NO_ID) || (StarShipPtr->crew_level == 0))
			{
				DrawStamp (&s);
				if (StarShipPtr->SpeciesID == NO_ID)
				{
					/* Dead ship - mark with an X. */
					s.origin.x -= 1;
					s.frame = SetAbsFrameIndex (StatusFrame, 3);
					DrawStamp (&s);
				}
			}
			else
			{
				/* Ship ran away */
				SetContextForeGroundColor (
						BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x14), 0x01));
				DrawFilledStamp (&s);
			}
		}

		hNextShip = _GetSuccLink (StarShipPtr);
		UnlockStarShip (&race_q[0], hBattleShip);
	}

	UnbatchGraphics ();
	
	DestroyDrawable (ReleaseDrawable (PickFrame));

	SetContext (OldContext);
}
