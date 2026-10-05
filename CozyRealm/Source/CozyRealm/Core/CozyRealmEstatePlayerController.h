#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CozyRealmEstatePlayerController.generated.h"

/**
 *  Player controller for the estate.
 *  Mouse-driven: the cursor stays visible and click/hover events are enabled for selecting facilities.
 */
UCLASS()
class ACozyRealmEstatePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ACozyRealmEstatePlayerController();

protected:

	virtual void BeginPlay() override;
};
