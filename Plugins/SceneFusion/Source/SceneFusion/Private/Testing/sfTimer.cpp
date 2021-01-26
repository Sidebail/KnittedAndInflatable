#include "sfTimer.h"

#include <Log.h>

#include <Containers/Ticker.h>


#define LOG_CHANNEL "sfTimer"

sfTimer::sfTimer()
{

}

sfTimer::~sfTimer()
{

}

FDelegateHandle sfTimer::StartTimer(sfAction::Action action, const TArray<FString>& args, FDateTime dateTime)
{
    FDelegateHandle handle = FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([action, args, dateTime](float time)->bool {
        if (FDateTime::Now() >= dateTime)
        {
            action(args);
            return false;
        }
        return true;
        }),
        1.0f / 60.0f);
    m_handles.Add(handle);
    return handle;
}

void sfTimer::StopTimer(FDelegateHandle handle)
{
    FTicker::GetCoreTicker().RemoveTicker(handle);
    m_handles.Remove(handle);
}

void sfTimer::StopAll()
{
    for(FDelegateHandle handle : m_handles)
    {
        FTicker::GetCoreTicker().RemoveTicker(handle);
    }
    m_handles.Empty();
}

#undef LOG_CHANNEL