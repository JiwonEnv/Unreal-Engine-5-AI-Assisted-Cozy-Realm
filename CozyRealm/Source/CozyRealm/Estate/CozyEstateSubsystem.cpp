#include "Estate/CozyEstateSubsystem.h"
#include "Facilities/CozyFacilityActor.h"
#include "CozyRealm.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
	BuildNewGameState();
	SpawnFacilityActors();
	bStarted = true;
	NotifyChanged();
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
			if (!GetFacilityDef(Row.TargetFacilityId))
			{
				Warn(FString::Printf(TEXT("성장 %s: 없는 시설 참조 %s"), *Key.ToString(), *Row.TargetFacilityId.ToString()));
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

	if (!GetItemDef(Config.SaleCurrencyId))
	{
		Warn(FString::Printf(TEXT("영지 설정: 판매 대금 재화 %s가 Items.csv에 없음"), *Config.SaleCurrencyId.ToString()));
	}

	UE_LOG(LogCozyRealm, Log, TEXT("[데이터 검사] 끝 · 문제 %d개"), Issues);
	return Issues;
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
		NotifyChanged(false);
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
	// 레벨당 속도 증가 (밭 🙋 +20%) → 한 주기 시간이 그만큼 짧아짐
	const double SpeedMultiplier = 1.0 + Def.SpeedBonusPerLevel * FMath::Max(0, Facility.Level - 1);
	return FMath::Max(0.1, static_cast<double>(Crop.ProductionSeconds) / SpeedMultiplier);
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
	const double SpeedMultiplier = 1.0 + Def.SpeedBonusPerLevel * FMath::Max(0, Facility.Level - 1);
	return FMath::Max(0.1, static_cast<double>(Recipe.Seconds) / SpeedMultiplier);
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
	if (!IsFacilityWorking(*Facility, *Def))
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
	NotifyChanged();
}

void UCozyEstateSubsystem::NotifyChanged(bool bStructural)
{
	OnEstateChanged.Broadcast(bStructural);
}

#undef LOCTEXT_NAMESPACE
