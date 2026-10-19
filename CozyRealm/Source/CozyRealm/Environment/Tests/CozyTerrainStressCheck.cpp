#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Environment/CozySurfaceComponent.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/World.h"
#include "RenderingThread.h"

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld CozyTerrainStressCheck(
	TEXT("Cozy.TerrainStressVerify"), TEXT("격리된 검증 맵에서 타일 설치·삭제와 도색·Undo 200회 검사"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const FString Folder = FPaths::ProjectSavedDir();
		if (!World || !World->IsGameWorld() || !World->GetMapName().Contains(TEXT("L_TerrainValidation"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("CozyTerrainTest"))
			|| !(Folder.Contains(TEXT("TerrainPlayUser")) || Folder.Contains(TEXT("TerrainPackageUser")))) return;
		auto* Estate = World->GetSubsystem<UCozyEstateSubsystem>();
		if (!Estate) return;
		Estate->Tick(0.f);
		const FGuid HillId(0x10000000, 0, 0, 1);
		const FVector Position(900, 100, 420);
		FCozySurfaceHit Surface;
		if (Estate->FindFloorTile(Position, HillId) || !CozySurface::GroundAt(World, Position, Surface, &HillId) || !Surface.Rules.IsValid()) return;
		const int32 BeforeTiles = Estate->GetState().FloorTiles.Num();
		const FCozyPaintMask BeforePaint = Surface.Rules->GetPaintMask();
		int32 Failed = 0; uint64 WarmMemory = 0; uint64 FinalMemory = 0;
		for (int32 I = 0; I < 200; ++I)
		{
			FText Reason;
			if (!Estate->EditFloorTile(Position, I % 3, I % 4, false, Reason, &HillId)) ++Failed;
			if (!Estate->EditFloorTile(Position, 0, 0, true, Reason, &HillId)) ++Failed;
			Surface.Rules->BeginPaintStroke();
			Surface.Rules->PaintAt(Position, 100, I % 3 + 1, 1);
			Surface.Rules->EndPaintStroke();
			if (!Surface.Rules->UndoPaint() || Surface.Rules->GetPaintMask().Pixels != BeforePaint.Pixels) ++Failed;
			if (Estate->GetState().FloorTiles.Num() != BeforeTiles) ++Failed;
			// 업로드 임시 버퍼가 쌓인 채 측정되지 않도록 시험에서만 렌더 명령을 비운다.
			FlushRenderingCommands();
			if (I == 49) WarmMemory = FPlatformMemory::GetStats().UsedPhysical;
		}
		Estate->FlushPendingSave(); FlushRenderingCommands();
		FinalMemory = FPlatformMemory::GetStats().UsedPhysical;
		Surface.Rules->RestorePaint(BeforePaint); Estate->SaveEstate(TEXT("지형 반복 검사 종료")); Estate->FlushPendingSave();
		const double DeltaMB = (double(FinalMemory) - double(WarmMemory)) / (1024 * 1024);
		if (DeltaMB > 32) ++Failed;
		const FString Report = FString::Printf(TEXT("설치·삭제·도색·Undo 200회 / 타일 개수 %d → %d\n50회 후 %.2fMiB / 200회 후 %.2fMiB / 차이 %.2fMiB\n실패 합계: %d\n주의: 단일 프로세스의 짧은 반복 검사이며 장기 누수 보장은 아님\n"),
			BeforeTiles, Estate->GetState().FloorTiles.Num(), double(WarmMemory) / (1024 * 1024), double(FinalMemory) / (1024 * 1024), DeltaMB, Failed);
		FFileHelper::SaveStringToFile(Report, *(Folder / TEXT("TerrainStressCheck.txt")));
		UE_LOG(LogTemp, Display, TEXT("[지형 반복 검사]\n%s"), *Report);
	}));
#endif
