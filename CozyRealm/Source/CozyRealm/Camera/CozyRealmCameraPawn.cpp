#include "Camera/CozyRealmCameraPawn.h"
#include "Camera/CozyRealmCameraSettings.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

ACozyRealmCameraPawn::ACozyRealmCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	// Pawns placed in a level get an AI controller by default, which would stop the player from possessing this camera
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// The boom only positions the camera; the camera never moves on its own
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Root);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	ApplyCameraConfig(FCozyRealmCameraConfig());
}

void ACozyRealmCameraPawn::ApplySettings()
{
	const FCozyRealmCameraConfig Config = Settings ? Settings->Config : FCozyRealmCameraConfig();

	// The pawn sits on the focus point so the boom rotates around it
	SetActorLocation(Config.FocusLocation);
	CurrentDistance = Config.Distance;
	CurrentYaw = Config.Yaw;
	ApplyCameraConfig(Config);
}

void ACozyRealmCameraPawn::Zoom(float Steps)
{
	const FCozyRealmCameraConfig Config = Settings ? Settings->Config : FCozyRealmCameraConfig();
	const float Factor = FMath::Pow(1.f - Config.ZoomStepRatio, Steps);
	CurrentDistance = FMath::Clamp(CurrentDistance * Factor, FMath::Min(Config.MinDistance, Config.MaxDistance), FMath::Max(Config.MinDistance, Config.MaxDistance));
	CameraBoom->TargetArmLength = CurrentDistance;
}

void ACozyRealmCameraPawn::RotateByPixels(float DeltaX)
{
	const FCozyRealmCameraConfig Config = Settings ? Settings->Config : FCozyRealmCameraConfig();
	CurrentYaw = FRotator::NormalizeAxis(CurrentYaw + DeltaX * Config.RotateDegreesPerPixel);
	CameraBoom->SetWorldRotation(FRotator(Config.Pitch, CurrentYaw, 0.f));
}

void ACozyRealmCameraPawn::ApplyCameraConfig(const FCozyRealmCameraConfig& Config)
{
	CameraBoom->SetWorldRotation(FRotator(Config.Pitch, Config.Yaw, 0.f));
	CameraBoom->TargetArmLength = Config.Distance;

	Camera->SetProjectionMode(Config.ProjectionMode);
	Camera->SetFieldOfView(Config.FieldOfView);
	Camera->SetOrthoWidth(Config.OrthoWidth);
}

void ACozyRealmCameraPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Keeps the editor viewport preview in sync when the asset is edited outside of play
	BindToSettings();
	ApplySettings();
}

void ACozyRealmCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	BindToSettings();
	ApplySettings();
}

void ACozyRealmCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromSettings();

	Super::EndPlay(EndPlayReason);
}

void ACozyRealmCameraPawn::Destroyed()
{
	UnbindFromSettings();

	Super::Destroyed();
}

void ACozyRealmCameraPawn::BindToSettings()
{
	if (BoundSettings.Get() == Settings)
	{
		return;
	}

	UnbindFromSettings();

	if (Settings)
	{
		SettingsChangedHandle = Settings->OnSettingsChanged.AddUObject(this, &ACozyRealmCameraPawn::HandleSettingsChanged);
		BoundSettings = Settings;
	}
}

void ACozyRealmCameraPawn::UnbindFromSettings()
{
	if (UCozyRealmCameraSettings* OldSettings = BoundSettings.Get())
	{
		OldSettings->OnSettingsChanged.Remove(SettingsChangedHandle);
	}

	BoundSettings.Reset();
	SettingsChangedHandle.Reset();
}

void ACozyRealmCameraPawn::HandleSettingsChanged(const UCozyRealmCameraSettings* ChangedSettings)
{
	ApplySettings();
}
