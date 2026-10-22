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
						return false;
					}
				}
				return true;
			}), 1.f);
		});
	}
} GCozyBadgeTestOnStart;
#endif
