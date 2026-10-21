#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

// 지형 검증 콘솔 명령(Cozy.Terrain*Verify)이 같이 쓰는 조건·기록 (격리된 검증 실행에서만 동작)
#if !UE_BUILD_SHIPPING
namespace CozyTerrainTest
{
	/** -CozyTerrainTest로 시험용 저장 폴더(TerrainPlayUser·TerrainPackageUser)에서 실행 중인 게임 월드인지 · bRequireValidationMap이면 L_TerrainValidation 맵만 */
	inline bool IsAllowed(const UWorld* World, bool bRequireValidationMap = true)
	{
		const FString Folder = FPaths::ProjectSavedDir();
		return World && World->IsGameWorld()
			&& (!bRequireValidationMap || World->GetMapName().Contains(TEXT("L_TerrainValidation")))
			&& FParse::Param(FCommandLine::Get(), TEXT("CozyTerrainTest"))
			&& (Folder.Contains(TEXT("TerrainPlayUser")) || Folder.Contains(TEXT("TerrainPackageUser")));
	}

	/** 검사 한 줄을 "통과: 이름" / "실패: 이름"으로 Report에 쌓고 실패 수를 센다 */
	inline auto MakeCheck(FString& Report, int32& Failed)
	{
		return [&Report, &Failed](bool Passed, const TCHAR* Name)
		{
			Report += FString::Printf(TEXT("%s: %s\n"), Passed ? TEXT("통과") : TEXT("실패"), Name);
			if (!Passed) ++Failed;
		};
	}
}
#endif
