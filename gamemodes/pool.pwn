/*
 *  Pool demo gamemode - shows how pool.inc plugs into the main callbacks.
 *  Needs: a_samp, physics.inc (+ foreach, modelsizes) and include/pool.inc on the include path.
 *  Build: pawncc gamemodes/pool.pwn -iinclude -i<path to physics/foreach/modelsizes> -ogamemodes/pool.amx
 *
 *  License: GPL-3.0-or-later (see LICENSE). Based on the pool demo and physics.inc by Peppe.
 */

#include <a_samp>
#include <physics>
#include <pool>

#define dcmd(%1,%2,%3) if ((strcmp((%3)[1], #%1, true, (%2)) == 0) && ((((%3)[(%2) + 1] == 0) && (dcmd_%1(playerid, ""))) || (((%3)[(%2) + 1] == 32) && (dcmd_%1(playerid, (%3)[(%2) + 2]))))) return 1

main() {}

public OnGameModeInit()
{
	SetGameModeText("Objects Physics - Pool Demo");
	AddPlayerClass(0, 2442.1621, 2059.5051, 10.8203, 269.1425, 0, 0, 0, 0, 0, 0);
	Pool_Init();
	return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
	SetPlayerPos(playerid, 1958.3783, 1343.1572, 15.3746);
	SetPlayerCameraPos(playerid, 1958.3783, 1343.1572, 15.3746);
	SetPlayerCameraLookAt(playerid, 1958.3783, 1343.1572, 15.3746);
	return 1;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
	dcmd(pool, 4, cmdtext);
	return 0;
}

dcmd_pool(playerid, const params[])
{
	Pool_Command(playerid, params);
	return 1;
}

public OnPlayerSpawn(playerid)
{
	Pool_OnPlayerSpawn(playerid);
	return 1;
}

public OnPlayerDeath(playerid, killerid, reason)
{
	Pool_Leave(playerid);
	return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
	Pool_Leave(playerid);
	return 1;
}

public OnPlayerInteriorChange(playerid, newinteriorid, oldinteriorid)
{
	Pool_Leave(playerid);
	return 1;
}

public OnPlayerKeyStateChange(playerid, newkeys, oldkeys)
{
	Pool_OnPlayerKeyStateChange(playerid, newkeys, oldkeys);
	return 1;
}

public PHY_OnObjectUpdate(objectid)
{
	Pool_OnObjectUpdate(objectid);
	return 1;
}

public PHY_OnObjectCollideWithObject(object1, object2)
{
	Pool_OnObjectCollideWithObject(object1, object2);
	return 1;
}

public PHY_OnObjectCollideWithWall(objectid, wallid)
{
	Pool_OnObjectCollideWithWall(objectid, wallid);
	return 1;
}
