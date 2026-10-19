#include "HAL/IConsoleManager.h"
#include "Environment/CozySurfaceComponent.h"
#include "Environment/CozySurfaceNavigation.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"

#if !UE_BUILD_SHIPPING
// 격리된 검증 실행에서만 사용한다. 일반 플레이 저장에는 접근하지 않는다.
static FAutoConsoleCommandWithWorld CozyTerrainWorldCheck(
	TEXT("Cozy.TerrainVerify"), TEXT("검증 맵의 실제 충돌·이동·도색·바닥 저장 흐름 검사"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const FString Saved = FPaths::ProjectSavedDir();
		if (!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("CozyTerrainTest")) || !World->GetMapName().Contains(TEXT("L_TerrainValidation"))
			|| !(Saved.Contains(TEXT("TerrainPlayUser")) || Saved.Contains(TEXT("TerrainPackageUser")))) return;
		auto* Estate = World->GetSubsystem<UCozyEstateSubsystem>();
		auto* Nav = World->GetSubsystem<UCozySurfaceNavigation>();
		if (!Estate || !Nav) return;
		Estate->Tick(0.f);
		FString Report; int32 Failed = 0;
		auto Check = [&](bool Passed, const TCHAR* Name)
		{
			Report += FString::Printf(TEXT("%s: %s\n"), Passed ? TEXT("통과") : TEXT("실패"), Name);
			if (!Passed) ++Failed;
		};
		const FGuid LandscapeId(0xC02E0001, 0, 0, 1), HillId(0x10000000, 0, 0, 1), UpperId(0x10000000, 0, 0, 3);
		Check(!Estate->GetState().Facilities.ContainsByPredicate([](const FCozyFacilityState& F) { return !F.bStored && !F.bHasSurfacePlacement; }), TEXT("이전 평지 시설에 공통 표면 참조 부여"));
		FCozySurfaceHit Lower, Upper, Hill; FText Reason;
		Check(CozySurface::GroundAt(World, FVector(-800, 0, 0), Lower, &LandscapeId), TEXT("겹친 위치 하부 지면 조회"));
		Check(CozySurface::GroundAt(World, FVector(-800, 0, 0), Upper, &UpperId) && Upper.Position.Z > Lower.Position.Z + 400, TEXT("겹친 위치 상부 분리"));
		Check(CozySurface::CheckFootprint(World, FVector(400, -200, 0), FVector2D(200), 50, Hill, Reason, &HillId), TEXT("언덕 위 평평한 전체 바닥 지지"));
		Check(!CozySurface::CheckFootprint(World, FVector(900, -200, 0), FVector2D(200), 50, Hill, Reason, &HillId), TEXT("언덕 끝에 걸친 시설 거부"));
		Check(!CozySurface::CheckFootprint(World, FVector(-200, -100, 0), FVector2D(200), 50, Hill, Reason), TEXT("경사로 위 시설 거부"));
		FCozySurfaceHit RampLow, RampHigh;
		const FGuid RampId(0x10000000, 0, 0, 2);
		Check(CozySurface::GroundAt(World, FVector(-350, 0, 0), RampLow, &RampId)
			&& CozySurface::GroundAt(World, FVector(150, 0, 0), RampHigh, &RampId)
			&& RampHigh.Position.Z > RampLow.Position.Z + 250, TEXT("검증 경사로가 언덕 방향으로 상승"));
		// 경사로 양 끝이 언덕 상판·평지와 턱 없이 맞닿는지 (예전 배치는 위쪽에 약 25cm 턱 · Claude 후속 2026-10-20)
		{
			FCozySurfaceHit RampTop, HillEdge, RampBottom, GroundEdge;
			const bool bTop = CozySurface::GroundAt(World, FVector(198, 0, 0), RampTop, &RampId) && CozySurface::GroundAt(World, FVector(202, 0, 0), HillEdge, &HillId);
			const bool bBottom = CozySurface::GroundAt(World, FVector(-486, 0, 0), RampBottom, &RampId) && CozySurface::GroundAt(World, FVector(-500, 0, 0), GroundEdge, &LandscapeId);
			const double TopStep = bTop ? HillEdge.Position.Z - RampTop.Position.Z : 999.0;
			const double BottomStep = bBottom ? RampBottom.Position.Z - GroundEdge.Position.Z : 999.0;
			Report += FString::Printf(TEXT("경사로 이음매 높이 차: 위(언덕-경사로) %.1fcm · 아래(경사로-평지) %.1fcm\n"), TopStep, BottomStep);
			Check(FMath::Abs(TopStep) <= 5.0, TEXT("경사로 위쪽 끝과 언덕 상판 사이 턱 5cm 이하"));
			Check(FMath::Abs(BottomStep) <= 8.0, TEXT("경사로 아래쪽 끝과 평지 사이 턱 8cm 이하"));
		}
		TArray<FVector> Path;
		const double Begin = FPlatformTime::Seconds();
		const bool RampPath = Nav->FindPath(FVector(-550, 150, 20), FVector(550, 150, 420), Path);
		Report += FString::Printf(TEXT("첫 경로 구축 %.2fms / 노드 %d / 경로점 %d\n"), (FPlatformTime::Seconds() - Begin) * 1000, Nav->GetNodeCount(), Path.Num());
		Check(RampPath, TEXT("경사로를 통한 언덕 접근"));
		Check(!Nav->FindPath(FVector(-550, 150, 20), FVector(-800, 0, 510), Path), TEXT("고립된 상판으로 순간 이동 경로 금지"));
		Check(!Nav->FindPath(FVector(-550, 150, 20), FVector(550, 150, 650), Path), TEXT("지면 없는 높이로 경로 보정 금지"));
		if (Upper.Rules.IsValid() && Lower.Rules.IsValid())
		{
			const FCozyPaintMask Original = Upper.Rules->GetPaintMask();
			const TArray<FColor> LowerBefore = Lower.Rules->GetPaintMask().Pixels;
			Upper.Rules->BeginPaintStroke(); Upper.Rules->PaintAt(Upper.Position, 100, 1, 1); Upper.Rules->EndPaintStroke();
			Check(LowerBefore == Lower.Rules->GetPaintMask().Pixels, TEXT("상판 도색 시 하부 표면 보존"));
			Check(Upper.Rules->UndoPaint() && Upper.Rules->GetPaintMask().Pixels == Original.Pixels, TEXT("실제 표면 도색 되돌리기"));
			Upper.Rules->RestorePaint(Original);
		}
		const FVector TilePosition(500, -100, 420);
		Check(Estate->EditFloorTile(TilePosition, 1, 1, false, Reason, &HillId), TEXT("언덕 바닥 타일 설치"));
		const FCozyFloorTile* Tile = Estate->FindFloorTile(TilePosition, HillId);
		const FGuid TileId = Tile ? Tile->InstanceId : FGuid();
		Check(TileId.IsValid() && Estate->StoreFloorTile(TileId, Reason), TEXT("바닥 타일 보관"));
		Check(Estate->EditFloorTile(TilePosition, 1, 2, false, Reason, &HillId, TileId), TEXT("같은 ID로 타일 꺼내기"));
		Estate->SaveEstate(TEXT("지형 통합 시험"));
		Estate->DebugReloadFromSave();
		Tile = Estate->FindFloorTile(TilePosition, HillId);
		Check(Tile && Tile->InstanceId == TileId && Tile->SurfaceId == HillId, TEXT("실제 파일 저장 후 타일 ID·표면 복원"));
		const FCozyFacilityState* Field = Estate->GetState().Facilities.FindByPredicate([](const FCozyFacilityState& F) { return F.DefinitionId == TEXT("Field"); });
		if (Field)
		{
			const FCozyFacilityState Before = *Field;
			Check(Estate->BeginPlacement(Before.InstanceId), TEXT("시설 이동 미리보기 시작"));
			Estate->SetPlacementSurface(HillId); Estate->UpdatePlacement(FIntPoint(8, 4), 0);
			Estate->CancelPlacement();
			Field = Estate->FindFacility(Before.InstanceId);
			Check(Field && Field->GridCoord == Before.GridCoord && Field->ActiveJobId == Before.ActiveJobId, TEXT("이동 취소 시 좌표·생산 작업 보존"));
			Estate->BeginPlacement(Before.InstanceId); Estate->SetPlacementSurface(HillId); Estate->UpdatePlacement(FIntPoint(8, 4), 0);
			const bool Placed = Estate->ConfirmPlacement(Reason);
			Check(Placed, TEXT("언덕 위 시설 이동 확정"));
			if (!Placed) Report += Reason.ToString() + TEXT("\n");
			Field = Estate->FindFacility(Before.InstanceId);
			Check(Field && Field->ActiveJobId == Before.ActiveJobId && Field->AssignedResidents == Before.AssignedResidents && Field->UnclaimedAmount == Before.UnclaimedAmount, TEXT("시설 이동 시 작업·주민·미수령 보존"));
			if (Placed)
			{
				Check(Estate->BeginPlacement(Before.InstanceId), TEXT("언덕 시설 다시 이동 시작"));
				Estate->SetPlacementSurface(LandscapeId); Estate->UpdatePlacement(Before.GridCoord, Before.Rotation);
				Check(Estate->ConfirmPlacement(Reason), TEXT("시설 원래 위치 복귀"));
			}
		}
		Report += FString::Printf(TEXT("실패 합계: %d\n"), Failed);
		FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TerrainWorldCheck.txt")));
		UE_LOG(LogTemp, Display, TEXT("[지형 통합 검사]\n%s"), *Report);
	}));
#endif
