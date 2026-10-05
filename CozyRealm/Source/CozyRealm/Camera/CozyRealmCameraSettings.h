#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Camera/CameraTypes.h"
#include "CozyRealmCameraSettings.generated.h"

/**
 *  Camera framing values. Kept as a struct so the pawn can fall back to these
 *  defaults when no settings asset is assigned.
 */
USTRUCT(BlueprintType)
struct FCozyRealmCameraConfig
{
	GENERATED_BODY()

	/** Perspective keeps a little depth; Orthographic is a true isometric look */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projection")
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode = ECameraProjectionMode::Perspective;

	/** Horizontal field of view in degrees (Perspective only). Narrow = flatter, more isometric */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projection", meta=(ClampMin="5.0", ClampMax="120.0", EditCondition="ProjectionMode==ECameraProjectionMode::Perspective"))
	float FieldOfView = 30.f;

	/** Visible world width in cm (Orthographic only) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projection", meta=(ClampMin="100.0", EditCondition="ProjectionMode==ECameraProjectionMode::Orthographic"))
	float OrthoWidth = 4000.f;

	/** World point the camera looks at (estate center) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing")
	FVector FocusLocation = FVector::ZeroVector;

	/** Look-down angle in degrees. -90 is straight down */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing", meta=(ClampMin="-89.0", ClampMax="-5.0"))
	float Pitch = -45.f;

	/** Direction the camera faces in degrees. 45 shows the estate as a diamond */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing")
	float Yaw = 45.f;

	/** Distance from the focus point in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing", meta=(ClampMin="100.0"))
	float Distance = 8500.f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCozyRealmCameraSettingsChanged, const class UCozyRealmCameraSettings*);

/**
 *  Data asset holding the estate camera setup.
 *  Edit it in the editor (also during Play In Editor) and every camera using it updates immediately.
 */
UCLASS(BlueprintType)
class UCozyRealmCameraSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ShowOnlyInnerProperties))
	FCozyRealmCameraConfig Config;

	/** Fired after any value is edited in the editor */
	FOnCozyRealmCameraSettingsChanged OnSettingsChanged;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
