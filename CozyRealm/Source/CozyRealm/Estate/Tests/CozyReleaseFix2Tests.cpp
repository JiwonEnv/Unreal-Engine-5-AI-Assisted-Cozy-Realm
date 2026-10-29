#include "Misc/AutomationTest.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Facilities/CozyFacilityActor.h"
#include "Settings/CozySettingsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// 출시 품질 수정 2차 자동 시험 (영지 일 알림 · 레벨 효과 · 바닥 칠하기 비용 · 메시 경로 · 데모 끝 · 프레임 낮추기 · Claude 2026-10-30)
// 1차 시험과 같이 빈 게임 월드에서 CSV만 읽어 새 게임 상태로 시험한다 (StartEstate·저장 없음).
#if WITH_DEV_AUTOMATION_TESTS
namespace CozyRelease2Test
{
	/** 시험용 빈 게임 월드와 그 영지 서비스 (CSV만 읽음 · 액터·저장 없음) */
	struct FTestEstate
	{
		UWorld* World = nullptr;
		UCozyEstateSubsystem* Estate = nullptr;
		FString Errors;

		FTestEstate()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CozyReleaseFix2Test"));
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

		bool AssignFreeResident(FName DefinitionId)
		{
			for (const FCozyResidentState& Resident : Estate->GetState().Residents)
			{
				if (!Resident.AssignedFacility.IsValid())
				{
					FText Reason;
					return Estate->AssignResident(Resident.InstanceId, IdOf(DefinitionId), Reason);
				}
			}
			return false;
		}

		/** 레벨을 그대로 바꿈 (데모 상한 Lv3을 넘는 Lv5 효과도 시험) */
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
	};

	/** 알림 종류별 개수 */
	struct FEventCounter
	{
		TMap<ECozyEstateEventKind, int32> Counts;
		int32 Total = 0;
		FDelegateHandle Handle;
		UCozyEstateSubsystem* Estate = nullptr;

		explicit FEventCounter(UCozyEstateSubsystem* InEstate) : Estate(InEstate)
		{
			Handle = Estate->OnCozyEstateEvent.AddLambda([this](const FCozyEstateEvent& Event)
			{
				++Counts.FindOrAdd(Event.Kind);
				++Total;
			});
		}
		~FEventCounter() { Estate->OnCozyEstateEvent.Remove(Handle); }
		int32 Of(ECozyEstateEventKind Kind) const { const int32* Found = Counts.Find(Kind); return Found ? *Found : 0; }
		void Reset() { Counts.Reset(); Total = 0; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyEstateEventOnceTest, "CozyRealm.Events.OncePerAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyEstateEventOnceTest::RunTest(const FString& Parameters)
{
	CozyRelease2Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	CozyRelease2Test::FEventCounter Counter(Estate);
	FText Message;

	// 주민 배치 2번 → 알림 2번
	TestTrue(TEXT("밭에 주민 배치"), Test.AssignFreeResident(TEXT("Field")));
	TestTrue(TEXT("제분소에 주민 배치"), Test.AssignFreeResident(TEXT("Mill")));
	TestEqual(TEXT("주민 배치 알림 2번"), Counter.Of(ECozyEstateEventKind::ResidentAssigned), 2);

	// 켜 둔 동안 생산 (미수령분에 쌓이기만 함 → 알림 없음) → 수확 1번
	Test.RunOnline(30);
	TestEqual(TEXT("자동 생산만으로는 알림 없음"), Counter.Of(ECozyEstateEventKind::Harvest), 0);
	const FCozyCollectResult Harvest = Estate->CollectUnclaimed(Test.IdOf(TEXT("Field")));
	TestTrue(TEXT("밀을 수확함"), Harvest.Moved > 0);
	TestEqual(TEXT("수확 알림 1번"), Counter.Of(ECozyEstateEventKind::Harvest), 1);
	// 빈 시설 수령은 알림 없음
	Estate->CollectUnclaimed(Test.IdOf(TEXT("Field")));
	TestEqual(TEXT("수령할 것이 없으면 알림 없음"), Counter.Of(ECozyEstateEventKind::Harvest), 1);

	// 가공 1회 → 완료 알림 1번 · 수령 → 수확 알림 +1
	Estate->DebugSetResource(TEXT("Wheat"), 30);
	TestTrue(TEXT("밀가루 1회 시작"), Estate->StartProcessing(Test.IdOf(TEXT("Mill")), TEXT("Mill_Flour"), 1, Message));
	Test.RunOnline(30);
	TestEqual(TEXT("가공 완료 알림 1번"), Counter.Of(ECozyEstateEventKind::ProcessingDone), 1);
	Estate->CollectUnclaimed(Test.IdOf(TEXT("Mill")));
	TestEqual(TEXT("밀가루 수령 알림 (수확·수령 2번째)"), Counter.Of(ECozyEstateEventKind::Harvest), 2);

	// 판매 1번
	TestTrue(TEXT("밀가루 1개 판매"), Estate->SellItem(Test.IdOf(TEXT("Shop")), TEXT("Flour"), 1, Message));
	TestEqual(TEXT("판매 알림 1번"), Counter.Of(ECozyEstateEventKind::Sale), 1);
	// 실패한 판매는 알림 없음
	Estate->SellItem(Test.IdOf(TEXT("Shop")), TEXT("Flour"), 999, Message);
	TestEqual(TEXT("실패한 판매는 알림 없음"), Counter.Of(ECozyEstateEventKind::Sale), 1);

	// 장식 구매 1번
	Estate->DebugSetResource(TEXT("Gold"), 100);
	TestTrue(TEXT("돌길 구매"), Estate->BuyDecoration(TEXT("StonePath"), Message));
	TestEqual(TEXT("구매 알림 1번"), Counter.Of(ECozyEstateEventKind::Purchase), 1);

	// 신사 업그레이드 완료 1번 (켜 둔 동안)
	Estate->DebugSetResource(TEXT("Wheat"), 10);
	Estate->DebugSetResource(TEXT("Flour"), 5);
	if (TestTrue(TEXT("신사 업그레이드 시작"), Estate->StartUpgrade(Test.IdOf(TEXT("Shrine")), Message)))
	{
		Test.RunOnline(200);
		TestEqual(TEXT("업그레이드 완료 알림 1번"), Counter.Of(ECozyEstateEventKind::UpgradeDone), 1);
	}
	TestEqual(TEXT("배치 확정은 하지 않아 0번"), Counter.Of(ECozyEstateEventKind::PlacementConfirmed), 0);

	// 방치 정산 중에는 알림 없음 (생산·가공은 진행됨)
	Counter.Reset();
	Estate->DebugSetResource(TEXT("Wheat"), 30);
	TestTrue(TEXT("밀가루 3회 시작"), Estate->StartProcessing(Test.IdOf(TEXT("Mill")), TEXT("Mill_Flour"), 3, Message));
	Counter.Reset();
	const int32 FieldBefore = Test.Find(TEXT("Field"))->UnclaimedAmount;
	Estate->DebugSkipOffline(3600.0);
	TestTrue(TEXT("방치 정산으로 밀이 쌓임"), Test.Find(TEXT("Field"))->UnclaimedAmount > FieldBefore);
	TestTrue(TEXT("방치 정산으로 밀가루가 완성됨"), Test.Find(TEXT("Mill"))->UnclaimedAmount > 0);
	TestEqual(TEXT("방치 정산 중 알림 0번"), Counter.Total, 0);
	// 절전 복귀 따라잡기(방치 정산과 같은 경로)도 알림 없음
	Estate->AdvanceRealSeconds(FMath::Max(120.0, static_cast<double>(Estate->GetConfig().CatchupGapSeconds) + 1.0), TEXT("자동 시험 따라잡기"));
	TestEqual(TEXT("따라잡기 정산 중 알림 0번"), Counter.Total, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyLevelEffectsTest, "CozyRealm.Growth.LevelEffectsSlotsAndResidents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyLevelEffectsTest::RunTest(const FString& Parameters)
{
	CozyRelease2Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	// 나가야: Lv1 3명 · 레벨당 +1
	TestEqual(TEXT("나가야 Lv1 주민 한도 3명"), Estate->GetResidentCapacity(), 3);
	TestTrue(TEXT("시작 주민 수 ≤ 한도"), Estate->GetState().Residents.Num() <= Estate->GetResidentCapacity());
	Test.SetLevel(TEXT("Nagaya"), 3);
	TestEqual(TEXT("나가야 Lv3 주민 한도 5명"), Estate->GetResidentCapacity(), 5);
	// 후신소: Lv1 1개 · Lv5 2개
	TestEqual(TEXT("후신소 Lv1 동시 업그레이드 1개"), Estate->GetUpgradeSlotCount(), 1);
	Test.SetLevel(TEXT("Hushinso"), 4);
	TestEqual(TEXT("후신소 Lv4 동시 업그레이드 1개"), Estate->GetUpgradeSlotCount(), 1);
	Test.SetLevel(TEXT("Hushinso"), 5);
	TestEqual(TEXT("후신소 Lv5 동시 업그레이드 2개"), Estate->GetUpgradeSlotCount(), 2);
	// 제분소(가공 시설): Lv1 1개 · Lv5 2개
	const FGuid Mill = Test.IdOf(TEXT("Mill"));
	TestEqual(TEXT("제분소 Lv1 동시 가공 1개"), Estate->GetProcessingSlotCount(Mill), 1);
	Test.SetLevel(TEXT("Mill"), 4);
	TestEqual(TEXT("제분소 Lv4 동시 가공 1개"), Estate->GetProcessingSlotCount(Mill), 1);
	Test.SetLevel(TEXT("Mill"), 5);
	TestEqual(TEXT("제분소 Lv5 동시 가공 2개"), Estate->GetProcessingSlotCount(Mill), 2);

	// 실제 가공 시작에도 적용: Lv5면 다른 레시피 없이도 같은 완료품 작업 2개가 칸 2개를 씀 (두 번째는 '제작 추가'가 아니라 새 칸)
	FText Message;
	TestTrue(TEXT("제분소에 주민 배치"), Test.AssignFreeResident(TEXT("Mill")));
	Estate->DebugSetResource(TEXT("Wheat"), 30);
	TestTrue(TEXT("첫 가공 시작"), Estate->StartProcessing(Mill, TEXT("Mill_Flour"), 1, Message));
	const FCozyRecipeQuote Second = Estate->GetRecipeQuote(Mill, TEXT("Mill_Flour"), 1);
	TestTrue(TEXT("Lv5: 둘째 작업도 시작 가능"), Second.bCanStart);
	TestFalse(TEXT("Lv5: 둘째 작업은 '제작 추가'가 아니라 새 칸"), Second.bAppend);

	// 업그레이드 견적에 효과 안내가 있음 (제분소 Lv1 → 2: 'Lv.5에서 2개')
	Test.SetLevel(TEXT("Mill"), 1);
	const FCozyUpgradeQuote Quote = Estate->GetUpgradeQuote(Mill);
	TestTrue(TEXT("제분소 업그레이드 효과 안내가 있음"), Quote.EffectLines.Num() > 0);
	const FCozyUpgradeQuote NagayaQuote = Estate->GetUpgradeQuote(Test.IdOf(TEXT("Nagaya")));
	TestTrue(TEXT("나가야 업그레이드 효과 안내가 있음"), NagayaQuote.EffectLines.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyFloorCostTest, "CozyRealm.Decoration.FloorPaintCost", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyFloorCostTest::RunTest(const FString& Parameters)
{
	CozyRelease2Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	TestTrue(TEXT("상점 '바닥' 목록이 있음 (Floors.csv)"), Estate->GetFloorCatalog().Num() >= 6);
	const FName Dirt = Estate->FindFloorId(ECozyFloorTool::Brush, 1);
	const FName Tile = Estate->FindFloorId(ECozyFloorTool::Tile, 0);
	TestEqual(TEXT("붓 1번 = 흙길"), Dirt, FName(TEXT("GroundDirt")));
	TestEqual(TEXT("타일 0번 = 나무 바닥 타일"), Tile, FName(TEXT("TileWood")));
	const FCozyFloorRow* Row = Estate->GetFloorDef(Dirt);
	const int32* PerCell = Row ? Row->CostPerCell.Find(TEXT("Gold")) : nullptr;
	if (!TestNotNull(TEXT("흙길 칸당 골드"), PerCell) || !TestTrue(TEXT("칸당 비용 > 0"), *PerCell > 0))
	{
		return false;
	}
	CozyRelease2Test::FEventCounter Counter(Estate);
	FText Message;
	// 4칸 비용 차감
	Estate->DebugSetResource(TEXT("Gold"), *PerCell * 4 + 3);
	TestTrue(TEXT("4칸 칠하기 비용 냄"), Estate->PayFloorCost(Dirt, 4, Message));
	TestEqual(TEXT("골드 = 칸당 비용 × 4 만큼 줄어듦"), Estate->GetAmount(TEXT("Gold")), 3);
	TestEqual(TEXT("구매 알림 1번"), Counter.Of(ECozyEstateEventKind::Purchase), 1);
	// 부족하면 실패 · 아무것도 바뀌지 않음
	Estate->DebugSetResource(TEXT("Gold"), *PerCell * 2 - 1);
	TestFalse(TEXT("2칸 비용이 모자라면 실패"), Estate->PayFloorCost(Dirt, 2, Message));
	TestEqual(TEXT("실패하면 골드 그대로"), Estate->GetAmount(TEXT("Gold")), *PerCell * 2 - 1);
	TestFalse(TEXT("실패 이유가 있음"), Message.IsEmpty());
	TestEqual(TEXT("실패하면 알림 없음"), Counter.Of(ECozyEstateEventKind::Purchase), 1);
	TestFalse(TEXT("견적도 칠할 수 없음"), Estate->GetFloorQuote(Dirt, 2).bCanPay);
	// 0칸(지우기 등)은 무료
	TestTrue(TEXT("0칸은 무료"), Estate->PayFloorCost(Dirt, 0, Message));
	// 칸 계산: 칸 가운데의 작은 원은 1칸 · 칸 모서리의 큰 원은 4칸
	const FVector Center = Estate->GetCellCenter(FIntPoint(5, 5));
	TestEqual(TEXT("칸 가운데 작은 붓 = 1칸"), Estate->GetCellsInCircle(Center, 20.f).Num(), 1);
	const float Half = Estate->GetCellSize() * 0.5f;
	TestEqual(TEXT("칸 모서리 붓 = 4칸"), Estate->GetCellsInCircle(Center + FVector(Half, Half, 0.f), 30.f).Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyMeshPathTest, "CozyRealm.Look.EmptyMeshPathKeepsGreybox", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyMeshPathTest::RunTest(const FString& Parameters)
{
	CozyRelease2Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FCozyFacilityRow* Mill = Estate->GetFacilityDef(TEXT("Mill"));
	if (!TestNotNull(TEXT("제분소 정의"), Mill))
	{
		return false;
	}
	TestTrue(TEXT("지금 CSV 제분소 메시 경로는 비어 있음"), Mill->GetStageMesh(1).IsNull() && Mill->GetStageMesh(3).IsNull());
	const FCozyFacilityRow* Lantern = Estate->GetFacilityDef(TEXT("Lantern"));
	TestTrue(TEXT("장식(등롱)도 시설 정의로 등록되고 메시 칸이 비어 있음"), Lantern && Lantern->MeshStage1.IsNull());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 빈 경로 → 회색 상자
	ACozyFacilityActor* Grey = Test.World->SpawnActor<ACozyFacilityActor>(ACozyFacilityActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("시설 액터 생성"), Grey))
	{
		return false;
	}
	Grey->InitFacility(FGuid::NewGuid(), *Mill, Estate->GetCellSize(), nullptr, 1);
	TestFalse(TEXT("빈 경로면 회색 상자"), Grey->IsUsingAssetMesh());
	// 경로가 있으면 그 메시 (엔진 기본 원뿔 · 2단계가 비면 1단계로)
	FCozyFacilityRow WithMesh = *Mill;
	WithMesh.MeshStage1 = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cone.Cone")));
	ACozyFacilityActor* Meshed = Test.World->SpawnActor<ACozyFacilityActor>(ACozyFacilityActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Meshed->InitFacility(FGuid::NewGuid(), WithMesh, Estate->GetCellSize(), nullptr, 2);
	TestTrue(TEXT("경로가 있으면 메시를 씀 (2단계 빈칸 → 1단계 메시)"), Meshed->IsUsingAssetMesh());
	// 못 읽는 경로 → 회색 상자로 되돌림
	FCozyFacilityRow Broken = *Mill;
	Broken.MeshStage1 = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/CozyRealm/Facilities/NoSuchMesh.NoSuchMesh")));
	ACozyFacilityActor* Fallback = Test.World->SpawnActor<ACozyFacilityActor>(ACozyFacilityActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Fallback->InitFacility(FGuid::NewGuid(), Broken, Estate->GetCellSize(), nullptr, 1);
	TestFalse(TEXT("못 읽는 경로면 회색 상자"), Fallback->IsUsingAssetMesh());
	Grey->Destroy();
	Meshed->Destroy();
	Fallback->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyDemoEndOnceTest, "CozyRealm.Demo.EndNoticeOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyDemoEndOnceTest::RunTest(const FString& Parameters)
{
	CozyRelease2Test::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const int32 Final = Estate->GetDemoFinalShrineLevel();
	TestEqual(TEXT("데모 마지막 신사 레벨 = 3 (Growth.csv)"), Final, 3);
	TestFalse(TEXT("시작할 때는 안내 없음"), Estate->ShouldShowDemoEnd());
	Test.SetLevel(TEXT("Shrine"), Final - 1);
	TestFalse(TEXT("마지막 전 레벨에서는 안내 없음"), Estate->ShouldShowDemoEnd());
	Test.SetLevel(TEXT("Shrine"), Final);
	TestTrue(TEXT("마지막 레벨에 닿으면 안내"), Estate->ShouldShowDemoEnd());
	Estate->MarkDemoEndShown();
	TestFalse(TEXT("한 번 보여 준 뒤에는 다시 안 뜸"), Estate->ShouldShowDemoEnd());
	// 저장 상태로 다시 넣어도 (불러오기) 다시 안 뜸
	Estate->DebugSetState(Estate->GetState());
	TestFalse(TEXT("불러온 뒤에도 다시 안 뜸"), Estate->ShouldShowDemoEnd());
	TestTrue(TEXT("기록이 상태에 남음"), Estate->GetState().bDemoEndShown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyFrameThrottleTest, "CozyRealm.Settings.BackgroundFrameThrottle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyFrameThrottleTest::RunTest(const FString& Parameters)
{
	// (플레이어 값, 앞에 있음, 작은 창, 뒤 상한, 작은 창 상한)
	TestEqual(TEXT("뒤에 있으면 60 → 15"), UCozySettingsSubsystem::ComputeThrottledFps(60, false, false, 15, 30), 15);
	TestEqual(TEXT("뒤에 있으면 제한 없음 → 15"), UCozySettingsSubsystem::ComputeThrottledFps(0, false, false, 15, 30), 15);
	TestEqual(TEXT("플레이어 값이 더 낮으면 그대로"), UCozySettingsSubsystem::ComputeThrottledFps(10, false, false, 15, 30), 10);
	TestEqual(TEXT("작은 창 60 → 30"), UCozySettingsSubsystem::ComputeThrottledFps(60, true, true, 15, 30), 30);
	TestEqual(TEXT("작은 창이 뒤에 있으면 15"), UCozySettingsSubsystem::ComputeThrottledFps(60, false, true, 15, 30), 15);
	TestEqual(TEXT("보통 창은 플레이어 값"), UCozySettingsSubsystem::ComputeThrottledFps(120, true, false, 15, 30), 120);
	TestEqual(TEXT("상한 0은 낮추지 않음"), UCozySettingsSubsystem::ComputeThrottledFps(60, false, false, 0, 30), 60);
	return true;
}
#endif
