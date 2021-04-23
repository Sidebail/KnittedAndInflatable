// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ThirdParty/Steamworks/Steamv15/sdk/public/steam/steam_api.h"
#define _STAT_ID( id,type,name ) { id, type, name, 0, 0, 0, 0 }
/**
 * 	@author Vladimir Vatsurin
 * 	This class is created to maintain the SteamAPI Stats. Class uses Steam SDK
 * 	Part of the code is copied from Steamworks Documentation
 */

/**
*	We first define a structure to hold our stats data received from steam,
*	define stat types in a handy enum and then provide
*	a macro for creating stats objects.
*/
enum EStatTypes
{
    STAT_INT = 0,
    STAT_FLOAT = 1,
    STAT_AVGRATE = 2,
};
struct Stat_t
{
	int m_ID;
	EStatTypes m_eStatType;
	const char *m_pchStatName;
	int m_iValue;
	float m_flValue;
	float m_flAvgNumerator;
	float m_flAvgDenominator;
};

/**
* Next we define a helper class that will wrap all of the Steam Stats API
* calls as well as creating all of the Steam callbacks.
*/
class CSteamStats
{
private:
	int64 m_iAppID; // Our current AppID
	Stat_t *m_pStats; // Stats data
	int m_iNumStats; // The number of Stats
	bool m_bInitialized; // Have we called Request stats and received the callback?

	public:
	CSteamStats(Stat_t *Stats, int NumStats);
	~CSteamStats();

	bool RequestStats();
	bool StoreStats();

	STEAM_CALLBACK( CSteamStats, OnUserStatsReceived, UserStatsReceived_t,
        m_CallbackUserStatsReceived );
	STEAM_CALLBACK( CSteamStats, OnUserStatsStored, UserStatsStored_t,
        m_CallbackUserStatsStored );
		
};
////////////////////////////////////////////////////////////////////////////

class INFLATED_MAIN_API SteamStats
{
	
public:
	SteamStats();
	~SteamStats();
};
