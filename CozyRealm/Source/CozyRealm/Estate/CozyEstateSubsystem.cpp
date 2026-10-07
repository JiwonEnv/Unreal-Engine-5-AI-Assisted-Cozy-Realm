#include "Estate/CozyEstateSubsystem.h"
#include "Facilities/CozyFacilityActor.h"
#include "CozyRealm.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Estate/CozyEstateSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "CozyEstate"

namespace CozyEstate
{
	/** CSV 폴더: <프로젝트>/Data/GameData (Content 밖이라 에디터가 자동 임포트하지 않음) */
	FString GetDataDir()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("GameData"));
	}

	/** 한 번의 Tick에서 처리할 최대 게임 초 (디버그 배속 ×100 대비) */
	constexpr int32 MaxStepsPerTick = 600;
}

// ---------------------------------------------------------------------------
// 시작

void UCozyEstateSubsystem::StartEstate()
{
	FString Errors;
	if (!LoadAllData(Errors))
	{
		UE_LOG(LogCozyRealm, Error, TEXT("게임 데이터를 읽지 못했습니다. %s"), *Errors);
		return;
	}
	if (!Errors.IsEmpty())
	{
		UE_LOG(LogCozyRealm, Warning, TEXT("게임 데이터 읽기 경고: %s"), *Errors);
	}

	ValidateData();
	// 저장 파일이 있으면 이어서, 없으면(또는 읽을 수 없으면) 새 게임
	FDateTime SavedUtc;
	const bool bLoaded = LoadEstateFromSave(SavedUtc);
	if (!bLoaded)
	{
		BuildNewGameState();
	}
	SpawnFacilityActors();
	bStarted = true;
	if (bLoaded)
	{
		// 저장 시각부터 지금까지 정산 → 바로 저장해 같은 시간을 두 번 받지 않게 함
		ApplyOfflineProgress((FDateTime::UtcNow() - SavedUtc).GetTotalSeconds(), TEXT("시작"));
	}
	SaveEstate(bLoaded ? TEXT("불러온 직후") : TEXT("새 게임"));
	NotifyChanged(true);
}

namespace CozyEstate
{
	static const TCHAR* SaveSlotName = TEXT("CozyRealm_Slot1");
	static constexpr int32 SaveUserIndex = 0;
	static constexpr int32 CurrentSaveFileVersion = 1;
}

bool UCozyEstateSubsystem::HasSaveFile() const
{
	return UGameplayStatics::DoesSaveGameExist(CozyEstate::SaveSlotName, CozyEstate::SaveUserIndex);
}

bool UCozyEstateSubsystem::SaveEstate(const FString& Reason)
{
	UCozyEstateSaveGame* Save = Cast<UCozyEstateSaveGame>(UGameplayStatics::CreateSaveGameObject(UCozyEstateSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	Save->FileVersion = CozyEstate::CurrentSaveFileVersion;
	Save->SavedUtc = FDateTime::UtcNow();
	Save->State = State;
	if (!UGameplayStatics::SaveGameToSlot(Save, CozyEstate::SaveSlotName, CozyEstate::SaveUserIndex))
	{
		UE_LOG(LogCozyRealm, Error, TEXT("저장 실패 (%s)"), *Reason);
		return false;
	}
	LastSavedUtc = Save->SavedUtc;
	AutosaveAccumulator = 0.0;
	UE_LOG(LogCozyRealm, Log, TEXT("저장: %s · 게임 시간 %.0f초 · 시설 %d · 작업 %d · %s(UTC)"), *Reason, State.GameSeconds, State.Facilities.Num(), State.Jobs.Num(), *LastSavedUtc.ToString());
	return true;
}

bool UCozyEstateSubsystem::LoadEstateFromSave(FDateTime& OutSavedUtc)
{
	if (!HasSaveFile())
	{
		return false;
	}
	UCozyEstateSaveGame* Save = Cast<UCozyEstateSaveGame>(UGameplayStatics::LoadGameFromSlot(CozyEstate::SaveSlotName, CozyEstate::SaveUserIndex));
	if (!Save || Save->FileVersion > CozyEstate::CurrentSaveFileVersion)
	{
		// 읽을 수 없는 파일은 지우지 않고 옆에 사본을 남긴 뒤 새 게임으로 (사용자 데이터 보호)
		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), FString(CozyEstate::SaveSlotName) + TEXT(".sav"));
		const FString Backup = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), FString::Printf(TEXT("%s_unreadable_%s.sav"), CozyEstate::SaveSlotName, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
		IFileManager::Get().Copy(*Backup, *Path);
		UE_LOG(LogCozyRealm, Error, TEXT("저장 파일을 읽을 수 없어 새 게임으로 시작합니다 · 원본 사본: %s"), *Backup);
		return false;
	}
	State = Save->State;
	OutSavedUtc = Save->SavedUtc;
	LastSavedUtc = Save->SavedUtc;
	SanitizeLoadedState();
	UE_LOG(LogCozyRealm, Log, TEXT("불러오기: 게임 시간 %.0f초 · 시설 %d · 주민 %d · 작업 %d · 저장 시각 %s(UTC)"), State.GameSeconds, State.Facilities.Num(), State.Residents.Num(), State.Jobs.Num(), *OutSavedUtc.ToString());
	return true;
}

void UCozyEstateSubsystem::SanitizeLoadedState()
{
	int32 Removed = 0;
	Removed += State.Facilities.RemoveAll([this](const FCozyFacilityState& Facility) { return !GetFacilityDef(Facility.DefinitionId); });
	Removed += State.Residents.RemoveAll([this](const FCozyResidentState& Resident) { return !GetResidentDef(Resident.DefinitionId); });
	for (FCozyResidentState& Resident : State.Residents)
	{
		if (Resident.AssignedFacility.IsValid() && !FindFacility(Resident.AssignedFacility))
		{
			Resident.AssignedFacility.Invalidate();
			++Removed;
		}
	}
	for (FCozyFacilityState& Facility : State.Facilities)
	{
		Removed += Facility.AssignedResidents.RemoveAll([this](const FGuid& ResidentId)
		{
			return !State.Residents.ContainsByPredicate([&ResidentId](const FCozyResidentState& Resident) { return Resident.InstanceId == ResidentId; });
		});
	}
	Removed += State.Jobs.RemoveAll([this](const FCozyJobRecord& Job)
	{
		if (!FindFacility(Job.FacilityId))
		{
			return true;
		}
		if (Job.Type == ECozyJobType::Growth)
		{
			return !GrowthTable || !GrowthTable->FindRow<FCozyGrowthRow>(Job.ContentId, TEXT(""), false);
		}
		if (Job.Type == ECozyJobType::Processing)
		{
			return !GetRecipeDef(Job.ContentId);
		}
		return false;
	});
	TArray<FName> UnknownItems;
	for (const TPair<FName, int32>& Pair : State.Resources)
	{
		if (!GetItemDef(Pair.Key))
		{
			UnknownItems.Add(Pair.Key);
		}
	}
	for (const FName& ItemId : UnknownItems)
	{
		State.Resources.Remove(ItemId);
		++Removed;
	}
	if (Removed > 0)
	{
		UE_LOG(LogCozyRealm, Warning, TEXT("[불러오기 검사] 데이터에 없는 참조 %d개를 정리했습니다"), Removed);
	}
}

void UCozyEstateSubsystem::DebugReloadFromSave()
{
	if (!HasSaveFile())
	{
		UE_LOG(LogCozyRealm, Warning, TEXT("불러올 저장 파일이 없습니다"));
		return;
	}
	DestroyFacilityActors();
	FDateTime SavedUtc;
	const bool bLoaded = LoadEstateFromSave(SavedUtc);
	if (!bLoaded)
	{
		BuildNewGameState();
	}
	SpawnFacilityActors();
	StepAccumulator = 0.0;
	if (bLoaded)
	{
		ApplyOfflineProgress((FDateTime::UtcNow() - SavedUtc).GetTotalSeconds(), TEXT("불러오기"));
	}
	SaveEstate(TEXT("불러온 직후"));
	NotifyChanged(true);
}

void UCozyEstateSubsystem::ApplyOfflineProgress(double AwaySeconds, const TCHAR* Source)
{
	OfflineReport = FCozyOfflineReport();
	OfflineReport.AwaySeconds = FMath::Max(0.0, AwaySeconds); // 시계를 되돌린 경우 0
	OfflineReport.AppliedSeconds = FMath::Min<double>(OfflineReport.AwaySeconds, FMath::Max(0.f, Config.OfflineMaxSeconds));
	OfflineReport.bClamped = OfflineReport.AwaySeconds > OfflineReport.AppliedSeconds;
	const int32 Steps = FMath::FloorToInt(OfflineReport.AppliedSeconds);
	if (Steps < 1)
	{
		UE_LOG(LogCozyRealm, Log, TEXT("방치 정산(%s): 꺼 둔 시간 %.1f초 · 정산할 시간 없음"), Source, OfflineReport.AwaySeconds);
		return;
	}

	// 정산 전 상태 (결과 표시용)
	TMap<FGuid, TPair<FName, int32>> UnclaimedBefore;
	TMap<FGuid, int32> LevelBefore;
	for (const FCozyFacilityState& Facility : State.Facilities)
	{
		UnclaimedBefore.Add(Facility.InstanceId, TPair<FName, int32>(Facility.UnclaimedItemId, Facility.UnclaimedAmount));
		LevelBefore.Add(Facility.InstanceId, Facility.Level);
	}

	// 접속 중과 같은 규칙으로 1초씩 진행: 주민 조건 · 미수령 한도 · 가공은 선택한 남은 회차까지 · 업그레이드는 시간이 지나면 완료 (D44)
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		State.GameSeconds += 1.0;
		StepJobs();
	}

	// 방치 재화 (🤖 2시간당 시간 부적 1 · 1시간당 심상 조각 1 · 남는 시간은 다음으로 이어지지 않음)
	const int32 Talismans = Config.OfflineTalismanEverySeconds > 0.f ? FMath::FloorToInt(OfflineReport.AppliedSeconds / Config.OfflineTalismanEverySeconds) : 0;
	const int32 Shards = Config.OfflineMindShardEverySeconds > 0.f ? FMath::FloorToInt(OfflineReport.AppliedSeconds / Config.OfflineMindShardEverySeconds) : 0;
	if (Talismans > 0)
	{
		AddResource(Config.SpeedupItemId, Talismans);
		OfflineReport.Lines.Add(FText::Format(LOCTEXT("OffTalisman", "{0} +{1}"), GetItemName(Config.SpeedupItemId), FText::AsNumber(Talismans)));
	}
	if (Shards > 0)
	{
		AddResource(TEXT("MindShard"), Shards);
		OfflineReport.Lines.Add(FText::Format(LOCTEXT("OffShard", "{0} +{1}"), GetItemName(TEXT("MindShard")), FText::AsNumber(Shards)));
	}

	// 시설별 결과: 미수령분 증가 · 업그레이드 완료
	for (const FCozyFacilityState& Facility : State.Facilities)
	{
		const FText Name = GetFacilityDisplayName(Facility.InstanceId);
		const TPair<FName, int32>* Before = UnclaimedBefore.Find(Facility.InstanceId);
		const int32 BeforeAmount = Before && Before->Key == Facility.UnclaimedItemId ? Before->Value : 0;
		if (Facility.UnclaimedAmount > BeforeAmount)
		{
			const FCozyFacilityRow* Def = GetFacilityDef(Facility.DefinitionId);
			OfflineReport.Lines.Add(FText::Format(LOCTEXT("OffUnclaimed", "{0}: 미수령 {1} +{2} (지금 {3}/{4} · 수확·수령해야 창고로)"),
				Name, GetItemName(Facility.UnclaimedItemId), FText::AsNumber(Facility.UnclaimedAmount - BeforeAmount), FText::AsNumber(Facility.UnclaimedAmount), FText::AsNumber(Def ? Def->UnclaimedCapacity : 0)));
		}
		const int32* OldLevel = LevelBefore.Find(Facility.InstanceId);
		if (OldLevel && Facility.Level > *OldLevel)
		{
			OfflineReport.Lines.Add(FText::Format(LOCTEXT("OffLevel", "{0}: 업그레이드 완료 Lv{1} → Lv{2}"), Name, FText::AsNumber(*OldLevel), FText::AsNumber(Facility.Level)));
		}
	}
	// 받은 것이 있을 때만 팝업 (잠깐 껐다 켠 경우 등은 로그만)
	bOfflineReportPending = OfflineReport.Lines.Num() > 0;
	UE_LOG(LogCozyRealm, Log, TEXT("방치 정산(%s): 꺼 둔 시간 %.0f초 · 정산 %d초%s · 시간 부적 +%d · 심상 조각 +%d · 결과 %d줄"),
		Source, OfflineReport.AwaySeconds, Steps, OfflineReport.bClamped ? TEXT(" (최대 12시간)") : TEXT(""), Talismans, Shards, OfflineReport.Lines.Num());
	bStructuralPending = false;
	NotifyChanged(true);
}

void UCozyEstateSubsystem::DebugSkipOffline(double Seconds)
{
	ApplyOfflineProgress(Seconds, TEXT("디버그 건너뛰기"));
	SaveEstate(TEXT("방치 정산 직후"));
}

void UCozyEstateSubsystem::DebugShiftSaveTime(double Seconds)
{
	UCozyEstateSaveGame* Save = HasSaveFile() ? Cast<UCozyEstateSaveGame>(UGameplayStatics::LoadGameFromSlot(CozyEstate::SaveSlotName, CozyEstate::SaveUserIndex)) : nullptr;
	if (!Save)
	{
		UE_LOG(LogCozyRealm, Warning, TEXT("저장 시각을 바꿀 저장 파일이 없습니다"));
		return;
	}
	Save->SavedUtc -= FTimespan::FromSeconds(Seconds);
	UGameplayStatics::SaveGameToSlot(Save, CozyEstate::SaveSlotName, CozyEstate::SaveUserIndex);
	UE_LOG(LogCozyRealm, Log, TEXT("디버그: 저장 시각을 %.0f초 과거로 · %s(UTC)"), Seconds, *Save->SavedUtc.ToString());
}

void UCozyEstateSubsystem::Deinitialize()
{
	// 게임 종료 · PIE 종료 시 저장
	if (bStarted)
	{
		SaveEstate(TEXT("종료"));
		bStarted = false;
	}
	Super::Deinitialize();
}

UDataTable* UCozyEstateSubsystem::LoadCsvTable(const FString& FileName, UScriptStruct* RowStruct, FString& OutErrors)
{
	const FString Path = FPaths::Combine(CozyEstate::GetDataDir(), FileName);
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		OutErrors += FString::Printf(TEXT("\n- 파일 없음: %s"), *Path);
		return nullptr;
	}

	UDataTable* Table = NewObject<UDataTable>(this, NAME_None, RF_Transient);
	Table->RowStruct = RowStruct;
	const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
	for (const FString& Problem : Problems)
	{
		OutErrors += FString::Printf(TEXT("\n- %s: %s"), *FileName, *Problem);
	}
	return Table;
}

bool UCozyEstateSubsystem::LoadAllData(FString& OutErrors)
{
	FacilityTable = LoadCsvTable(TEXT("Facilities.csv"), FCozyFacilityRow::StaticStruct(), OutErrors);
	CropTable = LoadCsvTable(TEXT("Crops.csv"), FCozyCropRow::StaticStruct(), OutErrors);
	ItemTable = LoadCsvTable(TEXT("Items.csv"), FCozyItemRow::StaticStruct(), OutErrors);
	ResidentTable = LoadCsvTable(TEXT("Residents.csv"), FCozyResidentRow::StaticStruct(), OutErrors);
	RecipeTable = LoadCsvTable(TEXT("Recipes.csv"), FCozyRecipeRow::StaticStruct(), OutErrors);
	GrowthTable = LoadCsvTable(TEXT("Growth.csv"), FCozyGrowthRow::StaticStruct(), OutErrors);
	StartFacilityTable = LoadCsvTable(TEXT("StartFacilities.csv"), FCozyStartFacilityRow::StaticStruct(), OutErrors);
	StartResidentTable = LoadCsvTable(TEXT("StartResidents.csv"), FCozyStartResidentRow::StaticStruct(), OutErrors);
	StartResourceTable = LoadCsvTable(TEXT("StartResources.csv"), FCozyStartResourceRow::StaticStruct(), OutErrors);
	RewardTable = LoadCsvTable(TEXT("Rewards.csv"), FCozyRewardRow::StaticStruct(), OutErrors);

	FString ConfigErrors;
	if (UDataTable* ConfigTable = LoadCsvTable(TEXT("EstateConfig.csv"), FCozyEstateConfigRow::StaticStruct(), ConfigErrors))
	{
		if (const FCozyEstateConfigRow* Row = ConfigTable->FindRow<FCozyEstateConfigRow>(TEXT("Default"), TEXT("EstateConfig")))
		{
			Config = *Row;
		}
		else
		{
			ConfigErrors += TEXT("\n- EstateConfig.csv에 Default 행이 없음 (기본값 사용)");
		}
	}
	OutErrors += ConfigErrors;

	// 시설 정의 · 작물 · 재료 · 시작 시설이 없으면 시작할 수 없다
	return FacilityTable && CropTable && ItemTable && ResidentTable && StartFacilityTable;
}

int32 UCozyEstateSubsystem::ValidateData() const
{
	int32 Issues = 0;
	auto Warn = [&Issues](const FString& Message)
	{
		++Issues;
		UE_LOG(LogCozyRealm, Warning, TEXT("[데이터 검사] %s"), *Message);
	};

	if (FacilityTable)
	{
		FacilityTable->ForeachRow<FCozyFacilityRow>(TEXT("Validate"), [&](const FName& Key, const FCozyFacilityRow& Row)
		{
			if (Row.MinResidents > Row.MaxResidents)
			{
				Warn(FString::Printf(TEXT("시설 %s: 최소 주민(%d) > 최대 주민(%d)"), *Key.ToString(), Row.MinResidents, Row.MaxResidents));
			}
			if (Row.Size.X <= 0 || Row.Size.Y <= 0 || Row.Size.X > Config.GridSize.X || Row.Size.Y > Config.GridSize.Y)
			{
				Warn(FString::Printf(TEXT("시설 %s: 크기 %dx%d가 영지 칸을 벗어남"), *Key.ToString(), Row.Size.X, Row.Size.Y));
			}
			for (const FName& CropId : Row.ProductionItems)
			{
				if (!GetCropDef(CropId))
				{
					Warn(FString::Printf(TEXT("시설 %s: 없는 작물 참조 %s"), *Key.ToString(), *CropId.ToString()));
				}
			}
			if (Row.Functions.Contains(ECozyFacilityFunction::Processing) && (Row.ProcessingSlots < 1 || Row.UnclaimedCapacity < 1))
			{
				Warn(FString::Printf(TEXT("시설 %s: 가공 시설인데 가공 칸(%d) 또는 미수령 한도(%d)가 0"), *Key.ToString(), Row.ProcessingSlots, Row.UnclaimedCapacity));
			}
			if (!Row.ManagerFacilityId.IsNone() && !GetFacilityDef(Row.ManagerFacilityId))
			{
				Warn(FString::Printf(TEXT("시설 %s: 없는 관리 시설 참조 %s"), *Key.ToString(), *Row.ManagerFacilityId.ToString()));
			}
		});
	}

	if (CropTable)
	{
		CropTable->ForeachRow<FCozyCropRow>(TEXT("Validate"), [&](const FName& Key, const FCozyCropRow& Row)
		{
			if (!GetItemDef(Row.ProducedItem))
			{
				Warn(FString::Printf(TEXT("작물 %s: 없는 재료 참조 %s"), *Key.ToString(), *Row.ProducedItem.ToString()));
			}
			if (Row.ProductionSeconds <= 0.f)
			{
				Warn(FString::Printf(TEXT("작물 %s: 생산 시간이 0 이하"), *Key.ToString()));
			}
		});
	}

	if (RecipeTable)
	{
		RecipeTable->ForeachRow<FCozyRecipeRow>(TEXT("Validate"), [&](const FName& Key, const FCozyRecipeRow& Row)
		{
			if (!GetFacilityDef(Row.FacilityId))
			{
				Warn(FString::Printf(TEXT("레시피 %s: 없는 시설 참조 %s"), *Key.ToString(), *Row.FacilityId.ToString()));
			}
			for (const TPair<FName, int32>& Input : Row.Inputs)
			{
				if (!GetItemDef(Input.Key))
				{
					Warn(FString::Printf(TEXT("레시피 %s: 없는 재료 참조 %s"), *Key.ToString(), *Input.Key.ToString()));
				}
				if (Input.Key == Row.OutputItem)
				{
					Warn(FString::Printf(TEXT("레시피 %s: 입력과 결과가 같음"), *Key.ToString()));
				}
			}
			if (!GetItemDef(Row.OutputItem))
			{
				Warn(FString::Printf(TEXT("레시피 %s: 없는 결과 재료 %s"), *Key.ToString(), *Row.OutputItem.ToString()));
			}
			if (Row.OutputAmount < 1 || Row.Seconds <= 0.f)
			{
				Warn(FString::Printf(TEXT("레시피 %s: 1회 개수(%d) 또는 시간(%.1f)이 0 이하"), *Key.ToString(), Row.OutputAmount, Row.Seconds));
			}
			const FCozyFacilityRow* Facility = GetFacilityDef(Row.FacilityId);
			if (Facility && Row.OutputAmount > Facility->UnclaimedCapacity)
			{
				Warn(FString::Printf(TEXT("레시피 %s: 1회 개수(%d)가 시설 미수령 한도(%d)보다 커서 시작할 수 없음"), *Key.ToString(), Row.OutputAmount, Facility->UnclaimedCapacity));
			}
		});
	}

	if (GrowthTable)
	{
		GrowthTable->ForeachRow<FCozyGrowthRow>(TEXT("Validate"), [&](const FName& Key, const FCozyGrowthRow& Row)
		{
			for (const FName& CropId : Row.UnlockCrops)
			{
				if (!GetCropDef(CropId))
				{
					Warn(FString::Printf(TEXT("성장 %s: 없는 해금 작물 %s"), *Key.ToString(), *CropId.ToString()));
				}
			}
			if (!GetFacilityDef(Row.TargetFacilityId))
			{
				Warn(FString::Printf(TEXT("성장 %s: 없는 시설 참조 %s"), *Key.ToString(), *Row.TargetFacilityId.ToString()));
			}
			for (const TPair<FName, int32>& Required : Row.RequiredFacilities)
			{
				if (!GetFacilityDef(Required.Key))
				{
					Warn(FString::Printf(TEXT("성장 %s: 없는 선행 시설 참조 %s"), *Key.ToString(), *Required.Key.ToString()));
				}
				if (Required.Key == Row.TargetFacilityId)
				{
					Warn(FString::Printf(TEXT("성장 %s: 자기 자신을 선행 조건으로 요구함"), *Key.ToString()));
				}
			}
			for (const FName& UnlockId : Row.UnlockFacilities)
			{
				if (!GetFacilityDef(UnlockId))
				{
					Warn(FString::Printf(TEXT("성장 %s: 없는 해금 시설 %s"), *Key.ToString(), *UnlockId.ToString()));
				}
			}
			for (const TPair<FName, int32>& Cost : Row.StartCost)
			{
				if (!GetItemDef(Cost.Key))
				{
					Warn(FString::Printf(TEXT("성장 %s: 없는 재료 참조 %s"), *Key.ToString(), *Cost.Key.ToString()));
				}
				const FCozyItemRow* Item = GetItemDef(Cost.Key);
				if (Item && Item->Category == ECozyItemCategory::Material && Cost.Value > Config.StorageCapPerItem)
				{
					Warn(FString::Printf(TEXT("성장 %s: %s %d개는 창고 한도(%d)를 넘어 모을 수 없음"), *Key.ToString(), *Cost.Key.ToString(), Cost.Value, Config.StorageCapPerItem));
				}
			}
		});
	}

	// 시작 시설: 정의 존재 · 영지 안 · 겹침 · 시작 작물
	if (StartFacilityTable)
	{
		TSet<FIntPoint> Occupied;
		StartFacilityTable->ForeachRow<FCozyStartFacilityRow>(TEXT("Validate"), [&](const FName& Key, const FCozyStartFacilityRow& Row)
		{
			const FCozyFacilityRow* Def = GetFacilityDef(Row.FacilityId);
			if (!Def)
			{
				Warn(FString::Printf(TEXT("시작 시설 %s: 없는 시설 참조 %s"), *Key.ToString(), *Row.FacilityId.ToString()));
				return;
			}
			const FIntPoint Size = (Row.Rotation % 2 == 0) ? Def->Size : FIntPoint(Def->Size.Y, Def->Size.X);
			for (int32 X = 0; X < Size.X; ++X)
			{
				for (int32 Y = 0; Y < Size.Y; ++Y)
				{
					const FIntPoint Cell = Row.GridCoord + FIntPoint(X, Y);
					if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= Config.GridSize.X || Cell.Y >= Config.GridSize.Y)
					{
						Warn(FString::Printf(TEXT("시작 시설 %s: 칸 (%d,%d)이 영지 밖"), *Key.ToString(), Cell.X, Cell.Y));
						return;
					}
					if (Occupied.Contains(Cell))
					{
						Warn(FString::Printf(TEXT("시작 시설 %s: 칸 (%d,%d)이 다른 시설과 겹침"), *Key.ToString(), Cell.X, Cell.Y));
						return;
					}
					Occupied.Add(Cell);
				}
			}
			if (!Row.SelectedCrop.IsNone() && !Def->ProductionItems.Contains(Row.SelectedCrop))
			{
				Warn(FString::Printf(TEXT("시작 시설 %s: 작물 %s은 이 시설의 생산 항목이 아님"), *Key.ToString(), *Row.SelectedCrop.ToString()));
			}
		});
	}

	if (StartResidentTable)
	{
		StartResidentTable->ForeachRow<FCozyStartResidentRow>(TEXT("Validate"), [&](const FName& Key, const FCozyStartResidentRow& Row)
		{
			if (!GetResidentDef(Row.ResidentId))
			{
				Warn(FString::Printf(TEXT("시작 주민 %s: 없는 주민 참조 %s"), *Key.ToString(), *Row.ResidentId.ToString()));
			}
		});
	}

	if (StartResourceTable)
	{
		StartResourceTable->ForeachRow<FCozyStartResourceRow>(TEXT("Validate"), [&](const FName& Key, const FCozyStartResourceRow& Row)
		{
			if (!GetItemDef(Row.ItemId))
			{
				Warn(FString::Printf(TEXT("시작 재화 %s: 없는 재료 참조 %s"), *Key.ToString(), *Row.ItemId.ToString()));
			}
		});
	}

	if (!GetItemDef(Config.SpeedupItemId) || Config.SpeedupSecondsPerItem <= 0.f)
	{
		Warn(FString::Printf(TEXT("영지 설정: 시간 단축 재화 %s가 없거나 1장당 시간(%.0f초)이 0 이하"), *Config.SpeedupItemId.ToString(), Config.SpeedupSecondsPerItem));
	}
	if (RewardTable)
	{
		RewardTable->ForeachRow<FCozyRewardRow>(TEXT("Validate"), [&](const FName& Key, const FCozyRewardRow& Row)
		{
			if (Row.RewardId.IsNone() || !GetItemDef(Row.ItemId) || Row.Amount <= 0)
			{
				Warn(FString::Printf(TEXT("보상 %s: 보상 ID가 비었거나 없는 재료(%s)·0 이하 수량"), *Key.ToString(), *Row.ItemId.ToString()));
			}
		});
	}
	if (!GetItemDef(Config.SaleCurrencyId))
	{
		Warn(FString::Printf(TEXT("영지 설정: 판매 대금 재화 %s가 Items.csv에 없음"), *Config.SaleCurrencyId.ToString()));
	}

	ValidateProgression(Warn);

	UE_LOG(LogCozyRealm, Log, TEXT("[데이터 검사] 끝 · 문제 %d개"), Issues);
	return Issues;
}

void UCozyEstateSubsystem::ValidateProgression(TFunctionRef<void(const FString&)> Warn) const
{
	if (!GrowthTable || !StartFacilityTable || !FacilityTable)
	{
		return;
	}
	struct FStep { FName Key; const FCozyGrowthRow* Row = nullptr; };
	TMap<FName, TArray<FStep>> StepsByTarget;
	GrowthTable->ForeachRow<FCozyGrowthRow>(TEXT("Progress"), [&StepsByTarget](const FName& Key, const FCozyGrowthRow& Row)
	{
		StepsByTarget.FindOrAdd(Row.TargetFacilityId).Add({ Key, &Row });
	});
	TSet<FName> ShrineIds;
	FacilityTable->ForeachRow<FCozyFacilityRow>(TEXT("Progress"), [&ShrineIds](const FName& Key, const FCozyFacilityRow& Row)
	{
		if (Row.Functions.Contains(ECozyFacilityFunction::ShrineCore))
		{
			ShrineIds.Add(Key);
		}
	});

	// 단계 빈칸 · 중복 · 신사 상한보다 낮은 조건 (🙋 다른 시설의 레벨 상한 = 신사 레벨)
	for (TPair<FName, TArray<FStep>>& Pair : StepsByTarget)
	{
		Pair.Value.Sort([](const FStep& A, const FStep& B) { return A.Row->FromLevel < B.Row->FromLevel; });
		int32 Expected = 1;
		for (const FStep& Step : Pair.Value)
		{
			const int32 From = Step.Row->FromLevel;
			if (From < Expected)
			{
				Warn(FString::Printf(TEXT("성장 %s: %s Lv%d→%d 단계가 중복됨"), *Step.Key.ToString(), *Pair.Key.ToString(), From, From + 1));
			}
			else if (From > Expected)
			{
				Warn(FString::Printf(TEXT("성장 %s: %s Lv%d→%d 단계가 없어 이 단계에 도달할 수 없음 (단계 빈칸)"), *Step.Key.ToString(), *Pair.Key.ToString(), Expected, Expected + 1));
			}
			Expected = From + 1;
			if (!ShrineIds.Contains(Pair.Key) && Step.Row->RequiredShrineLevel < From + 1)
			{
				Warn(FString::Printf(TEXT("성장 %s: %s Lv%d는 신사 상한상 신사 Lv%d 이상이 필요한데 조건이 신사 Lv%d임"), *Step.Key.ToString(), *Pair.Key.ToString(), From + 1, From + 1, Step.Row->RequiredShrineLevel));
			}
		}
	}

	// 새 게임 시설에서 시작해, 조건을 만족하는 단계를 더 이상 없을 때까지 적용 (재료는 보지 않음)
	TMap<FName, int32> Levels;
	StartFacilityTable->ForeachRow<FCozyStartFacilityRow>(TEXT("Progress"), [&Levels](const FName& Key, const FCozyStartFacilityRow& Row)
	{
		int32& Level = Levels.FindOrAdd(Row.FacilityId);
		Level = FMath::Max(Level, Row.Level);
	});
	auto ShrineLevelOf = [&Levels, &ShrineIds]()
	{
		int32 Level = 0;
		for (const FName& Id : ShrineIds)
		{
			Level = FMath::Max(Level, Levels.FindRef(Id));
		}
		return Level;
	};
	TSet<FName> Applied;
	for (bool bChanged = true; bChanged;)
	{
		bChanged = false;
		for (const TPair<FName, TArray<FStep>>& Pair : StepsByTarget)
		{
			for (const FStep& Step : Pair.Value)
			{
				const int32* Current = Levels.Find(Pair.Key);
				if (Applied.Contains(Step.Key) || !Current || *Current != Step.Row->FromLevel || ShrineLevelOf() < Step.Row->RequiredShrineLevel)
				{
					continue;
				}
				bool bMet = true;
				for (const TPair<FName, int32>& Required : Step.Row->RequiredFacilities)
				{
					bMet &= Levels.FindRef(Required.Key) >= Required.Value;
				}
				if (!bMet)
				{
					continue;
				}
				Levels.Add(Pair.Key, Step.Row->FromLevel + 1);
				for (const FName& UnlockId : Step.Row->UnlockFacilities)
				{
					Levels.FindOrAdd(UnlockId, 1);
				}
				Applied.Add(Step.Key);
				bChanged = true;
			}
		}
	}

	// 도달하지 못한 단계: 시설마다 처음 막힌 단계만 원인과 함께 알림
	for (const TPair<FName, TArray<FStep>>& Pair : StepsByTarget)
	{
		for (const FStep& Step : Pair.Value)
		{
			if (Applied.Contains(Step.Key))
			{
				continue;
			}
			FString Reason;
			if (!Levels.Contains(Pair.Key))
			{
				Reason = TEXT("시작 시설에도 없고 어떤 단계로도 해금되지 않음");
			}
			else if (ShrineLevelOf() < Step.Row->RequiredShrineLevel)
			{
				Reason = FString::Printf(TEXT("신사 Lv%d이 필요하지만 신사는 최대 Lv%d까지만 오를 수 있음"), Step.Row->RequiredShrineLevel, ShrineLevelOf());
			}
			else
			{
				for (const TPair<FName, int32>& Required : Step.Row->RequiredFacilities)
				{
					if (Levels.FindRef(Required.Key) < Required.Value)
					{
						Reason = FString::Printf(TEXT("%s Lv%d이 필요하지만 최대 Lv%d까지만 오를 수 있음 (상한·최대 레벨 또는 순환 조건)"), *Required.Key.ToString(), Required.Value, Levels.FindRef(Required.Key));
						break;
					}
				}
				if (Reason.IsEmpty())
				{
					Reason = TEXT("앞 단계에 도달하지 못함");
				}
			}
			Warn(FString::Printf(TEXT("성장 %s: %s Lv%d→%d에 도달할 수 없음 · %s"), *Step.Key.ToString(), *Pair.Key.ToString(), Step.Row->FromLevel, Step.Row->FromLevel + 1, *Reason));
			break;
		}
	}
}

void UCozyEstateSubsystem::BuildNewGameState()
{
	State = FCozyEstateState();

	if (StartFacilityTable)
	{
		StartFacilityTable->ForeachRow<FCozyStartFacilityRow>(TEXT("NewGame"), [this](const FName& Key, const FCozyStartFacilityRow& Row)
		{
			if (!GetFacilityDef(Row.FacilityId))
			{
				return;
			}
			FCozyFacilityState Facility;
			Facility.InstanceId = FGuid::NewGuid();
			Facility.DefinitionId = Row.FacilityId;
			Facility.GridCoord = Row.GridCoord;
			Facility.Rotation = Row.Rotation;
			Facility.Level = FMath::Max(1, Row.Level);
			Facility.SelectedCropId = Row.SelectedCrop;
			State.Facilities.Add(Facility);
		});
	}

	if (StartResidentTable)
	{
		StartResidentTable->ForeachRow<FCozyStartResidentRow>(TEXT("NewGame"), [this](const FName& Key, const FCozyStartResidentRow& Row)
		{
			FCozyResidentState Resident;
			Resident.InstanceId = FGuid::NewGuid();
			Resident.DefinitionId = Row.ResidentId;
			// D7: 처음에는 나가야에 미배치
			State.Residents.Add(Resident);
		});
	}

	if (StartResourceTable)
	{
		StartResourceTable->ForeachRow<FCozyStartResourceRow>(TEXT("NewGame"), [this](const FName& Key, const FCozyStartResourceRow& Row)
		{
			State.Resources.FindOrAdd(Row.ItemId) = Row.Amount;
		});
	}
}

void UCozyEstateSubsystem::SpawnFacilityActors()
{
	for (const FCozyFacilityState& Facility : State.Facilities)
	{
		SpawnFacilityActor(Facility);
	}
}

void UCozyEstateSubsystem::SpawnFacilityActor(const FCozyFacilityState& Facility)
{
	UWorld* World = GetWorld();
	const FCozyFacilityRow* Def = GetFacilityDef(Facility.DefinitionId);
	if (!World || !Def)
	{
		return;
	}

	const FIntPoint Size = (Facility.Rotation % 2 == 0) ? Def->Size : FIntPoint(Def->Size.Y, Def->Size.X);
	FVector Center = Config.GridOrigin + FVector(
		(Facility.GridCoord.X + Size.X * 0.5f) * Config.CellSize,
		(Facility.GridCoord.Y + Size.Y * 0.5f) * Config.CellSize,
		0.f);

	// 실제 지면 높이에 맞춤 (영지 판 두께가 바뀌어도 시설·오라가 묻히지 않게)
	FHitResult GroundHit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(CozyFacilityGround), false);
	for (ACozyFacilityActor* Existing : FacilityActors)
	{
		TraceParams.AddIgnoredActor(Existing);
	}
	if (World->LineTraceSingleByChannel(GroundHit, Center + FVector(0.f, 0.f, 2000.f), Center - FVector(0.f, 0.f, 2000.f), ECC_Visibility, TraceParams))
	{
		Center.Z = GroundHit.ImpactPoint.Z;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACozyFacilityActor* Actor = World->SpawnActor<ACozyFacilityActor>(ACozyFacilityActor::StaticClass(), Center, FRotator(0.f, 90.f * Facility.Rotation, 0.f), Params);
	if (Actor)
	{
		Actor->InitFacility(Facility.InstanceId, *Def, Config.CellSize);
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("Facility_%s"), *Facility.DefinitionId.ToString()));
#endif
		FacilityActors.Add(Actor);
	}
}

void UCozyEstateSubsystem::DestroyFacilityActors()
{
	for (ACozyFacilityActor* Actor : FacilityActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	FacilityActors.Reset();
}

// ---------------------------------------------------------------------------
// 데이터 조회

const FCozyFacilityRow* UCozyEstateSubsystem::GetFacilityDef(FName Id) const
{
	return FacilityTable ? FacilityTable->FindRow<FCozyFacilityRow>(Id, TEXT(""), false) : nullptr;
}

const FCozyCropRow* UCozyEstateSubsystem::GetCropDef(FName Id) const
{
	return CropTable ? CropTable->FindRow<FCozyCropRow>(Id, TEXT(""), false) : nullptr;
}

const FCozyItemRow* UCozyEstateSubsystem::GetItemDef(FName Id) const
{
	return ItemTable ? ItemTable->FindRow<FCozyItemRow>(Id, TEXT(""), false) : nullptr;
}

const FCozyResidentRow* UCozyEstateSubsystem::GetResidentDef(FName Id) const
{
	return ResidentTable ? ResidentTable->FindRow<FCozyResidentRow>(Id, TEXT(""), false) : nullptr;
}

TArray<FName> UCozyEstateSubsystem::GetHudItems() const
{
	TArray<TPair<int32, FName>> Ordered;
	if (ItemTable)
	{
		ItemTable->ForeachRow<FCozyItemRow>(TEXT("Hud"), [&Ordered](const FName& Key, const FCozyItemRow& Row)
		{
			if (Row.HudOrder > 0)
			{
				Ordered.Emplace(Row.HudOrder, Key);
			}
		});
	}
	Ordered.Sort([](const TPair<int32, FName>& A, const TPair<int32, FName>& B) { return A.Key < B.Key; });

	TArray<FName> Result;
	for (const TPair<int32, FName>& Pair : Ordered)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

// ---------------------------------------------------------------------------
// 상태 조회

const FCozyFacilityState* UCozyEstateSubsystem::FindFacility(const FGuid& Id) const
{
	return State.Facilities.FindByPredicate([&Id](const FCozyFacilityState& F) { return F.InstanceId == Id; });
}

const FCozyResidentState* UCozyEstateSubsystem::FindResident(const FGuid& Id) const
{
	return State.Residents.FindByPredicate([&Id](const FCozyResidentState& R) { return R.InstanceId == Id; });
}

const FCozyJobRecord* UCozyEstateSubsystem::FindJob(const FGuid& Id) const
{
	return Id.IsValid() ? State.Jobs.FindByPredicate([&Id](const FCozyJobRecord& J) { return J.JobId == Id; }) : nullptr;
}

FCozyFacilityState* UCozyEstateSubsystem::FindFacilityMutable(const FGuid& Id)
{
	return const_cast<FCozyFacilityState*>(FindFacility(Id));
}

FCozyResidentState* UCozyEstateSubsystem::FindResidentMutable(const FGuid& Id)
{
	return const_cast<FCozyResidentState*>(FindResident(Id));
}

FCozyJobRecord* UCozyEstateSubsystem::FindJobMutable(const FGuid& Id)
{
	return const_cast<FCozyJobRecord*>(FindJob(Id));
}

FText UCozyEstateSubsystem::GetFacilityDisplayName(const FGuid& FacilityId) const
{
	if (const FCozyFacilityState* Facility = FindFacility(FacilityId))
	{
		if (const FCozyFacilityRow* Def = GetFacilityDef(Facility->DefinitionId))
		{
			return Def->DisplayName;
		}
	}
	return FText::GetEmpty();
}

FText UCozyEstateSubsystem::GetResidentDisplayName(const FGuid& ResidentId) const
{
	const FCozyResidentState* Resident = FindResident(ResidentId);
	if (!Resident)
	{
		return FText::GetEmpty();
	}
	const FCozyResidentRow* Def = GetResidentDef(Resident->DefinitionId);
	const FText Base = Def ? Def->DisplayName : FText::FromName(Resident->DefinitionId);

	// 같은 종류가 여러 명이면 순번을 붙여 구분 (오니비 1, 오니비 2)
	int32 Index = 0;
	int32 SameKind = 0;
	for (const FCozyResidentState& Other : State.Residents)
	{
		if (Other.DefinitionId == Resident->DefinitionId)
		{
			++SameKind;
			if (Other.InstanceId == ResidentId)
			{
				Index = SameKind;
			}
		}
	}
	return SameKind > 1 ? FText::Format(LOCTEXT("ResidentIndexed", "{0} {1}"), Base, FText::AsNumber(Index)) : Base;
}

FCozyProductionView UCozyEstateSubsystem::GetProductionView(const FGuid& FacilityId) const
{
	FCozyProductionView View;
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def || !Def->Functions.Contains(ECozyFacilityFunction::Production))
	{
		return View;
	}
	View.bHasProduction = true;
	View.CollectButtonLabel = Def->CollectButtonLabel;
	View.SpeedMultiplier = static_cast<float>(GetSpeedMultiplier(*Facility, *Def));
	if (!Def->ManagerFacilityId.IsNone())
	{
		const FCozyFacilityRow* ManagerDef = GetFacilityDef(Def->ManagerFacilityId);
		View.bHasGrowthSource = true;
		View.GrowthSourceName = ManagerDef ? ManagerDef->DisplayName : FText::FromName(Def->ManagerFacilityId);
		View.GrowthSourceLevel = GetHighestLevelOf(Def->ManagerFacilityId);
	}
	{
		// 미수령 · 창고 상태는 생산·가공 공통 계산을 그대로 씀 (D31)
		const FCozyUnclaimedView Unclaimed = GetUnclaimedView(FacilityId);
		View.UnclaimedCapacity = Unclaimed.Capacity;
		View.UnclaimedAmount = Unclaimed.Amount;
		View.UnclaimedItemName = Unclaimed.ItemName;
		View.StoredAmount = Unclaimed.StoredAmount;
		View.StorageCap = Unclaimed.StorageCap;
		View.CollectableNow = Unclaimed.CollectableNow;
	}

	const FCozyCropRow* Crop = GetCropDef(Facility->SelectedCropId);
	if (!Crop)
	{
		View.Status = LOCTEXT("NoCrop", "키울 작물이 정해지지 않았습니다");
		return View;
	}
	if (!IsFacilityWorking(*Facility, *Def))
	{
		View.Status = LOCTEXT("NoResident", "주민이 없어 쉬고 있습니다");
		return View;
	}

	const FCozyJobRecord* Job = FindJob(Facility->ActiveJobId);
	if (!Job)
	{
		View.bWorking = true;
		View.Status = LOCTEXT("Starting", "곧 생산을 시작합니다");
		return View;
	}
	if (Job->State == ECozyJobState::Held)
	{
		View.Status = Job->HeldReason;
		View.Progress01 = 1.f;
		return View;
	}

	View.bWorking = true;
	View.CycleSeconds = static_cast<float>(Job->DurationSeconds);
	const double Elapsed = FMath::Max(0.0, State.GameSeconds - Job->StartGameSeconds - Job->PausedSeconds);
	const double IntoCycle = Elapsed - Job->PaidCycles * Job->DurationSeconds;
	View.Progress01 = Job->DurationSeconds > 0.0 ? FMath::Clamp(static_cast<float>(IntoCycle / Job->DurationSeconds), 0.f, 1.f) : 0.f;
	View.RemainingSeconds = static_cast<float>(FMath::Max(0.0, Job->DurationSeconds - IntoCycle));
	View.Status = FText::Format(LOCTEXT("Producing", "{0} 생산 중"), Crop->DisplayName);
	return View;
}

// ---------------------------------------------------------------------------
// 재료·재화

int32 UCozyEstateSubsystem::GetAmount(FName ItemId) const
{
	const int32* Found = State.Resources.Find(ItemId);
	return Found ? *Found : 0;
}

bool UCozyEstateSubsystem::CanStore(FName ItemId, int32 Amount) const
{
	const FCozyItemRow* Item = GetItemDef(ItemId);
	if (!Item)
	{
		return false;
	}
	if (Item->Category == ECozyItemCategory::Currency)
	{
		return true;
	}
	return GetAmount(ItemId) + Amount <= Config.StorageCapPerItem;
}

int32 UCozyEstateSubsystem::GetStorageSpace(FName ItemId) const
{
	const FCozyItemRow* Item = GetItemDef(ItemId);
	if (!Item)
	{
		return 0;
	}
	if (Item->Category == ECozyItemCategory::Currency)
	{
		return MAX_int32;
	}
	return FMath::Max(0, Config.StorageCapPerItem - GetAmount(ItemId));
}

TArray<FName> UCozyEstateSubsystem::GetStorageItems() const
{
	TArray<FName> Materials;
	TArray<FName> Currencies;
	if (ItemTable)
	{
		ItemTable->ForeachRow<FCozyItemRow>(TEXT("Storage"), [&](const FName& Key, const FCozyItemRow& Row)
		{
			(Row.Category == ECozyItemCategory::Material ? Materials : Currencies).Add(Key);
		});
	}
	Materials.Append(Currencies);
	return Materials;
}

int32 UCozyEstateSubsystem::AddResource(FName ItemId, int32 Amount)
{
	const FCozyItemRow* Item = GetItemDef(ItemId);
	if (!Item || Amount <= 0)
	{
		return 0;
	}
	int32& Current = State.Resources.FindOrAdd(ItemId);
	int32 Added = Amount;
	if (Item->Category == ECozyItemCategory::Material)
	{
		Added = FMath::Clamp(Config.StorageCapPerItem - Current, 0, Amount);
	}
	Current += Added;
	return Added;
}

// ---------------------------------------------------------------------------
// 미수령 생산물

int32 UCozyEstateSubsystem::GetUnclaimedTotal(const FGuid& FacilityId) const
{
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	return Facility ? Facility->UnclaimedAmount : 0;
}

FCozyUnclaimedView UCozyEstateSubsystem::GetUnclaimedView(const FGuid& FacilityId) const
{
	FCozyUnclaimedView View;
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		return View;
	}
	View.Capacity = Def->UnclaimedCapacity;
	View.Amount = Facility->UnclaimedAmount;
	View.Reserved = GetReservedUnclaimed(*Facility);
	View.ItemId = GetLockedOutputItem(*Facility);

	// 표시할 품목: 맡은 품목이 없으면 밭은 선택한 작물의 생산물로 안내
	FName ShownItem = View.ItemId;
	if (ShownItem.IsNone())
	{
		if (const FCozyCropRow* Crop = GetCropDef(Facility->SelectedCropId))
		{
			ShownItem = Crop->ProducedItem;
		}
	}
	View.ItemName = ShownItem.IsNone() ? LOCTEXT("NoUnclaimedItem", "없음") : GetItemName(ShownItem);
	if (!ShownItem.IsNone())
	{
		const FCozyItemRow* ItemDef = GetItemDef(ShownItem);
		View.StoredAmount = GetAmount(ShownItem);
		View.StorageCap = (ItemDef && ItemDef->Category == ECozyItemCategory::Material) ? Config.StorageCapPerItem : 0;
		View.CollectableNow = FMath::Min(View.Amount, GetStorageSpace(ShownItem));
	}
	return View;
}

int32 UCozyEstateSubsystem::GetReservedUnclaimed(const FCozyFacilityState& Facility) const
{
	int32 Reserved = 0;
	for (const FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.FacilityId == Facility.InstanceId && Job.Type == ECozyJobType::Processing)
		{
			Reserved += FMath::Max(0, Job.TotalRuns - Job.PaidCycles) * Job.OutputPerRun;
		}
	}
	return Reserved;
}

FName UCozyEstateSubsystem::GetLockedOutputItem(const FCozyFacilityState& Facility) const
{
	if (Facility.UnclaimedAmount > 0 && !Facility.UnclaimedItemId.IsNone())
	{
		return Facility.UnclaimedItemId;
	}
	for (const FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.FacilityId == Facility.InstanceId && Job.Type == ECozyJobType::Processing && !Job.OutputItemId.IsNone())
		{
			return Job.OutputItemId;
		}
	}
	return NAME_None;
}

bool UCozyEstateSubsystem::CanAcceptOutputItem(const FGuid& FacilityId, FName ItemId, FText& OutReason) const
{
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	if (!Facility)
	{
		OutReason = LOCTEXT("NoFacility", "시설을 찾을 수 없습니다");
		return false;
	}
	// 🙋 기준은 레시피 이름이 아니라 완료품 ID (같은 완료품을 만드는 다른 레시피는 허용 · D31)
	if (Facility->UnclaimedAmount > 0 && Facility->UnclaimedItemId != ItemId)
	{
		const FText Name = GetItemName(Facility->UnclaimedItemId);
		OutReason = FText::Format(LOCTEXT("OtherItemUnclaimed", "다른 품목을 만들려면 남은 {0}을(를) 모두 수령해 주세요 (미수령 {0} {1}개)"), Name, FText::AsNumber(Facility->UnclaimedAmount));
		return false;
	}
	for (const FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.FacilityId == FacilityId && Job.Type == ECozyJobType::Processing && Job.OutputItemId != ItemId)
		{
			OutReason = FText::Format(LOCTEXT("OtherItemInProgress", "{0}을(를) 만드는 작업이 진행 중입니다 · 다른 품목은 그 작업이 끝나거나 취소된 뒤에 만들 수 있습니다"), GetItemName(Job.OutputItemId));
			return false;
		}
	}
	return true;
}

void UCozyEstateSubsystem::AddUnclaimed(FCozyFacilityState& Facility, FName ItemId, int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	if (Facility.UnclaimedAmount <= 0)
	{
		Facility.UnclaimedItemId = ItemId;
	}
	ensureMsgf(Facility.UnclaimedItemId == ItemId, TEXT("미수령 공간에 다른 품목을 넣으려 함 (D31 위반)"));
	Facility.UnclaimedAmount += Amount;
}

FText UCozyEstateSubsystem::GetItemName(FName ItemId) const
{
	const FCozyItemRow* ItemDef = GetItemDef(ItemId);
	return ItemDef ? ItemDef->DisplayName : FText::FromName(ItemId);
}

FCozyCollectResult UCozyEstateSubsystem::CollectUnclaimed(const FGuid& FacilityId)
{
	FCozyCollectResult Result;
	FCozyFacilityState* Facility = FindFacilityMutable(FacilityId);
	if (!Facility)
	{
		Result.Message = LOCTEXT("CollectNoFacility", "시설을 찾을 수 없습니다");
		return Result;
	}
	if (GetUnclaimedTotal(FacilityId) <= 0)
	{
		Result.Message = LOCTEXT("CollectNothing", "수령할 생산물이 없습니다");
		return Result;
	}

	// 🙋 창고에 들어갈 만큼만 옮기고 나머지는 이 시설에 남김 · 생산물은 버리지 않음 (D27)
	// 시설에서 빼는 것과 창고에 넣는 것을 한 번에 처리 → 버튼을 여러 번 눌러도 같은 생산물이 두 번 들어가지 않음
	const FText FirstItemName = GetItemName(Facility->UnclaimedItemId);
	const int32 Added = AddResource(Facility->UnclaimedItemId, Facility->UnclaimedAmount);
	Facility->UnclaimedAmount -= Added;
	Result.Moved = Added;
	Result.Remaining = Facility->UnclaimedAmount;
	if (Facility->UnclaimedAmount <= 0)
	{
		// 모두 수령하면 품목 칸이 비어 다른 품목을 고를 수 있음 (D31)
		Facility->UnclaimedAmount = 0;
		Facility->UnclaimedItemId = NAME_None;
	}

	// 이번 클릭의 결과만 담는다 (현재 미수령량·창고 수량은 UI가 최신 값으로 따로 보여 줌)
	if (Result.Moved <= 0)
	{
		Result.Message = FText::Format(LOCTEXT("CollectStorageFull", "창고에 {0}을(를) 받을 공간이 없어 옮기지 못했습니다 · 생산물은 시설에 그대로"), FirstItemName);
	}
	else
	{
		Result.bSuccess = true;
		Result.Message = Result.Remaining > 0
			? FText::Format(LOCTEXT("CollectPartial", "{0} {1}개를 창고로 옮겼습니다 · 창고 공간이 부족해 일부는 시설에 남김"), FirstItemName, FText::AsNumber(Result.Moved))
			: FText::Format(LOCTEXT("CollectAll", "{0} {1}개를 창고로 옮겼습니다"), FirstItemName, FText::AsNumber(Result.Moved));
	}
	UE_LOG(LogCozyRealm, Log, TEXT("수령: %s · 옮김 %d · 남음 %d"), *GetFacilityDisplayName(FacilityId).ToString(), Result.Moved, Result.Remaining);
	NotifyChanged();
	return Result;
}

// ---------------------------------------------------------------------------
// 판매 (판매소 · 판매가는 Items.csv의 SellPrice · 대금 재화는 EstateConfig.csv의 SaleCurrencyId)

TArray<FName> UCozyEstateSubsystem::GetSaleListItems() const
{
	TArray<FName> Result;
	if (ItemTable)
	{
		ItemTable->ForeachRow<FCozyItemRow>(TEXT("Sale"), [&Result](const FName& Key, const FCozyItemRow& Row)
		{
			if (Row.Category == ECozyItemCategory::Material)
			{
				Result.Add(Key);
			}
		});
	}
	return Result;
}

FCozySellQuote UCozyEstateSubsystem::GetSellQuote(const FGuid& ShopFacilityId, FName ItemId, int32 Amount) const
{
	FCozySellQuote Quote;
	Quote.Amount = FMath::Max(0, Amount);
	Quote.CurrencyName = GetItemName(Config.SaleCurrencyId);
	const FCozyFacilityState* Facility = FindFacility(ShopFacilityId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	const FCozyItemRow* Item = GetItemDef(ItemId);
	Quote.ItemName = Item ? Item->DisplayName : FText::FromName(ItemId);
	if (!Def || !Def->Functions.Contains(ECozyFacilityFunction::Sales))
	{
		Quote.BlockReason = LOCTEXT("SellNoShop", "판매소에서만 팔 수 있습니다");
		return Quote;
	}
	if (!IsFacilityWorking(*Facility, *Def))
	{
		Quote.BlockReason = LOCTEXT("SellShopIdle", "판매소가 작동하지 않습니다");
		return Quote;
	}
	if (!Item || Item->Category != ECozyItemCategory::Material || Item->SellPrice <= 0)
	{
		Quote.BlockReason = FText::Format(LOCTEXT("SellNotAllowed", "{0}은(는) 팔 수 없는 재료입니다"), Quote.ItemName);
		return Quote;
	}
	if (!GetItemDef(Config.SaleCurrencyId))
	{
		Quote.BlockReason = LOCTEXT("SellNoCurrency", "판매 대금 재화가 데이터에 없습니다");
		return Quote;
	}
	Quote.UnitPrice = Item->SellPrice;
	Quote.TotalPrice = Item->SellPrice * Quote.Amount;
	// 창고에 있는 재료만 판다 (시설의 미수령분은 수령해야 창고 재료가 됨 · D27)
	Quote.MaxAmount = GetAmount(ItemId);
	if (Quote.MaxAmount <= 0)
	{
		Quote.BlockReason = FText::Format(LOCTEXT("SellNone", "창고에 {0} 재고가 없습니다"), Quote.ItemName);
	}
	else if (Quote.Amount < 1)
	{
		Quote.BlockReason = LOCTEXT("SellNoAmount", "판매 수량을 1개 이상 골라 주세요");
	}
	else if (Quote.Amount > Quote.MaxAmount)
	{
		Quote.BlockReason = FText::Format(LOCTEXT("SellOverMax", "선택한 {0}개는 창고 보유량 {1}개보다 많습니다 · 수량을 줄여 주세요"), FText::AsNumber(Quote.Amount), FText::AsNumber(Quote.MaxAmount));
	}
	Quote.bCanSell = Quote.BlockReason.IsEmpty();
	return Quote;
}

bool UCozyEstateSubsystem::SellItem(const FGuid& ShopFacilityId, FName ItemId, int32 Amount, FText& OutMessage)
{
	// 판매 직전 재확인 · 실패하면 재료·재화 모두 그대로
	const FCozySellQuote Quote = GetSellQuote(ShopFacilityId, ItemId, Amount);
	if (!Quote.bCanSell)
	{
		OutMessage = Quote.BlockReason;
		UE_LOG(LogCozyRealm, Log, TEXT("판매 실패: %s ×%d · %s"), *ItemId.ToString(), Amount, *OutMessage.ToString());
		return false;
	}
	const int32 Before = GetAmount(Config.SaleCurrencyId);
	State.Resources.FindOrAdd(ItemId) -= Quote.Amount;
	AddResource(Config.SaleCurrencyId, Quote.TotalPrice);
	const int32 After = GetAmount(Config.SaleCurrencyId);
	OutMessage = FText::Format(LOCTEXT("SellOk", "{0} {1}개를 팔아 {2} {3}을(를) 받았습니다"), Quote.ItemName, FText::AsNumber(Quote.Amount), Quote.CurrencyName, FText::AsNumber(Quote.TotalPrice));
	UE_LOG(LogCozyRealm, Log, TEXT("판매: %s ×%d · 개당 %d · %s %d → %d (+%d)"), *ItemId.ToString(), Quote.Amount, Quote.UnitPrice, *Config.SaleCurrencyId.ToString(), Before, After, After - Before);
	NotifyChanged();
	return true;
}

// ---------------------------------------------------------------------------
// 주민 배치

bool UCozyEstateSubsystem::CanAcceptResident(const FGuid& FacilityId, FText& OutFailReason) const
{
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		OutFailReason = LOCTEXT("NoFacility", "시설을 찾을 수 없습니다");
		return false;
	}
	if (Def->MaxResidents <= 0)
	{
		OutFailReason = LOCTEXT("NoSlots", "이 시설에는 아직 주민 배치 칸이 없습니다");
		return false;
	}
	if (Facility->AssignedResidents.Num() >= Def->MaxResidents)
	{
		OutFailReason = LOCTEXT("SlotsFull", "배치 칸이 가득 찼습니다");
		return false;
	}
	return true;
}

bool UCozyEstateSubsystem::AssignResident(const FGuid& ResidentId, const FGuid& FacilityId, FText& OutFailReason)
{
	FCozyResidentState* Resident = FindResidentMutable(ResidentId);
	if (!Resident)
	{
		OutFailReason = LOCTEXT("NoResidentFound", "주민을 찾을 수 없습니다");
		return false;
	}
	if (Resident->AssignedFacility == FacilityId)
	{
		OutFailReason = LOCTEXT("AlreadyHere", "이미 이 시설에 배치되어 있습니다");
		return false;
	}
	if (!CanAcceptResident(FacilityId, OutFailReason))
	{
		return false;
	}

	// 다른 시설에 있으면 옮긴다 (주민 이동 자체는 막지 않음 · 떠난 시설은 필요 인원 기준으로 처리)
	RemoveResidentFromFacility(*Resident);

	FCozyFacilityState* Facility = FindFacilityMutable(FacilityId);
	Facility->AssignedResidents.Add(ResidentId);
	Resident->AssignedFacility = FacilityId;
	// 필요 인원이 다시 채워지면 일시 정지했던 가공을 멈춘 시점부터 이어서 (D30)
	UpdateProcessingPause(*Facility);
	UE_LOG(LogCozyRealm, Log, TEXT("주민 배치: %s → %s"), *GetResidentDisplayName(ResidentId).ToString(), *GetFacilityDisplayName(FacilityId).ToString());
	NotifyChanged();
	return true;
}

bool UCozyEstateSubsystem::UnassignResident(const FGuid& ResidentId, FText& OutFailReason)
{
	FCozyResidentState* Resident = FindResidentMutable(ResidentId);
	if (!Resident)
	{
		OutFailReason = LOCTEXT("NoResidentFound", "주민을 찾을 수 없습니다");
		return false;
	}
	if (!Resident->AssignedFacility.IsValid())
	{
		OutFailReason = LOCTEXT("NotAssigned", "배치되어 있지 않습니다");
		return false;
	}
	UE_LOG(LogCozyRealm, Log, TEXT("주민 배치 해제: %s ← %s"), *GetResidentDisplayName(ResidentId).ToString(), *GetFacilityDisplayName(Resident->AssignedFacility).ToString());
	RemoveResidentFromFacility(*Resident);
	NotifyChanged();
	return true;
}

void UCozyEstateSubsystem::RemoveResidentFromFacility(FCozyResidentState& Resident)
{
	if (!Resident.AssignedFacility.IsValid())
	{
		return;
	}
	if (FCozyFacilityState* OldFacility = FindFacilityMutable(Resident.AssignedFacility))
	{
		OldFacility->AssignedResidents.Remove(Resident.InstanceId);
		// 자동 생산은 주기 초기화(D26), 가공은 진행도를 유지한 채 일시 정지(D30) · 주민 이동 자체는 막지 않음
		CancelProductionIfUnderstaffed(*OldFacility);
		UpdateProcessingPause(*OldFacility);
	}
	Resident.AssignedFacility.Invalidate();
}

void UCozyEstateSubsystem::CancelProductionIfUnderstaffed(FCozyFacilityState& Facility)
{
	const FCozyFacilityRow* Def = GetFacilityDef(Facility.DefinitionId);
	if (!Def || !Def->Functions.Contains(ECozyFacilityFunction::Production) || IsFacilityWorking(Facility, *Def))
	{
		// 주민 한 명이 빠져도 필요 인원을 채우고 있으면 그대로 진행
		return;
	}
	const int32 JobIndex = State.Jobs.IndexOfByPredicate([&Facility](const FCozyJobRecord& J) { return J.JobId == Facility.ActiveJobId; });
	if (JobIndex != INDEX_NONE)
	{
		// 진행분은 버리고 작업 기록에서 지움 · 이미 지급한 생산물은 재료 보유량에 그대로 남음
		State.Jobs.RemoveAt(JobIndex);
		UE_LOG(LogCozyRealm, Log, TEXT("필요 인원 미달: %s의 생산 주기를 취소하고 진행도를 초기화"), *GetFacilityDisplayName(Facility.InstanceId).ToString());
	}
	Facility.ActiveJobId.Invalidate();
}

// ---------------------------------------------------------------------------
// 시간

void UCozyEstateSubsystem::SetTimeScale(float NewScale)
{
	TimeScale = FMath::Clamp(NewScale, 0.f, 100.f);
	NotifyChanged();
}

bool UCozyEstateSubsystem::IsNight() const
{
	switch (NightOverride)
	{
	case ECozyNightOverride::ForceDay:
		return false;
	case ECozyNightOverride::ForceNight:
		return true;
	default:
		break;
	}
	// 밤 판정은 PC 현지 시각 (기획서 11 기반 시스템)
	const int32 Hour = FDateTime::Now().GetHour();
	return Config.NightStartHour > Config.NightEndHour
		? (Hour >= Config.NightStartHour || Hour < Config.NightEndHour)
		: (Hour >= Config.NightStartHour && Hour < Config.NightEndHour);
}

void UCozyEstateSubsystem::SetNightOverride(ECozyNightOverride NewOverride)
{
	NightOverride = NewOverride;
	NotifyChanged();
}

void UCozyEstateSubsystem::Tick(float DeltaTime)
{
	if (!bStarted)
	{
		return;
	}

	// 자동 저장은 실제 시간 기준 (배속과 무관)
	AutosaveAccumulator += DeltaTime;
	if (Config.AutosaveSeconds > 0.f && AutosaveAccumulator >= Config.AutosaveSeconds)
	{
		SaveEstate(TEXT("자동"));
	}

	StepAccumulator += static_cast<double>(DeltaTime) * TimeScale;
	int32 Steps = 0;
	while (StepAccumulator >= 1.0 && Steps < CozyEstate::MaxStepsPerTick)
	{
		StepAccumulator -= 1.0;
		State.GameSeconds += 1.0;
		StepJobs();
		++Steps;
	}
	if (Steps > 0)
	{
		// 시간만 흐름: UI는 숫자·진행 바만 갱신 (버튼을 다시 만들면 클릭이 끊김)
		// 업그레이드가 끝났으면 구조 변경으로 알림 (레벨·해금 작물 표시 갱신)
		NotifyChanged(bStructuralPending);
		bStructuralPending = false;
	}
}

TStatId UCozyEstateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCozyEstateSubsystem, STATGROUP_Tickables);
}

bool UCozyEstateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ---------------------------------------------------------------------------
// 작업 진행

void UCozyEstateSubsystem::StepJobs()
{
	for (FCozyFacilityState& Facility : State.Facilities)
	{
		const FCozyFacilityRow* Def = GetFacilityDef(Facility.DefinitionId);
		if (Def && Def->Functions.Contains(ECozyFacilityFunction::Production))
		{
			StepProduction(Facility, *Def);
		}
		if (Def && Def->Functions.Contains(ECozyFacilityFunction::Processing))
		{
			StepProcessing(Facility, *Def);
		}
	}
	StepGrowth();
}

bool UCozyEstateSubsystem::IsFacilityWorking(const FCozyFacilityState& Facility, const FCozyFacilityRow& Def) const
{
	if (!Def.bRequiresResidents)
	{
		return true;
	}
	return Facility.AssignedResidents.Num() >= FMath::Max(1, Def.MinResidents);
}

double UCozyEstateSubsystem::GetProductionDuration(const FCozyFacilityState& Facility, const FCozyCropRow& Crop, const FCozyFacilityRow& Def) const
{
	// 속도 배율이 오르면 한 주기 시간이 짧아짐 · 주기를 새로 시작할 때만 계산하므로 진행 중인 주기는 그대로 (D40)
	return FMath::Max(0.1, static_cast<double>(Crop.ProductionSeconds) / GetSpeedMultiplier(Facility, Def));
}

void UCozyEstateSubsystem::StepProduction(FCozyFacilityState& Facility, const FCozyFacilityRow& Def)
{
	const FCozyCropRow* Crop = GetCropDef(Facility.SelectedCropId);
	if (!Crop || !IsFacilityWorking(Facility, Def))
	{
		// 필요 인원 미달이면 진행 중인 주기를 취소 (다시 채워지면 0초부터 새 주기)
		CancelProductionIfUnderstaffed(Facility);
		return;
	}

	FCozyJobRecord* Job = FindJobMutable(Facility.ActiveJobId);
	if (!Job)
	{
		FCozyJobRecord NewJob;
		NewJob.JobId = FGuid::NewGuid();
		NewJob.FacilityId = Facility.InstanceId;
		NewJob.Type = ECozyJobType::Production;
		NewJob.ContentId = Facility.SelectedCropId;
		NewJob.StartGameSeconds = State.GameSeconds;
		NewJob.DurationSeconds = GetProductionDuration(Facility, *Crop, Def);
		Facility.ActiveJobId = NewJob.JobId;
		UE_LOG(LogCozyRealm, Log, TEXT("생산 주기 시작: %s(%s) %s · 한 주기 %.2f초 (속도 ×%.2f)"), *GetFacilityDisplayName(Facility.InstanceId).ToString(),
			*Facility.GridCoord.ToString(), *Facility.SelectedCropId.ToString(), NewJob.DurationSeconds, GetSpeedMultiplier(Facility, Def));
		State.Jobs.Add(NewJob);
		return;
	}

	// 보류(미수령 공간 가득) 중이면 이 시설에 자리가 생겼을 때 이어서 진행
	const int32 Capacity = Def.UnclaimedCapacity;
	auto HasUnclaimedSpace = [&Facility, Capacity, Crop]()
	{
		// 한 번에 한 종류만 (D31): 비어 있거나 같은 품목이고, 한도 안일 때만
		const bool bSameItem = Facility.UnclaimedAmount <= 0 || Facility.UnclaimedItemId == Crop->ProducedItem;
		return bSameItem && Facility.UnclaimedAmount + Crop->ProducedAmount <= Capacity;
	};
	if (Job->State == ECozyJobState::Held)
	{
		if (!HasUnclaimedSpace())
		{
			return;
		}
		Job->PausedSeconds += State.GameSeconds - Job->HeldSinceGameSeconds;
		Job->State = ECozyJobState::Running;
		Job->HeldReason = FText::GetEmpty();
	}

	const double Elapsed = State.GameSeconds - Job->StartGameSeconds - Job->PausedSeconds;
	const int32 DueCycles = Job->DurationSeconds > 0.0 ? FMath::FloorToInt32(Elapsed / Job->DurationSeconds) : 0;
	const int32 PaidBefore = Job->PaidCycles;
	while (Job->PaidCycles < DueCycles)
	{
		// 🙋 이 시설의 미수령 공간이 꽉 차면 이 시설만 생산을 멈춘다 (D27 · 공용 창고 한도와 별개)
		if (!HasUnclaimedSpace())
		{
			Job->State = ECozyJobState::Held;
			Job->HeldSinceGameSeconds = State.GameSeconds;
			Job->HeldReason = LOCTEXT("UnclaimedFull", "미수령 공간이 가득 차 생산을 멈췄습니다");
			break;
		}
		// 완료분은 보유 재료가 아니라 시설의 미수령분으로 · 주기 기록과 함께 (같은 주기를 두 번 넘기지 않음)
		AddUnclaimed(Facility, Crop->ProducedItem, Crop->ProducedAmount);
		++Job->PaidCycles;
	}
	// 🙋 공통 성장 효과가 바뀌었으면 진행 중인 주기는 그대로 끝내고, 다음 주기는 새 속도로 새 작업을 시작 (D40)
	// 이번 단계에서 주기가 실제로 끝났을 때만 교체 → 진행 중인 주기의 시간·진행도는 바뀌지 않음
	if (Job->State == ECozyJobState::Running && Job->PaidCycles > PaidBefore
		&& !FMath::IsNearlyEqual(Job->DurationSeconds, GetProductionDuration(Facility, *Crop, Def), 0.001))
	{
		const double CycleStart = Job->StartGameSeconds + Job->PausedSeconds + Job->PaidCycles * Job->DurationSeconds;
		const FGuid OldJobId = Job->JobId;
		State.Jobs.RemoveAll([&OldJobId](const FCozyJobRecord& J) { return J.JobId == OldJobId; });
		FCozyJobRecord NewJob;
		NewJob.JobId = FGuid::NewGuid();
		NewJob.FacilityId = Facility.InstanceId;
		NewJob.Type = ECozyJobType::Production;
		NewJob.ContentId = Facility.SelectedCropId;
		NewJob.StartGameSeconds = CycleStart;
		NewJob.DurationSeconds = GetProductionDuration(Facility, *Crop, Def);
		Facility.ActiveJobId = NewJob.JobId;
		State.Jobs.Add(NewJob);
		UE_LOG(LogCozyRealm, Log, TEXT("생산 주기 시작(새 속도 적용): %s(%s) %s · 한 주기 %.2f초 (속도 ×%.2f)"), *GetFacilityDisplayName(Facility.InstanceId).ToString(),
			*Facility.GridCoord.ToString(), *Facility.SelectedCropId.ToString(), NewJob.DurationSeconds, GetSpeedMultiplier(Facility, Def));
	}
}

// ---------------------------------------------------------------------------
// 공통 가공 (D28~D33 · 제분소·제빵소·공방이 모두 이 함수들을 씀)

const FCozyRecipeRow* UCozyEstateSubsystem::GetRecipeDef(FName Id) const
{
	return RecipeTable ? RecipeTable->FindRow<FCozyRecipeRow>(Id, TEXT(""), false) : nullptr;
}

TArray<FName> UCozyEstateSubsystem::GetFacilityRecipes(const FGuid& FacilityId) const
{
	TArray<FName> Result;
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	if (!Facility || !RecipeTable)
	{
		return Result;
	}
	RecipeTable->ForeachRow<FCozyRecipeRow>(TEXT("Recipes"), [&](const FName& Key, const FCozyRecipeRow& Row)
	{
		if (Row.FacilityId == Facility->DefinitionId && (!Row.bTestOnly || bShowTestRecipes))
		{
			Result.Add(Key);
		}
	});
	return Result;
}

void UCozyEstateSubsystem::SetShowTestRecipes(bool bShow)
{
	bShowTestRecipes = bShow;
	NotifyChanged();
}

double UCozyEstateSubsystem::GetProcessingDuration(const FCozyFacilityState& Facility, const FCozyRecipeRow& Recipe, const FCozyFacilityRow& Def) const
{
	return FMath::Max(0.1, static_cast<double>(Recipe.Seconds) / GetSpeedMultiplier(Facility, Def));
}

FCozyRecipeQuote UCozyEstateSubsystem::GetRecipeQuote(const FGuid& FacilityId, FName RecipeId, int32 Runs) const
{
	FCozyRecipeQuote Quote;
	Quote.Runs = FMath::Max(0, Runs);
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	const FCozyRecipeRow* Recipe = GetRecipeDef(RecipeId);
	if (!Def || !Recipe || Recipe->FacilityId != Facility->DefinitionId || !Def->Functions.Contains(ECozyFacilityFunction::Processing))
	{
		Quote.BlockReason = LOCTEXT("QuoteInvalid", "이 시설에서 만들 수 없는 레시피입니다");
		return Quote;
	}
	Quote.bValid = true;
	Quote.OutputPerRun = FMath::Max(1, Recipe->OutputAmount);
	Quote.OutputName = GetItemName(Recipe->OutputItem);
	Quote.SecondsPerRun = GetProcessingDuration(*Facility, *Recipe, *Def);
	Quote.TotalOutput = Quote.OutputPerRun * Quote.Runs;
	Quote.TotalSeconds = Quote.SecondsPerRun * Quote.Runs;

	// ① 재료: 창고의 사용 가능한 보유량으로 몇 회분인가 (미수령분은 재료가 아님 · D28)
	Quote.MaxByMaterials = MAX_int32;
	FText MaterialReason;
	for (const TPair<FName, int32>& Input : Recipe->Inputs)
	{
		FCozyRecipeQuote::FInput& Line = Quote.Inputs.AddDefaulted_GetRef();
		Line.ItemId = Input.Key;
		Line.Need = Input.Value * Quote.Runs;
		Line.Have = GetAmount(Input.Key);
		const int32 PerRun = FMath::Max(1, Input.Value);
		const int32 Possible = Line.Have / PerRun;
		if (Possible < Quote.MaxByMaterials)
		{
			Quote.MaxByMaterials = Possible;
			if (Possible <= 0)
			{
				MaterialReason = FText::Format(LOCTEXT("QuoteNoMaterial", "재료가 부족합니다 · 1회에 {0} {1}개 필요 (보유 {2}개)"), GetItemName(Input.Key), FText::AsNumber(Input.Value), FText::AsNumber(Line.Have));
			}
		}
	}
	if (Quote.MaxByMaterials == MAX_int32)
	{
		Quote.MaxByMaterials = 0;
	}

	// ② 공간: 남은 미수령 공간 − 진행·일시 정지 작업이 확보한 공간 (D32·D33)
	const int32 FreeSpace = FMath::Max(0, Def->UnclaimedCapacity - Facility->UnclaimedAmount - GetReservedUnclaimed(*Facility));
	Quote.MaxBySpace = FreeSpace / Quote.OutputPerRun;

	// ③ 시작 조건: 주민 · 가공 칸 · 같은 품목 (안 맞으면 최대 0회)
	FText ConditionReason;
	int32 ActiveJobs = 0;
	for (const FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.FacilityId == FacilityId && Job.Type == ECozyJobType::Processing)
		{
			++ActiveJobs;
		}
	}
	FText ItemReason;
	if (IsFacilityUpgrading(FacilityId))
	{
		// 🙋 업그레이드 중에는 새 가공을 시작할 수 없음 · 완료 후 다시 가공 (D36)
		ConditionReason = LOCTEXT("QuoteUpgrading", "업그레이드 중이라 가공을 시작할 수 없습니다 · 업그레이드가 끝나면 다시 가공할 수 있습니다");
	}
	else if (!IsFacilityWorking(*Facility, *Def))
	{
		ConditionReason = FText::Format(LOCTEXT("QuoteNoResident", "주민이 부족해 가공을 시작할 수 없습니다 (필요 {0}명)"), FText::AsNumber(FMath::Max(1, Def->MinResidents)));
	}
	else if (ActiveJobs >= Def->ProcessingSlots)
	{
		ConditionReason = FText::Format(LOCTEXT("QuoteNoSlot", "가공 칸이 모두 사용 중입니다 (동시 {0}개) · 지금 작업이 끝나거나 취소되면 시작할 수 있습니다"), FText::AsNumber(Def->ProcessingSlots));
	}
	else if (!CanAcceptOutputItem(FacilityId, Recipe->OutputItem, ItemReason))
	{
		ConditionReason = ItemReason;
	}

	Quote.MaxRuns = ConditionReason.IsEmpty() ? FMath::Min(Quote.MaxByMaterials, Quote.MaxBySpace) : 0;

	if (!ConditionReason.IsEmpty())
	{
		Quote.BlockReason = ConditionReason;
	}
	else if (Quote.MaxByMaterials <= 0)
	{
		Quote.BlockReason = MaterialReason;
	}
	else if (Quote.MaxBySpace <= 0)
	{
		Quote.BlockReason = FText::Format(LOCTEXT("QuoteNoSpace", "미수령 공간이 부족합니다 · 남은 공간 {0}개 (1회에 {1}개 필요) · 완료품을 수령하면 공간이 생깁니다"), FText::AsNumber(FreeSpace), FText::AsNumber(Quote.OutputPerRun));
	}
	else if (Quote.Runs < 1)
	{
		Quote.BlockReason = LOCTEXT("QuoteNoRuns", "제작 횟수를 1회 이상 골라 주세요");
	}
	else if (Quote.Runs > Quote.MaxRuns)
	{
		Quote.BlockReason = FText::Format(LOCTEXT("QuoteOverMax", "선택한 {0}회는 지금 최대 {1}회를 넘습니다 · 수량을 줄여 주세요"), FText::AsNumber(Quote.Runs), FText::AsNumber(Quote.MaxRuns));
	}
	Quote.bCanStart = Quote.BlockReason.IsEmpty();
	return Quote;
}

bool UCozyEstateSubsystem::StartProcessing(const FGuid& FacilityId, FName RecipeId, int32 Runs, FText& OutMessage)
{
	// 시작 직전 조건 재확인 · 실패하면 재료 차감·작업 등록 모두 하지 않음 (D28·D32)
	const FCozyRecipeQuote Quote = GetRecipeQuote(FacilityId, RecipeId, Runs);
	FCozyFacilityState* Facility = FindFacilityMutable(FacilityId);
	const FCozyRecipeRow* Recipe = GetRecipeDef(RecipeId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Quote.bCanStart || !Facility || !Recipe || !Def)
	{
		OutMessage = Quote.BlockReason.IsEmpty() ? LOCTEXT("StartFailed", "가공을 시작할 수 없습니다") : Quote.BlockReason;
		UE_LOG(LogCozyRealm, Log, TEXT("가공 시작 실패: %s %s ×%d · %s"), *GetFacilityDisplayName(FacilityId).ToString(), *RecipeId.ToString(), Runs, *OutMessage.ToString());
		return false;
	}

	// 선택한 전체 횟수의 재료를 한 번에 차감 · 회차마다 다시 차감하지 않음 (D33)
	FString UsedText;
	for (const FCozyRecipeQuote::FInput& Input : Quote.Inputs)
	{
		State.Resources.FindOrAdd(Input.ItemId) -= Input.Need;
		UsedText += FString::Printf(TEXT("%s%s %d개"), UsedText.IsEmpty() ? TEXT("") : TEXT(", "), *GetItemName(Input.ItemId).ToString(), Input.Need);
	}

	// 작업 등록 · 남은 회차 × 1회 개수만큼 미수령 공간을 확보한 것으로 계산됨
	FCozyJobRecord Job;
	Job.JobId = FGuid::NewGuid();
	Job.FacilityId = FacilityId;
	Job.Type = ECozyJobType::Processing;
	Job.ContentId = RecipeId;
	Job.StartGameSeconds = State.GameSeconds;
	Job.DurationSeconds = Quote.SecondsPerRun;
	Job.TotalRuns = Runs;
	Job.OutputItemId = Recipe->OutputItem;
	Job.OutputPerRun = Quote.OutputPerRun;
	State.Jobs.Add(Job);

	OutMessage = FText::Format(LOCTEXT("StartOk", "{0} {1}회 제작을 시작했습니다 (완료품 {2}개) · 재료 {3} 사용"),
		Quote.OutputName, FText::AsNumber(Runs), FText::AsNumber(Quote.TotalOutput), FText::FromString(UsedText));
	UE_LOG(LogCozyRealm, Log, TEXT("가공 시작: %s %s ×%d · 재료 %s 차감 · 확보 %d"), *GetFacilityDisplayName(FacilityId).ToString(), *RecipeId.ToString(), Runs, *UsedText, Quote.TotalOutput);
	NotifyChanged();
	return true;
}

bool UCozyEstateSubsystem::CancelProcessing(const FGuid& JobId, FText& OutMessage)
{
	const int32 JobIndex = State.Jobs.IndexOfByPredicate([&JobId](const FCozyJobRecord& J) { return J.JobId == JobId && J.Type == ECozyJobType::Processing; });
	if (JobIndex == INDEX_NONE)
	{
		OutMessage = LOCTEXT("CancelNoJob", "취소할 가공 작업이 없습니다");
		return false;
	}
	const FCozyJobRecord Job = State.Jobs[JobIndex];
	// 완성된 회차의 완료품은 이미 미수령분에 있으므로 그대로 둔다 · 미완료 회차는 재료 반환·완료품 없이 종료
	// 작업 기록을 지우면 그 작업이 확보했던 공간도 함께 풀린다 (D29·D33)
	State.Jobs.RemoveAt(JobIndex);
	const int32 Done = Job.PaidCycles;
	const int32 Dropped = FMath::Max(0, Job.TotalRuns - Job.PaidCycles);
	OutMessage = FText::Format(LOCTEXT("CancelOk", "{0} 제작을 취소했습니다 · 완성된 {1}회분({2}개)은 시설에 남김 · 미완료 {3}회는 재료를 돌려받지 않고 종료"),
		GetItemName(Job.OutputItemId), FText::AsNumber(Done), FText::AsNumber(Done * Job.OutputPerRun), FText::AsNumber(Dropped));
	UE_LOG(LogCozyRealm, Log, TEXT("가공 취소: %s · 완성 %d회 유지 · 미완료 %d회 종료(반환 없음)"), *GetFacilityDisplayName(Job.FacilityId).ToString(), Done, Dropped);
	NotifyChanged();
	return true;
}

TArray<FCozyProcessingJobView> UCozyEstateSubsystem::GetProcessingJobs(const FGuid& FacilityId) const
{
	TArray<FCozyProcessingJobView> Result;
	for (const FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.FacilityId != FacilityId || Job.Type != ECozyJobType::Processing)
		{
			continue;
		}
		FCozyProcessingJobView& View = Result.AddDefaulted_GetRef();
		View.JobId = Job.JobId;
		View.OutputName = GetItemName(Job.OutputItemId);
		View.CompletedRuns = Job.PaidCycles;
		View.TotalRuns = Job.TotalRuns;
		View.OutputPerRun = Job.OutputPerRun;
		View.bPaused = Job.State == ECozyJobState::Held;
		// 일시 정지 중에는 멈춘 시각까지만 진행으로 침 (멈춘 시간은 진행에 포함하지 않음 · D30)
		const double Now = View.bPaused ? Job.HeldSinceGameSeconds : State.GameSeconds;
		const double Elapsed = FMath::Max(0.0, Now - Job.StartGameSeconds - Job.PausedSeconds);
		const double IntoRun = FMath::Clamp(Elapsed - Job.PaidCycles * Job.DurationSeconds, 0.0, Job.DurationSeconds);
		View.RunProgress01 = Job.DurationSeconds > 0.0 ? static_cast<float>(IntoRun / Job.DurationSeconds) : 0.f;
		View.RunRemainingSeconds = static_cast<float>(Job.DurationSeconds - IntoRun);
		View.TotalRemainingSeconds = static_cast<float>(FMath::Max(0, Job.TotalRuns - Job.PaidCycles) * Job.DurationSeconds - IntoRun);
		View.Status = View.bPaused ? Job.HeldReason
			: FText::Format(LOCTEXT("ProcessingRun", "{0} 제작 중 · {1}/{2}회째"), View.OutputName, FText::AsNumber(FMath::Min(Job.PaidCycles + 1, Job.TotalRuns)), FText::AsNumber(Job.TotalRuns));
	}
	return Result;
}

void UCozyEstateSubsystem::UpdateProcessingPause(FCozyFacilityState& Facility)
{
	const FCozyFacilityRow* Def = GetFacilityDef(Facility.DefinitionId);
	if (!Def || !Def->Functions.Contains(ECozyFacilityFunction::Processing))
	{
		return;
	}
	const bool bWorking = IsFacilityWorking(Facility, *Def);
	for (FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.FacilityId != Facility.InstanceId || Job.Type != ECozyJobType::Processing)
		{
			continue;
		}
		if (!bWorking && Job.State == ECozyJobState::Running)
		{
			// 🙋 판단 기준은 '필요 인원 미달' · 현재 회차 진행도와 투입 재료를 그대로 둔 채 멈춤
			Job.State = ECozyJobState::Held;
			Job.HeldSinceGameSeconds = State.GameSeconds;
			Job.HeldReason = LOCTEXT("ProcessingPaused", "주민 부족으로 가공 일시 정지");
			UE_LOG(LogCozyRealm, Log, TEXT("가공 일시 정지(주민 부족): %s · %d/%d회 완료"), *GetFacilityDisplayName(Facility.InstanceId).ToString(), Job.PaidCycles, Job.TotalRuns);
		}
		else if (bWorking && Job.State == ECozyJobState::Held)
		{
			// 멈춘 시점부터 이어서 · 재료를 다시 차감하지 않음
			Job.PausedSeconds += State.GameSeconds - Job.HeldSinceGameSeconds;
			Job.State = ECozyJobState::Running;
			Job.HeldReason = FText::GetEmpty();
			UE_LOG(LogCozyRealm, Log, TEXT("가공 재개: %s · 멈춘 시간 %.0f초는 진행에서 제외"), *GetFacilityDisplayName(Facility.InstanceId).ToString(), Job.PausedSeconds);
		}
	}
}

void UCozyEstateSubsystem::StepProcessing(FCozyFacilityState& Facility, const FCozyFacilityRow& Def)
{
	UpdateProcessingPause(Facility);
	for (int32 Index = State.Jobs.Num() - 1; Index >= 0; --Index)
	{
		FCozyJobRecord& Job = State.Jobs[Index];
		if (Job.FacilityId != Facility.InstanceId || Job.Type != ECozyJobType::Processing || Job.State != ECozyJobState::Running)
		{
			continue;
		}
		// 한 회씩 순서대로: 회차가 끝날 때마다 완료품이 미수령분에 쌓임 (D33) · 공간은 시작할 때 확보해 둠
		const double Elapsed = State.GameSeconds - Job.StartGameSeconds - Job.PausedSeconds;
		const int32 DueRuns = Job.DurationSeconds > 0.0 ? FMath::Min(Job.TotalRuns, FMath::FloorToInt32(Elapsed / Job.DurationSeconds)) : 0;
		while (Job.PaidCycles < DueRuns)
		{
			AddUnclaimed(Facility, Job.OutputItemId, Job.OutputPerRun);
			++Job.PaidCycles;
			UE_LOG(LogCozyRealm, Log, TEXT("가공 회차 완료: %s %s %d/%d회 · 미수령 %d"), *GetFacilityDisplayName(Facility.InstanceId).ToString(), *Job.OutputItemId.ToString(), Job.PaidCycles, Job.TotalRuns, Facility.UnclaimedAmount);
		}
		if (Job.PaidCycles >= Job.TotalRuns)
		{
			UE_LOG(LogCozyRealm, Log, TEXT("가공 완료: %s %s %d회 모두 끝남"), *GetFacilityDisplayName(Facility.InstanceId).ToString(), *Job.OutputItemId.ToString(), Job.TotalRuns);
			State.Jobs.RemoveAt(Index);
		}
	}
}

// ---------------------------------------------------------------------------
// 공통 성장 · 업그레이드 (후신소 · 신사·시설·관리 시설이 모두 같은 처리 · 차이는 성장 설정표의 행)

const FCozyGrowthRow* UCozyEstateSubsystem::FindGrowthRow(FName DefinitionId, int32 FromLevel, FName* OutRowId) const
{
	const FCozyGrowthRow* Found = nullptr;
	if (GrowthTable)
	{
		GrowthTable->ForeachRow<FCozyGrowthRow>(TEXT("Growth"), [&](const FName& Key, const FCozyGrowthRow& Row)
		{
			if (!Found && Row.TargetFacilityId == DefinitionId && Row.FromLevel == FromLevel)
			{
				Found = &Row;
				if (OutRowId)
				{
					*OutRowId = Key;
				}
			}
		});
	}
	return Found;
}

int32 UCozyEstateSubsystem::GetHighestLevelOf(FName DefinitionId) const
{
	int32 Level = 0;
	for (const FCozyFacilityState& Facility : State.Facilities)
	{
		if (Facility.DefinitionId == DefinitionId)
		{
			Level = FMath::Max(Level, Facility.Level);
		}
	}
	return Level;
}

double UCozyEstateSubsystem::GetSpeedMultiplier(const FCozyFacilityState& Facility, const FCozyFacilityRow& Def) const
{
	// 🙋 관리 시설이 정해진 시설(밭)은 자기 레벨이 아니라 관리 시설 단계의 공통 효과를 받음 (D39)
	// 관리 시설이 아직 없으면 기본 속도 · 이후 새로 지은 밭도 같은 계산이라 현재 단계 효과를 받음
	if (!Def.ManagerFacilityId.IsNone())
	{
		const FCozyFacilityRow* ManagerDef = GetFacilityDef(Def.ManagerFacilityId);
		const int32 ManagerLevel = GetHighestLevelOf(Def.ManagerFacilityId);
		if (!ManagerDef || ManagerLevel <= 0)
		{
			return 1.0;
		}
		return 1.0 + ManagerDef->SpeedBonusPerLevel * FMath::Max(0, ManagerLevel - 1);
	}
	return 1.0 + Def.SpeedBonusPerLevel * FMath::Max(0, Facility.Level - 1);
}

int32 UCozyEstateSubsystem::GetShrineLevel() const
{
	int32 Level = 0;
	for (const FCozyFacilityState& Facility : State.Facilities)
	{
		const FCozyFacilityRow* Def = GetFacilityDef(Facility.DefinitionId);
		if (Def && Def->Functions.Contains(ECozyFacilityFunction::ShrineCore))
		{
			Level = FMath::Max(Level, Facility.Level);
		}
	}
	return Level;
}

int32 UCozyEstateSubsystem::GetUpgradeSlotCount() const
{
	int32 Slots = 0;
	for (const FCozyFacilityState& Facility : State.Facilities)
	{
		const FCozyFacilityRow* Def = GetFacilityDef(Facility.DefinitionId);
		if (Def && Def->Functions.Contains(ECozyFacilityFunction::UpgradeQueue))
		{
			Slots += Def->UpgradeSlots;
		}
	}
	return Slots;
}

bool UCozyEstateSubsystem::IsFacilityUpgrading(const FGuid& FacilityId) const
{
	return State.Jobs.ContainsByPredicate([&FacilityId](const FCozyJobRecord& Job) { return Job.FacilityId == FacilityId && Job.Type == ECozyJobType::Growth; });
}

TArray<FGuid> UCozyEstateSubsystem::GetUpgradableFacilities() const
{
	TSet<FName> Targets;
	if (GrowthTable)
	{
		GrowthTable->ForeachRow<FCozyGrowthRow>(TEXT("Targets"), [&Targets](const FName& Key, const FCozyGrowthRow& Row)
		{
			Targets.Add(Row.TargetFacilityId);
		});
	}
	TArray<FGuid> Result;
	for (const FCozyFacilityState& Facility : State.Facilities)
	{
		if (Targets.Contains(Facility.DefinitionId))
		{
			Result.Add(Facility.InstanceId);
		}
	}
	return Result;
}

FCozyUpgradeQuote UCozyEstateSubsystem::GetUpgradeQuote(const FGuid& FacilityId) const
{
	FCozyUpgradeQuote Quote;
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		Quote.BlockReason = LOCTEXT("UpNoFacility", "시설을 찾을 수 없습니다");
		return Quote;
	}
	Quote.FromLevel = Facility->Level;
	Quote.ToLevel = Facility->Level + 1;
	Quote.bUpgrading = IsFacilityUpgrading(FacilityId);
	const FCozyGrowthRow* Row = FindGrowthRow(Facility->DefinitionId, Facility->Level, &Quote.GrowthRowId);
	if (!Row)
	{
		Quote.BlockReason = Quote.bUpgrading ? LOCTEXT("UpBusyNoRow", "업그레이드 중입니다")
			: FText::Format(LOCTEXT("UpNoRow", "다음 단계(Lv{0} → Lv{1}) 업그레이드가 아직 없습니다"), FText::AsNumber(Quote.FromLevel), FText::AsNumber(Quote.ToLevel));
		return Quote;
	}
	Quote.bValid = true;
	Quote.Seconds = Row->Seconds;
	Quote.RequiredShrineLevel = Row->RequiredShrineLevel;
	for (const TPair<FName, int32>& Cost : Row->StartCost)
	{
		Quote.Costs.Add({ Cost.Key, Cost.Value, GetAmount(Cost.Key) });
	}
	for (const FName& CropId : Row->UnlockCrops)
	{
		const FCozyCropRow* Crop = GetCropDef(CropId);
		Quote.UnlockCropNames.Add(Crop ? Crop->DisplayName : FText::FromName(CropId));
	}
	for (const FName& UnlockId : Row->UnlockFacilities)
	{
		const FCozyFacilityRow* Unlock = GetFacilityDef(UnlockId);
		Quote.UnlockFacilityNames.Add(Unlock ? Unlock->DisplayName : FText::FromName(UnlockId));
	}
	// 조건 목록: 신사 상한 → 선행 시설 (UI의 ✅ 표시와 '이동' 버튼용 · D41)
	if (Row->RequiredShrineLevel > 0)
	{
		FName ShrineDefId;
		for (const FCozyFacilityState& Other : State.Facilities)
		{
			const FCozyFacilityRow* OtherDef = GetFacilityDef(Other.DefinitionId);
			if (OtherDef && OtherDef->Functions.Contains(ECozyFacilityFunction::ShrineCore))
			{
				ShrineDefId = Other.DefinitionId;
				break;
			}
		}
		const int32 ShrineNow = GetShrineLevel();
		Quote.Conditions.Add({ FText::Format(LOCTEXT("CondShrine", "신사 Lv{0}"), FText::AsNumber(Row->RequiredShrineLevel)),
			ShrineNow >= Row->RequiredShrineLevel, ShrineDefId, Row->RequiredShrineLevel, ShrineNow });
	}
	for (const TPair<FName, int32>& Required : Row->RequiredFacilities)
	{
		const FCozyFacilityRow* RequiredDef = GetFacilityDef(Required.Key);
		const int32 Have = GetHighestLevelOf(Required.Key);
		Quote.Conditions.Add({ FText::Format(LOCTEXT("CondFacility", "{0} Lv{1}"), RequiredDef ? RequiredDef->DisplayName : FText::FromName(Required.Key), FText::AsNumber(Required.Value)),
			Have >= Required.Value, Required.Key, Required.Value, Have });
	}
	if (Def->Functions.Contains(ECozyFacilityFunction::FieldManagement))
	{
		Quote.NextFieldSpeedMultiplier = 1.f + Def->SpeedBonusPerLevel * FMath::Max(0, Quote.ToLevel - 1);
	}

	// 🙋 D36: 가공 시설이면 진행·일시 정지 중인 가공을 종료하고 미완료 회차의 재료를 반환 (완성된 회차는 미수령분에 남음)
	TMap<FName, int32> RefundMap;
	for (const FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.FacilityId != FacilityId || Job.Type != ECozyJobType::Processing)
		{
			continue;
		}
		const int32 Remaining = FMath::Max(0, Job.TotalRuns - Job.PaidCycles);
		if (const FCozyRecipeRow* Recipe = GetRecipeDef(Job.ContentId))
		{
			for (const TPair<FName, int32>& Input : Recipe->Inputs)
			{
				RefundMap.FindOrAdd(Input.Key) += Input.Value * Remaining;
			}
		}
		Quote.EndingJobs.Add(FText::Format(LOCTEXT("UpEndingJob", "{0} {1}/{2}회 완성 · 미완료 {3}회 종료"),
			GetItemName(Job.OutputItemId), FText::AsNumber(Job.PaidCycles), FText::AsNumber(Job.TotalRuns), FText::AsNumber(Remaining)));
	}
	for (const TPair<FName, int32>& Refund : RefundMap)
	{
		if (Refund.Value > 0)
		{
			Quote.Refunds.Add({ Refund.Key, Refund.Value, GetAmount(Refund.Key) });
		}
	}

	int32 ActiveUpgrades = 0;
	for (const FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.Type == ECozyJobType::Growth)
		{
			++ActiveUpgrades;
		}
	}
	const int32 Slots = GetUpgradeSlotCount();
	const int32 ShrineLevel = GetShrineLevel();

	if (Quote.bUpgrading)
	{
		Quote.BlockReason = LOCTEXT("UpBusy", "이미 업그레이드 중입니다");
	}
	else if (Slots <= 0)
	{
		Quote.BlockReason = LOCTEXT("UpNoQueue", "후신소가 없어 업그레이드할 수 없습니다");
	}
	else if (ActiveUpgrades >= Slots)
	{
		// 🙋 슬롯이 가득 차면 시작하지 않음 · 비용 차감·작업 등록 없음 (D25)
		Quote.BlockReason = FText::Format(LOCTEXT("UpSlotsFull", "동시 작업 한도에 도달했습니다 (동시 {0}개) · 진행 중인 업그레이드가 끝나면 시작할 수 있습니다"), FText::AsNumber(Slots));
	}
	else if (ShrineLevel < Row->RequiredShrineLevel)
	{
		Quote.BlockReason = FText::Format(LOCTEXT("UpShrine", "신사 Lv{0} 이상이 필요합니다 (지금 신사 Lv{1})"), FText::AsNumber(Row->RequiredShrineLevel), FText::AsNumber(ShrineLevel));
	}
	else if (const FCozyUpgradeQuote::FCondition* Missing = Quote.Conditions.FindByPredicate([](const FCozyUpgradeQuote::FCondition& Condition) { return !Condition.bMet; }))
	{
		// 🙋 선행 시설 조건 (D41) · 비용 차감·작업 등록 없음
		Quote.BlockReason = FText::Format(LOCTEXT("UpRequired", "선행 조건이 부족합니다 · {0} 필요 (지금 Lv{1})"), Missing->Label, FText::AsNumber(Missing->Current));
	}
	else
	{
		for (const FCozyUpgradeQuote::FAmount& Cost : Quote.Costs)
		{
			if (Cost.Have < Cost.Amount)
			{
				Quote.BlockReason = FText::Format(LOCTEXT("UpNoCost", "시작 비용이 부족합니다 · {0} {1}개 필요 (보유 {2}개)"), GetItemName(Cost.ItemId), FText::AsNumber(Cost.Amount), FText::AsNumber(Cost.Have));
				break;
			}
		}
	}
	if (Quote.BlockReason.IsEmpty() && Quote.Refunds.Num() > 0)
	{
		// 🙋 D38: 시작 비용을 뺀 뒤의 최종 재고에 반환 재료가 전부 들어가야 시작 (재료를 버리거나 한도를 넘기지 않음)
		FString Shortages;
		for (const FCozyUpgradeQuote::FAmount& Refund : Quote.Refunds)
		{
			const FCozyItemRow* Item = GetItemDef(Refund.ItemId);
			if (!Item || Item->Category != ECozyItemCategory::Material)
			{
				continue;
			}
			int32 CostOfSame = 0;
			for (const FCozyUpgradeQuote::FAmount& Cost : Quote.Costs)
			{
				if (Cost.ItemId == Refund.ItemId)
				{
					CostOfSame += Cost.Amount;
				}
			}
			const int32 Final = Refund.Have - CostOfSame + Refund.Amount;
			if (Final > Config.StorageCapPerItem)
			{
				Shortages += FString::Printf(TEXT("%s%s 공간 %d개 부족"), Shortages.IsEmpty() ? TEXT("") : TEXT(", "), *GetItemName(Refund.ItemId).ToString(), Final - Config.StorageCapPerItem);
			}
		}
		if (!Shortages.IsEmpty())
		{
			Quote.BlockReason = FText::Format(LOCTEXT("UpNoRefundSpace", "창고 공간이 부족해 가공 재료를 반환할 수 없습니다 · {0} · 재료를 쓰거나 팔아 공간을 만든 뒤 시작해 주세요"), FText::FromString(Shortages));
		}
	}
	Quote.bCanStart = Quote.BlockReason.IsEmpty();
	return Quote;
}

bool UCozyEstateSubsystem::StartUpgrade(const FGuid& FacilityId, FText& OutMessage)
{
	// 시작 직전 최신 상태로 재검사 · 실패하면 가공·재료·예약 공간·비용 모두 그대로 (D38)
	const FCozyUpgradeQuote Quote = GetUpgradeQuote(FacilityId);
	if (!Quote.bCanStart)
	{
		OutMessage = Quote.BlockReason;
		UE_LOG(LogCozyRealm, Log, TEXT("업그레이드 시작 실패: %s · %s"), *GetFacilityDisplayName(FacilityId).ToString(), *OutMessage.ToString());
		return false;
	}

	// 한 묶음: 비용 차감 → 가공 종료·미완료 재료 반환·예약 공간 해제 → 작업 등록
	FString CostText;
	for (const FCozyUpgradeQuote::FAmount& Cost : Quote.Costs)
	{
		State.Resources.FindOrAdd(Cost.ItemId) -= Cost.Amount;
		CostText += FString::Printf(TEXT("%s%s %d"), CostText.IsEmpty() ? TEXT("") : TEXT(", "), *GetItemName(Cost.ItemId).ToString(), Cost.Amount);
	}
	const int32 EndedJobs = State.Jobs.RemoveAll([&FacilityId](const FCozyJobRecord& Job) { return Job.FacilityId == FacilityId && Job.Type == ECozyJobType::Processing; });
	FString RefundText;
	for (const FCozyUpgradeQuote::FAmount& Refund : Quote.Refunds)
	{
		const int32 Added = AddResource(Refund.ItemId, Refund.Amount);
		ensureMsgf(Added == Refund.Amount, TEXT("반환 공간 검사를 통과했는데 다 들어가지 않음"));
		RefundText += FString::Printf(TEXT("%s%s %d개"), RefundText.IsEmpty() ? TEXT("") : TEXT(", "), *GetItemName(Refund.ItemId).ToString(), Added);
	}

	FCozyJobRecord Job;
	Job.JobId = FGuid::NewGuid();
	Job.FacilityId = FacilityId;
	Job.Type = ECozyJobType::Growth;
	Job.ContentId = Quote.GrowthRowId;
	Job.StartGameSeconds = State.GameSeconds;
	Job.DurationSeconds = Quote.Seconds;
	State.Jobs.Add(Job);

	const FText FacilityName = GetFacilityDisplayName(FacilityId);
	OutMessage = EndedJobs > 0
		? FText::Format(LOCTEXT("UpStartedRefund", "{0} Lv{1} → Lv{2} 업그레이드를 시작했습니다 · 비용 {3} · 가공 {4}건 종료, 재료 {5} 반환 (완성품은 시설에 남음)"),
			FacilityName, FText::AsNumber(Quote.FromLevel), FText::AsNumber(Quote.ToLevel), FText::FromString(CostText.IsEmpty() ? TEXT("없음") : CostText), FText::AsNumber(EndedJobs), FText::FromString(RefundText))
		: FText::Format(LOCTEXT("UpStarted", "{0} Lv{1} → Lv{2} 업그레이드를 시작했습니다 · 비용 {3}"),
			FacilityName, FText::AsNumber(Quote.FromLevel), FText::AsNumber(Quote.ToLevel), FText::FromString(CostText.IsEmpty() ? TEXT("없음") : CostText));
	UE_LOG(LogCozyRealm, Log, TEXT("업그레이드 시작: %s Lv%d→%d · 비용 %s · 가공 종료 %d건 · 반환 %s · %.0f초"),
		*FacilityName.ToString(), Quote.FromLevel, Quote.ToLevel, *CostText, EndedJobs, *RefundText, Quote.Seconds);
	NotifyChanged();
	return true;
}

TArray<FCozyUpgradeJobView> UCozyEstateSubsystem::GetUpgradeJobs() const
{
	TArray<FCozyUpgradeJobView> Result;
	for (const FCozyJobRecord& Job : State.Jobs)
	{
		if (Job.Type != ECozyJobType::Growth)
		{
			continue;
		}
		FCozyUpgradeJobView& View = Result.AddDefaulted_GetRef();
		View.JobId = Job.JobId;
		View.FacilityId = Job.FacilityId;
		View.FacilityName = GetFacilityDisplayName(Job.FacilityId);
		const FCozyGrowthRow* Row = GrowthTable ? GrowthTable->FindRow<FCozyGrowthRow>(Job.ContentId, TEXT(""), false) : nullptr;
		View.ToLevel = Row ? Row->FromLevel + 1 : 0;
		const double Elapsed = FMath::Max(0.0, State.GameSeconds - Job.StartGameSeconds);
		View.Progress01 = Job.DurationSeconds > 0.0 ? FMath::Clamp(static_cast<float>(Elapsed / Job.DurationSeconds), 0.f, 1.f) : 1.f;
		View.RemainingSeconds = static_cast<float>(FMath::Max(0.0, Job.DurationSeconds - Elapsed));
	}
	return Result;
}

FCozySpeedupQuote UCozyEstateSubsystem::GetSpeedupQuote(const FGuid& JobId, int32 Count) const
{
	FCozySpeedupQuote Quote;
	Quote.Count = Count;
	Quote.Owned = GetAmount(Config.SpeedupItemId);
	Quote.SecondsPerItem = Config.SpeedupSecondsPerItem;
	const FCozyJobRecord* Job = State.Jobs.FindByPredicate([&JobId](const FCozyJobRecord& Each) { return Each.JobId == JobId; });
	if (!Job || Job->Type != ECozyJobType::Growth)
	{
		Quote.BlockReason = LOCTEXT("SpeedNoJob", "단축할 업그레이드가 없습니다");
		return Quote;
	}
	if (Quote.SecondsPerItem <= 0.f)
	{
		Quote.BlockReason = LOCTEXT("SpeedNoConfig", "시간 단축 설정이 없습니다");
		return Quote;
	}
	Quote.bValid = true;
	Quote.RemainingBefore = static_cast<float>(FMath::Max(0.0, Job->DurationSeconds - (State.GameSeconds - Job->StartGameSeconds)));
	Quote.MaxUseful = FMath::CeilToInt(Quote.RemainingBefore / Quote.SecondsPerItem);
	const float Wanted = Quote.SecondsPerItem * FMath::Max(0, Count);
	Quote.Reduce = FMath::Min(Wanted, Quote.RemainingBefore);
	Quote.RemainingAfter = Quote.RemainingBefore - Quote.Reduce;
	Quote.Wasted = Wanted - Quote.Reduce;
	if (Count < 1)
	{
		Quote.BlockReason = LOCTEXT("SpeedZero", "쓸 부적 수를 골라 주세요");
	}
	else if (Quote.RemainingBefore <= 0.f)
	{
		Quote.BlockReason = LOCTEXT("SpeedDone", "이미 끝난 업그레이드입니다");
	}
	else if (Quote.Owned < Count)
	{
		Quote.BlockReason = FText::Format(LOCTEXT("SpeedNoItem", "시간 부적이 부족합니다 · {0}장 필요 (보유 {1}장)"), FText::AsNumber(Count), FText::AsNumber(Quote.Owned));
	}
	Quote.bCanApply = Quote.BlockReason.IsEmpty();
	return Quote;
}

bool UCozyEstateSubsystem::ApplySpeedup(const FGuid& JobId, int32 Count, FText& OutMessage)
{
	// 확정 직전에 다시 계산 · 실패하면 부적·시간 모두 그대로 (D10~D12)
	const FCozySpeedupQuote Quote = GetSpeedupQuote(JobId, Count);
	if (!Quote.bCanApply)
	{
		OutMessage = Quote.BlockReason;
		return false;
	}
	FCozyJobRecord* Job = State.Jobs.FindByPredicate([&JobId](const FCozyJobRecord& Each) { return Each.JobId == JobId; });
	State.Resources.FindOrAdd(Config.SpeedupItemId) -= Count;
	Job->DurationSeconds -= Quote.Reduce;
	const FText Name = GetFacilityDisplayName(Job->FacilityId);
	OutMessage = Quote.Wasted > 0.f
		? FText::Format(LOCTEXT("SpeedDoneWaste", "{0} 업그레이드를 부적 {1}장으로 {2}초 줄였습니다 · 남은 시간보다 많아 {3}초는 버려졌습니다"), Name, FText::AsNumber(Count), FText::AsNumber(FMath::RoundToInt(Quote.Reduce)), FText::AsNumber(FMath::RoundToInt(Quote.Wasted)))
		: FText::Format(LOCTEXT("SpeedDoneOk", "{0} 업그레이드를 부적 {1}장으로 {2}초 줄였습니다"), Name, FText::AsNumber(Count), FText::AsNumber(FMath::RoundToInt(Quote.Reduce)));
	UE_LOG(LogCozyRealm, Log, TEXT("시간 단축: %s · 부적 %d장 · %.0f초 단축 · 남은 %.0f→%.0f초 · 버림 %.0f초"), *Name.ToString(), Count, Quote.Reduce, Quote.RemainingBefore, Quote.RemainingAfter, Quote.Wasted);
	// 남은 시간이 0이 되면 바로 완료 처리
	StepGrowth();
	NotifyChanged(bStructuralPending);
	bStructuralPending = false;
	return true;
}

bool UCozyEstateSubsystem::HasGrantedReward(FName RewardId) const
{
	return State.GrantedRewards.Contains(RewardId);
}

bool UCozyEstateSubsystem::GrantOneTimeReward(FName RewardId, FText& OutMessage)
{
	if (HasGrantedReward(RewardId))
	{
		OutMessage = LOCTEXT("RewardAlready", "이미 받은 보상입니다");
		return false;
	}
	TArray<const FCozyRewardRow*> Rows;
	if (RewardTable)
	{
		RewardTable->ForeachRow<FCozyRewardRow>(TEXT("Grant"), [&](const FName& Key, const FCozyRewardRow& Row)
		{
			if (Row.RewardId == RewardId)
			{
				Rows.Add(&Row);
			}
		});
	}
	if (Rows.Num() == 0)
	{
		OutMessage = LOCTEXT("RewardMissing", "없는 보상입니다");
		return false;
	}
	FString Given;
	for (const FCozyRewardRow* Row : Rows)
	{
		const int32 Added = AddResource(Row->ItemId, Row->Amount);
		Given += FString::Printf(TEXT("%s%s %d"), Given.IsEmpty() ? TEXT("") : TEXT(", "), *GetItemName(Row->ItemId).ToString(), Added);
	}
	State.GrantedRewards.Add(RewardId);
	OutMessage = FText::Format(LOCTEXT("RewardGiven", "보상을 받았습니다 · {0}"), FText::FromString(Given));
	UE_LOG(LogCozyRealm, Log, TEXT("일회성 보상 지급: %s · %s"), *RewardId.ToString(), *Given);
	NotifyChanged();
	return true;
}

void UCozyEstateSubsystem::StepGrowth()
{
	for (int32 Index = State.Jobs.Num() - 1; Index >= 0; --Index)
	{
		const FCozyJobRecord& Job = State.Jobs[Index];
		if (Job.Type != ECozyJobType::Growth || State.GameSeconds - Job.StartGameSeconds < Job.DurationSeconds)
		{
			continue;
		}
		const FCozyGrowthRow* Row = GrowthTable ? GrowthTable->FindRow<FCozyGrowthRow>(Job.ContentId, TEXT(""), false) : nullptr;
		if (FCozyFacilityState* Facility = FindFacilityMutable(Job.FacilityId))
		{
			// 효과: 레벨 +1 · 신사 상한·관리 시설 속도·작물 해금은 레벨에서 계산되므로 따로 저장하지 않음
			// 밭의 진행 중인 주기는 시작할 때 정한 시간 그대로 끝나고 다음 주기부터 새 효과 (D40)
			Facility->Level = Row ? FMath::Max(Facility->Level, Row->FromLevel + 1) : Facility->Level + 1;
			UE_LOG(LogCozyRealm, Log, TEXT("업그레이드 완료: %s Lv%d"), *GetFacilityDisplayName(Job.FacilityId).ToString(), Facility->Level);
		}
		State.Jobs.RemoveAt(Index);
		bStructuralPending = true;
	}
}

FCozyFieldManagementView UCozyEstateSubsystem::GetFieldManagementView(const FGuid& ManagerFacilityId) const
{
	FCozyFieldManagementView View;
	const FCozyFacilityState* Facility = FindFacility(ManagerFacilityId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		return View;
	}
	View.Level = Facility->Level;
	View.bUpgrading = IsFacilityUpgrading(ManagerFacilityId);
	View.CurrentMultiplier = 1.f + Def->SpeedBonusPerLevel * FMath::Max(0, View.Level - 1);
	FName RowId;
	if (const FCozyGrowthRow* Next = FindGrowthRow(Facility->DefinitionId, Facility->Level, &RowId))
	{
		View.NextMultiplier = 1.f + Def->SpeedBonusPerLevel * View.Level;
		for (const FName& CropId : Next->UnlockCrops)
		{
			const FCozyCropRow* Crop = GetCropDef(CropId);
			View.NextUnlockCrops.Add(Crop ? Crop->DisplayName : FText::FromName(CropId));
		}
	}
	// 이 관리 시설이 맡는 생산 시설과, 그 시설들이 키울 수 있는 작물 중 해금된 것
	TSet<FName> Shown;
	if (FacilityTable)
	{
		FacilityTable->ForeachRow<FCozyFacilityRow>(TEXT("Managed"), [&](const FName& Key, const FCozyFacilityRow& Row)
		{
			if (Row.ManagerFacilityId != Facility->DefinitionId)
			{
				return;
			}
			for (const FName& CropId : Row.ProductionItems)
			{
				if (!Shown.Contains(CropId) && IsCropUnlocked(CropId))
				{
					Shown.Add(CropId);
					const FCozyCropRow* Crop = GetCropDef(CropId);
					View.UnlockedCrops.Add(Crop ? Crop->DisplayName : FText::FromName(CropId));
				}
			}
		});
	}
	for (const FCozyFacilityState& Other : State.Facilities)
	{
		const FCozyFacilityRow* OtherDef = GetFacilityDef(Other.DefinitionId);
		if (OtherDef && OtherDef->ManagerFacilityId == Facility->DefinitionId)
		{
			++View.ManagedFacilities;
		}
	}
	return View;
}

// ---------------------------------------------------------------------------
// 작물 선택 (개별 밭 · 해금은 데이터와 관리 시설 레벨로 계산 · D37)

bool UCozyEstateSubsystem::IsCropUnlocked(FName CropId) const
{
	const FCozyCropRow* Crop = GetCropDef(CropId);
	if (!Crop)
	{
		return false;
	}
	if (Crop->bStartUnlocked)
	{
		return true;
	}
	// 이 작물을 해금하는 성장 단계를 이미 지난 시설이 있으면 해금 (저장 데이터를 따로 두지 않음)
	bool bUnlocked = false;
	if (GrowthTable)
	{
		GrowthTable->ForeachRow<FCozyGrowthRow>(TEXT("Unlock"), [&](const FName& Key, const FCozyGrowthRow& Row)
		{
			if (!bUnlocked && Row.UnlockCrops.Contains(CropId) && GetHighestLevelOf(Row.TargetFacilityId) > Row.FromLevel)
			{
				bUnlocked = true;
			}
		});
	}
	return bUnlocked;
}

bool UCozyEstateSubsystem::IsFacilityUnlocked(FName DefinitionId) const
{
	if (GetHighestLevelOf(DefinitionId) > 0)
	{
		return true;
	}
	// 이 시설을 해금하는 성장 단계를 이미 지났으면 해금 (작물 해금과 같은 방식 · 저장 데이터를 따로 두지 않음)
	bool bUnlocked = false;
	if (GrowthTable)
	{
		GrowthTable->ForeachRow<FCozyGrowthRow>(TEXT("UnlockFacility"), [&](const FName& Key, const FCozyGrowthRow& Row)
		{
			if (!bUnlocked && Row.UnlockFacilities.Contains(DefinitionId) && GetHighestLevelOf(Row.TargetFacilityId) > Row.FromLevel)
			{
				bUnlocked = true;
			}
		});
	}
	return bUnlocked;
}

FText UCozyEstateSubsystem::GetCropUnlockHint(FName CropId) const
{
	FText Hint = LOCTEXT("CropLockedUnknown", "아직 해금 방법이 정해지지 않았습니다");
	bool bFound = false;
	if (GrowthTable)
	{
		GrowthTable->ForeachRow<FCozyGrowthRow>(TEXT("Hint"), [&](const FName& Key, const FCozyGrowthRow& Row)
		{
			if (!bFound && Row.UnlockCrops.Contains(CropId))
			{
				bFound = true;
				const FCozyFacilityRow* Def = GetFacilityDef(Row.TargetFacilityId);
				const FText Name = Def ? Def->DisplayName : FText::FromName(Row.TargetFacilityId);
				Hint = GetHighestLevelOf(Row.TargetFacilityId) > 0
					? FText::Format(LOCTEXT("CropLockedHint", "{0} Lv{1}에서 해금"), Name, FText::AsNumber(Row.FromLevel + 1))
					: FText::Format(LOCTEXT("CropLockedNoFacility", "{0} Lv{1}에서 해금 ({0}이 아직 없습니다)"), Name, FText::AsNumber(Row.FromLevel + 1));
			}
		});
	}
	return Hint;
}

bool UCozyEstateSubsystem::CanSelectCrop(const FGuid& FacilityId, FName CropId, FText& OutReason) const
{
	const FCozyFacilityState* Facility = FindFacility(FacilityId);
	const FCozyFacilityRow* Def = Facility ? GetFacilityDef(Facility->DefinitionId) : nullptr;
	const FCozyCropRow* Crop = GetCropDef(CropId);
	if (!Def || !Crop || !Def->bProductionItemSelectable || !Def->ProductionItems.Contains(CropId))
	{
		OutReason = LOCTEXT("CropNotHere", "이 시설에서 키울 수 없는 작물입니다");
		return false;
	}
	if (Facility->SelectedCropId == CropId)
	{
		OutReason = LOCTEXT("CropSame", "이미 키우는 작물입니다");
		return false;
	}
	if (!IsCropUnlocked(CropId))
	{
		OutReason = FText::Format(LOCTEXT("CropLocked", "{0}: {1}"), Crop->DisplayName, GetCropUnlockHint(CropId));
		return false;
	}
	// 🙋 미수령 작물이 남아 있거나 진행 중인 생산 주기가 있으면 다른 품목으로 바꿀 수 없음 (D31 · D37)
	if (!CanAcceptOutputItem(FacilityId, Crop->ProducedItem, OutReason))
	{
		return false;
	}
	if (FindJob(Facility->ActiveJobId))
	{
		OutReason = LOCTEXT("CropRunning", "진행 중인 생산 주기가 있어 작물을 바꿀 수 없습니다 · 주민을 빼서 생산을 멈춘 뒤 바꿀 수 있습니다");
		return false;
	}
	return true;
}

bool UCozyEstateSubsystem::SelectCrop(const FGuid& FacilityId, FName CropId, FText& OutMessage)
{
	if (!CanSelectCrop(FacilityId, CropId, OutMessage))
	{
		UE_LOG(LogCozyRealm, Log, TEXT("작물 변경 실패: %s → %s · %s"), *GetFacilityDisplayName(FacilityId).ToString(), *CropId.ToString(), *OutMessage.ToString());
		return false;
	}
	FCozyFacilityState* Facility = FindFacilityMutable(FacilityId);
	const FCozyCropRow* Crop = GetCropDef(CropId);
	Facility->SelectedCropId = CropId;
	OutMessage = FText::Format(LOCTEXT("CropChanged", "키우는 작물을 {0}(으)로 바꿨습니다"), Crop->DisplayName);
	UE_LOG(LogCozyRealm, Log, TEXT("작물 변경: %s → %s"), *GetFacilityDisplayName(FacilityId).ToString(), *CropId.ToString());
	NotifyChanged();
	return true;
}

// ---------------------------------------------------------------------------
// 디버그

void UCozyEstateSubsystem::DebugAddResource(FName ItemId, int32 Amount)
{
	AddResource(ItemId, Amount);
	NotifyChanged();
}

void UCozyEstateSubsystem::DebugSetResource(FName ItemId, int32 Amount)
{
	if (GetItemDef(ItemId))
	{
		State.Resources.FindOrAdd(ItemId) = FMath::Clamp(Amount, 0, Config.StorageCapPerItem);
		NotifyChanged();
	}
}

bool UCozyEstateSubsystem::DebugAddFacility(FName DefinitionId)
{
	const FCozyFacilityRow* Def = GetFacilityDef(DefinitionId);
	if (!Def)
	{
		return false;
	}

	// 지금 차 있는 칸
	TSet<FIntPoint> Occupied;
	for (const FCozyFacilityState& Facility : State.Facilities)
	{
		if (const FCozyFacilityRow* OtherDef = GetFacilityDef(Facility.DefinitionId))
		{
			const FIntPoint OtherSize = (Facility.Rotation % 2 == 0) ? OtherDef->Size : FIntPoint(OtherDef->Size.Y, OtherDef->Size.X);
			for (int32 X = 0; X < OtherSize.X; ++X)
			{
				for (int32 Y = 0; Y < OtherSize.Y; ++Y)
				{
					Occupied.Add(Facility.GridCoord + FIntPoint(X, Y));
				}
			}
		}
	}

	// 첫 번째 빈 자리에 놓음
	for (int32 GridY = 0; GridY + Def->Size.Y <= Config.GridSize.Y; ++GridY)
	{
		for (int32 GridX = 0; GridX + Def->Size.X <= Config.GridSize.X; ++GridX)
		{
			bool bFree = true;
			for (int32 X = 0; X < Def->Size.X && bFree; ++X)
			{
				for (int32 Y = 0; Y < Def->Size.Y && bFree; ++Y)
				{
					bFree = !Occupied.Contains(FIntPoint(GridX + X, GridY + Y));
				}
			}
			if (bFree)
			{
				FCozyFacilityState Facility;
				Facility.InstanceId = FGuid::NewGuid();
				Facility.DefinitionId = DefinitionId;
				Facility.GridCoord = FIntPoint(GridX, GridY);
				Facility.Level = 1;
				if (Def->ProductionItems.Num() > 0)
				{
					Facility.SelectedCropId = Def->ProductionItems[0];
				}
				State.Facilities.Add(Facility);
				SpawnFacilityActor(Facility);
				UE_LOG(LogCozyRealm, Log, TEXT("[디버그] 테스트용 %s 추가 (%d,%d)"), *DefinitionId.ToString(), GridX, GridY);
				NotifyChanged();
				return true;
			}
		}
	}
	return false;
}

void UCozyEstateSubsystem::DebugRestartNewGame()
{
	DestroyFacilityActors();
	BuildNewGameState();
	SpawnFacilityActors();
	StepAccumulator = 0.0;
	SaveEstate(TEXT("새 게임"));
	NotifyChanged();
}

void UCozyEstateSubsystem::NotifyChanged(bool bStructural)
{
	OnEstateChanged.Broadcast(bStructural);
}

#undef LOCTEXT_NAMESPACE
