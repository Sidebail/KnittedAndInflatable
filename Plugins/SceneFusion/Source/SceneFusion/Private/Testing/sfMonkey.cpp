#include "sfMonkey.h"
#include "sfSpawnActivity.h"
#include "sfDeleteActivity.h"
#include "sfMoveActivity.h"
#include "sfRenameActivity.h"
#include "sfParentActivity.h"
#include "sfConnectActivity.h"
#include "sfAddFoliageActivity.h"
#include "sfRemoveFoliageActivity.h"

#include <Log.h>
#include <Editor.h>
#include <Runtime/Core/Public/Containers/Ticker.h>
#include <Runtime/CoreUObject/Public/UObject/UObjectGlobals.h>

#define LOG_CHANNEL "sfMonkey"

sfMonkey::sfMonkey() :
    m_isRunning{ false }
{
    UseDefaults();
}

sfMonkey::~sfMonkey()
{
    
}

bool sfMonkey::IsRunning()
{
    return m_isRunning;
}

void sfMonkey::UseDefaults()
{
    m_activities.Empty();
    m_activities.Add(MakeShareable(new sfSpawnActivity("spawn", 1)));
    m_activities.Add(MakeShareable(new sfDeleteActivity("delete", 0.5f)));
    m_activities.Add(MakeShareable(new sfMoveActivity("move", 2)));
    m_activities.Add(MakeShareable(new sfRenameActivity("rename", 1)));
    m_activities.Add(MakeShareable(new sfParentActivity("parent", 1)));
    m_activities.Add(MakeShareable(new sfAddFoliageActivity("addFoliage", 0)));
    m_activities.Add(MakeShareable(new sfRemoveFoliageActivity("removeFoliage", 0)));
    m_activities.Add(MakeShareable(new sfConnectActivity("connect", 0)));
}

void sfMonkey::Reset()
{
    UseDefaults();
    for (TSharedPtr<sfBaseActivity> activityPtr : m_activities)
    {
        activityPtr->Weight() = 0.0f;
    }
}

TSharedPtr<sfBaseActivity> sfMonkey::GetActivity(const FString& name)
{
    for (TSharedPtr<sfBaseActivity> activityPtr : m_activities)
    {
        if (name.Equals(activityPtr->Name(), ESearchCase::IgnoreCase))
        {
            return activityPtr;
        }
    }
    return nullptr;
}

void sfMonkey::Start()
{
    if (!m_isRunning)
    {
        m_isRunning = true;
        m_tickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &sfMonkey::Tick),
            1.0f / 60.0f);
        m_onActorDeletedHandle = GEngine->OnLevelActorDeleted().AddRaw(this, &sfMonkey::OnActorDeleted);
    }
}

void sfMonkey::Stop()
{
    if (m_isRunning)
    {
        m_isRunning = false;
        FTicker::GetCoreTicker().RemoveTicker(m_tickHandle);
        GEngine->OnLevelActorDeleted().Remove(m_onActorDeletedHandle);
    }
}

bool sfMonkey::Tick(float deltaTime)
{
    if (!m_currentActivity.IsValid())
    {
        float totalWeight = 0.0f;
        for (TSharedPtr<sfBaseActivity> activityPtr : m_activities)
        {
            totalWeight += activityPtr->Weight();
        }
        float choice = FMath::FRand() * totalWeight;
        for (TSharedPtr<sfBaseActivity> activityPtr : m_activities)
        {
            choice -= activityPtr->Weight();
            if (choice < 0.0f)
            {
                m_currentActivity = activityPtr;
                activityPtr->Duration() = FMath::FRandRange(0.5f, 3.0f);
                activityPtr->Start();
                break;
            }
        }
    }
    else
    {
        m_currentActivity->Duration() -= deltaTime;
        if (m_currentActivity->Duration() > 0)
        {
            m_currentActivity->Tick(deltaTime);
        }
        else
        {
            m_currentActivity->Finish();
            m_currentActivity.Reset();
        }
    }
    return true;
}

void sfMonkey::OnActorDeleted(AActor* actorPtr)
{
    if (m_currentActivity.IsValid())
    {
        m_currentActivity->OnActorDeleted(actorPtr);
    }
}

#undef LOG_CHANNEL
