#include "../Public/sfSessionInfo.h"
#include "../Public/SceneFusion.h"

sfSessionInfo::sfSessionInfo() :
    ProjectName{ "Default" },
    Creator{ "Test" },
    UnrealProjectName{ "Project" },
    LevelName{ "Level" },
    LaunchApplication{ "Unreal" },
    CanStop{ false },
    RequiredVersion{ SceneFusion::Version() },
    RoomInfoPtr{ nullptr }
{
    Time = FDateTime::Now();
    StartTime = FDateTime::Now();
}