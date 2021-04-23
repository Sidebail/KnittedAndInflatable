// Fill out your copyright notice in the Description page of Project Settings.


#include "SteamStats.h"

SteamStats::SteamStats()
{
    
}
/**
 * PARAMETER: The constructor takes a pointer to an array of stats along with the length of the array.
 * The formatting of that array will be covered in the main game code later.
 *
 * RETURNS N/A
 *
 * WHAT IT DOES The constructor initializes a number of members along with grabbing the app ID
 * we are currently running as. In addition it hooks up the call back methods to handle
 * asynchronous calls made to Steam. Finally it makes an initial call to RequestStats()
 * to get stats and achievements for the current user.
 */
CSteamStats::CSteamStats(Stat_t *Stats, int NumStats) :
 m_iAppID( 0 ),
 m_bInitialized( false ),
 m_CallbackUserStatsReceived( this, &CSteamStats::OnUserStatsReceived ),
 m_CallbackUserStatsStored( this, &CSteamStats::OnUserStatsStored )
{
    m_iAppID = SteamUtils()->GetAppID();
    m_pStats = Stats;
    m_iNumStats = NumStats;
    RequestStats();
    
}

CSteamStats::~CSteamStats()
{
}

/**
 * PARAMETERS - None
 * 
 * RETURNS - a bool representing if the call succeeded or not. If the call failed then most likely Steam is not initialized.
 * Make sure you have a steam client open when you try to make this call and that SteamAPI_Init has been called before it.
 * 
 * WHAT IT DOES - This method basically wraps a call to ISteamUserStats::RequestCurrentStats which is an asynchronous
 * call to steam requesting the stats of the current user. This call needs to be made before you can set any stats or
 * achievements. The initial call to this method is made in the constructor.
 * You can call it again any time after that if you want to check on updated stats or achievements.
 */
bool CSteamStats::RequestStats()
{
    // Is Steam loaded? If not we can't get stats.
    if ( NULL == SteamUserStats() || NULL == SteamUser() )
    {
        return false;
    }
    // Is the user logged on?  If not we can't get stats.
    if ( !SteamUser()->BLoggedOn() )
    {
        return false;
    }
    // Request user stats.
    return SteamUserStats()->RequestCurrentStats();
}

/**
 * PARAMETERS - None
 *
 * RETURNS - a bool representing if the call succeeded or not.
 * If the call failed then most likely Steam is not initialized.
 * Make sure you have a steam client open when you try to make this call and that SteamAPI_Init has been called before it.
 *
 * WHAT IT DOES - This method basically wraps a call to ISteamUserStats::StoreStats which is an asynchronous call
 * to steam that stores the stats of the current user on the server. This call needs to be made anytime you want
 * to update the stats of the user.
 */
bool CSteamStats::StoreStats()
{
    if ( m_bInitialized )
    {
        // load stats
        for ( int iStat = 0; iStat < m_iNumStats; ++iStat )
        {
            Stat_t &stat = m_pStats[iStat];
            switch (stat.m_eStatType)
            {
            case STAT_INT:
                SteamUserStats()->SetStat( stat.m_pchStatName, stat.m_iValue );
                break;

            case STAT_FLOAT:
                SteamUserStats()->SetStat( stat.m_pchStatName, stat.m_flValue );
                break;

            case STAT_AVGRATE:
                SteamUserStats()->UpdateAvgRateStat(stat.m_pchStatName, stat.m_flAvgNumerator, stat.m_flAvgDenominator );
                // The averaged result is calculated for us
                SteamUserStats()->GetStat(stat.m_pchStatName, &stat.m_flValue );
                break;

            default:
                break;
            }
        }

        return SteamUserStats()->StoreStats();
    }else
    {
        UE_LOG(LogTemp, Error, TEXT("STEAM STATS: Failed to store, since Stats are NOT INITIALIZED!"));
        return  false;
    }
}

/**
 * PARAMETERS - User Received Callback Pointer
 *
 * RETURNS - Nothing
 *
 * WHAT IT DOES - This method is a callback that is called anytime you attempt to request stats.
 * Stats are requested by using RequestStats().
 * The method updates the member variable m_Stats to reflect the latest stats data returned from Steam.
 *
 */
void CSteamStats::OnUserStatsReceived( UserStatsReceived_t *pCallback )
{
    // we may get callbacks for other games' stats arriving, ignore them
    if ( m_iAppID == pCallback->m_nGameID )
    {
        if ( k_EResultOK == pCallback->m_eResult )
        {
            UE_LOG(LogTemp, Verbose, TEXT("STEAM STATS: Received stats and achievements from Steam"));
            // load stats
            for ( int iStat = 0; iStat < m_iNumStats; ++iStat )
            {
                Stat_t &stat = m_pStats[iStat];
                switch (stat.m_eStatType)
                {
                case STAT_INT:
                    SteamUserStats()->GetStat(stat.m_pchStatName, &stat.m_iValue);
                    break;

                case STAT_FLOAT:
                case STAT_AVGRATE:
                    SteamUserStats()->GetStat(stat.m_pchStatName, &stat.m_flValue);
                    break;

                default:
                    break;
                }
            }
            m_bInitialized = true;
        }
        else
        {
            char buffer[128];
            _snprintf( buffer, 128, "RequestStats - failed, %d\n", pCallback->m_eResult );
            //TODO UE_LOG(LogTemp, Verbose, TEXT("%s", buffer));
        }
    }
}

/**
 * PARAMETERS - N/A
 *
 * RETURNS - Nothing
 *
 * WHAT IT DOES - This method is a callback that is called anytime you attempt to store stats on Steam.
 * If any of the stats that we tried to set broke a constraint they will be
 * reverted to their old value so we reload their values.
 */
void CSteamStats::OnUserStatsStored( UserStatsStored_t *pCallback )
{
    // we may get callbacks for other games' stats arriving, ignore them
    if ( m_iAppID == pCallback->m_nGameID )
    {
        if ( k_EResultOK == pCallback->m_eResult )
        {
            UE_LOG(LogTemp, Verbose, TEXT("StoreStats - success"));
        }
        else if ( k_EResultInvalidParam == pCallback->m_eResult )
        {
            // One or more stats we set broke a constraint. They've been reverted,
            // and we should re-iterate the values now to keep in sync.
            UE_LOG(LogTemp, Verbose, TEXT("StoreStats - some failed to validate"));
            // Fake up a callback here so that we re-load the values.
            UserStatsReceived_t callback;
            callback.m_eResult = k_EResultOK;
            callback.m_nGameID = m_iAppID;
            OnUserStatsReceived( &callback );
        }
        else
        {
            char buffer[128];
            _snprintf( buffer, 128, "StoreStats - failed, %d\n", pCallback->m_eResult );
            //TODO UE_LOG(LogTemp, Verbose, TEXT("%s", buffer));
        }
    }
}

SteamStats::~SteamStats()
{
}
