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
	View.UnclaimedCapacity = Def->UnclaimedCapacity;
	View.UnclaimedAmount = GetUnclaimedTotal(FacilityId);
	View.CollectButtonLabel = Def->CollectButtonLabel;
	{
		FName ShownItem = NAME_None;
		if (const FCozyCropRow* SelectedCrop = GetCropDef(Facility->SelectedCropId))
		{
			ShownItem = SelectedCrop->ProducedItem;
		}
		for (const TPair<FName, int32>& Pair : Facility->UnclaimedItems)
		{
			if (Pair.Value > 0)
			{
				ShownItem = Pair.Key;
				break;
			}
		}
		const FCozyItemRow* ShownDef = GetItemDef(ShownItem);
		View.UnclaimedItemName = ShownDef ? ShownDef->DisplayName : FText::FromName(ShownItem);
		// 창고 상태 (미수령분은 포함하지 않음) · 지금 수령 가능한 수량
		View.StoredAmount = GetAmount(ShownItem);
		View.StorageCap = (ShownDef && ShownDef->Category == ECozyItemCategory::Material) ? Config.StorageCapPerItem : 0;
		const int32 Space = GetStorageSpace(ShownItem);
		View.CollectableNow = FMath::Min(View.UnclaimedAmount, Space);
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
	int32 Total = 0;
	if (const FCozyFacilityState* Facility = FindFacility(FacilityId))
	{
		for (const TPair<FName, int32>& Pair : Facility->UnclaimedItems)
		{
			Total += Pair.Value;
		}
	}
	return Total;
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
	FText FirstItemName;
	for (TPair<FName, int32>& Pair : Facility->UnclaimedItems)
	{
		if (Pair.Value <= 0)
		{
			continue;
		}
		const int32 Added = AddResource(Pair.Key, Pair.Value);
		Pair.Value -= Added;
		Result.Moved += Added;
		Result.Remaining += Pair.Value;
		if (FirstItemName.IsEmpty())
		{
			const FCozyItemRow* ItemDef = GetItemDef(Pair.Key);
			FirstItemName = ItemDef ? ItemDef->DisplayName : FText::FromName(Pair.Key);
		}
	}
	for (auto It = Facility->UnclaimedItems.CreateIterator(); It; ++It)
	{
		if (It.Value() <= 0)
		{
			It.RemoveCurrent();
		}
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
		CancelProductionIfUnderstaffed(*OldFacility);
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
		int32 Total = 0;
		for (const TPair<FName, int32>& Pair : Facility.UnclaimedItems)
		{
			Total += Pair.Value;
		}
		return Total + Crop->ProducedAmount <= Capacity;
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
		Facility.UnclaimedItems.FindOrAdd(Crop->ProducedItem) += Crop->ProducedAmount;
		++Job->PaidCycles;
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
