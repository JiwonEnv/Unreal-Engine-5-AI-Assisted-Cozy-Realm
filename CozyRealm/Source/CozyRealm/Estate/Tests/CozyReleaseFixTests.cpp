#include "Misc/AutomationTest.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Estate/CozyEstateSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// 출시 품질 수정 1차 자동 시험 (방치 정산 · 실제 시각 따라잡기 · 팥 숨김 · 안내 · 레벨 효과 · 불러오기 검증 · Claude 2026-10-30)
// 실제 저장 파일을 건드리지 않도록 빈 게임 월드에서 CSV만 읽어 새 게임 상태로 시험한다 (StartEstate·저장 없음).
#if WITH_DEV_AUTOMATION_TESTS
namespace CozyReleaseTest
{
	/** 시험용 빈 게임 월드와 그 영지 서비스 (CSV만 읽음 · 액터·저장 없음) */
	struct FTestEstate
	{
		UWorld* World = nullptr;
		UCozyEstateSubsystem* Estate = nullptr;
		FString Errors;

		FTestEstate()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CozyReleaseFixTest"));
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

		/** 정의 ID로 첫 시설 (보관 여부 무관 · 없으면 nullptr) */
		const FCozyFacilityState* Find(FName DefinitionId) const
		{
			return Estate->GetState().Facilities.FindByPredicate([DefinitionId](const FCozyFacilityState& Each) { return Each.DefinitionId == DefinitionId; });
		}

		FGuid IdOf(FName DefinitionId) const
		{
			const FCozyFacilityState* Facility = Find(DefinitionId);
			return Facility ? Facility->InstanceId : FGuid();
		}

		/** 빈 주민 한 명을 시설에 배치 */
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

		/** 상태의 시설 하나를 고친 뒤 다시 넣는다 (불러오기와 같은 정리 포함) */
		void EditFacility(FName DefinitionId, TFunctionRef<void(FCozyFacilityState&)> Edit)
		{
			FCozyEstateState State = Estate->GetState();
			for (FCozyFacilityState& Facility : State.Facilities)
			{
				if (Facility.DefinitionId == DefinitionId)
				{
					Edit(Facility);
					break;
				}
			}
			Estate->DebugSetState(State);
		}
	};

	/** 방치 재화 지급 결과 (시간 부적 · 심상 조각 · 누적 시간) */
	struct FRewardSnapshot
	{
		int32 Talismans = 0;
		int32 Shards = 0;
		double TalismanSeconds = 0.0;
		double ShardSeconds = 0.0;
		double GameSeconds = 0.0;
		int32 FieldUnclaimed = 0;
	};

	static FRewardSnapshot Snapshot(const FTestEstate& Test)
	{
		const FCozyEstateConfigRow& Config = Test.Estate->GetConfig();
		FRewardSnapshot Out;
		Out.Talismans = Test.Estate->GetAmount(Config.SpeedupItemId);
		Out.Shards = Test.Estate->GetAmount(Config.OfflineShardItemId);
		Out.TalismanSeconds = Test.Estate->GetState().TalismanRewardSeconds;
		Out.ShardSeconds = Test.Estate->GetState().MindShardRewardSeconds;
		Out.GameSeconds = Test.Estate->GetGameSeconds();
		const FCozyFacilityState* Field = Test.Find(TEXT("Field"));
		Out.FieldUnclaimed = Field ? Field->UnclaimedAmount : 0;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyOfflineCapTest, "CozyRealm.Offline.TwelveHourCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyOfflineCapTest::RunTest(const FString& Parameters)
{
	CozyReleaseTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FCozyEstateConfigRow& Config = Estate->GetConfig();
	TestEqual(TEXT("EstateConfig: 방치 최대 12시간"), FMath::RoundToInt32(Config.OfflineMaxSeconds), 12 * 3600);

	// 🙋 미수령 한도는 최대 속도에서 12시간 생산량 이상 (모든 생산·가공 시설 · 데이터 규칙)
	for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
	{
		const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility.DefinitionId);
		const int32 Need = Estate->GetMaxOfflineOutput(Facility.DefinitionId);
		if (Def && Need > 0)
		{
			TestTrue(*FString::Printf(TEXT("%s 미수령 한도 %d ≥ 12시간 최대 생산량 %d"), *Facility.DefinitionId.ToString(), Def->UnclaimedCapacity, Need), Def->UnclaimedCapacity >= Need);
		}
	}
	TestTrue(TEXT("밭의 12시간 최대 생산량이 예전 한도 20보다 큼"), Estate->GetMaxOfflineOutput(TEXT("Field")) > 20);

	// 밭에 주민을 두고 13시간 방치 → 12시간만 정산 · 부적 6 · 심상 조각 12 · 밀은 12시간치가 전부 쌓임
	TestTrue(TEXT("밭에 주민 배치"), Test.AssignFreeResident(TEXT("Field")));
	const CozyReleaseTest::FRewardSnapshot Before = CozyReleaseTest::Snapshot(Test);
	Estate->DebugSkipOffline(13.0 * 3600.0);
	const CozyReleaseTest::FRewardSnapshot After = CozyReleaseTest::Snapshot(Test);
	const FCozyOfflineReport* Report = Estate->GetPendingOfflineReport();
	if (TestNotNull(TEXT("방치 보상 결과가 있음"), Report))
	{
		TestEqual(TEXT("정산 시간은 12시간"), FMath::RoundToInt32(Report->AppliedSeconds), 12 * 3600);
		TestTrue(TEXT("12시간을 넘었다고 표시"), Report->bClamped);
	}
	TestEqual(TEXT("게임 시간도 12시간만 흐름"), FMath::RoundToInt32(After.GameSeconds - Before.GameSeconds), 12 * 3600);
	TestEqual(TEXT("시간 부적 = 12시간 ÷ 2시간"), After.Talismans - Before.Talismans, FMath::FloorToInt32(Config.OfflineMaxSeconds / Config.OfflineTalismanEverySeconds));
	TestEqual(TEXT("심상 조각 = 12시간 ÷ 1시간"), After.Shards - Before.Shards, FMath::FloorToInt32(Config.OfflineMaxSeconds / Config.OfflineMindShardEverySeconds));
	const FCozyCropRow* Wheat = Estate->GetCropDef(TEXT("Wheat"));
	const int32 Expected = Wheat ? FMath::FloorToInt32((Config.OfflineMaxSeconds - 1.f) / Wheat->ProductionSeconds) * Wheat->ProducedAmount : 0;
	TestTrue(*FString::Printf(TEXT("밭 미수령 밀이 12시간치 쌓임 (%d ≥ %d)"), After.FieldUnclaimed, Expected), After.FieldUnclaimed >= Expected && Expected > 20);
	const FCozyProductionView View = Estate->GetProductionView(Test.IdOf(TEXT("Field")));
	TestTrue(TEXT("미수령이 한도 안이라 밭이 멈추지 않음"), View.UnclaimedAmount < View.UnclaimedCapacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyOfflineNoDoubleTest, "CozyRealm.Offline.NoDoublePay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyOfflineNoDoubleTest::RunTest(const FString& Parameters)
{
	CozyReleaseTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FCozyEstateConfigRow& Config = Estate->GetConfig();
	const double Half = Config.OfflineTalismanEverySeconds * 0.75; // 1.5시간 × 2 = 3시간

	// 같은 3시간을 두 번에 나눠 받아도 한 번에 받은 것과 같음 (남는 시간은 이어지고 두 번 받지 않음)
	Estate->DebugSkipOffline(Half);
	const CozyReleaseTest::FRewardSnapshot First = CozyReleaseTest::Snapshot(Test);
	TestEqual(TEXT("1.5시간: 부적 0 (2시간이 안 됨)"), First.Talismans, 0);
	TestEqual(TEXT("1.5시간: 남은 누적 1.5시간"), FMath::RoundToInt32(First.TalismanSeconds), FMath::RoundToInt32(Half));
	Estate->DebugSkipOffline(Half);
	const CozyReleaseTest::FRewardSnapshot Second = CozyReleaseTest::Snapshot(Test);
	TestEqual(TEXT("1.5시간 + 1.5시간: 부적 1 (남은 시간이 이어짐)"), Second.Talismans, 1);
	TestEqual(TEXT("1.5시간 + 1.5시간: 심상 조각 3"), Second.Shards, FMath::FloorToInt32(Half * 2.0 / Config.OfflineMindShardEverySeconds));
	TestEqual(TEXT("부적 누적은 1시간 남음"), FMath::RoundToInt32(Second.TalismanSeconds), FMath::RoundToInt32(Half * 2.0 - Config.OfflineTalismanEverySeconds));

	// 0초 · 음수(시계를 되돌림)는 아무것도 주지 않음
	Estate->DebugSkipOffline(0.0);
	Estate->DebugSkipOffline(-7200.0);
	Estate->AdvanceRealSeconds(-7200.0, TEXT("시험"));
	const CozyReleaseTest::FRewardSnapshot Third = CozyReleaseTest::Snapshot(Test);
	TestEqual(TEXT("다시 정산해도 부적 그대로"), Third.Talismans, Second.Talismans);
	TestEqual(TEXT("다시 정산해도 심상 조각 그대로"), Third.Shards, Second.Shards);
	TestEqual(TEXT("다시 정산해도 게임 시간 그대로"), Third.GameSeconds, Second.GameSeconds);

	// 누적 시간은 저장 상태에 있어 저장 왕복 뒤에도 이어짐
	UCozyEstateSaveGame* Save = Cast<UCozyEstateSaveGame>(UGameplayStatics::CreateSaveGameObject(UCozyEstateSaveGame::StaticClass()));
	Save->FileVersion = 3;
	Save->State = Estate->GetState();
	TArray<uint8> Bytes;
	TestTrue(TEXT("메모리에 저장"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	const UCozyEstateSaveGame* Loaded = Cast<UCozyEstateSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (TestNotNull(TEXT("메모리에서 불러오기"), Loaded))
	{
		TestEqual(TEXT("부적 누적 시간 유지"), Loaded->State.TalismanRewardSeconds, Second.TalismanSeconds);
		TestEqual(TEXT("심상 조각 누적 시간 유지"), Loaded->State.MindShardRewardSeconds, Second.ShardSeconds);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyOnlineOfflineSameTest, "CozyRealm.Offline.OnlineMatchesOffline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyOnlineOfflineSameTest::RunTest(const FString& Parameters)
{
	const double Total = 3.0 * 3600.0;
	CozyReleaseTest::FRewardSnapshot Online, Offline;
	{
		// 켜 둔 동안: 따라잡기 기준보다 짧은 간격으로 3시간 (Tick과 같은 경로)
		CozyReleaseTest::FTestEstate Test;
		if (!TestNotNull(TEXT("시험 영지 준비 (켜 둔 동안)"), Test.Estate))
		{
			AddInfo(Test.Errors);
			return false;
		}
		TestTrue(TEXT("밭에 주민 배치"), Test.AssignFreeResident(TEXT("Field")));
		const double Chunk = FMath::Max(1.0, FMath::Min(10.0, Test.Estate->GetConfig().CatchupGapSeconds * 0.5));
		for (double Done = 0.0; Done < Total; Done += Chunk)
		{
			Test.Estate->AdvanceRealSeconds(Chunk, TEXT("시험"));
		}
		TestNull(TEXT("짧은 간격은 방치 보상 창 없음"), Test.Estate->GetPendingOfflineReport());
		Online = CozyReleaseTest::Snapshot(Test);
	}
	{
		// 꺼 둔 동안: 같은 3시간을 방치 정산으로
		CozyReleaseTest::FTestEstate Test;
		if (!TestNotNull(TEXT("시험 영지 준비 (꺼 둔 동안)"), Test.Estate))
		{
			AddInfo(Test.Errors);
			return false;
		}
		TestTrue(TEXT("밭에 주민 배치"), Test.AssignFreeResident(TEXT("Field")));
		Test.Estate->DebugSkipOffline(Total);
		Offline = CozyReleaseTest::Snapshot(Test);
	}
	TestEqual(TEXT("게임 시간 같음"), Online.GameSeconds, Offline.GameSeconds);
	TestEqual(TEXT("시간 부적 같음"), Online.Talismans, Offline.Talismans);
	TestEqual(TEXT("심상 조각 같음"), Online.Shards, Offline.Shards);
	TestEqual(TEXT("부적 누적 시간 같음"), Online.TalismanSeconds, Offline.TalismanSeconds);
	TestEqual(TEXT("밭 미수령 밀 같음"), Online.FieldUnclaimed, Offline.FieldUnclaimed);
	TestEqual(TEXT("3시간 → 부적 1"), Online.Talismans, 1);
	TestEqual(TEXT("3시간 → 심상 조각 3"), Online.Shards, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyRealTimeCatchupTest, "CozyRealm.Offline.RealTimeCatchup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyRealTimeCatchupTest::RunTest(const FString& Parameters)
{
	CozyReleaseTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FCozyEstateConfigRow& Config = Estate->GetConfig();
	TestTrue(TEXT("EstateConfig: 따라잡기 기준이 있음"), Config.CatchupGapSeconds > 1.f);

	// 음수 간격 (시계를 뒤로 돌림) → 무시
	const double Start = Estate->GetGameSeconds();
	TestEqual(TEXT("음수 간격은 진행 0"), Estate->AdvanceRealSeconds(-3600.0, TEXT("시험")), 0);
	TestEqual(TEXT("음수 간격은 게임 시간 그대로"), Estate->GetGameSeconds(), Start);
	TestNull(TEXT("음수 간격은 방치 보상 없음"), Estate->GetPendingOfflineReport());

	// 기준보다 짧은 간격 → 그대로 진행
	const double Short = FMath::FloorToDouble(Config.CatchupGapSeconds * 0.5);
	Estate->AdvanceRealSeconds(Short, TEXT("시험"));
	TestEqual(TEXT("짧은 간격은 그만큼 진행"), Estate->GetGameSeconds(), Start + Short);

	// 큰 간격 (절전 복귀 5시간) → 방치 정산과 같은 경로 · 결과 창
	Estate->AdvanceRealSeconds(5.0 * 3600.0, TEXT("시험"));
	TestEqual(TEXT("5시간 따라잡기"), Estate->GetGameSeconds(), Start + Short + 5.0 * 3600.0);
	const FCozyOfflineReport* Report = Estate->GetPendingOfflineReport();
	if (TestNotNull(TEXT("큰 간격은 방치 보상 결과가 생김"), Report))
	{
		TestEqual(TEXT("정산 5시간"), FMath::RoundToInt32(Report->AppliedSeconds), 5 * 3600);
		TestFalse(TEXT("12시간 안이라 잘리지 않음"), Report->bClamped);
	}
	TestEqual(TEXT("5시간 → 부적 2"), Estate->GetAmount(Config.SpeedupItemId), 2);
	TestEqual(TEXT("5시간 → 심상 조각 5"), Estate->GetAmount(Config.OfflineShardItemId), 5);

	// 아주 큰 간격 (20시간) → 12시간 상한
	Estate->DismissOfflineReport();
	const double BeforeHuge = Estate->GetGameSeconds();
	Estate->AdvanceRealSeconds(20.0 * 3600.0, TEXT("시험"));
	TestEqual(TEXT("20시간이어도 12시간만"), FMath::RoundToInt32(Estate->GetGameSeconds() - BeforeHuge), FMath::RoundToInt32(Config.OfflineMaxSeconds));
	Report = Estate->GetPendingOfflineReport();
	TestTrue(TEXT("12시간 상한 표시"), Report && Report->bClamped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyRedBeanHiddenTest, "CozyRealm.Crops.RedBeanHiddenInDemo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyRedBeanHiddenTest::RunTest(const FString& Parameters)
{
	CozyReleaseTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FGuid Field = Test.IdOf(TEXT("Field"));
	TArray<FName> Choices = Estate->GetCropChoices(Field);
	TestTrue(TEXT("밭 작물 목록에 밀"), Choices.Contains(TEXT("Wheat")));
	TestFalse(TEXT("밭 작물 목록에 팥 없음"), Choices.Contains(TEXT("RedBean")));
	TestFalse(TEXT("팥 해금 단계 없음 (6주차까지)"), Estate->HasCropUnlockPath(TEXT("RedBean")));

	// 밭 관리 시설을 끝까지 올려도 팥은 잠김 · 고를 수 없음
	Test.EditFacility(TEXT("FieldOffice"), [Estate](FCozyFacilityState& Facility) { Facility.Level = Estate->GetMaxFacilityLevel(TEXT("FieldOffice")); });
	TestFalse(TEXT("밭 관리 시설 최대 레벨에서도 팥 잠김"), Estate->IsCropUnlocked(TEXT("RedBean")));
	TestFalse(TEXT("최대 레벨에서도 목록에 팥 없음"), Estate->GetCropChoices(Field).Contains(TEXT("RedBean")));
	FText Reason;
	TestFalse(TEXT("팥으로 바꿀 수 없음"), Estate->CanSelectCrop(Field, TEXT("RedBean"), Reason));

	// 데이터 검사: 쓸 곳도 팔 곳도 없는 재료로 팥이 잡힘 · 밀·밀가루는 아님
	TArray<FName> DeadEnd, Unused;
	Estate->FindUnusableMaterials(DeadEnd, Unused);
	TestTrue(TEXT("팥은 쓸 곳·팔 곳이 없는 재료로 잡힘"), DeadEnd.Contains(TEXT("RedBean")) || Unused.Contains(TEXT("RedBean")));
	TestFalse(TEXT("밀은 경고 아님"), DeadEnd.Contains(TEXT("Wheat")) || Unused.Contains(TEXT("Wheat")));
	TestFalse(TEXT("밀가루는 경고 아님"), DeadEnd.Contains(TEXT("Flour")) || Unused.Contains(TEXT("Flour")));
	TestFalse(TEXT("데모 데이터에는 만들 수 있는데 쓸 곳이 없는 재료가 없음"), DeadEnd.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyRedBeanLoadTest, "CozyRealm.Crops.RedBeanSaveMigrates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyRedBeanLoadTest::RunTest(const FString& Parameters)
{
	CozyReleaseTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FGuid Field = Test.IdOf(TEXT("Field"));

	// ① 팥 밭 · 미수령 팥 5 · 진행 중인 팥 주기가 저장된 파일
	{
		FCozyEstateState State = Estate->GetState();
		FCozyFacilityState* Facility = State.Facilities.FindByPredicate([&Field](const FCozyFacilityState& Each) { return Each.InstanceId == Field; });
		Facility->SelectedCropId = TEXT("RedBean");
		Facility->UnclaimedItemId = TEXT("RedBean");
		Facility->UnclaimedAmount = 5;
		FCozyJobRecord Job;
		Job.JobId = FGuid::NewGuid();
		Job.FacilityId = Field;
		Job.Type = ECozyJobType::Production;
		Job.ContentId = TEXT("RedBean");
		Job.DurationSeconds = 15.0;
		Facility->ActiveJobId = Job.JobId;
		State.Jobs.Add(Job);
		Estate->DebugSetState(State);
	}
	const FCozyFacilityState* Loaded = Estate->FindFacility(Field);
	TestEqual(TEXT("불러오면 밭 작물은 밀"), Loaded->SelectedCropId, FName(TEXT("Wheat")));
	TestEqual(TEXT("미수령 팥은 창고로 옮김 (버리지 않음)"), Estate->GetAmount(TEXT("RedBean")), 5);
	TestEqual(TEXT("미수령분 비움"), Loaded->UnclaimedAmount, 0);
	TestFalse(TEXT("진행 중이던 팥 주기는 정리됨"), Estate->GetState().Jobs.ContainsByPredicate([](const FCozyJobRecord& Job) { return Job.ContentId == TEXT("RedBean"); }));

	// ② 창고가 거의 가득 (98/100) 이면 들어갈 만큼만 옮기고 나머지는 밭에 남김
	{
		FCozyEstateState State = Estate->GetState();
		State.Resources.Add(TEXT("RedBean"), Estate->GetConfig().StorageCapPerItem - 2);
		FCozyFacilityState* Facility = State.Facilities.FindByPredicate([&Field](const FCozyFacilityState& Each) { return Each.InstanceId == Field; });
		Facility->SelectedCropId = TEXT("RedBean");
		Facility->UnclaimedItemId = TEXT("RedBean");
		Facility->UnclaimedAmount = 5;
		Facility->ActiveJobId.Invalidate();
		Estate->DebugSetState(State);
	}
	Loaded = Estate->FindFacility(Field);
	TestEqual(TEXT("창고 가득까지만"), Estate->GetAmount(TEXT("RedBean")), Estate->GetConfig().StorageCapPerItem);
	TestEqual(TEXT("남은 팥 3개는 밭 미수령으로"), Loaded->UnclaimedAmount, 3);
	TestEqual(TEXT("작물은 밀"), Loaded->SelectedCropId, FName(TEXT("Wheat")));

	// ③ 밀 밭은 그대로
	Test.EditFacility(TEXT("Field"), [](FCozyFacilityState& Facility) { Facility.UnclaimedAmount = 0; Facility.UnclaimedItemId = NAME_None; });
	TestEqual(TEXT("밀 밭은 바뀌지 않음"), Estate->FindFacility(Field)->SelectedCropId, FName(TEXT("Wheat")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTutorialSaveTest, "CozyRealm.Tutorial.DoneFlagRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyTutorialSaveTest::RunTest(const FString& Parameters)
{
	FCozyEstateState Saved;
	{
		CozyReleaseTest::FTestEstate Test;
		if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
		{
			AddInfo(Test.Errors);
			return false;
		}
		TestFalse(TEXT("새 게임은 안내 안 끝남"), Test.Estate->IsTutorialDone());
		Test.Estate->SetTutorialDone(true);
		TestTrue(TEXT("안내 완료 표시"), Test.Estate->IsTutorialDone());
		Saved = Test.Estate->GetState();
	}
	UCozyEstateSaveGame* Save = Cast<UCozyEstateSaveGame>(UGameplayStatics::CreateSaveGameObject(UCozyEstateSaveGame::StaticClass()));
	Save->FileVersion = 3;
	Save->State = Saved;
	TArray<uint8> Bytes;
	TestTrue(TEXT("메모리에 저장"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	const UCozyEstateSaveGame* Loaded = Cast<UCozyEstateSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("메모리에서 불러오기"), Loaded))
	{
		return false;
	}
	CozyReleaseTest::FTestEstate Reloaded;
	if (!TestNotNull(TEXT("두 번째 시험 영지"), Reloaded.Estate))
	{
		return false;
	}
	TestFalse(TEXT("다른 새 게임은 안내가 다시 뜸 (설정 파일 값과 무관)"), Reloaded.Estate->IsTutorialDone());
	Reloaded.Estate->DebugSetState(Loaded->State);
	TestTrue(TEXT("저장 왕복 뒤에도 안내 완료 유지"), Reloaded.Estate->IsTutorialDone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTutorialRewardTest, "CozyRealm.Tutorial.RewardOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyTutorialRewardTest::RunTest(const FString& Parameters)
{
	CozyReleaseTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const TArray<const FCozyTutorialRow*> Steps = Estate->GetTutorialSteps();
	TestEqual(TEXT("안내 5단계"), Steps.Num(), 5);
	if (Steps.Num() < 5)
	{
		return false;
	}
	// 2단계 수확 목표는 3단계 가공 1회에 필요한 밀 이상
	const FCozyRecipeRow* Flour = Estate->GetRecipeDef(TEXT("Mill_Flour"));
	const int32* WheatPerRun = Flour ? Flour->Inputs.Find(TEXT("Wheat")) : nullptr;
	TestTrue(TEXT("2단계 목표 재료는 밀"), Steps[1]->GoalItem == TEXT("Wheat"));
	TestTrue(TEXT("2단계 목표 ≥ 밀가루 1회 밀 수"), WheatPerRun && Steps[1]->GoalAmount >= *WheatPerRun);
	// 5단계: 신사 업그레이드 + 시간 단축 · 보상은 Rewards.csv에 있는 튜토리얼 부적
	TestEqual(TEXT("5단계 목표 = 시간 단축"), Steps[4]->Goal, FName(TEXT("Speedup")));
	TestEqual(TEXT("5단계 목표 재료 = 시간 부적"), Steps[4]->GoalItem, Estate->GetConfig().SpeedupItemId);
	const FName RewardId = Steps[4]->RewardId;
	TestFalse(TEXT("5단계에 보상 ID가 있음"), RewardId.IsNone());

	// 한 번만 지급
	const FName Talisman = Estate->GetConfig().SpeedupItemId;
	const int32 Before = Estate->GetAmount(Talisman);
	FText Message;
	TestTrue(TEXT("처음에는 지급"), Estate->GrantOneTimeReward(RewardId, Message));
	TestEqual(TEXT("부적 +1"), Estate->GetAmount(Talisman), Before + 1);
	TestFalse(TEXT("두 번째는 지급 안 함"), Estate->GrantOneTimeReward(RewardId, Message));
	TestEqual(TEXT("두 번째 뒤에도 +1 그대로"), Estate->GetAmount(Talisman), Before + 1);
	// 저장 상태(받은 보상 목록)로 다시 넣어도 다시 받지 않음
	Estate->DebugSetState(Estate->GetState());
	TestFalse(TEXT("불러온 뒤에도 다시 받지 않음"), Estate->GrantOneTimeReward(RewardId, Message));
	TestEqual(TEXT("불러온 뒤에도 +1 그대로"), Estate->GetAmount(Talisman), Before + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyStoredLevelTest, "CozyRealm.Growth.StoredFacilityNoLevelEffect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyStoredLevelTest::RunTest(const FString& Parameters)
{
	CozyReleaseTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FGuid Field = Test.IdOf(TEXT("Field"));
	const FGuid Mill = Test.IdOf(TEXT("Mill"));
	const int32 MaxOffice = Estate->GetMaxFacilityLevel(TEXT("FieldOffice"));
	TestTrue(TEXT("밭 관리 시설 최대 레벨 2 이상"), MaxOffice >= 2);

	// 놓인 밭 관리 시설 최대 레벨 → 밭 속도 오름
	Test.EditFacility(TEXT("FieldOffice"), [MaxOffice](FCozyFacilityState& Facility) { Facility.Level = MaxOffice; Facility.bStored = false; });
	const float PlacedSpeed = Estate->GetProductionView(Field).SpeedMultiplier;
	TestTrue(TEXT("놓인 관리 시설은 속도 효과"), PlacedSpeed > 1.01f);

	// 같은 시설을 보관하면 효과 없음 · 선행 조건도 못 채움
	Test.EditFacility(TEXT("FieldOffice"), [](FCozyFacilityState& Facility) { Facility.bStored = true; });
	TestEqual(TEXT("보관 중인 관리 시설은 속도 효과 없음"), Estate->GetProductionView(Field).SpeedMultiplier, 1.f);
	const FCozyUpgradeQuote Quote = Estate->GetUpgradeQuote(Mill);
	const FCozyUpgradeQuote::FCondition* Condition = Quote.Conditions.FindByPredicate([](const FCozyUpgradeQuote::FCondition& Each) { return Each.FacilityId == TEXT("FieldOffice"); });
	if (TestNotNull(TEXT("제분소 업그레이드에 밭 관리 시설 조건"), Condition))
	{
		TestFalse(TEXT("보관 중인 관리 시설은 선행 조건에 안 셈"), Condition->bMet);
		TestEqual(TEXT("보관 중이면 지금 레벨 0으로 표시"), Condition->Current, 0);
	}
	// 해금(작물·시설)은 진행 기록이라 보관해도 유지
	TestTrue(TEXT("보관해도 밭 관리 시설은 해금된 시설"), Estate->IsFacilityUnlocked(TEXT("FieldOffice")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozySanitizeNumbersTest, "CozyRealm.Save.SanitizeNumbers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozySanitizeNumbersTest::RunTest(const FString& Parameters)
{
	CozyReleaseTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	FCozyEstateState State = Estate->GetState();
	State.Resources.Add(TEXT("Wheat"), -5);
	State.Resources.Add(TEXT("Gold"), -100);
	for (FCozyFacilityState& Facility : State.Facilities)
	{
		if (Facility.DefinitionId == TEXT("Field"))
		{
			Facility.UnclaimedItemId = TEXT("Wheat");
			Facility.UnclaimedAmount = 999999;
		}
		else if (Facility.DefinitionId == TEXT("Mill"))
		{
			Facility.Level = 99;
		}
		else if (Facility.DefinitionId == TEXT("Shop"))
		{
			Facility.Level = 0;
			Facility.UnclaimedItemId = TEXT("Flour");
			Facility.UnclaimedAmount = -3;
		}
	}
	Estate->DebugSetState(State);
	TestEqual(TEXT("음수 밀 → 0"), Estate->GetAmount(TEXT("Wheat")), 0);
	TestEqual(TEXT("음수 골드 → 0"), Estate->GetAmount(TEXT("Gold")), 0);
	const FCozyFacilityState* Field = Test.Find(TEXT("Field"));
	const FCozyFacilityRow* FieldDef = Estate->GetFacilityDef(TEXT("Field"));
	TestEqual(TEXT("미수령분은 한도로 자름"), Field->UnclaimedAmount, FieldDef->UnclaimedCapacity);
	TestEqual(TEXT("데이터보다 높은 레벨 → 최대 레벨"), Test.Find(TEXT("Mill"))->Level, Estate->GetMaxFacilityLevel(TEXT("Mill")));
	const FCozyFacilityState* Shop = Test.Find(TEXT("Shop"));
	TestEqual(TEXT("레벨 0 → 1"), Shop->Level, 1);
	TestEqual(TEXT("음수 미수령 → 0"), Shop->UnclaimedAmount, 0);
	TestTrue(TEXT("미수령 0이면 품목 칸도 비움"), Shop->UnclaimedItemId.IsNone());
	return true;
}
#endif
