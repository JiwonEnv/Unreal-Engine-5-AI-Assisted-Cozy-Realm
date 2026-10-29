// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogCozyRealm, Log, All);

namespace CozyDebug
{
	/**
	 *  디버그 도구(F1 창 · 개발용 콘솔 명령 · 시험용 레시피)를 쓸 수 있는가.
	 *  출시(Shipping) 빌드에서는 항상 false · 그 밖에는 에디터(PIE·자동 시험) 또는 실행 인자 -CozyDebugCommands가 있을 때만 true.
	 *  (Development 패키지를 그냥 실행하면 꺼짐 · 출시 품질 점검 2026-10-30)
	 */
	COZYREALM_API bool IsEnabled();
}
