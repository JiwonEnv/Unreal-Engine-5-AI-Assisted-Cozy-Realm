#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "CozyWorldExplorer.generated.h"

UCLASS()
class COZYREALM_API ACozyWorldExplorer : public ACharacter
{
	GENERATED_BODY()
public:
	ACozyWorldExplorer();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	void Visit(int32 Index);
	void ToggleOverview();
	/** 검토 모드에서만 실제 이동 컴포넌트로 3초 보행을 시연한다. */
	UFUNCTION(Exec) void WorldWalkCheck();
	UPROPERTY(VisibleAnywhere) TObjectPtr<class USpringArmComponent> Arm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UCameraComponent> Camera;
private:
	bool bOverview=false;
	FVector ReturnPosition;
	FVector WalkCheckStart;
	float WalkCheckRemaining=0;
	void Forward(float V); void Back(float V); void Right(float V); void Left(float V);
	void Yaw(float V); void Pitch(float V); void Zoom(float V);
	void Zone1(); void Zone2(); void Zone3(); void Zone4(); void Zone5();
};

/** 영지 서브시스템을 시작하지 않으므로 실제 저장 파일을 읽거나 쓰지 않는 검토 모드. */
UCLASS()
class COZYREALM_API ACozyWorldExplorerMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ACozyWorldExplorerMode();
};

UCLASS()
class COZYREALM_API UCozyWorldGuide : public UUserWidget
{
	GENERATED_BODY()
};

UCLASS()
class COZYREALM_API ACozyWorldExplorerController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
private:
	UPROPERTY() TObjectPtr<class UUserWidget> Guide;
};
