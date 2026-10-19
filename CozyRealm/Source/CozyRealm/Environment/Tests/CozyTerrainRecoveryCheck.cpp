#include "HAL/IConsoleManager.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Estate/CozyEstateSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld CozyTerrainRecoveryCheck(
	TEXT("Cozy.TerrainRecoveryVerify"), TEXT("격리된 시험 저장의 이전 버전·누락 표면·손상·미래 버전 복구 검사"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const FString Folder = FPaths::ProjectSavedDir();
		if (!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("CozyTerrainTest"))
			|| !(Folder.Contains(TEXT("TerrainPlayUser")) || Folder.Contains(TEXT("TerrainPackageUser")))) return;
		auto* Estate = World->GetSubsystem<UCozyEstateSubsystem>();
		if (!Estate) return;
		Estate->SaveEstate(TEXT("복구 검사 시작")); Estate->FlushPendingSave();
		const FCozyEstateState Original = Estate->GetState();
		const FString SlotPath = Folder / TEXT("SaveGames/CozyRealm_Slot1.sav");
		FString Report; int32 Failed = 0;
		auto Check = [&](bool Passed, const TCHAR* Name)
		{
			Report += FString::Printf(TEXT("%s: %s\n"), Passed ? TEXT("통과") : TEXT("실패"), Name);
			if (!Passed) ++Failed;
		};
		auto WriteSlot = [&](const FCozyEstateState& State, int32 Version)
		{
			Estate->FlushPendingSave();
			auto* Save = Cast<UCozyEstateSaveGame>(UGameplayStatics::CreateSaveGameObject(UCozyEstateSaveGame::StaticClass()));
			Save->State = State; Save->FileVersion = Version; Save->SavedUtc = FDateTime::UtcNow();
			return UGameplayStatics::SaveGameToSlot(Save, TEXT("CozyRealm_Slot1"), 0);
		};
		auto Reload = [&]() { Estate->DebugReloadFromSave(); Estate->Tick(0.f); Estate->FlushPendingSave(); };
		Check(WriteSlot(Original, 1), TEXT("시험용 이전 버전 저장 생성")); Reload();
		Check(IFileManager::Get().FileExists(*(Folder / TEXT("SaveGames/CozyRealm_Slot1_before_terrain_v1.sav")))
			&& Estate->GetState().SaveVersion == 2, TEXT("이전 버전 원본 보관 후 버전 2 이관"));
		FCozyEstateState Missing = Original;
		FCozyFloorTile Tile; Tile.InstanceId = FGuid::NewGuid(); Tile.SurfaceId = FGuid::NewGuid(); Tile.Cell = FIntPoint(5, 5);
		Missing.FloorTiles.Add(Tile);
		FCozyFloorTile MissingAsset = Tile; MissingAsset.InstanceId = FGuid::NewGuid();
		MissingAsset.SurfaceId = FGuid(0xC02E0001, 0, 0, 1); MissingAsset.DefinitionId = TEXT("RemovedFloorAsset");
		Missing.FloorTiles.Add(MissingAsset);
		FCozyFacilityState MissingFacility; MissingFacility.InstanceId = FGuid::NewGuid(); MissingFacility.DefinitionId = TEXT("RemovedFacilityAsset");
		Missing.Facilities.Add(MissingFacility); Missing.TerrainRevision = 999;
		if (!Missing.PaintMasks.IsEmpty()) Missing.PaintMasks[0].Pixels[0] = FColor(0, 0, 0, 0);
		Check(WriteSlot(Missing, 2), TEXT("누락 표면·손상 도색 시험 저장 생성")); Reload();
		const FCozyFloorTile* Recovered = Estate->GetState().FloorTiles.FindByPredicate([&](const FCozyFloorTile& F) { return F.InstanceId == Tile.InstanceId; });
		Check(Recovered && Recovered->bStored, TEXT("누락된 표면의 타일은 ID를 보존해 보관"));
		const FCozyFloorTile* AssetRecovered = Estate->GetState().FloorTiles.FindByPredicate([&](const FCozyFloorTile& F) { return F.InstanceId == MissingAsset.InstanceId; });
		Check(AssetRecovered && AssetRecovered->bStored && AssetRecovered->DefinitionId == MissingAsset.DefinitionId, TEXT("누락 타일 에셋을 다른 외형으로 바꾸지 않고 보관"));
		const FCozyFacilityState* FacilityRecovered = Estate->FindFacility(MissingFacility.InstanceId);
		Check(FacilityRecovered && FacilityRecovered->bStored && FacilityRecovered->DefinitionId == MissingFacility.DefinitionId, TEXT("누락 시설 에셋의 ID와 상태를 보관"));
		Check(Estate->GetState().TerrainRevision != 999, TEXT("맵 리비전 변경 시 지지면 재검사와 이관"));
		if (!Missing.PaintMasks.IsEmpty())
		{
			const FCozyPaintMask* Pending = Estate->GetState().PaintMasks.FindByPredicate([&](const FCozyPaintMask& M) { return M.DomainId == Missing.PaintMasks[0].DomainId; });
			Check(Pending && Pending->Pixels[0] == FColor(0, 0, 0, 0), TEXT("적용 불가 도색은 기본 마스크로 덮어쓰지 않고 보존"));
		}
		Check(WriteSlot(Original, 2), TEXT("정상 시험 상태 복귀")); Reload();
		Estate->SaveEstate(TEXT("직전 정상본 준비")); Estate->FlushPendingSave();
		const TArray<uint8> Broken = {0x42, 0x41, 0x44};
		Check(FFileHelper::SaveArrayToFile(Broken, *SlotPath), TEXT("현재 저장 손상 모의")); Reload();
		TArray<FString> DamagedCopies;
		IFileManager::Get().FindFiles(DamagedCopies, *(SlotPath + TEXT(".damaged_*")), true, false);
		Check(!DamagedCopies.IsEmpty() && Estate->GetRecoveryNotice().Contains(TEXT("복구")), TEXT("손상 사본 보관과 직전 정상본 복구"));
		Check(Estate->GetState().Facilities.Num() == Original.Facilities.Num(), TEXT("복구 후 시설 수 보존"));
		Check(WriteSlot(Original, 99), TEXT("미래 버전 시험 저장 생성")); Reload();
		Check(!Estate->SaveEstate(TEXT("미래 버전 덮어쓰기 검사")), TEXT("미지원 미래 버전 덮어쓰기 차단"));
		auto* Future = Cast<UCozyEstateSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CozyRealm_Slot1"), 0));
		Check(Future && Future->FileVersion == 99, TEXT("미래 버전 파일 원형 보존"));
		Check(WriteSlot(Original, 2), TEXT("복구 시험 종료 후 정상본 복귀")); Reload();
		Report += FString::Printf(TEXT("실패 합계: %d\n"), Failed);
		FFileHelper::SaveStringToFile(Report, *(Folder / TEXT("TerrainRecoveryCheck.txt")));
		UE_LOG(LogTemp, Display, TEXT("[지형 복구 검사]\n%s"), *Report);
	}));
#endif
