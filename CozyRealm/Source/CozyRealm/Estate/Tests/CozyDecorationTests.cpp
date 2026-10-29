#include "Misc/AutomationTest.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Estate/CozyEstateSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// 꾸미기 상점 · 꾸미기 점수 자동 시험 (4주차 기능 2 · Claude 2026-10-29)
// 실제 저장 파일을 건드리지 않도록 빈 게임 월드에서 CSV만 읽어 새 게임 상태로 시험한다 (StartEstate·저장 없음).
#if WITH_DEV_AUTOMATION_TESTS
namespace CozyDecorationTest
{
	/** 시험용 빈 게임 월드와 그 영지 서비스 (CSV만 읽음 · 액터·저장 없음) */
	struct FTestEstate
	{
		UWorld* World = nullptr;
		UCozyEstateSubsystem* Estate = nullptr;
		FString Errors;

		FTestEstate()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CozyDecorationTest"));
			if (World)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
				Estate = World->GetSubsystem<UCozyEstateSubsystem>();
			}
			if (Estate && !Estate->DebugInitDataOnly(Errors))
			{
				Estate = nullptr;
			}
			if (Estate)
			{
				// 지면 추적 없이 칸 높이를 정하려고 모든 시설을 높이 0의 표면에 놓인 것으로 둔다
				FCozyEstateState State = Estate->GetState();
				for (FCozyFacilityState& Facility : State.Facilities)
				{
					Facility.bHasSurfacePlacement = true;
					Facility.SurfaceHeight = 0.f;
				}
				Estate->DebugSetState(State);
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

		/** 장식 정의 ID로 상태의 장식 하나 (없으면 nullptr) */
		const FCozyFacilityState* FindDecoration(FName DefinitionId) const
		{
			return Estate->GetState().Facilities.FindByPredicate([DefinitionId](const FCozyFacilityState& Each) { return Each.DefinitionId == DefinitionId; });
		}

		int32 CountOf(FName DefinitionId) const
		{
			int32 Count = 0;
			for (const FCozyFacilityState& Each : Estate->GetState().Facilities)
			{
				Count += Each.DefinitionId == DefinitionId ? 1 : 0;
			}
			return Count;
		}

		/** 장식을 보관함에서 꺼내 Coord에 놓은 상태로 바꾼다 (배치 확정과 같은 상태값만) */
		void PlaceDecoration(const FGuid& Id, FIntPoint Coord, int32 Rotation = 0)
		{
			FCozyEstateState State = Estate->GetState();
			for (FCozyFacilityState& Facility : State.Facilities)
			{
				if (Facility.InstanceId == Id)
				{
					Facility.bStored = false;
					Facility.GridCoord = Coord;
					Facility.Rotation = Rotation;
					Facility.bHasSurfacePlacement = true;
					Facility.SurfaceHeight = 0.f;
				}
			}
			Estate->DebugSetState(State);
		}

		/** 장식 하나를 그 가격만큼 재화를 맞춘 뒤 산다 · 산 장식 ID */
		FGuid BuyWithExactFunds(FName DecorationId)
		{
			const FCozyDecorationQuote Quote = Estate->GetDecorationQuote(DecorationId);
			for (const FCozyDecorationQuote::FAmount& Cost : Quote.Costs)
			{
				Estate->DebugSetResource(Cost.ItemId, Cost.Amount);
			}
			FText Message;
			TSet<FGuid> Before;
			for (const FCozyFacilityState& Each : Estate->GetState().Facilities)
			{
				Before.Add(Each.InstanceId);
			}
			if (!Estate->BuyDecoration(DecorationId, Message))
			{
				return FGuid();
			}
			for (const FCozyFacilityState& Each : Estate->GetState().Facilities)
			{
				if (!Before.Contains(Each.InstanceId))
				{
					return Each.InstanceId;
				}
			}
			return FGuid();
		}

		/** 영지 안에서 아무것과도 겹치지 않는 첫 칸 (Id의 크기 기준) */
		bool FindFreeCell(const FGuid& Id, FIntPoint& OutCell) const
		{
			const FIntPoint Grid = Estate->GetGridSize();
			const FIntPoint Size = Estate->GetFootprint(Id, 0);
			for (int32 Y = 0; Y + Size.Y <= Grid.Y; ++Y)
			{
				for (int32 X = 0; X + Size.X <= Grid.X; ++X)
				{
					FGuid Other;
					if (!Estate->FindPlacementOverlap(Id, FIntPoint(X, Y), 0, 0.f, Other))
					{
						OutCell = FIntPoint(X, Y);
						return true;
					}
				}
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyDecorationScoreBonusTest, "CozyRealm.Decoration.ScoreBonusSteps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyDecorationScoreBonusTest::RunTest(const FString& Parameters)
{
	CozyDecorationTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	const FCozyEstateConfigRow& Config = Test.Estate->GetConfig();
	TestEqual(TEXT("EstateConfig: 50점마다"), Config.DecorPointsPerStep, 50);
	// 🙋 50점마다 +5% · 최대 +25%
	const int32 Scores[] = { 0, 49, 50, 120, 300 };
	const int32 Expected[] = { 0, 0, 5, 10, 25 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Scores); ++Index)
	{
		const float Bonus = UCozyEstateSubsystem::ComputeDecorationBonus(Scores[Index], Config.DecorPointsPerStep, Config.DecorBonusPerStep, Config.DecorBonusMax);
		TestEqual(*FString::Printf(TEXT("%d점 → +%d%%"), Scores[Index], Expected[Index]), FMath::RoundToInt(Bonus * 100.f), Expected[Index]);
	}
	TestEqual(TEXT("단계 점수가 0이면 보너스 없음"), UCozyEstateSubsystem::ComputeDecorationBonus(500, 0, 0.05f, 0.25f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyDecorationPurchaseTest, "CozyRealm.Decoration.PurchaseDeductsExactly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyDecorationPurchaseTest::RunTest(const FString& Parameters)
{
	CozyDecorationTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const TArray<FName> Catalog = Estate->GetDecorationCatalog();
	TestTrue(TEXT("상점 목록에 장식이 있음"), Catalog.Num() > 0);
	TestTrue(TEXT("등롱이 상점에 있음"), Catalog.Contains(TEXT("Lantern")));
	TestTrue(TEXT("장식은 시설 정의로도 등록됨 (배치 모드 공용)"), Estate->GetFacilityDef(TEXT("Lantern")) != nullptr);

	// 골드로 사는 장식: 가격 + 5골드를 가진 상태에서 사면 정확히 가격만큼 줄고 보관함에 1개
	const FCozyDecorationQuote Quote = Estate->GetDecorationQuote(TEXT("Lantern"));
	TestTrue(TEXT("등롱 가격이 골드 하나"), Quote.Costs.Num() == 1 && Quote.Costs[0].ItemId == TEXT("Gold"));
	const int32 Price = Quote.Costs.Num() > 0 ? Quote.Costs[0].Amount : 0;
	Estate->DebugSetResource(TEXT("Gold"), Price + 5);
	FText Message;
	TestTrue(TEXT("돈이 충분하면 구매 성공"), Estate->BuyDecoration(TEXT("Lantern"), Message));
	TestEqual(TEXT("가격만큼 정확히 차감"), Estate->GetAmount(TEXT("Gold")), 5);
	TestEqual(TEXT("등롱 1개"), Test.CountOf(TEXT("Lantern")), 1);
	const FCozyFacilityState* Bought = Test.FindDecoration(TEXT("Lantern"));
	TestTrue(TEXT("산 장식은 보관함에 있음 (🤖 구매 → 보관함)"), Bought && Bought->bStored);
	TestEqual(TEXT("보관함 장식은 점수 없음"), Estate->GetDecorationScore(), 0);

	// 돈이 모자라면 실패하고 아무것도 바뀌지 않음
	TestFalse(TEXT("돈이 모자라면 구매 실패"), Estate->BuyDecoration(TEXT("Lantern"), Message));
	TestFalse(TEXT("실패 이유가 있음"), Message.IsEmpty());
	TestEqual(TEXT("실패하면 골드 그대로"), Estate->GetAmount(TEXT("Gold")), 5);
	TestEqual(TEXT("실패하면 장식 수 그대로"), Test.CountOf(TEXT("Lantern")), 1);
	TestFalse(TEXT("견적도 살 수 없음으로 표시"), Estate->GetDecorationQuote(TEXT("Lantern")).bCanBuy);

	// 심상 조각으로 사는 장식 (벚나무)
	const FCozyDecorationQuote Cherry = Estate->GetDecorationQuote(TEXT("CherryTree"));
	TestTrue(TEXT("벚나무 가격은 심상 조각"), Cherry.Costs.Num() == 1 && Cherry.Costs[0].ItemId == TEXT("MindShard"));
	Estate->DebugSetResource(TEXT("MindShard"), 0);
	TestFalse(TEXT("심상 조각이 없으면 실패"), Estate->BuyDecoration(TEXT("CherryTree"), Message));
	TestEqual(TEXT("실패하면 벚나무 없음"), Test.CountOf(TEXT("CherryTree")), 0);
	const int32 ShardPrice = Cherry.Costs.Num() > 0 ? Cherry.Costs[0].Amount : 0;
	Estate->DebugSetResource(TEXT("MindShard"), ShardPrice);
	TestTrue(TEXT("심상 조각이 맞으면 성공"), Estate->BuyDecoration(TEXT("CherryTree"), Message));
	TestEqual(TEXT("심상 조각 0개 남음"), Estate->GetAmount(TEXT("MindShard")), 0);
	TestEqual(TEXT("골드는 그대로"), Estate->GetAmount(TEXT("Gold")), 5);
	TestFalse(TEXT("없는 장식은 살 수 없음"), Estate->BuyDecoration(TEXT("NoSuchDecoration"), Message));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyDecorationStoredTest, "CozyRealm.Decoration.StoredDoesNotCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyDecorationStoredTest::RunTest(const FString& Parameters)
{
	CozyDecorationTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FCozyDecorationRow* Lantern = Estate->GetDecorationDef(TEXT("Lantern"));
	if (!TestNotNull(TEXT("등롱 정의"), Lantern))
	{
		return false;
	}
	// 밭의 생산 속도 배율 (꾸미기 점수 0일 때)
	const FCozyFacilityState* Field = Estate->GetState().Facilities.FindByPredicate([](const FCozyFacilityState& Each) { return Each.DefinitionId == TEXT("Field") && !Each.bStored; });
	TestNotNull(TEXT("시작 밭이 있음"), Field);
	const float BaseSpeed = Field ? Estate->GetProductionView(Field->InstanceId).SpeedMultiplier : 1.f;
	TestTrue(TEXT("점수 0이면 밭 속도 배율이 1 이상"), BaseSpeed >= 1.f);

	// 50점을 넘도록 등롱을 사서 보관함에만 두면 점수 0 · 보너스 0
	const int32 Needed = FMath::DivideAndRoundUp(Estate->GetConfig().DecorPointsPerStep, FMath::Max(1, Lantern->Points));
	TArray<FGuid> Bought;
	for (int32 Index = 0; Index < Needed; ++Index)
	{
		Bought.Add(Test.BuyWithExactFunds(TEXT("Lantern")));
	}
	TestFalse(TEXT("모두 구매됨"), Bought.Contains(FGuid()));
	TestEqual(TEXT("보관함 장식만 있으면 점수 0"), Estate->GetDecorationScore(), 0);
	TestEqual(TEXT("보관함 장식만 있으면 보너스 0"), Estate->GetDecorationScoreView().Bonus, 0.f);

	// 빈 칸에 하나씩 놓으면 놓은 것만 점수
	for (int32 Index = 0; Index < Bought.Num(); ++Index)
	{
		FIntPoint Cell;
		if (!TestTrue(TEXT("빈 칸 찾기"), Test.FindFreeCell(Bought[Index], Cell)))
		{
			return false;
		}
		Test.PlaceDecoration(Bought[Index], Cell);
		TestEqual(*FString::Printf(TEXT("%d개 놓으면 %d점"), Index + 1, (Index + 1) * Lantern->Points), Estate->GetDecorationScore(), (Index + 1) * Lantern->Points);
	}
	const FCozyDecorationScoreView View = Estate->GetDecorationScoreView();
	TestEqual(TEXT("50점 이상이면 +5%"), FMath::RoundToInt(View.Bonus * 100.f), 5);
	if (Field)
	{
		TestTrue(TEXT("생산 속도 배율에 꾸미기 보너스가 곱해짐"), FMath::IsNearlyEqual(Estate->GetProductionView(Field->InstanceId).SpeedMultiplier, BaseSpeed * 1.05f, 0.001f));
	}

	// 하나를 다시 보관하면 그만큼 점수가 빠짐 (돌려받는 재화 없음)
	const int32 GoldBefore = Estate->GetAmount(TEXT("Gold"));
	FText Message;
	TestTrue(TEXT("장식 보관 가능"), Estate->CanStoreFacility(Bought[0], Message));
	FCozyEstateState State = Estate->GetState();
	State.Facilities.FindByPredicate([&](const FCozyFacilityState& Each) { return Each.InstanceId == Bought[0]; })->bStored = true;
	Estate->DebugSetState(State);
	TestEqual(TEXT("보관하면 점수에서 빠짐"), Estate->GetDecorationScore(), (Bought.Num() - 1) * Lantern->Points);
	TestEqual(TEXT("보관해도 재화는 그대로"), Estate->GetAmount(TEXT("Gold")), GoldBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyDecorationOverlapTest, "CozyRealm.Decoration.OverlapWithFacilityRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyDecorationOverlapTest::RunTest(const FString& Parameters)
{
	CozyDecorationTest::FTestEstate Test;
	if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
	{
		AddInfo(Test.Errors);
		return false;
	}
	UCozyEstateSubsystem* Estate = Test.Estate;
	const FCozyFacilityState* Facility = Estate->GetState().Facilities.FindByPredicate([Estate](const FCozyFacilityState& Each) { return !Each.bStored && !Estate->IsDecoration(Each.DefinitionId); });
	if (!TestNotNull(TEXT("시작 시설이 있음"), Facility))
	{
		return false;
	}
	const FGuid FacilityId = Facility->InstanceId;
	const FIntPoint FacilityCell = Facility->GridCoord;

	// 장식 → 시설 자리: 겹침으로 거부
	const FGuid Path = Test.BuyWithExactFunds(TEXT("StonePath"));
	const FGuid Lantern = Test.BuyWithExactFunds(TEXT("Lantern"));
	TestTrue(TEXT("돌길·등롱 구매"), Path.IsValid() && Lantern.IsValid());
	FGuid Other;
	TestTrue(TEXT("시설 칸에 장식을 놓으면 겹침"), Estate->FindPlacementOverlap(Lantern, FacilityCell, 0, 0.f, Other));
	TestEqual(TEXT("겹친 상대는 그 시설"), Other, FacilityId);
	TestTrue(TEXT("걸을 수 있는 돌길도 시설과는 겹칠 수 없음"), Estate->FindPlacementOverlap(Path, FacilityCell, 0, 0.f, Other));

	// 빈 칸에 등롱을 놓으면 그 칸에 시설을 옮길 수 없음 (반대 방향)
	FIntPoint Free;
	if (!TestTrue(TEXT("빈 칸 찾기"), Test.FindFreeCell(Lantern, Free)))
	{
		return false;
	}
	TestFalse(TEXT("빈 칸은 겹치지 않음"), Estate->FindPlacementOverlap(Lantern, Free, 0, 0.f, Other));
	Test.PlaceDecoration(Lantern, Free);
	TestTrue(TEXT("장식 칸에 시설을 놓으면 겹침"), Estate->FindPlacementOverlap(FacilityId, Free, 0, 0.f, Other));
	// 1×1끼리는 그 장식 하나와만 겹침 (시설은 칸이 커서 이웃 시설과 먼저 겹칠 수 있음)
	TestTrue(TEXT("장식 칸에 다른 장식을 놓으면 겹침"), Estate->FindPlacementOverlap(Path, Free, 0, 0.f, Other));
	TestEqual(TEXT("겹친 상대는 그 장식"), Other, Lantern);
	TestFalse(TEXT("등롱 칸은 걸을 수 없음"), Estate->IsCellWalkable(Free));

	// 2×1 장식은 회전하면 차지 칸이 바뀜
	const FGuid Bench = Test.BuyWithExactFunds(TEXT("Bench"));
	TestEqual(TEXT("나무 의자 2×1"), Estate->GetFootprint(Bench, 0), FIntPoint(2, 1));
	TestEqual(TEXT("90° 돌리면 1×2"), Estate->GetFootprint(Bench, 1), FIntPoint(1, 2));

	// 🤖 돌길은 걸을 수 있음
	FIntPoint PathCell;
	if (TestTrue(TEXT("돌길 빈 칸 찾기"), Test.FindFreeCell(Path, PathCell)))
	{
		Test.PlaceDecoration(Path, PathCell);
		TestTrue(TEXT("돌길 칸은 걸을 수 있음"), Estate->IsCellWalkable(PathCell));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyDecorationSaveTest, "CozyRealm.Decoration.SaveLoadRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyDecorationSaveTest::RunTest(const FString& Parameters)
{
	FCozyEstateState Saved;
	int32 SavedScore = 0;
	FGuid PlacedId, StoredId;
	FIntPoint PlacedCell;
	{
		CozyDecorationTest::FTestEstate Test;
		if (!TestNotNull(TEXT("시험 영지 준비 (CSV 읽기)"), Test.Estate))
		{
			AddInfo(Test.Errors);
			return false;
		}
		PlacedId = Test.BuyWithExactFunds(TEXT("Bench"));
		StoredId = Test.BuyWithExactFunds(TEXT("Pond"));
		if (!TestTrue(TEXT("빈 칸 찾기"), Test.FindFreeCell(PlacedId, PlacedCell)))
		{
			return false;
		}
		Test.PlaceDecoration(PlacedId, PlacedCell, 1);
		SavedScore = Test.Estate->GetDecorationScore();
		TestTrue(TEXT("놓은 의자만 점수"), SavedScore == Test.Estate->GetDecorationDef(TEXT("Bench"))->Points);
		Saved = Test.Estate->GetState();
	}

	// 저장 파일과 같은 방식으로 메모리에 쓰고 읽기 (실제 슬롯은 건드리지 않음)
	UCozyEstateSaveGame* Save = Cast<UCozyEstateSaveGame>(UGameplayStatics::CreateSaveGameObject(UCozyEstateSaveGame::StaticClass()));
	Save->FileVersion = 2;
	Save->State = Saved;
	TArray<uint8> Bytes;
	TestTrue(TEXT("메모리에 저장"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	const UCozyEstateSaveGame* Loaded = Cast<UCozyEstateSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("메모리에서 불러오기"), Loaded))
	{
		return false;
	}

	// 새 영지에 불러온 상태를 넣으면 (불러오기 정리 포함) 장식·점수가 그대로
	CozyDecorationTest::FTestEstate Reloaded;
	if (!TestNotNull(TEXT("두 번째 시험 영지"), Reloaded.Estate))
	{
		return false;
	}
	Reloaded.Estate->DebugSetState(Loaded->State);
	const FCozyFacilityState* Placed = Reloaded.Estate->FindFacility(PlacedId);
	const FCozyFacilityState* Stored = Reloaded.Estate->FindFacility(StoredId);
	TestTrue(TEXT("놓은 장식 유지 (정의 · 칸 · 회전 · 보관 아님)"), Placed && Placed->DefinitionId == TEXT("Bench") && Placed->GridCoord == PlacedCell && Placed->Rotation == 1 && !Placed->bStored && Placed->bHasSurfacePlacement);
	TestTrue(TEXT("보관함 장식 유지"), Stored && Stored->DefinitionId == TEXT("Pond") && Stored->bStored);
	TestEqual(TEXT("불러온 뒤 꾸미기 점수 같음"), Reloaded.Estate->GetDecorationScore(), SavedScore);
	TestEqual(TEXT("시설·장식 수 같음"), Reloaded.Estate->GetState().Facilities.Num(), Saved.Facilities.Num());
	return true;
}
#endif
