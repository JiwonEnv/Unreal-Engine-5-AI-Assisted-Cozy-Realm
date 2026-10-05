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
};
