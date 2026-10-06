#include "Core/CozyRealmEstateGameMode.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "Camera/CozyRealmCameraPawn.h"
#include "Estate/CozyEstateSubsystem.h"
#include "EngineUtils.h"

ACozyRealmEstateGameMode::ACozyRealmEstateGameMode()
{
	DefaultPawnClass = ACozyRealmCameraPawn::StaticClass();
	PlayerControllerClass = ACozyRealmEstatePlayerController::StaticClass();
}

void ACozyRealmEstateGameMode::RestartPlayer(AController* NewPlayer)
{
	// Prefer the camera placed in the level, so its settings asset is the one in use
	if (NewPlayer && !NewPlayer->GetPawn())
	{
		for (TActorIterator<ACozyRealmCameraPawn> It(GetWorld()); It; ++It)
		{
			if (!It->GetController())
			{
				NewPlayer->Possess(*It);
				return;
			}
		}
	}

	Super::RestartPlayer(NewPlayer);
}

void ACozyRealmEstateGameMode::StartPlay()
{
	// 액터들의 BeginPlay보다 먼저 영지 상태를 만들어 둔다
	if (UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>())
	{
		Estate->StartEstate();
	}

	Super::StartPlay();
}
