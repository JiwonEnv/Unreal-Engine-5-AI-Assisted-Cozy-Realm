#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CozyRealmCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UCozyRealmCameraSettings;
struct FCozyRealmCameraConfig;

/**
 *  Fixed quarter-view camera that looks down on the estate.
 *  All framing values come from a UCozyRealmCameraSettings asset so they can be tuned without code.
 */
UCLASS()
class ACozyRealmCameraPawn : public APawn
{
	GENERATED_BODY()

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> Camera;

public:

	/** 사용할 카메라 설정 에셋 (DA_CameraSettings_...). 비워 두면 코드의 기본값을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
	TObjectPtr<UCozyRealmCameraSettings> Settings;

	ACozyRealmCameraPawn();

	/** 설정 에셋을 다시 읽어 카메라에 적용한다 */
	UFUNCTION(BlueprintCallable, Category="Camera")
	void ApplySettings();

	/** 마우스 휠 줌 (Steps > 0 가까이) · 최소·최대 거리 안에서만 */
	void Zoom(float Steps);
	/** 휠 버튼 드래그 회전 (마우스 이동 픽셀) */
	void RotateByPixels(float DeltaX);

	/** 따라갈 대상 (주인공) · 정하는 순간 그 자리로 바로 옮김 · nullptr이면 따라가지 않음 */
	void SetFollowTarget(AActor* Target);
	/** 지금 카메라가 바라보는 방향 (도 · WASD 방향 계산용) */
	float GetCurrentYaw() const { return CurrentYaw; }

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;

private:

	/** Updates boom and camera only; the actor location is left alone */
	void ApplyCameraConfig(const FCozyRealmCameraConfig& Config);
	void BindToSettings();
	void UnbindFromSettings();
	void HandleSettingsChanged(const UCozyRealmCameraSettings* ChangedSettings);

	/** 플레이 중 줌·회전 값 (설정 에셋은 바꾸지 않음) */
	float CurrentDistance = 8500.f;
	float CurrentYaw = 45.f;

	TWeakObjectPtr<AActor> FollowTarget;

	/** Settings asset the change delegate is currently bound to */
	TWeakObjectPtr<UCozyRealmCameraSettings> BoundSettings;
	FDelegateHandle SettingsChangedHandle;
};
