#include "Misc/AutomationTest.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/OutputDeviceRedirector.h"

// 출시 품질 수정 3차 자동 시험 (창고 한도 10시간치 · 창고 레벨업 · 창고 없는 저장 · 데모 Lv5 성장 데이터 · Claude 2026-10-30)
// 1·2차 시험과 같이 빈 게임 월드에서 CSV만 읽어 새 게임 상태로 시험한다 (StartEstate·저장 없음).
#if WITH_DEV_AUTOMATION_TESTS
namespace CozyRelease3Test
{
	/** 시험용 빈 게임 월드와 그 영지 서비스 (CSV만 읽음 · 액터·저장 없음) */
	struct FTestEstate
	{
		UWorld* World = nullptr;
		UCozyEstateSubsystem* Estate = nullptr;
		FString Errors;

		FTestEstate()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CozyReleaseFix3Test"));
			if (World)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
				Estate = World->GetSubsystem<UCozyEstateSubsystem>();
			}
			if (Estate && !Estate->DebugInitDataOnly(Errors))
			{
				Estate = nullptr;
			}
		}

		~FTestEstate()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		const FCozyFacilityState* Find(FName DefinitionId) const
		{
			return Estate->GetState().Facilities.FindByPredicate([DefinitionId](const FCozyFacilityState& Each) { return Each.DefinitionId == DefinitionId; });
		}

		FGuid IdOf(FName DefinitionId) const
		{
			const FCozyFacilityState* Facility = Find(DefinitionId);
			return Facility ? Facility->InstanceId : FGuid();
		}

		void SetLevel(FName DefinitionId, int32 Level)
		{
			Estate->DebugSetFacilityLevel(IdOf(DefinitionId), Level);
		}

		/** 켜 둔 동안처럼 Seconds초를 진행 (따라잡기 간격보다 짧게 나눠서) */
		void RunOnline(int32 Seconds)
		{
			while (Seconds > 0)
			{
				const int32 Step = FMath::Min(Seconds, 30);
				Estate->AdvanceRealSeconds(Step, TEXT("자동 시험"));
				Seconds -= Step;
			}
		}

		/** 창고 기능 시설 정의 ID (없으면 None) */
		FName StorageId() const
		{
			for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
			{
				const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility.DefinitionId);
				if (Def && Def->Functions.Contains(ECozyFacilityFunction::Storage))
				{
					return Facility.DefinitionId;
				}
			}
			return NAME_None;
		}

		/** 놓인 시설끼리 칸이 겹치는가 */
		bool AnyOverlap() const
		{
			const TArray<FCozyFacilityState>& All = Estate->GetState().Facilities;
			for (int32 A = 0; A < All.Num(); ++A)
			{
				for (int32 B = A + 1; B < All.Num(); ++B)
				{
					if (All[A].bStored || All[B].bStored)
					{
						continue;
					}
					const FIntPoint SizeA = Estate->GetFootprint(All[A].InstanceId, All[A].Rotation);
					const FIntPoint SizeB = Estate->GetFootprint(All[B].InstanceId, All[B].Rotation);
					if (All[A].GridCoord.X < All[B].GridCoord.X + SizeB.X && All[B].GridCoord.X < All[A].GridCoord.X + SizeA.X
						&& All[A].GridCoord.Y < All[B].GridCoord.Y + SizeB.Y && All[B].GridCoord.Y < All[A].GridCoord.Y + SizeA.Y)
					{
						return true;
					}
				}
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyStorageStartCapTest, "CozyRealm.Storage.StartCapTenHours", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyStorageStartCapTest::RunTest(const FString& Parameters)
{
	CozyRelease3Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FName StorageId = Test.StorageId();
	TestFalse(TEXT("새 게임에 창고 시설이 있음"), StorageId.IsNone());
	const FCozyFacilityState* Storage = Test.Find(StorageId);
	if (!TestNotNull(TEXT("창고 상태"), Storage))
	{
		return false;
	}
	TestEqual(TEXT("창고 시작 레벨 1"), Storage->Level, 1);
	TestFalse(TEXT("창고는 영지에 놓여 있음"), Storage->bStored);

	// 🙋 처음 한도 = 시작 상태에서 10시간 동안 쌓이는 수량 (시작 밭 작물 · 시작 속도 · 꾸미기 없음)
	const FCozyFacilityState* Field = Test.Find(TEXT("Field"));
	const FCozyFacilityRow* FieldDef = Field ? Estate->GetFacilityDef(Field->DefinitionId) : nullptr;
	const FCozyCropRow* Crop = Field ? Estate->GetCropDef(Field->SelectedCropId) : nullptr;
	if (!TestNotNull(TEXT("시작 밭 작물"), Crop) || !TestNotNull(TEXT("밭 정의"), FieldDef))
	{
		return false;
	}
	const FCozyFacilityRow* Manager = Estate->GetFacilityDef(FieldDef->ManagerFacilityId);
	const FCozyFacilityState* ManagerState = Test.Find(FieldDef->ManagerFacilityId);
	const double StartSpeed = (Manager && ManagerState) ? 1.0 + Manager->SpeedBonusPerLevel * (ManagerState->Level - 1) : 1.0;
	const int32 TenHours = FMath::FloorToInt32(10.0 * 3600.0 * StartSpeed / Crop->ProductionSeconds + UE_KINDA_SMALL_NUMBER) * Crop->ProducedAmount;
	TestEqual(TEXT("창고 처음 한도 = 시작 밭의 10시간 생산량"), Estate->GetStorageCap(), TenHours);
	TestEqual(TEXT("밀 10초당 1 기준 3600개"), Estate->GetStorageCap(), 3600);
	TestEqual(TEXT("Lv1 한도 = EstateConfig StorageCapPerItem"), Estate->GetStorageCap(), Estate->GetConfig().StorageCapPerItem);

	// 12시간 쌓인 밀(수천 개)을 수확하면 한도까지 들어가고 나머지는 밭에 남음
	FCozyEstateState Edited = Estate->GetState();
	for (FCozyFacilityState& Each : Edited.Facilities)
	{
		if (Each.DefinitionId == TEXT("Field"))
		{
			Each.UnclaimedItemId = Crop->ProducedItem;
			Each.UnclaimedAmount = 4320;
		}
	}
	Estate->DebugSetState(Edited);
	const FCozyUnclaimedView Before = Estate->GetUnclaimedView(Test.IdOf(TEXT("Field")));
	TestEqual(TEXT("수령 화면의 창고 한도 = 새 한도 (예전 100 아님)"), Before.StorageCap, 3600);
	TestEqual(TEXT("지금 수령 가능 = 창고 한도"), Before.CollectableNow, 3600);
	const FCozyCollectResult First = Estate->CollectUnclaimed(Test.IdOf(TEXT("Field")));
	TestEqual(TEXT("한도까지 3600개가 창고로"), First.Moved, 3600);
	TestEqual(TEXT("창고 밀 3600"), Estate->GetAmount(Crop->ProducedItem), 3600);
	TestEqual(TEXT("남은 720개는 밭에"), Test.Find(TEXT("Field"))->UnclaimedAmount, 720);
	const FCozyCollectResult Second = Estate->CollectUnclaimed(Test.IdOf(TEXT("Field")));
	TestEqual(TEXT("창고가 가득이면 옮기지 못함"), Second.Moved, 0);
	TestTrue(*FString::Printf(TEXT("'창고 공간' 안내 (%s)"), *Second.Message.ToString()), Second.Message.ToString().Contains(TEXT("공간")));
	TestEqual(TEXT("창고 공간 0"), Estate->GetStorageSpace(Crop->ProducedItem), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyStorageLevelUpTest, "CozyRealm.Storage.LevelUpRaisesCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyStorageLevelUpTest::RunTest(const FString& Parameters)
{
	CozyRelease3Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FName StorageId = Test.StorageId();
	const FCozyFacilityRow* Def = Estate->GetFacilityDef(StorageId);
	if (!TestNotNull(TEXT("창고 정의"), Def))
	{
		return false;
	}
	const int32 Base = Estate->GetStorageCap();
	TestTrue(TEXT("레벨당 배율이 1보다 큼"), Def->StorageCapGrowthPerLevel > 1.f);

	// 후신소 공통 업그레이드로 창고 Lv1→2 (신사 Lv2 필요 · 골드 비용)
	Test.SetLevel(TEXT("Shrine"), 2);
	Estate->DebugSetResource(TEXT("Gold"), 500);
	const FCozyUpgradeQuote Quote = Estate->GetUpgradeQuote(Test.IdOf(StorageId));
	TestTrue(TEXT("창고 업그레이드 단계가 있음"), Quote.bValid);
	TestTrue(*FString::Printf(TEXT("창고 업그레이드 시작 가능 (%s)"), *Quote.BlockReason.ToString()), Quote.bCanStart);
	TestTrue(TEXT("견적에 창고 한도 효과 안내"), Quote.EffectLines.ContainsByPredicate([](const FText& Line) { return Line.ToString().Contains(TEXT("창고 한도")); }));
	FText Message;
	TestTrue(*FString::Printf(TEXT("창고 업그레이드 시작 (%s)"), *Message.ToString()), Estate->StartUpgrade(Test.IdOf(StorageId), Message));
	TestEqual(TEXT("업그레이드 중에는 한도 그대로"), Estate->GetStorageCap(), Base);
	Test.RunOnline(FMath::CeilToInt32(Quote.Seconds) + 2);
	TestEqual(TEXT("창고 Lv2"), Test.Find(StorageId)->Level, 2);
	const int32 Lv2 = Estate->GetStorageCap();
	TestEqual(TEXT("Lv2 한도 = Lv1 × 배율"), Lv2, FMath::RoundToInt32(Base * Def->StorageCapGrowthPerLevel));
	TestTrue(TEXT("한도가 늘어남"), Lv2 > Base);

	// 레벨마다 계속 늘어남 · 수령·추가가 새 한도를 씀
	int32 Previous = Lv2;
	for (int32 Level = 3; Level <= Estate->GetMaxFacilityLevel(StorageId); ++Level)
	{
		Test.SetLevel(StorageId, Level);
		TestTrue(*FString::Printf(TEXT("Lv%d 한도 %d > Lv%d 한도 %d"), Level, Estate->GetStorageCap(), Level - 1, Previous), Estate->GetStorageCap() > Previous);
		Previous = Estate->GetStorageCap();
	}
	Estate->DebugSetResource(TEXT("Wheat"), 0);
	TestEqual(TEXT("한도까지 더해짐"), Estate->AddResource(TEXT("Wheat"), Previous + 50), Previous);
	TestEqual(TEXT("최대 레벨 창고 한도 = Lv1 × 배율^(최대-1)"), Previous,
		FMath::RoundToInt32(Base * FMath::Pow(Def->StorageCapGrowthPerLevel, static_cast<float>(Estate->GetMaxFacilityLevel(StorageId) - 1))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyStorageOldSaveTest, "CozyRealm.Storage.OldSaveGetsStorage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyStorageOldSaveTest::RunTest(const FString& Parameters)
{
	CozyRelease3Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FName StorageId = Test.StorageId();
	const FIntPoint StartCoord = Test.Find(StorageId)->GridCoord;
	const int32 FacilityCount = Estate->GetState().Facilities.Num();

	// ① 창고가 생기기 전 저장 (창고 없음 · 밀 80개) → 불러오면 시작 좌표에 Lv1 창고
	FCozyEstateState Old = Estate->GetState();
	Old.Facilities.RemoveAll([StorageId](const FCozyFacilityState& Each) { return Each.DefinitionId == StorageId; });
	Old.Resources.Add(TEXT("Wheat"), 80);
	Estate->DebugSetState(Old);
	const FCozyFacilityState* Added = Test.Find(StorageId);
	if (!TestNotNull(TEXT("불러오면 창고가 생김"), Added))
	{
		return false;
	}
	TestEqual(TEXT("시설 수가 새 게임과 같음"), Estate->GetState().Facilities.Num(), FacilityCount);
	TestFalse(TEXT("영지에 놓임 (보관함 아님)"), Added->bStored);
	TestEqual(TEXT("Lv1"), Added->Level, 1);
	TestTrue(TEXT("시작 좌표가 비어 있으면 그 자리"), Added->GridCoord == StartCoord);
	TestFalse(TEXT("다른 시설과 겹치지 않음"), Test.AnyOverlap());
	TestEqual(TEXT("재료는 그대로"), Estate->GetAmount(TEXT("Wheat")), 80);
	// 다시 불러와도 두 번 놓지 않음
	Estate->DebugSetState(Estate->GetState());
	int32 Count = 0;
	for (const FCozyFacilityState& Each : Estate->GetState().Facilities)
	{
		Count += Each.DefinitionId == StorageId ? 1 : 0;
	}
	TestEqual(TEXT("다시 불러와도 창고 1개"), Count, 1);

	// ② 시작 좌표를 다른 시설이 차지한 저장 → 빈 자리에 놓고 겹치지 않음
	FCozyEstateState Blocked = Estate->GetState();
	Blocked.Facilities.RemoveAll([StorageId](const FCozyFacilityState& Each) { return Each.DefinitionId == StorageId; });
	for (FCozyFacilityState& Each : Blocked.Facilities)
	{
		if (Each.DefinitionId == TEXT("Shop"))
		{
			Each.GridCoord = StartCoord;
		}
	}
	Estate->DebugSetState(Blocked);
	const FCozyFacilityState* Moved = Test.Find(StorageId);
	if (TestNotNull(TEXT("자리가 막혀도 창고가 생김"), Moved))
	{
		TestFalse(TEXT("빈 자리에 놓임"), Moved->bStored);
		TestTrue(TEXT("막힌 시작 좌표가 아님"), Moved->GridCoord != StartCoord);
		TestFalse(TEXT("다른 시설과 겹치지 않음"), Test.AnyOverlap());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyDemoLv5Test, "CozyRealm.Demo.EndNoticeAtShrineLv5", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyDemoLv5Test::RunTest(const FString& Parameters)
{
	CozyRelease3Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	TestEqual(TEXT("🙋 데모 마지막 신사 레벨 = 5 (Growth.csv에서 읽음)"), Estate->GetDemoFinalShrineLevel(), 5);
	for (int32 Level = 1; Level <= 4; ++Level)
	{
		Test.SetLevel(TEXT("Shrine"), Level);
		TestFalse(*FString::Printf(TEXT("신사 Lv%d에서는 안내 없음"), Level), Estate->ShouldShowDemoEnd());
	}
	Test.SetLevel(TEXT("Shrine"), 5);
	TestTrue(TEXT("신사 Lv5에 닿으면 안내"), Estate->ShouldShowDemoEnd());
	Estate->MarkDemoEndShown();
	TestFalse(TEXT("한 번만"), Estate->ShouldShowDemoEnd());
	Estate->DebugSetState(Estate->GetState());
	TestFalse(TEXT("불러온 뒤에도 다시 안 뜸"), Estate->ShouldShowDemoEnd());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyDemoGrowthDataTest, "CozyRealm.Growth.DemoDataToShrineLv5", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyDemoGrowthDataTest::RunTest(const FString& Parameters)
{
	CozyRelease3Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const int32 Final = Estate->GetDemoFinalShrineLevel();
	TestEqual(TEXT("데모 마지막 신사 레벨 5"), Final, 5);

	// 신사 Lv N+1 ← 나가야·후신소·밭 관리 시설·제분소 Lv N (D41) · 이 네 시설과 창고는 Lv1→5 단계가 모두 있음
	const TArray<FName> Pillars = { TEXT("Nagaya"), TEXT("Hushinso"), TEXT("FieldOffice"), TEXT("Mill") };
	TArray<FName> Targets = Pillars;
	Targets.Add(TEXT("Shrine"));
	Targets.Add(Test.StorageId());
	for (const FName& Target : Targets)
	{
		TestEqual(*FString::Printf(TEXT("%s 최대 레벨 = 신사 최대 레벨"), *Target.ToString()), Estate->GetMaxFacilityLevel(Target), Final);
	}
	for (int32 From = 1; From < Final; ++From)
	{
		for (const FName& Target : Targets)
		{
			Test.SetLevel(Target, From);
			const FCozyUpgradeQuote Quote = Estate->GetUpgradeQuote(Test.IdOf(Target));
			const FString What = FString::Printf(TEXT("%s Lv%d→%d"), *Target.ToString(), From, From + 1);
			if (!TestTrue(*FString::Printf(TEXT("%s 단계 있음"), *What), Quote.bValid))
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("%s 다음 레벨"), *What), Quote.ToLevel, From + 1);
			TestTrue(*FString::Printf(TEXT("%s 시간 > 0"), *What), Quote.Seconds > 0.0);
			TestTrue(*FString::Printf(TEXT("%s 비용 있음"), *What), Quote.Costs.Num() > 0);
			if (Target == TEXT("Shrine"))
			{
				for (const FName& Pillar : Pillars)
				{
					const bool bHasCondition = Quote.Conditions.ContainsByPredicate([&Pillar, From](const FCozyUpgradeQuote::FCondition& Condition)
					{
						return Condition.FacilityId == Pillar && Condition.Label.ToString().Contains(FString::Printf(TEXT("Lv.%d"), From));
					});
					TestTrue(*FString::Printf(TEXT("%s 조건에 %s Lv%d"), *What, *Pillar.ToString(), From), bHasCondition);
				}
			}
			else
			{
				// 🙋 시설 Lv N→N+1 비용 = 20 × 1.5^(N-1) 골드 (🤖 소수는 반올림)
				const FCozyUpgradeQuote::FAmount* Gold = Quote.Costs.FindByPredicate([](const FCozyUpgradeQuote::FAmount& Cost) { return Cost.ItemId == TEXT("Gold"); });
				if (TestNotNull(*FString::Printf(TEXT("%s 골드 비용"), *What), Gold))
				{
					TestEqual(*FString::Printf(TEXT("%s 비용 공식"), *What), Gold->Amount, FMath::RoundToInt32(20.0 * FMath::Pow(1.5, static_cast<double>(From - 1))));
				}
				// 🙋 신사 상한: 시설 Lv N+1 ← 신사 Lv N+1
				const bool bShrineCap = Quote.Conditions.ContainsByPredicate([From](const FCozyUpgradeQuote::FCondition& Condition)
				{
					return Condition.FacilityId == TEXT("Shrine") && Condition.Label.ToString().Contains(FString::Printf(TEXT("Lv.%d"), From + 1));
				});
				TestTrue(*FString::Printf(TEXT("%s 신사 Lv%d 상한 조건"), *What, From + 1), bShrineCap);
			}
			// 🙋 제분소 Lv N ← 밭 관리 시설 Lv N (D41)
			if (Target == TEXT("Mill"))
			{
				TestTrue(*FString::Printf(TEXT("%s 조건에 밭 관리 시설 Lv%d"), *What, From), Quote.Conditions.ContainsByPredicate([From](const FCozyUpgradeQuote::FCondition& Condition)
				{
					return Condition.FacilityId == TEXT("FieldOffice") && Condition.Label.ToString().Contains(FString::Printf(TEXT("Lv.%d"), From));
				}));
			}
		}
	}

	// 데이터 검사 전체 (단계 빈칸 · 도달할 수 없는 단계 · 창고 한도를 넘는 비용 · 미수령 한도 등)
	// 데모에서 숨긴 재료(팥·쌀)의 '만드는 곳도 쓰는 곳도 없음' 경고는 이번 작업 전부터 있던 알려진 경고라 따로 셈
	struct FValidateCapture : public FOutputDevice
	{
		TArray<FString> Lines;
		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			const FString Text(Message);
			if (Text.Contains(TEXT("[데이터 검사]")) && !Text.Contains(TEXT("끝 ·")))
			{
				Lines.Add(Text);
			}
		}
	} Capture;
	GLog->AddOutputDevice(&Capture);
	const int32 Issues = Estate->ValidateData();
	GLog->Flush();
	GLog->RemoveOutputDevice(&Capture);
	int32 Known = 0;
	for (const FString& Line : Capture.Lines)
	{
		const bool bHiddenMaterial = Line.Contains(TEXT("만드는 곳도 쓰는 곳도 판매가도 없음"));
		Known += bHiddenMaterial ? 1 : 0;
		if (!bHiddenMaterial)
		{
			AddError(FString::Printf(TEXT("데이터 검사 문제: %s"), *Line));
		}
	}
	TestEqual(TEXT("데이터 검사 문제 = 숨긴 재료 경고뿐 (성장·창고·시작 시설 문제 0개)"), Issues, Known);
	return true;
}
#endif
