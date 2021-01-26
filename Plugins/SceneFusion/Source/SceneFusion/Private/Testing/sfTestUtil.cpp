#include "sfTestUtil.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfConfig.h"

#include <Log.h>

TSharedPtr<sfMonkey> sfTestUtil::m_monkeyPtr;
IConsoleCommand* sfTestUtil::m_monkeyCommandPtr = nullptr;
TSharedPtr<sfTimer> sfTestUtil::m_timerPtr;
IConsoleCommand* sfTestUtil::m_timerCommandPtr = nullptr;
TSharedPtr<sfAction> sfTestUtil::m_actionPtr;
IConsoleCommand* sfTestUtil::m_logDebugCommandPtr = nullptr;

#define LOG_CHANNEL "sfTestUtil"

void sfTestUtil::RegisterCommands()
{
    m_monkeyCommandPtr = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("SFMonkey"),
        TEXT("Usage: SFMonkey [options][activity]. They monkey randomly performs activities that can be configured. "
            "Each activity has a weight effecting its chance of being chosen. If no arguments are given, toggles the "
            "monkey on or off. If at least one argument is provided, starts the monkey."
            "Options:\n"
            "  -r | -reset: Sets all activity weights to 0 and clears all activity configuration.\n"
            "  -d | -default: Restores activities to their default configuration and weights.\n"
            "  [activity]=[number]: Sets the weight of an activity.\n"
            "Activities:\n"
            "  spawn [options|paths]: Randomly adds assets to the level. At least one path relative to /Game/ must be "
                "provided to tell the monkey where to look for assets. -r or -reset will clear the paths."
            "  delete: Randomly deletes actors from the level.\n"
            "  move: Moves random actors in a random direction.\n"
            "  rename: Randomly renames actors.\n"
            "  parent: Randomly reparents actors.\n"
            "  addFoliage: Randomly adds foliage. You must have at least one foliage type in your level.\n"
            "  removeFoliage: Randomly removes foliage.\n"
            "  connect: [host port] connects to a session if not connected, otherwise disconnects. The host and port "
                "to connect to can be configured and by default are localhost:8000"
        ),
        FConsoleCommandWithArgsDelegate::CreateStatic(&sfTestUtil::Monkey));

    m_timerCommandPtr = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("SFRun"),
        TEXT("Usage: SFRun [-at time] action [action args]. Run the given action."
        "  -at time: Sets time to run the action. The time format should be YYYY.MM.DD-HH.MM.SS or HH.MM.SS\n"
        ),
        FConsoleCommandWithArgsDelegate::CreateStatic(&sfTestUtil::Run));

    m_logDebugCommandPtr = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("SFLogDebug"),
        TEXT("Usage: SFLogDebug [true|false|on|off|1|0]. Enables or disables logging debug messages to console. If no "
            "arguments are provided, toggles debug logging."
        ),
        FConsoleCommandWithArgsDelegate::CreateStatic(&sfTestUtil::LogDebug));
}

void sfTestUtil::CleanUp()
{
    if (m_monkeyPtr.IsValid())
    {
        m_monkeyPtr->Stop();
        m_monkeyPtr.Reset();
    }
    IConsoleManager::Get().UnregisterConsoleObject(m_monkeyCommandPtr);

    if (m_actionPtr.IsValid())
    {
        m_actionPtr.Reset();
    }
    if (m_timerPtr.IsValid())
    {
        m_timerPtr.Reset();
    }
    IConsoleManager::Get().UnregisterConsoleObject(m_timerCommandPtr);

    IConsoleManager::Get().UnregisterConsoleObject(m_logDebugCommandPtr);
}

void sfTestUtil::CombineQuotedArgs(const TArray<FString>& inArgs, TArray<FString>& outArgs)
{
    FString quotedArg;
    bool inQuotes = false;
    for (const FString& arg : inArgs)
    {
        if (!inQuotes)
        {
            if (!arg.StartsWith("\""))
            {
                outArgs.Add(arg);
                continue;
            }
            if (arg.EndsWith("\""))
            {
                // Remove opening and closing quotes
                outArgs.Add(arg.Mid(1, arg.Len() - 2));
                continue;
            }
            quotedArg = arg.RightChop(1);// remove opening quote
            inQuotes = true;
        }
        else
        {
            if (!arg.EndsWith("\""))
            {
                quotedArg += " " + arg;
                continue;
            }
            quotedArg += " " + arg.LeftChop(1); // remove closing quote
            outArgs.Add(quotedArg);
            inQuotes = false;
        }
    }
}

void sfTestUtil::Monkey(const TArray<FString>& args)
{
    if (!m_monkeyPtr.IsValid())
    {
        m_monkeyPtr = MakeShareable(new sfMonkey());
    }
    if (args.Num() == 0)
    {
        if (m_monkeyPtr->IsRunning())
        {
            m_monkeyPtr->Stop();
        }
        else
        {
            m_monkeyPtr->Start();
        }
    }
    else
    {
        TArray<FString> parsedArgs;
        CombineQuotedArgs(args, parsedArgs);
        for (int i = 0; i < parsedArgs.Num(); i++)
        {
            const FString& arg = parsedArgs[i];
            if (arg.Equals("-r", ESearchCase::IgnoreCase) || arg.Equals("-reset", ESearchCase::IgnoreCase))
            {
                m_monkeyPtr->Reset();
                continue;
            }
            if (arg.Equals("-d", ESearchCase::IgnoreCase) || arg.Equals("-default", ESearchCase::IgnoreCase))
            {
                m_monkeyPtr->UseDefaults();
                continue;
            }
            int index = arg.Find("=");
            if (index >= 0)
            {
                FString name = arg.Left(index);
                TSharedPtr<sfBaseActivity> activityPtr = m_monkeyPtr->GetActivity(name);
                if (activityPtr.IsValid())
                {
                    FString weight = arg.RightChop(index + 1);
                    activityPtr->Weight() = FCString::Atof(*weight);
                    continue;
                }
            }
            else
            {
                TSharedPtr<sfBaseActivity> activityPtr = m_monkeyPtr->GetActivity(arg);
                if (activityPtr.IsValid())
                {
                    activityPtr->HandleArgs(parsedArgs, i + 1);
                    break;
                }
            }
            KS::Log::Warning("Unknown SFMonkey command arg " + std::string(TCHAR_TO_UTF8(*arg)), LOG_CHANNEL);
        }
        m_monkeyPtr->Start();
    }
    if (m_monkeyPtr->IsRunning())
    {
        KS::Log::Info("Monkey is on.", LOG_CHANNEL);
    }
    else
    {
        KS::Log::Info("Monkey is off.", LOG_CHANNEL);
    }
}

void sfTestUtil::Run(const TArray<FString>& args)
{
    if (!m_timerPtr.IsValid())
    {
        m_timerPtr = MakeShareable(new sfTimer());
    }
    if (!m_actionPtr.IsValid())
    {
        m_actionPtr = MakeShareable(new sfAction());
    }
    if (args.Num() == 0)
    {
        m_timerPtr->StopAll();
    }
    else
    {
        TArray<FString> parsedArgs;
        CombineQuotedArgs(args, parsedArgs);
        FString actionName;

        if (parsedArgs[0].Equals("-at", ESearchCase::IgnoreCase) && parsedArgs.Num() >= 3)
        {
            FString timeStr = parsedArgs[1];
            actionName = parsedArgs[2];

            sfAction::Action action = m_actionPtr->Get(actionName);
            if (action == nullptr)
            {
                return;
            }

            FDateTime dateTime;
            if (timeStr.Len() == 8) //hh.mm.ss
            {
                timeStr.InsertAt(0, FDateTime::Now().ToString().Left(11));
            }
            if (!FDateTime::Parse(timeStr, dateTime))//yyyy.mm.dd-hh.mm.ss
            {
                KS::Log::Warning("Wrong date time format.", LOG_CHANNEL);
                return;
            }
            if (dateTime < FDateTime::Now())
            {
                KS::Log::Warning("Cannot execute an action in the past.", LOG_CHANNEL);
                return;
            }

            parsedArgs.RemoveAt(0, 3, true);
            m_timerPtr->StartTimer(action, parsedArgs, dateTime);
        }
        else
        {
            actionName = parsedArgs[0];
            sfAction::Action action = m_actionPtr->Get(actionName);
            if (action != nullptr)
            {
                parsedArgs.RemoveAt(0);
                action(parsedArgs);
            }
        }
    }
}

void sfTestUtil::LogDebug(const TArray<FString>& args)
{
    bool logDebug = !sfConfig::Get().LogDebug;
    if (args.Num() > 0)
    {
        const FString& arg = args[0];
        if (sfUnrealUtils::IsStringFalse(arg))
        {
            logDebug = false;
        }
        else if (sfUnrealUtils::IsStringTrue(arg))
        {
            logDebug = true;
        }
    }
    if (logDebug != sfConfig::Get().LogDebug)
    {
        sfConfig::Get().LogDebug = logDebug;
        sfConfig::Get().Save();
        KS::Log::Instance()->UnregisterHandler("Root", &SceneFusion::HandleLog);
        if (logDebug)
        {
            KS::Log::Instance()->RegisterHandler("Root", &SceneFusion::HandleLog, KS::LogLevel::LOG_ALL, true);
        }
        else
        {
            KS::Log::Instance()->RegisterHandler("Root", &SceneFusion::HandleLog, KS::LogLevel::LOG_ALL & ~KS::LogLevel::LOG_DEBUG, true);
        }
    }
    if (logDebug)
    {
        KS::Log::Info("Debug logging enabled", LOG_CHANNEL);
    }
    else
    {
        KS::Log::Info("Debug logging disabled", LOG_CHANNEL);
    }
}

#undef LOG_CHANNEL