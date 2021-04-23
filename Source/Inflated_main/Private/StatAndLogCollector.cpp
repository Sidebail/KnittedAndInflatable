// Fill out your copyright notice in the Description page of Project Settings.


#include "StatAndLogCollector.h"

bool UStatAndLogCollector::SendStatToSteam(FString SteamStat_Name)
{
    // Initialize Steam API
    bool bIsSteamInitialized  = SteamAPI_Init();
    
    /**
    *	These are stats from Knitted and Inflatable Steam Stats dashboard
    *	Please, refer to https://partner.steamgames.com/apps/stats/1324840#stat4_edit
    */
    Stat_t steamStats[] =
        {
        _STAT_ID( 1, STAT_INT, "NUM_ATTEMPT"),
        _STAT_ID( 2, STAT_INT, "NUM_DEATHS"),
        _STAT_ID( 3, STAT_INT, "NUM_COINS"),
        };
    
    // Global access to Stats Object
    CSteamStats* g_SteamStats = NULL;

    // Initializing the stats array
    if(bIsSteamInitialized)
    {
        g_SteamStats = new CSteamStats(steamStats, 3);
    }else
    {
        UE_LOG(LogTemp, Error, TEXT("SatAndLogCollector: SteamAPI has been not initialized!"));
        return false;
    }
    
    SteamAPI_RunCallbacks();

    if (g_SteamStats)
        g_SteamStats->StoreStats();

    if (g_SteamStats)
    {
        delete g_SteamStats;
        return true;
    }
    return false;  
}
