/*
 *  pool.inc - 8-ball / practice pool for SA-MP, built on physics.inc (uPeppe)
 *
 *  Requires: a_samp, physics (included automatically if you haven't already)
 *
 *  Call from your main callbacks (see pool.pwn)
 *      Pool_Init()                                   OnGameModeInit
 *      Pool_Command(playerid, params[])              your /pool command
 *      Pool_Leave(playerid)                          OnPlayerDisconnect / OnPlayerDeath / OnPlayerInteriorChange
 *      Pool_OnPlayerSpawn(playerid)                  OnPlayerSpawn
 *      Pool_OnPlayerKeyStateChange(playerid, n, o)   OnPlayerKeyStateChange
 *      Pool_OnObjectUpdate(objectid)                 PHY_OnObjectUpdate
 *      Pool_OnObjectCollideWithObject(o1, o2)        PHY_OnObjectCollideWithObject
 *      Pool_OnObjectCollideWithWall(objectid, wall)  PHY_OnObjectCollideWithWall
 *
 *  Controls: RMB toggles aiming next to the cue ball, A/D rotate, hold LMB to charge power and
 *  release to shoot, SPACE cycles cameras, ENTER chalks the cue.
 *
 *  Rules (simplified 8-ball)
 *      The table stays open after the break; the first legal pocketed ball after it decides solids (1-7)
 *      or stripes (9-15). Keep shooting while you pocket your own group. Fouls (scratch, no ball hit,
 *      wrong ball hit first) pass the turn and the cue ball returns to the head spot (no ball in hand).
 *      Sink the 8 last to win; sinking it early or with a foul loses; on an open table it re-racks.
 *      The match result is announced to the whole server; the practice result only to the players at the table.
 *
 *  Protection
 *      Anybody within POOL_PROTECT_* (table + 5 m), game running or not, can't swing a melee weapon: the
 *      swing is cancelled by freezing the player until LMB is released. Balls can't be pushed by players
 *      and any ball moving without a shot is stopped. Players taking part can't climb onto the table.
 *
 *  UI (info box, bottom right: header, remaining balls, power bar, key guide)
 *      Anchored by POOL_BOX_RIGHT / POOL_BOX_BOTTOM (margins of the money counter and the radar).
 *      The panel is a box textdraw so its alpha works (POOL_PANEL_COLOR: lower alpha = more see-through).
 *      Box textdraws are not pixel-exact; POOL_RECT_* compensate (measured in game: height is about
 *      9.2 * letterY + 8 and the box starts about 4.5 above its y). The power bar parts are sprites (exact).
 *      Ball icons are font 5 model previews with TextDrawBackgroundColor 0 (no tile); POOL_PREVIEW_RX/RY/RZ
 *      turn the ball so the stripe and number show, POOL_PREVIEW_ZOOM sizes it (smaller = bigger ball).
 *      The ball number is the index of Pool_BallModel (3003 cue, 3002 and 3100-3105 solids 1-7, 3106 eight,
 *      2995-3001 stripes 9-15) and of Pool_BallPos (rack slots, 8 in the middle of row three).
 *
 *  Texts and colours
 *      All texts are POOL_TXT_*, Czech without diacritics (safe on any client / file encoding).
 *      Chat messages are sent in POOL_THEME (also the power bar colour); keep the {005580} codes inside
 *      POOL_TXT_* equal to it. Nicknames are highlighted with {FFFFFF}. A chat line may not exceed 144
 *      characters; the longest is POOL_TXT_WIN with two 24-character nicknames.
 *
 *  License: GPL-3.0-or-later (see LICENSE). Based on the pool demo and physics.inc by Peppe.
 */

#if defined _pool_included
	#endinput
#endif
#define _pool_included
#include <a_samp>

#if !defined PHY_MODE_2D
	#include <physics>
#endif

#define POOL_TXT_POWER              "SILA"
#define POOL_TXT_PRACTICE           "TRENINK"
#define POOL_TXT_HUD_TURN           "NA TAHU: %s"
#define POOL_TXT_GUIDE_1            "(RMB) zamerit - (A/D) mireni - (LMB) drz = sila, pustit = uder - (ENTER) krida"
#define POOL_TXT_GUIDE_2            "/pool opustit stul"

#define POOL_TXT_NOT_TURN           "[8-Ball] Nejsi na tahu."
#define POOL_TXT_TABLE_MATCH        "[8-Ball] Stul prave pouziva zapas."
#define POOL_TXT_TABLE_BUSY         "[8-Ball] Stul je obsazeny."
#define POOL_TXT_BAD_ID             "[8-Ball] Neplatne ID hrace."
#define POOL_TXT_USAGE              "[8-Ball] Pouziti: /pool [id hrace]"
#define POOL_TXT_INVITED            "[8-Ball] Pozval jsi hrace {FFFFFF}%s{005580} na hru."
#define POOL_TXT_INVITE_GOT         "[8-Ball] Hrac {FFFFFF}%s{005580} te zve na hru. Pro prijeti napis /pool %d."
#define POOL_TXT_START              "[8-Ball] Zapas zacal! Rozehrava {FFFFFF}%s{005580}."
#define POOL_TXT_RERACK             "[8-Ball] Koule c. 8 spadla pri volnem stole - nove rozestaveni."
#define POOL_TXT_GROUP              "[8-Ball] {FFFFFF}%s{005580} hraje {FFFFFF}%s{005580}."
#define POOL_TXT_FOUL               "[8-Ball] Faul {FFFFFF}%s{005580} (%s)."
#define POOL_TXT_WIN                "[8-Ball] {FFFFFF}%s{005580} vyhral zapas proti {FFFFFF}%s{005580} (%s)."
#define POOL_TXT_WIN_ILLEGAL        "souper potopil 8 nepravidelne"
#define POOL_TXT_WIN_EIGHT          "potopil kouli c. 8"
#define POOL_TXT_WIN_LEFT           "souper odesel od stolu"
#define POOL_TXT_WINNER             "[8-Ball] Vitez: {FFFFFF}%s{005580} (body: %d)."

#define POOL_WORLD                  3
#define POOL_ATTACH_SLOT            0
#define POOL_BALLS                  16
#define POOL_POCKETS                6
#define POOL_CUE_BALL               0
#define POOL_EIGHT                  8
#define POOL_NONE                   -1
#define POOL_CAMERAS                3
#define POOL_TICK_MS                21
#define POOL_INVITE_MS              30000
#define POOL_ATTACK_MS              300
#define POOL_PHY_MODEL              3003
#define POOL_THEME                  0x005580FF
#define POOL_POWER_STEP             2.6
#define POOL_PROTECT_MAX_WEAPON     15

#define POOL_TABLE_Z                998.86785889
#define POOL_CENTER_X               510.11267089844
#define POOL_CENTER_Y               -84.831642150879
#define POOL_POCKET_RADIUS          0.0825
#define POOL_AIM_RANGE              1.5
#define POOL_MAX_PLAYER_Z           999.5
#define POOL_AIM_ARC                30.0
#define POOL_AIM_STEP               0.9

#define POOL_NOCLIMB_MIN_X          509.24
#define POOL_NOCLIMB_MAX_X          510.99
#define POOL_NOCLIMB_MIN_Y          -86.17
#define POOL_NOCLIMB_MAX_Y          -83.52

#define POOL_PROTECT_MIN_X          504.589
#define POOL_PROTECT_MAX_X          515.636
#define POOL_PROTECT_MIN_Y          -90.818
#define POOL_PROTECT_MAX_Y          -78.869
#define POOL_PROTECT_MIN_Z          994.0
#define POOL_PROTECT_MAX_Z          1003.0

#define POOL_BOX_RIGHT              608.0
#define POOL_BOX_BOTTOM             420.0
#define POOL_BOX_W                  186.0
#define POOL_BOX_PAD_Y              5.0
#define POOL_BOX_H                  (56.0 + 2.0 * POOL_BOX_PAD_Y)
#define POOL_BOX_X                  (POOL_BOX_RIGHT - POOL_BOX_W)
#define POOL_BOX_Y                  (POOL_BOX_BOTTOM - POOL_BOX_H)
#define POOL_BOX_PAD                6.0
#define POOL_PANEL_COLOR            0x00000060

#define POOL_RECT_FACTOR            9.2
#define POOL_RECT_BASE              8.0
#define POOL_RECT_TOP               4.5

#define POOL_BAR_X                  (POOL_BOX_X + POOL_BOX_PAD)
#define POOL_BAR_Y                  (POOL_BOX_Y + POOL_BOX_PAD_Y + 28.0)
#define POOL_BAR_W                  (POOL_BOX_W - 2.0 * POOL_BOX_PAD)
#define POOL_BAR_H                  8.0

#define POOL_ICON_SIZE              11.4
#define POOL_ICON_STEP              11.6
#define POOL_PREVIEW_RX             -45.0
#define POOL_PREVIEW_RY             0.0
#define POOL_PREVIEW_RZ             45.0
#define POOL_PREVIEW_ZOOM           0.6

#define POOL_SND_BALL               31800
#define POOL_SND_POCKET             31803
#define POOL_SND_CHALK              31807
#define POOL_SND_WALL               31808
#define POOL_SND_SHOT               31810

#define POOL_GROUP_NONE             0
#define POOL_GROUP_SOLID            1
#define POOL_GROUP_STRIPE           2

#define POOL_FOUL_NONE              0
#define POOL_FOUL_SCRATCH           1
#define POOL_FOUL_NOHIT             2
#define POOL_FOUL_WRONG             3

enum E_POOL_BALL
{
	pbObject,
	bool:pbActive
}

enum E_POOL_PLAYER
{
	bool:ppPlaying,
	bool:ppChalking,
	bool:ppBlocked,
	bool:ppFireHeld,
	bool:ppHasSafe,
	ppWeapon,
	ppCamera,
	ppScore,
	ppGroup,
	ppInviter,
	ppInviteTime,
	Float:ppAimBase,
	Float:ppAimAngle,
	Float:ppSafeX,
	Float:ppSafeY,
	Float:ppSafeZ
}

static const Pool_GroupName[3][8] = {"VOLNY", "CELE", "PULKY"};
static const Pool_FoulName[4][24] = {"", "potopena bila", "nic netrefeno", "spatna prvni koule"};

static const Pool_BallModel[POOL_BALLS] =
{
	3003,
	3002, 3100, 3101, 3102, 3103, 3104, 3105,
	3106,
	2995, 2996, 2997, 2998, 2999, 3000, 3001
};

static const Float:Pool_BallPos[POOL_BALLS][2] =
{
	{510.11218261719, -84.40771484375},
	{510.10882568359, -85.166389465332},
	{510.14270019531, -85.232612609863},
	{510.0676574707,  -85.232200622559},
	{510.18600463867, -85.295257568359},
	{510.14828491211, -85.365989685059},
	{510.03665161133, -85.299163818359},
	{509.96197509766, -85.427406311035},
	{510.11242675781, -85.297294616699},
	{510.07455444336, -85.365234375},
	{510.00054931641, -85.363563537598},
	{510.25915527344, -85.431137084961},
	{510.18399047852, -85.430549621582},
	{510.10900878906, -85.43196105957},
	{510.03570556641, -85.432624816895},
	{510.22308349609, -85.362342834473}
};

static const Float:Pool_Pockets[POOL_POCKETS][2] =
{
	{509.61123657, -85.79737091},
	{510.67373657, -84.84423065},
	{510.61914062, -83.88769531},
	{509.61077881, -83.89227295},
	{510.61825562, -85.80107880},
	{509.55642700, -84.84602356}
};

static const Float:Pool_Corners[4][2] =
{
	{509.589, -85.818}, {510.636, -85.818}, {510.636, -83.869}, {509.589, -83.869}
};

static const Float:Pool_SideCam[2][3] =
{
	{511.84469604492, -84.831642150879, 1001.4904174805},
	{508.7971496582,  -84.831642150879, 1001.4904174805}
};

static
	gPoolBall[POOL_BALLS][E_POOL_BALL],
	gPoolPlayer[MAX_PLAYERS][E_POOL_PLAYER],
	Text:gPoolBox[5],
	Text:gPoolFill,
	Text:gPoolHeader,
	Text:gPoolBallHud[15],
	bool:gPoolStarted,
	bool:gPoolMatch,
	bool:gPoolBreak,
	gPoolPlayerOne = POOL_NONE,
	gPoolPlayerTwo = POOL_NONE,
	gPoolTurn = POOL_NONE,
	gPoolAimer = POOL_NONE,
	gPoolShooter = POOL_NONE,
	gPoolAimObject = INVALID_OBJECT_ID,
	gPoolShotScore,
	gPoolShotMask,
	gPoolFirstHit = POOL_NONE,
	gPoolFirstPocket = POOL_NONE,
	Float:gPoolPower,
	bool:gPoolPowerDown;

forward Pool_Sound(soundid);
forward Pool_Tick();
forward Pool_RestoreTurn(playerid);
forward Pool_RestoreWeapon(playerid);
forward Pool_Unfreeze(playerid);

static Float:Pool_Dist2D(Float:x1, Float:y1, Float:x2, Float:y2)
{
	return floatsqroot((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

static Float:Pool_AngleTo(Float:fx, Float:fy, Float:tx, Float:ty)
{
	new Float:angle = atan2(ty - fy, tx - fx) - 90.0;
	return (angle < 0.0) ? (angle + 360.0) : angle;
}

static Pool_Offset(Float:x, Float:y, Float:angle, Float:dist, &Float:ox, &Float:oy)
{
	ox = x + dist * floatsin(-angle, degrees);
	oy = y + dist * floatcos(-angle, degrees);
}

static bool:Pool_KeyPressed(key, newkeys, oldkeys)
{
	return (newkeys & key) && !(oldkeys & key);
}

static bool:Pool_KeyReleased(key, newkeys, oldkeys)
{
	return !(newkeys & key) && (oldkeys & key);
}

static Pool_BallGroup(ball)
{
	if(ball >= 1 && ball <= 7)
	{
		return POOL_GROUP_SOLID;
	}
	if(ball >= 9 && ball <= 15)
	{
		return POOL_GROUP_STRIPE;
	}
	return POOL_GROUP_NONE;
}

static Pool_GetBallPos(ball, &Float:x, &Float:y, &Float:z)
{
	return GetObjectPos(gPoolBall[ball][pbObject], x, y, z);
}

static Pool_FindBall(objectid)
{
	for(new i; i < POOL_BALLS; i++)
	{
		if(gPoolBall[i][pbActive] && gPoolBall[i][pbObject] == objectid)
		{
			return i;
		}
	}
	return POOL_NONE;
}

static Pool_DestroyBall(ball)
{
	if(gPoolBall[ball][pbActive])
	{
		PHY_DeleteObject(gPoolBall[ball][pbObject]);
	}
	if(IsValidObject(gPoolBall[ball][pbObject]))
	{
		DestroyObject(gPoolBall[ball][pbObject]);
	}
	gPoolBall[ball][pbObject] = INVALID_OBJECT_ID;
	gPoolBall[ball][pbActive] = false;
	return 1;
}

static Pool_SpawnBall(ball, bool:physics)
{
	Pool_DestroyBall(ball);
	new obj = CreateObject(Pool_BallModel[ball], Pool_BallPos[ball][0], Pool_BallPos[ball][1], POOL_TABLE_Z, 0.0, 0.0, 0.0);
	gPoolBall[ball][pbObject] = obj;
	if(!physics)
	{
		return 1;
	}

	PHY_InitObject(obj, POOL_PHY_MODEL, _, _, PHY_MODE_2D);
	PHY_ToggleObjectPlayerColls(obj, false);
	PHY_SetObjectFriction(obj, 0.40);
	PHY_RollObject(obj, _, PHY_ROLLING_MODE_ADVANCED);
	PHY_SetObjectWorld(obj, POOL_WORLD);
	gPoolBall[ball][pbActive] = true;
	return 1;
}

static Pool_ResetBalls(bool:physics)
{
	for(new i; i < POOL_BALLS; i++)
	{
		Pool_SpawnBall(i, physics);
	}
	return 1;
}

static Pool_GetPocket(ball)
{
	new Float:x, Float:y, Float:z;
	Pool_GetBallPos(ball, x, y, z);
	for(new i; i < POOL_POCKETS; i++)
	{
		if(Pool_Dist2D(x, y, Pool_Pockets[i][0], Pool_Pockets[i][1]) < POOL_POCKET_RADIUS)
		{
			return i;
		}
	}
	return POOL_NONE;
}

static Pool_CountBalls()
{
	new count;
	for(new i; i < POOL_BALLS; i++)
	{
		if(i == POOL_CUE_BALL || gPoolBall[i][pbActive])
		{
			count++;
		}
	}
	return count;
}

static Pool_GroupBefore(group)
{
	new count;
	for(new i = 1; i < POOL_BALLS; i++)
	{
		if(Pool_BallGroup(i) == group && (gPoolBall[i][pbActive] || (gPoolShotMask & (1 << i))))
		{
			count++;
		}
	}
	return count;
}

static bool:Pool_MaskHasGroup(group)
{
	for(new i = 1; i < POOL_BALLS; i++)
	{
		if(Pool_BallGroup(i) == group && (gPoolShotMask & (1 << i)))
		{
			return true;
		}
	}
	return false;
}

static bool:Pool_AllStopped()
{
	for(new i; i < POOL_BALLS; i++)
	{
		if(gPoolBall[i][pbActive] && PHY_IsObjectMoving(gPoolBall[i][pbObject]))
		{
			return false;
		}
	}
	return true;
}

static Pool_FreezeStrayBalls()
{
	for(new i; i < POOL_BALLS; i++)
	{
		if(gPoolBall[i][pbActive] && PHY_IsObjectMoving(gPoolBall[i][pbObject]))
		{
			PHY_SetObjectVelocity(gPoolBall[i][pbObject], 0.0, 0.0);
		}
	}
	return 1;
}

static Pool_CountPlayers()
{
	new count;
	foreach (new i : Player)
	{
		if(gPoolPlayer[i][ppPlaying])
		{
			count++;
		}
	}
	return count;
}

static Pool_GetBestScore()
{
	new best = cellmin;
	foreach (new i : Player)
	{
		if(gPoolPlayer[i][ppPlaying] && gPoolPlayer[i][ppScore] > best)
		{
			best = gPoolPlayer[i][ppScore];
		}
	}
	return best;
}

static Pool_Opponent(playerid)
{
	return (playerid == gPoolPlayerOne) ? gPoolPlayerTwo : gPoolPlayerOne;
}

static Pool_Say(playerid, const text[])
{
	SendClientMessage(playerid, POOL_THEME, text);
	return 1;
}

static Pool_Broadcast(const text[])
{
	foreach (new i : Player)
	{
		if(gPoolPlayer[i][ppPlaying])
		{
			SendClientMessage(i, POOL_THEME, text);
		}
	}
	return 1;
}

public Pool_Sound(soundid)
{
	foreach (new i : Player)
	{
		if(gPoolPlayer[i][ppPlaying])
		{
			PlayerPlaySound(i, soundid, 0.0, 0.0, 0.0);
		}
	}
	return 1;
}

static bool:Pool_InProtectedZone(playerid)
{
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	return (x > POOL_PROTECT_MIN_X && x < POOL_PROTECT_MAX_X && y > POOL_PROTECT_MIN_Y && y < POOL_PROTECT_MAX_Y && z > POOL_PROTECT_MIN_Z && z < POOL_PROTECT_MAX_Z);
}

static Pool_GuardTable(playerid)
{
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);

	new bool:overTable = (x > POOL_NOCLIMB_MIN_X && x < POOL_NOCLIMB_MAX_X && y > POOL_NOCLIMB_MIN_Y && y < POOL_NOCLIMB_MAX_Y);
	if(!overTable)
	{
		if(z < POOL_MAX_PLAYER_Z)
		{
			gPoolPlayer[playerid][ppHasSafe] = true;
			gPoolPlayer[playerid][ppSafeX] = x;
			gPoolPlayer[playerid][ppSafeY] = y;
			gPoolPlayer[playerid][ppSafeZ] = z;
		}
		return 1;
	}

	if(z < POOL_MAX_PLAYER_Z || !gPoolPlayer[playerid][ppHasSafe])
	{
		return 0;
	}
	return SetPlayerPos(playerid, gPoolPlayer[playerid][ppSafeX], gPoolPlayer[playerid][ppSafeY], gPoolPlayer[playerid][ppSafeZ]);
}

static Text:Pool_CreateRect(Float:x, Float:y, Float:w, Float:h, color)
{
	new str[4];
	format(str, sizeof str, "_");

	new Text:td = TextDrawCreate(x, y + POOL_RECT_TOP, str);
	TextDrawFont(td, 1);
	TextDrawLetterSize(td, 1.0, (h - POOL_RECT_BASE) / POOL_RECT_FACTOR);
	TextDrawColor(td, -1);
	TextDrawSetShadow(td, 0);
	TextDrawSetOutline(td, 0);
	TextDrawBackgroundColor(td, 0);
	TextDrawUseBox(td, 1);
	TextDrawBoxColor(td, color);
	TextDrawTextSize(td, x + w, 0.0);
	return td;
}

static Text:Pool_CreateSprite(Float:x, Float:y, Float:w, Float:h, color)
{
	new str[16];
	format(str, sizeof str, "LD_SPAC:white");

	new Text:td = TextDrawCreate(x, y, str);
	TextDrawFont(td, 4);
	TextDrawColor(td, color);
	TextDrawSetShadow(td, 0);
	TextDrawSetOutline(td, 0);
	TextDrawBackgroundColor(td, 0);
	TextDrawTextSize(td, w, h);
	return td;
}

static Text:Pool_CreateText(Float:x, Float:y, const text[], alignment, Float:lx, Float:ly, color = -1, outline = 255, font = 1)
{
	new str[256];
	format(str, sizeof str, "%s", text);

	new Text:td = TextDrawCreate(x, y, str);
	TextDrawAlignment(td, alignment);
	TextDrawFont(td, font);
	TextDrawLetterSize(td, lx, ly);
	TextDrawColor(td, color);
	TextDrawSetOutline(td, 1);
	TextDrawBackgroundColor(td, outline);
	TextDrawSetProportional(td, 1);
	return td;
}

static Text:Pool_CreateBallIcon(slot)
{
	new str[4];
	format(str, sizeof str, "_");

	new Text:td = TextDrawCreate(POOL_BOX_X + POOL_BOX_PAD + slot * POOL_ICON_STEP, POOL_BOX_Y + POOL_BOX_PAD_Y + 14.0, str);
	TextDrawFont(td, 5);
	TextDrawBackgroundColor(td, 0);
	TextDrawColor(td, -1);
	TextDrawSetOutline(td, 0);
	TextDrawTextSize(td, POOL_ICON_SIZE, POOL_ICON_SIZE);
	TextDrawSetPreviewModel(td, Pool_BallModel[slot + 1]);
	TextDrawSetPreviewRot(td, POOL_PREVIEW_RX, POOL_PREVIEW_RY, POOL_PREVIEW_RZ, POOL_PREVIEW_ZOOM);
	return td;
}

static Pool_CreateHud()
{
	new Float:x = POOL_BOX_X, Float:y = POOL_BOX_Y;

	gPoolBox[0] = Pool_CreateRect(x, y, POOL_BOX_W, POOL_BOX_H, POOL_PANEL_COLOR);
	gPoolBox[1] = Pool_CreateSprite(POOL_BAR_X, POOL_BAR_Y, POOL_BAR_W, POOL_BAR_H, 0xC0C0C0FF);
	gPoolFill = Pool_CreateSprite(POOL_BAR_X, POOL_BAR_Y, 1.0, POOL_BAR_H, POOL_THEME);
	gPoolBox[2] = Pool_CreateText(POOL_BAR_X, POOL_BAR_Y, POOL_TXT_POWER, 1, 0.2, 0.6, -1, 255, 2);
	gPoolBox[3] = Pool_CreateText(x + POOL_BOX_PAD, y + POOL_BOX_PAD_Y + 39.0, POOL_TXT_GUIDE_1, 1, 0.12, 0.9);
	gPoolBox[4] = Pool_CreateText(x + POOL_BOX_PAD, y + POOL_BOX_PAD_Y + 46.0, POOL_TXT_GUIDE_2, 1, 0.12, 0.9);
	gPoolHeader = Pool_CreateText(x + POOL_BOX_PAD, y + POOL_BOX_PAD_Y + 3.0, " ", 1, 0.28, 1.0);

	for(new i; i < sizeof gPoolBallHud; i++)
	{
		gPoolBallHud[i] = Pool_CreateBallIcon(i);
	}
	return 1;
}

static Pool_ShowHud(playerid)
{
	TextDrawTextSize(gPoolFill, 1.0, POOL_BAR_H);
	return TextDrawShowForPlayer(playerid, gPoolFill);
}

static Pool_HideHud(playerid)
{
	return TextDrawHideForPlayer(playerid, gPoolFill);
}

static Pool_ShowBox(playerid)
{
	for(new i; i < sizeof gPoolBox; i++)
	{
		TextDrawShowForPlayer(playerid, gPoolBox[i]);
	}
	return TextDrawShowForPlayer(playerid, gPoolHeader);
}

static Pool_HideBox(playerid)
{
	for(new i; i < sizeof gPoolBox; i++)
	{
		TextDrawHideForPlayer(playerid, gPoolBox[i]);
	}
	for(new i; i < sizeof gPoolBallHud; i++)
	{
		TextDrawHideForPlayer(playerid, gPoolBallHud[i]);
	}
	TextDrawHideForPlayer(playerid, gPoolHeader);
	return TextDrawHideForPlayer(playerid, gPoolFill);
}

static Pool_SetLabel(Text:td, const text[])
{
	new str[64];
	format(str, sizeof str, "%s", text);
	return TextDrawSetString(td, str);
}

static Pool_RefreshHud()
{
	new text[64], name[MAX_PLAYER_NAME], count;
	if(gPoolMatch && gPoolTurn != POOL_NONE)
	{
		new group = gPoolPlayer[gPoolTurn][ppGroup];
		GetPlayerName(gPoolTurn, name, sizeof name);
		format(text, sizeof text, POOL_TXT_HUD_TURN, name);
		if(group != POOL_GROUP_NONE)
		{
			strcat(text, " - ", sizeof text);
			strcat(text, Pool_GroupName[group], sizeof text);
		}
	}
	else
	{
		format(text, sizeof text, POOL_TXT_PRACTICE);
	}
	Pool_SetLabel(gPoolHeader, text);

	for(new b = 1; b < POOL_BALLS; b++)
	{
		if(gPoolBall[b][pbActive])
		{
			TextDrawSetPreviewModel(gPoolBallHud[count], Pool_BallModel[b]);
			count++;
		}
	}

	foreach (new i : Player)
	{
		if(!gPoolPlayer[i][ppPlaying])
		{
			continue;
		}
		Pool_ShowBox(i);
		for(new slot; slot < sizeof gPoolBallHud; slot++)
		{
			if(slot < count)
			{
				TextDrawShowForPlayer(i, gPoolBallHud[slot]);
			}
			else
			{
				TextDrawHideForPlayer(i, gPoolBallHud[slot]);
			}
		}
	}
	return 1;
}

static Pool_SetSideCamera(playerid, side)
{
	SetPlayerCameraPos(playerid, Pool_SideCam[side][0], Pool_SideCam[side][1], Pool_SideCam[side][2]);
	return SetPlayerCameraLookAt(playerid, POOL_CENTER_X, POOL_CENTER_Y, POOL_TABLE_Z);
}

static Pool_SetCamera(playerid)
{
	new mode = gPoolPlayer[playerid][ppCamera];
	if(mode > 0)
	{
		return Pool_SetSideCamera(playerid, mode - 1);
	}

	new Float:bx, Float:by, Float:bz, Float:cx, Float:cy;
	Pool_GetBallPos(POOL_CUE_BALL, bx, by, bz);
	Pool_Offset(bx, by, gPoolPlayer[playerid][ppAimAngle] + 180.0, 0.675, cx, cy);
	SetPlayerCameraPos(playerid, cx, cy, POOL_TABLE_Z + 0.28);
	return SetPlayerCameraLookAt(playerid, bx, by, bz + 0.17);
}

static Pool_RestoreView(playerid)
{
	TogglePlayerControllable(playerid, 1);
	return SetCameraBehindPlayer(playerid);
}

static Pool_UpdateAim(playerid)
{
	new Float:px, Float:py, Float:pz, Float:bx, Float:by, Float:bz, Float:x, Float:y,
		Float:angle = gPoolPlayer[playerid][ppAimAngle];

	GetPlayerPos(playerid, px, py, pz);
	Pool_GetBallPos(POOL_CUE_BALL, bx, by, bz);

	Pool_Offset(bx, by, angle + 180.0, 0.085, x, y);
	SetObjectPos(gPoolAimObject, x, y, bz);
	SetObjectRot(gPoolAimObject, 7.0, 0.0, angle + 180.0);

	Pool_Offset(bx, by, angle + 180.0, Pool_Dist2D(px, py, bx, by), px, py);
	SetPlayerPos(playerid, px, py, pz);
	SetPlayerFacingAngle(playerid, angle);
	return Pool_SetCamera(playerid);
}

static Pool_StartAim(playerid)
{
	new Float:px, Float:py, Float:pz, Float:bx, Float:by, Float:bz;
	GetPlayerPos(playerid, px, py, pz);
	Pool_GetBallPos(POOL_CUE_BALL, bx, by, bz);
	if(pz >= POOL_MAX_PLAYER_Z || Pool_Dist2D(px, py, bx, by) >= POOL_AIM_RANGE)
	{
		return 0;
	}

	new Float:angle = Pool_AngleTo(px, py, bx, by);
	gPoolPlayer[playerid][ppAimBase] = angle;
	gPoolPlayer[playerid][ppAimAngle] = angle;
	gPoolAimer = playerid;
	gPoolPower = 1.0;
	gPoolPowerDown = false;
	gPoolAimObject = CreateObject(3004, bx, by, bz, 7.0, 0.0, angle + 180.0);

	TogglePlayerControllable(playerid, 0);
	SetPlayerArmedWeapon(playerid, 0);
	Pool_UpdateAim(playerid);
	ApplyAnimation(playerid, "POOL", "POOL_Med_Start", 50.0, 0, 0, 0, 1, 1, 1);
	return Pool_ShowHud(playerid);
}

static Pool_ClearAim(playerid)
{
	gPoolAimer = POOL_NONE;
	if(IsValidObject(gPoolAimObject))
	{
		DestroyObject(gPoolAimObject);
	}
	gPoolAimObject = INVALID_OBJECT_ID;
	return Pool_HideHud(playerid);
}

static Pool_CancelAim(playerid)
{
	Pool_ClearAim(playerid);
	Pool_RestoreView(playerid);
	GivePlayerWeapon(playerid, 7, 1);
	return ApplyAnimation(playerid, "CARRY", "crry_prtial", 1.0, 0, 0, 0, 0, 0, 1);
}

static Pool_CycleCamera(playerid)
{
	if(gPoolAimer != playerid)
	{
		return 0;
	}
	gPoolPlayer[playerid][ppCamera] = (gPoolPlayer[playerid][ppCamera] + 1) % POOL_CAMERAS;
	return Pool_SetCamera(playerid);
}

static Pool_TickPower(playerid)
{
	gPoolPower += gPoolPowerDown ? -POOL_POWER_STEP : POOL_POWER_STEP;
	if(gPoolPower <= 0.0)
	{
		gPoolPowerDown = false;
		gPoolPower = POOL_POWER_STEP;
	}
	else if(gPoolPower > 100.0)
	{
		gPoolPowerDown = true;
		gPoolPower = 100.0 - POOL_POWER_STEP;
	}
	TextDrawTextSize(gPoolFill, POOL_BAR_W * gPoolPower / 100.0, POOL_BAR_H);
	return TextDrawShowForPlayer(playerid, gPoolFill);
}

static Pool_TickRotate(playerid, lr)
{
	new Float:angle = gPoolPlayer[playerid][ppAimAngle] + ((lr > 0) ? POOL_AIM_STEP : -POOL_AIM_STEP);
	if(floatabs(gPoolPlayer[playerid][ppAimBase] - angle) >= POOL_AIM_ARC)
	{
		return 0;
	}
	gPoolPlayer[playerid][ppAimAngle] = angle;
	return Pool_UpdateAim(playerid);
}

static Pool_TickAim(playerid)
{
	new keys, ud, lr;
	GetPlayerKeys(playerid, keys, ud, lr);
	if(keys & KEY_FIRE)
	{
		return Pool_TickPower(playerid);
	}
	if(lr)
	{
		return Pool_TickRotate(playerid, lr);
	}
	return 0;
}

static Pool_ToggleAim(playerid)
{
	if(!Pool_AllStopped())
	{
		return 0;
	}
	if(gPoolAimer == playerid)
	{
		return Pool_CancelAim(playerid);
	}
	if(gPoolAimer != POOL_NONE || gPoolPlayer[playerid][ppChalking] || !gPoolBall[POOL_CUE_BALL][pbActive])
	{
		return 0;
	}
	if(gPoolMatch && gPoolTurn != playerid)
	{
		return Pool_Say(playerid, POOL_TXT_NOT_TURN);
	}
	return Pool_StartAim(playerid);
}

static Pool_Stop()
{
	if(gPoolAimer != POOL_NONE)
	{
		Pool_CancelAim(gPoolAimer);
	}
	foreach (new i : Player)
	{
		if(gPoolPlayer[i][ppPlaying])
		{
			Pool_HideBox(i);
		}
	}
	gPoolStarted = false;
	gPoolMatch = false;
	gPoolBreak = false;
	gPoolPlayerOne = POOL_NONE;
	gPoolPlayerTwo = POOL_NONE;
	gPoolTurn = POOL_NONE;
	gPoolShooter = POOL_NONE;
	return Pool_ResetBalls(false);
}

static Pool_AddScore(points)
{
	if(gPoolShooter == POOL_NONE || !gPoolPlayer[gPoolShooter][ppPlaying])
	{
		return 0;
	}

	new text[16];
	gPoolPlayer[gPoolShooter][ppScore] += points;
	gPoolShotScore += points;
	if(gPoolShotScore > 0)
	{
		format(text, sizeof text, "~b~~h~+%d", gPoolShotScore);
	}
	else
	{
		format(text, sizeof text, "~r~~h~%d", gPoolShotScore);
	}

	GameTextForPlayer(gPoolShooter, text, 100000, 4);
	return PlayerPlaySound(gPoolShooter, 1250, 0.0, 0.0, 0.0);
}

static Pool_Shoot(playerid)
{
	if(gPoolAimer != playerid)
	{
		return 0;
	}

	new Float:speed = 0.4 + gPoolPower * 0.02,
		Float:angle = gPoolPlayer[playerid][ppAimAngle];

	ApplyAnimation(playerid, "POOL", "POOL_Med_Shot", 3.0, 0, 0, 0, 0, 0, 1);
	PHY_SetObjectVelocity(gPoolBall[POOL_CUE_BALL][pbObject], speed * floatsin(-angle, degrees), speed * floatcos(-angle, degrees));
	if(gPoolPlayer[playerid][ppCamera] == 0)
	{
		Pool_SetSideCamera(playerid, random(2));
	}

	Pool_Sound(POOL_SND_SHOT);
	Pool_ClearAim(playerid);
	GivePlayerWeapon(playerid, 7, 1);
	gPoolShooter = playerid;
	gPoolShotScore = 0;
	gPoolShotMask = 0;
	gPoolFirstHit = POOL_NONE;
	gPoolFirstPocket = POOL_NONE;
	return 1;
}

static Pool_Chalk(playerid)
{
	if(gPoolAimer == playerid || gPoolPlayer[playerid][ppChalking])
	{
		return 0;
	}

	gPoolPlayer[playerid][ppChalking] = true;
	SetPlayerArmedWeapon(playerid, 0);
	SetPlayerAttachedObject(playerid, POOL_ATTACH_SLOT, 338, 6, 0.0, 0.07, -0.85, 0.0, 0.0, 0.0);
	ApplyAnimation(playerid, "POOL", "POOL_ChalkCue", 3.0, 0, 0, 0, 0, 0, 1);
	SetTimerEx("Pool_Sound", 1400, false, "i", POOL_SND_CHALK);
	SetTimerEx("Pool_RestoreWeapon", 3500, false, "i", playerid);
	return 1;
}

static Pool_BlockAttack(playerid)
{
	if(gPoolAimer == playerid || gPoolShooter == playerid || gPoolPlayer[playerid][ppBlocked])
	{
		return 0;
	}

	new weapon = GetPlayerWeapon(playerid);
	if(weapon > POOL_PROTECT_MAX_WEAPON || !Pool_InProtectedZone(playerid))
	{
		return 0;
	}

	gPoolPlayer[playerid][ppBlocked] = true;
	gPoolPlayer[playerid][ppWeapon] = weapon;
	TogglePlayerControllable(playerid, 0);
	SetPlayerArmedWeapon(playerid, 0);
	ClearAnimations(playerid, 1);
	return SetTimerEx("Pool_Unfreeze", POOL_ATTACK_MS, false, "i", playerid);
}

public Pool_Unfreeze(playerid)
{
	if(!gPoolPlayer[playerid][ppBlocked])
	{
		return 0;
	}
	if(gPoolAimer == playerid || gPoolShooter == playerid)
	{
		gPoolPlayer[playerid][ppBlocked] = false;
		return 0;
	}

	new keys, ud, lr;
	GetPlayerKeys(playerid, keys, ud, lr);
	if(gPoolPlayer[playerid][ppFireHeld] || (keys & KEY_FIRE))
	{
		return SetTimerEx("Pool_Unfreeze", 100, false, "i", playerid);
	}

	gPoolPlayer[playerid][ppBlocked] = false;
	TogglePlayerControllable(playerid, 1);
	if(!gPoolPlayer[playerid][ppPlaying])
	{
		return SetPlayerArmedWeapon(playerid, gPoolPlayer[playerid][ppWeapon]);
	}
	if(gPoolPlayer[playerid][ppChalking])
	{
		return 1;
	}
	SetPlayerArmedWeapon(playerid, 7);
	return ApplyAnimation(playerid, "CARRY", "crry_prtial", 1.0, 0, 0, 0, 0, 0, 1);
}

static Pool_Finish()
{
	new shooter = gPoolShooter, best = Pool_GetBestScore(), name[MAX_PLAYER_NAME], text[144];

	Pool_Stop();
	if(shooter != POOL_NONE)
	{
		Pool_RestoreView(shooter);
	}

	foreach (new i : Player)
	{
		if(!gPoolPlayer[i][ppPlaying] || gPoolPlayer[i][ppScore] != best)
		{
			continue;
		}
		GetPlayerName(i, name, sizeof name);
		format(text, sizeof text, POOL_TXT_WINNER, name, best);
		Pool_Broadcast(text);
	}
	foreach (new i : Player)
	{
		gPoolPlayer[i][ppPlaying] = false;
	}
	return 1;
}

static Pool_EndMatch(winner, const reason[])
{
	new a = gPoolPlayerOne, b = gPoolPlayerTwo, winnerName[MAX_PLAYER_NAME], loserName[MAX_PLAYER_NAME], text[144];

	GetPlayerName(winner, winnerName, sizeof winnerName);
	GetPlayerName(Pool_Opponent(winner), loserName, sizeof loserName);
	format(text, sizeof text, POOL_TXT_WIN, winnerName, loserName, reason);
	SendClientMessageToAll(POOL_THEME, text);

	Pool_RestoreView(a);
	Pool_RestoreView(b);
	Pool_Stop();
	gPoolPlayer[a][ppPlaying] = false;
	gPoolPlayer[b][ppPlaying] = false;
	return 1;
}

static Pool_GetFoul(shooter)
{
	if(gPoolShotMask & (1 << POOL_CUE_BALL))
	{
		return POOL_FOUL_SCRATCH;
	}
	if(gPoolFirstHit == POOL_NONE)
	{
		return POOL_FOUL_NOHIT;
	}

	new group = gPoolPlayer[shooter][ppGroup];
	if(group == POOL_GROUP_NONE)
	{
		return POOL_FOUL_NONE;
	}
	if(Pool_GroupBefore(group) > 0)
	{
		return (Pool_BallGroup(gPoolFirstHit) == group) ? POOL_FOUL_NONE : POOL_FOUL_WRONG;
	}
	return (gPoolFirstHit == POOL_EIGHT) ? POOL_FOUL_NONE : POOL_FOUL_WRONG;
}

static Pool_JudgeShot(shooter)
{
	new other = Pool_Opponent(shooter),
		group = gPoolPlayer[shooter][ppGroup],
		foul = Pool_GetFoul(shooter),
		bool:wasBreak = gPoolBreak,
		bool:keep,
		name[MAX_PLAYER_NAME], text[144];

	gPoolBreak = false;

	if(gPoolShotMask & (1 << POOL_EIGHT))
	{
		if(group == POOL_GROUP_NONE)
		{
			Pool_Broadcast(POOL_TXT_RERACK);
			Pool_ResetBalls(true);
			gPoolBreak = true;
			gPoolTurn = foul ? other : shooter;
			return Pool_RefreshHud();
		}
		if(foul || Pool_GroupBefore(group) > 0)
		{
			return Pool_EndMatch(other, POOL_TXT_WIN_ILLEGAL);
		}
		return Pool_EndMatch(shooter, POOL_TXT_WIN_EIGHT);
	}

	if(group == POOL_GROUP_NONE && !wasBreak && !foul && gPoolFirstPocket != POOL_NONE)
	{
		group = Pool_BallGroup(gPoolFirstPocket);
		gPoolPlayer[shooter][ppGroup] = group;
		gPoolPlayer[other][ppGroup] = (group == POOL_GROUP_SOLID) ? POOL_GROUP_STRIPE : POOL_GROUP_SOLID;
		GetPlayerName(shooter, name, sizeof name);
		format(text, sizeof text, POOL_TXT_GROUP, name, Pool_GroupName[group]);
		Pool_Broadcast(text);
	}

	if(foul)
	{
		GetPlayerName(shooter, name, sizeof name);
		format(text, sizeof text, POOL_TXT_FOUL, name, Pool_FoulName[foul]);
		Pool_Broadcast(text);
	}
	else if(wasBreak)
	{
		keep = (gPoolFirstPocket != POOL_NONE);
	}
	else
	{
		keep = (group != POOL_GROUP_NONE && Pool_MaskHasGroup(group));
	}

	if(!keep)
	{
		gPoolTurn = other;
	}
	return Pool_RefreshHud();
}

static Pool_EndTurn()
{
	new shooter = gPoolShooter;
	if(shooter == POOL_NONE)
	{
		return 0;
	}

	gPoolShooter = POOL_NONE;
	SetTimerEx("Pool_RestoreTurn", 800, false, "i", shooter);
	if(gPoolMatch)
	{
		return Pool_JudgeShot(shooter);
	}
	return 1;
}

static Pool_SinkBall(ball, pocket)
{
	new Float:vx, Float:vy, Float:vz, obj = gPoolBall[ball][pbObject];

	PHY_GetObjectVelocity(obj, vx, vy, vz);
	PHY_DeleteObject(obj);
	gPoolBall[ball][pbActive] = false;
	MoveObject(obj, Pool_Pockets[pocket][0], Pool_Pockets[pocket][1], POOL_TABLE_Z - 0.25, floatsqroot(vx * vx + vy * vy) + 0.2);

	Pool_Sound(POOL_SND_POCKET + random(3));
	gPoolShotMask |= (1 << ball);
	if(gPoolFirstPocket == POOL_NONE && Pool_BallGroup(ball) != POOL_GROUP_NONE)
	{
		gPoolFirstPocket = ball;
	}
	if(!gPoolMatch)
	{
		Pool_AddScore((ball == POOL_CUE_BALL) ? -1 : 1);
	}
	Pool_RefreshHud();

	if(!gPoolMatch && Pool_CountBalls() <= 1)
	{
		return Pool_Finish();
	}
	if(Pool_AllStopped())
	{
		return Pool_EndTurn();
	}
	return 1;
}

public Pool_RestoreTurn(playerid)
{
	if(gPoolStarted && !gPoolBall[POOL_CUE_BALL][pbActive])
	{
		Pool_SpawnBall(POOL_CUE_BALL, true);
	}
	if(gPoolAimer == playerid || !IsPlayerConnected(playerid))
	{
		return 0;
	}

	GameTextForPlayer(playerid, " ", 100000, 4);
	return Pool_RestoreView(playerid);
}

public Pool_RestoreWeapon(playerid)
{
	gPoolPlayer[playerid][ppChalking] = false;
	RemovePlayerAttachedObject(playerid, POOL_ATTACH_SLOT);
	GivePlayerWeapon(playerid, 7, 1);
	return ApplyAnimation(playerid, "CARRY", "crry_prtial", 1.0, 0, 0, 0, 0, 0, 1);
}

public Pool_Tick()
{
	if(!gPoolStarted)
	{
		return 0;
	}
	if(gPoolAimer != POOL_NONE)
	{
		Pool_TickAim(gPoolAimer);
	}

	if(gPoolShooter == POOL_NONE)
	{
		Pool_FreezeStrayBalls();
	}
	else if(Pool_AllStopped())
	{
		Pool_EndTurn();
	}

	foreach (new i : Player)
	{
		if(gPoolPlayer[i][ppPlaying])
		{
			Pool_GuardTable(i);
		}
	}
	return 1;
}

static Pool_Enroll(playerid)
{
	gPoolPlayer[playerid][ppPlaying] = true;
	gPoolPlayer[playerid][ppScore] = 0;
	gPoolPlayer[playerid][ppGroup] = POOL_GROUP_NONE;
	gPoolPlayer[playerid][ppInviteTime] = 0;
	gPoolPlayer[playerid][ppBlocked] = false;
	gPoolPlayer[playerid][ppFireHeld] = false;
	gPoolPlayer[playerid][ppHasSafe] = false;
	GivePlayerWeapon(playerid, 7, 1);
	return PlayerPlaySound(playerid, 1085, 0.0, 0.0, 0.0);
}

static Pool_Join(playerid)
{
	if(gPoolMatch)
	{
		return Pool_Say(playerid, POOL_TXT_TABLE_MATCH);
	}

	Pool_Enroll(playerid);
	if(!gPoolStarted)
	{
		gPoolStarted = true;
		Pool_ResetBalls(true);
	}
	return Pool_RefreshHud();
}

static bool:Pool_TableFree(a, b)
{
	if(gPoolMatch)
	{
		return false;
	}
	foreach (new i : Player)
	{
		if(gPoolPlayer[i][ppPlaying] && i != a && i != b)
		{
			return false;
		}
	}
	return true;
}

static bool:Pool_HasInvite(playerid, from)
{
	new time = gPoolPlayer[playerid][ppInviteTime];
	return gPoolPlayer[playerid][ppInviter] == from && time && (GetTickCount() - time) < POOL_INVITE_MS;
}

static Pool_StartMatch(a, b)
{
	new name[MAX_PLAYER_NAME], text[144];

	Pool_Stop();
	gPoolPlayerOne = a;
	gPoolPlayerTwo = b;
	gPoolMatch = true;
	gPoolStarted = true;
	gPoolBreak = true;
	gPoolTurn = random(2) ? a : b;

	Pool_Enroll(a);
	Pool_Enroll(b);
	Pool_ResetBalls(true);
	Pool_RefreshHud();

	GetPlayerName(gPoolTurn, name, sizeof name);
	format(text, sizeof text, POOL_TXT_START, name);
	return Pool_Broadcast(text);
}

static Pool_Invite(playerid, targetid)
{
	if(targetid == playerid || !IsPlayerConnected(targetid))
	{
		return Pool_Say(playerid, POOL_TXT_BAD_ID);
	}
	if(!Pool_TableFree(playerid, targetid))
	{
		return Pool_Say(playerid, POOL_TXT_TABLE_BUSY);
	}
	if(Pool_HasInvite(playerid, targetid))
	{
		return Pool_StartMatch(targetid, playerid);
	}

	new name[MAX_PLAYER_NAME], text[144];
	gPoolPlayer[targetid][ppInviter] = playerid;
	gPoolPlayer[targetid][ppInviteTime] = GetTickCount();

	GetPlayerName(targetid, name, sizeof name);
	format(text, sizeof text, POOL_TXT_INVITED, name);
	Pool_Say(playerid, text);

	GetPlayerName(playerid, name, sizeof name);
	format(text, sizeof text, POOL_TXT_INVITE_GOT, name, playerid);
	return Pool_Say(targetid, text);
}

stock Pool_Leave(playerid)
{
	gPoolPlayer[playerid][ppInviteTime] = 0;
	gPoolPlayer[playerid][ppFireHeld] = false;
	if(!gPoolPlayer[playerid][ppPlaying])
	{
		return 0;
	}

	if(gPoolAimer == playerid)
	{
		Pool_CancelAim(playerid);
	}
	if(gPoolPlayer[playerid][ppBlocked])
	{
		gPoolPlayer[playerid][ppBlocked] = false;
		TogglePlayerControllable(playerid, 1);
	}
	Pool_HideBox(playerid);
	if(gPoolMatch)
	{
		return Pool_EndMatch(Pool_Opponent(playerid), POOL_TXT_WIN_LEFT);
	}

	gPoolPlayer[playerid][ppPlaying] = false;
	if(Pool_CountPlayers() > 0)
	{
		return 1;
	}
	return Pool_Stop();
}

stock Pool_Command(playerid, const params[])
{
	if(!params[0])
	{
		if(gPoolPlayer[playerid][ppPlaying])
		{
			return Pool_Leave(playerid);
		}
		return Pool_Join(playerid);
	}
	if(params[0] < '0' || params[0] > '9')
	{
		return Pool_Say(playerid, POOL_TXT_USAGE);
	}
	return Pool_Invite(playerid, strval(params));
}

stock Pool_Init()
{
	for(new i; i < POOL_BALLS; i++)
	{
		gPoolBall[i][pbObject] = INVALID_OBJECT_ID;
	}
	Pool_ResetBalls(false);
	Pool_CreateHud();

	for(new i; i < sizeof Pool_Corners; i++)
	{
		new next = (i + 1) % sizeof Pool_Corners;
		PHY_SetWallWorld(PHY_CreateWall(Pool_Corners[i][0], Pool_Corners[i][1], Pool_Corners[next][0], Pool_Corners[next][1]), POOL_WORLD);
	}
	SetTimer("Pool_Tick", POOL_TICK_MS, true);
	return 1;
}

stock Pool_OnPlayerSpawn(playerid)
{
	ApplyAnimation(playerid, "POOL", "null", 0.0, 0, 0, 0, 0, 0);
	return Pool_Leave(playerid);
}

stock Pool_OnPlayerKeyStateChange(playerid, newkeys, oldkeys)
{
	if(Pool_KeyPressed(KEY_FIRE, newkeys, oldkeys))
	{
		gPoolPlayer[playerid][ppFireHeld] = true;
		Pool_BlockAttack(playerid);
	}
	if(Pool_KeyReleased(KEY_FIRE, newkeys, oldkeys))
	{
		gPoolPlayer[playerid][ppFireHeld] = false;
		Pool_Shoot(playerid);
	}

	if(!gPoolStarted || !gPoolPlayer[playerid][ppPlaying])
	{
		return 0;
	}
	if(Pool_KeyReleased(KEY_SECONDARY_ATTACK, newkeys, oldkeys))
	{
		Pool_Chalk(playerid);
	}
	if(Pool_KeyReleased(KEY_JUMP, newkeys, oldkeys))
	{
		Pool_CycleCamera(playerid);
	}
	if(Pool_KeyReleased(KEY_HANDBRAKE, newkeys, oldkeys))
	{
		Pool_ToggleAim(playerid);
	}
	return 1;
}

stock Pool_OnObjectUpdate(objectid)
{
	if(!gPoolStarted)
	{
		return 0;
	}

	new ball = Pool_FindBall(objectid);
	if(ball == POOL_NONE || !PHY_IsObjectMoving(objectid))
	{
		return 0;
	}

	new pocket = Pool_GetPocket(ball);
	if(pocket == POOL_NONE)
	{
		return 0;
	}
	return Pool_SinkBall(ball, pocket);
}

stock Pool_OnObjectCollideWithObject(object1, object2)
{
	if(!gPoolStarted)
	{
		return 0;
	}

	new a = Pool_FindBall(object1), b = Pool_FindBall(object2);
	if(a == POOL_NONE && b == POOL_NONE)
	{
		return 0;
	}

	if(gPoolFirstHit == POOL_NONE)
	{
		if(a == POOL_CUE_BALL)
		{
			gPoolFirstHit = b;
		}
		else if(b == POOL_CUE_BALL)
		{
			gPoolFirstHit = a;
		}
	}
	return Pool_Sound(POOL_SND_BALL + random(3));
}

stock Pool_OnObjectCollideWithWall(objectid, wallid)
{
	#pragma unused wallid
	if(!gPoolStarted || Pool_FindBall(objectid) == POOL_NONE)
	{
		return 0;
	}
	return Pool_Sound(POOL_SND_WALL);
}
