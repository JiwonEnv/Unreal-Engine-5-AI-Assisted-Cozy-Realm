#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "CozyTitleGameMode.generated.h"

class UCozyTitleWidget;
class ACameraActor;
class ULevelStreamingDynamic;

/**
 *  타이틀 맵(L_Title) 게임 모드.
 *  영지 게임 모드와 달리 StartEstate를 부르지 않는다 → 타이틀에 들어가기만 해서는 저장 생성 · 방치 정산 · 생산이 일어나지 않는다.
 */
UCLASS()
class ACozyTitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACozyTitleGameMode();
};

/** 타이틀 화면을 띄우고 배경(실제 영지 풍경)과 카메라를 준비한다 */
UCLASS()
class ACozyTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACozyTitlePlayerController();

	/** 배경 맵이 화면에 보이기 시작했는지 (타이틀 화면이 종이 바탕을 걷어낼 때 씀) */
	bool IsBackgroundVisible() const;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCozyTitleWidget> Title;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> TitleCamera;

	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> Background;

	double StartTime = 0.0;
};
