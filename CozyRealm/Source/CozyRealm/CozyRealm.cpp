// Copyright Epic Games, Inc. All Rights Reserved.

#include "CozyRealm.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, CozyRealm, "CozyRealm" );

DEFINE_LOG_CATEGORY(LogCozyRealm)

bool CozyDebug::IsEnabled()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	// 실행 인자는 실행 중에 바뀌지 않으므로 한 번만 읽음
	static const bool bCommandLine = FParse::Param(FCommandLine::Get(), TEXT("CozyDebugCommands"));
	return GIsEditor || bCommandLine;
#endif
}
