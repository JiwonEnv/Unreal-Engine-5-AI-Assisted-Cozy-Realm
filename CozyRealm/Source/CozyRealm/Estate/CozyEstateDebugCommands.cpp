#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Engine/World.h"
#include "CozyRealm.h"

#if !UE_BUILD_SHIPPING
// 시설 위 수령 배지·진행 막대 시험용 (2026-10-22). 저장 데이터가 바뀌므로 실행 인자 -CozyDebugCommands가 있을 때만 (격리 폴더 시험용)
namespace CozyDebug
{
	/** 주민을 밭·제분소에 배치 · 밀 30 · 밀가루 3회 가공 시작 · Seconds만큼 방치 시간 건너뛰기 */
	void RunBadgeTest(UWorld* World, double Seconds)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("CozyDebugCommands")))
		{
			UE_LOG(LogCozyRealm, Warning, TEXT("[디버그] 수령 배지 시험은 실행 인자 -CozyDebugCommands가 있을 때만 (저장 데이터가 바뀜)"));
			return;
		}
		UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
		if (!Estate || !Estate->IsStarted())
		{
			return;
		}
		auto FindFacility = [Estate](FName DefinitionId)
		{
			const FCozyFacilityState* Found = Estate->GetState().Facilities.FindByPredicate([DefinitionId](const FCozyFacilityState& F) { return F.DefinitionId == DefinitionId && !F.bStored; });
			return Found ? Found->InstanceId : FGuid();
		};
		const FGuid Field = FindFacility(TEXT("Field"));
		const FGuid Mill = FindFacility(TEXT("Mill"));
		TArray<FGuid> Free;
		for (const FCozyResidentState& Resident : Estate->GetState().Residents)
		{
			if (!Resident.AssignedFacility.IsValid())
			{
				Free.Add(Resident.InstanceId);
			}
		}
		FText Reason;
		const bool bField = Field.IsValid() && Free.Num() > 0 && Estate->AssignResident(Free[0], Field, Reason);
		const bool bMill = Mill.IsValid() && Free.Num() > 1 && Estate->AssignResident(Free[1], Mill, Reason);
		Estate->DebugSetResource(TEXT("Wheat"), 30);
		const bool bProcessing = Mill.IsValid() && Estate->StartProcessing(Mill, TEXT("Mill_Flour"), 3, Reason);
		if (Seconds > 0.0)
		{
			Estate->DebugSkipOffline(Seconds);
		}
		UE_LOG(LogCozyRealm, Log, TEXT("[디버그] 수령 배지 시험: 밭 배치 %d · 제분소 배치 %d · 가공 시작 %d · 방치 %.0f초 · %s"),
			bField ? 1 : 0, bMill ? 1 : 0, bProcessing ? 1 : 0, Seconds, *Reason.ToString());
	}
}

namespace CozyDebug
{
	/** 상태 칩 시험: 밭 주민 빼기(주민 없음) · 창고 밀가루 가득(제분소 창고 공간 부족) · 판매소 업그레이드 시작(업그레이드 중 · 재료를 채워 줌) */
	void RunStateTest(UWorld* World)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("CozyDebugCommands")))
		{
			return;
		}
		UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
		if (!Estate || !Estate->IsStarted())
		{
			return;
		}
		FText Reason;
		bool bUnassigned = false;
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			if (Facility.DefinitionId == TEXT("Field") && Facility.AssignedResidents.Num() > 0)
			{
				bUnassigned = Estate->UnassignResident(Facility.AssignedResidents[0], Reason);
				break;
			}
		}
		Estate->DebugSetResource(TEXT("Flour"), Estate->GetStorageCap());
		for (const TCHAR* Item : { TEXT("Gold"), TEXT("Wheat"), TEXT("Flour"), TEXT("TimeTalisman"), TEXT("MindShard") })
		{
			Estate->DebugAddResource(Item, 50);
		}
		bool bUpgrade = false;
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			if (Facility.DefinitionId == TEXT("Shop"))
			{
				bUpgrade = Estate->StartUpgrade(Facility.InstanceId, Reason);
				break;
			}
		}
		Estate->DebugSetResource(TEXT("Flour"), Estate->GetStorageCap());
		// -CozyStatePause: 제분소 주민도 빼서 가공 일시 정지 (창고 공간 부족 대신 일시 정지 표시를 보려고 · 밀가루는 비움)
		bool bMillPaused = false;
		if (FParse::Param(FCommandLine::Get(), TEXT("CozyStatePause")))
		{
			Estate->DebugSetResource(TEXT("Flour"), 0);
			for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
			{
				if (Facility.DefinitionId == TEXT("Mill") && Facility.AssignedResidents.Num() > 0)
				{
					bMillPaused = Estate->UnassignResident(Facility.AssignedResidents[0], Reason);
					break;
				}
			}
		}
		UE_LOG(LogCozyRealm, Log, TEXT("[디버그] 상태 칩 시험: 밭 주민 빼기 %d · 판매소 업그레이드 %d · 제분소 주민 빼기 %d · %s"), bUnassigned ? 1 : 0, bUpgrade ? 1 : 0, bMillPaused ? 1 : 0, *Reason.ToString());
	}
}

// 첫 실행 안내 시험: 밭 수령 → (가공은 수령 배지 시험에서 시작됨) → 판매소에서 밀 1개 판매 · 안내 단계가 넘어가는지 로그로 확인
static FAutoConsoleCommandWithWorld CozyTutorialWalk(
	TEXT("Cozy.TutorialWalk"), TEXT("안내 시험: 밭·제분소 수령 · 밀가루 1개 판매 (-CozyDebugCommands 필요)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
		if (!FParse::Param(FCommandLine::Get(), TEXT("CozyDebugCommands")) || !Estate || !Estate->IsStarted())
		{
			return;
		}
		FText Message;
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			if (Facility.DefinitionId == TEXT("Field") || Facility.DefinitionId == TEXT("Mill"))
			{
				Estate->CollectUnclaimed(Facility.InstanceId);
			}
		}
		// 제분소 가공이 없으면 1회 시작 (3단계 '밀가루 만들기' 확인)
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			if (Facility.DefinitionId == TEXT("Mill") && Estate->GetProcessingJobs(Facility.InstanceId).Num() == 0)
			{
				Estate->DebugAddResource(TEXT("Wheat"), 3);
				Estate->StartProcessing(Facility.InstanceId, TEXT("Mill_Flour"), 1, Message);
				break;
			}
		}
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			if (Facility.DefinitionId == TEXT("Shop"))
			{
				Estate->SellItem(Facility.InstanceId, TEXT("Flour"), 1, Message);
				break;
			}
		}
		UE_LOG(LogCozyRealm, Log, TEXT("[디버그] 안내 시험: 밭 수령 · 판매 %s"), *Message.ToString());
	}));

// 꾸미기 점수 보기 (읽기만 하므로 실행 인자 없이 사용 가능 · 4주차 기능 2)
static FAutoConsoleCommandWithWorld CozyDecorScore(
	TEXT("Cozy.DecorScore"), TEXT("꾸미기 점수 · 생산 보너스 · 다음 단계까지 · 장식 목록을 로그([꾸미기])로"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr)
		{
			Estate->DebugDescribeDecorations();
		}
	}));

static FAutoConsoleCommandWithWorld CozyStateTest(
	TEXT("Cozy.StateTest"), TEXT("상태 칩 시험: 밭 주민 빼기 · 창고 밀가루 가득 · 판매소 업그레이드 시작 (-CozyDebugCommands 필요)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { CozyDebug::RunStateTest(World); }));

static FAutoConsoleCommandWithWorldAndArgs CozyBadgeTest(
	TEXT("Cozy.BadgeTest"), TEXT("수령 배지 시험: 주민을 밭·제분소에 배치 · 밀 30 · 밀가루 3회 가공 시작 · [초]만큼 방치 시간 건너뛰기 (기본 120 · -CozyDebugCommands 필요)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		CozyDebug::RunBadgeTest(World, Args.Num() > 0 ? FCString::Atod(*Args[0]) : 120.0);
	}));

// 실행 인자 -CozyBadgeTest=<초>: 영지가 시작되면 한 번 자동 실행 (키 입력 없이 시험 · 콘솔 입력이 어려운 자동 확인용)
static struct FCozyBadgeTestOnStart
{
	FCozyBadgeTestOnStart()
	{
		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			double Seconds = 0.0;
			if (!FParse::Value(FCommandLine::Get(), TEXT("CozyBadgeTest="), Seconds))
			{
				return;
			}
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Seconds](float)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* World = Context.World();
					const UCozyEstateSubsystem* Estate = World && World->IsGameWorld() ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
					if (Estate && Estate->IsStarted())
					{
						CozyDebug::RunBadgeTest(World, Seconds);
						if (FParse::Param(FCommandLine::Get(), TEXT("CozyStateTest")))
						{
							CozyDebug::RunStateTest(World);
						}
						// -CozyTutorialWalk: 안내가 2단계로 넘어간 뒤(약 3초) 밭 수령, 다시 3초 뒤 판매
						if (FParse::Param(FCommandLine::Get(), TEXT("CozyTutorialWalk")))
						{
							for (const float Delay : { 3.f, 6.f, 9.f })
							{
								FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([World](float)
								{
									GEngine->Exec(World, TEXT("Cozy.TutorialWalk"));
									return false;
								}), Delay);
							}
						}
						return false;
					}
				}
				return true;
			}), 1.f);
		});
	}
} GCozyBadgeTestOnStart;
#endif
