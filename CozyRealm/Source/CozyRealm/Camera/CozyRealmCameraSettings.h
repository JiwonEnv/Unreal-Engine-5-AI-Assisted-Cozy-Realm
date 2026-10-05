#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Camera/CameraTypes.h"
#include "CozyRealmCameraSettings.generated.h"

/**
 *  카메라 구도 값.
 *  설정 에셋이 연결되지 않았을 때 카메라가 이 기본값을 쓸 수 있도록 구조체로 둔다.
 */
USTRUCT(BlueprintType)
struct FCozyRealmCameraConfig
{
	GENERATED_BODY()

	/**
	 *  카메라 투영 방식.
	 *  Perspective(원근): 멀수록 작아 보여서 깊이감이 있다.
	 *  Orthographic(직교): 거리와 상관없이 크기가 같아서 완전한 아이소메트릭처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projection")
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode = ECameraProjectionMode::Perspective;

	/**
	 *  가로 화각(도). 원근일 때만 쓴다.
	 *  작을수록 평평해서 아이소메트릭에 가깝고, 클수록 원근감이 강하다.
	 *  화각을 줄이면 Distance를 늘려야 같은 범위가 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projection", meta=(ClampMin="5.0", ClampMax="120.0", EditCondition="ProjectionMode==ECameraProjectionMode::Perspective"))
	float FieldOfView = 30.f;

	/**
	 *  화면에 보이는 가로 폭(cm). 직교일 때만 쓴다.
	 *  24m 영지가 다 들어오려면 4000 이상.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projection", meta=(ClampMin="100.0", EditCondition="ProjectionMode==ECameraProjectionMode::Orthographic"))
	float OrthoWidth = 4000.f;

	/** 카메라가 바라보는 지점(cm). 보통 영지 중앙인 (0, 0, 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing")
	FVector FocusLocation = FVector::ZeroVector;

	/**
	 *  내려다보는 각도(도).
	 *  -90이면 바로 위에서 내려다보고, -30이면 낮게 비스듬히 본다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing", meta=(ClampMin="-89.0", ClampMax="-5.0"))
	float Pitch = -45.f;

	/**
	 *  카메라가 바라보는 방향(도).
	 *  45면 영지가 마름모로, 0이면 네모 그대로 정면에서 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing")
	float Yaw = 45.f;

	/**
	 *  바라보는 지점에서 카메라까지 거리(cm). 8500 = 85m.
	 *  원근일 때 멀수록 화면에 보이는 범위가 넓어진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing", meta=(ClampMin="100.0"))
	float Distance = 8500.f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCozyRealmCameraSettingsChanged, const class UCozyRealmCameraSettings*);

/**
 *  영지 카메라 설정 에셋.
 *  에디터에서 값을 바꾸면(플레이 중에도) 이 에셋을 쓰는 카메라에 바로 반영된다.
 */
UCLASS(BlueprintType)
class UCozyRealmCameraSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** 카메라 구도 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ShowOnlyInnerProperties))
	FCozyRealmCameraConfig Config;

	/** 에디터에서 값을 고친 뒤 호출된다 */
	FOnCozyRealmCameraSettingsChanged OnSettingsChanged;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
