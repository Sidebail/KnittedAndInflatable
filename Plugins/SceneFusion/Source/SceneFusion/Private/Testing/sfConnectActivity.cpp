#include "sfConnectActivity.h"
#include "../../Public/SceneFusion.h"

sfConnectActivity::sfConnectActivity(const FString& name, float weight) :
    sfBaseActivity{ name, weight },
    m_host{ "localhost" },
    m_port{ 8000 }
{}

void sfConnectActivity::HandleArgs(const TArray<FString>& args, int index)
{
    if (index >= args.Num())
    {
        return;
    }
    int i = args[index].Find(":");
    if (i >= 0)
    {
        m_host = TCHAR_TO_UTF8(*args[index].Left(i));
        m_port = FCString::Atoi(*args[index].RightChop(i + 1));
    }
    else
    {
        m_host = TCHAR_TO_UTF8(*args[index]);
        if (args.Num() > index + 1)
        {
            m_port = FCString::Atoi(*args[index + 1]);
        }
    }
}

void sfConnectActivity::Start()
{
    if (SceneFusion::Service->Session() != nullptr && SceneFusion::Service->Session()->IsConnected())
    {
        SceneFusion::Service->LeaveSession();
    }
    else
    {
        TSharedPtr<sfSessionInfo> sessionInfoPtr = MakeShareable(new sfSessionInfo());
        sessionInfoPtr->RoomInfoPtr = ksRoomInfo::Create();
        sessionInfoPtr->RoomInfoPtr->Host() = m_host;
        sessionInfoPtr->RoomInfoPtr->Port() = m_port;
        SceneFusion::IsSessionCreator = false;
        SceneFusion::JoinSession(sessionInfoPtr);
    }
}