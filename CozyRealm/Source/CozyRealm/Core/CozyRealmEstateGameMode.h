#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CozyRealmEstateGameMode.generated.h"

/**
 *  Game mode for the estate level.
 *  The player views the estate through the camera pawn placed in the level;
 *  if none is placed, a default camera pawn is spawned instead.
 */
UCLASS()
class ACozyRealmEstateGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ACozyRealmEstateGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;

	/** 영지 데이터를 읽고 시작 시설·주민·재화를 만든 뒤 플레이를 시작한다 */
	virtual void StartPlay() override;
};
