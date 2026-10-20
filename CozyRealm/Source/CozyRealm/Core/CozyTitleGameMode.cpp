#include "Core/CozyTitleGameMode.h"
#include "UI/CozyTitleWidget.h"
#include "UI/CozyTitleSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Estate/CozyEstateSubsystem.h"
#include "CozyRealm.h"

ACozyTitleGameMode::ACozyTitleGameMode()
{
	PlayerControllerClass = ACozyTitlePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

ACozyTitlePlayerController::ACozyTitlePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ACozyTitlePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	const UCozyTitleSettings* Config = UCozyTitleSettings::Get();
	StartTime = GetWorld()->GetTimeSeconds();
	// 타이틀 월드에서만 낮으로 보이게 (영지에 들어가면 게임 시계 그대로 · 저장하지 않음)
	if (Config->bForceDaytime)
		if (UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>()) Estate->SetNightOverride(ECozyNightOverride::ForceDay);

	// 배경: 실제 영지 맵을 풍경만 불러온다 (이 맵의 게임 모드가 영지를 시작하지 않으므로 시설·저장·정산 없음)
	if (!Config->BackgroundLevel.IsNull())
	{
		bool bOk = false;
		Background = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(GetWorld(), Config->BackgroundLevel, FVector::ZeroVector, FRotator::ZeroRotator, bOk);
		bBackgroundRequested = bOk && Background;
		UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 배경 맵 불러오기 %s: %s"), bBackgroundRequested ? TEXT("시작") : TEXT("실패"), *Config->BackgroundLevel.ToString());
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TitleCamera = GetWorld()->SpawnActor<ACameraActor>(Config->CameraLocation, Config->CameraRotation, Params);
	if (TitleCamera)
	{
		TitleCamera->GetCameraComponent()->SetFieldOfView(Config->CameraFOV);
		TitleCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		SetViewTarget(TitleCamera);
	}

	Title = CreateWidget<UCozyTitleWidget>(this, UCozyTitleWidget::StaticClass());
	if (Title)
	{
		Title->AddToViewport(0);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Title->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		Title->FocusRoot();
	}
}

void ACozyTitlePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const UCozyTitleSettings* Config = UCozyTitleSettings::Get();
	if (TitleCamera && Config->CameraDrift > 0.f)
	{
		// 아주 천천히 옆으로 미끄러지는 카메라 (약 40초 주기)
		const double T = GetWorld()->GetTimeSeconds() - StartTime;
		const FVector Side = FRotationMatrix(FRotator(0.f, Config->CameraRotation.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
		TitleCamera->SetActorLocation(Config->CameraLocation + Side * (Config->CameraDrift * FMath::Sin(T * 2.0 * PI / 40.0)));
	}
}

bool ACozyTitlePlayerController::IsBackgroundVisible() const
{
	return Background && Background->IsLevelVisible();
}
