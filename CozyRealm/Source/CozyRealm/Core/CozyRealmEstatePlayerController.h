#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CozyRealmEstatePlayerController.generated.h"

class UCozyHudWidget;
class ACozyFacilityActor;

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

	/** 지금 선택된 시설 (없으면 nullptr) */
	ACozyFacilityActor* GetSelectedFacility() const { return SelectedFacility.Get(); }

protected:

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** 영지 화면 UI 클래스 (재화 표시 · 기능 아이콘 · 전용 창 · 디버그 메뉴) */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCozyHudWidget> HudClass;

private:

	/** 왼쪽 클릭: 시설이면 선택하고 기능 아이콘을, 빈 곳이면 선택 해제 */
	void HandleWorldClick();
	void SelectFacility(ACozyFacilityActor* Facility);
	void ClearSelection();

	UPROPERTY(Transient)
	TObjectPtr<UCozyHudWidget> Hud;

	TWeakObjectPtr<ACozyFacilityActor> SelectedFacility;
};
