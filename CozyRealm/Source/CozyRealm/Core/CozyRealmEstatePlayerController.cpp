#include "Core/CozyRealmEstatePlayerController.h"
#include "Facilities/CozyFacilityActor.h"
#include "UI/CozyHudWidget.h"
#include "Estate/CozyEstateSubsystem.h"
#include "EngineUtils.h"
#include "Camera/CozyRealmCameraPawn.h"

ACozyRealmEstatePlayerController::ACozyRealmEstatePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	HudClass = UCozyHudWidget::StaticClass();
}

void ACozyRealmEstatePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Keep the cursor free so the player can click both the world and UI
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	if (IsLocalController() && HudClass)
	{
		Hud = CreateWidget<UCozyHudWidget>(this, HudClass);
		if (Hud)
		{
			Hud->AddToViewport();
		}
	}
}

void ACozyRealmEstatePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// UI 위젯이 먼저 클릭을 받으면 여기까지 오지 않음 (GameAndUI 입력 모드)
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		HandleWorldClick();
	}
	if (WasInputKeyJustPressed(EKeys::F1) && Hud)
	{
		Hud->ToggleDebugPanel();
	}
	// 단축키 (기획서 4 조작 · 3주차 기능 3) · 창 안의 글자 입력은 아직 없어 그대로 받음
	if (Hud && WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		if (UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>())
		{
			Hud->ShowToast(Estate->CollectAllProduction());
		}
	}
	if (Hud && WasInputKeyJustPressed(EKeys::Tab))
	{
		ClearSelection();
		Hud->ToggleNagayaWindow();
	}
	if (Hud && WasInputKeyJustPressed(EKeys::I))
	{
		ClearSelection();
		Hud->ToggleStorageWindow();
	}

	// 카메라: 휠 줌 · 휠 버튼 드래그 회전 (창이 열려 있으면 창 스크롤이 우선)
	if (ACozyRealmCameraPawn* CameraPawn = Cast<ACozyRealmCameraPawn>(GetPawn()))
	{
		if (!Hud || !Hud->IsWindowOpen())
		{
			if (WasInputKeyJustPressed(EKeys::MouseScrollUp))
			{
				CameraPawn->Zoom(1.f);
			}
			if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
			{
				CameraPawn->Zoom(-1.f);
			}
		}
		if (IsInputKeyDown(EKeys::MiddleMouseButton))
		{
			float DeltaX = 0.f;
			float DeltaY = 0.f;
			GetInputMouseDelta(DeltaX, DeltaY);
			if (!FMath::IsNearlyZero(DeltaX))
			{
				// 마우스 델타는 축 감도가 곱해진 값이라 픽셀로 되돌림 (DefaultInput MouseX 0.07)
				CameraPawn->RotateByPixels(DeltaX / 0.07f);
			}
		}
	}

	if (WasInputKeyJustPressed(EKeys::Escape) && Hud)
	{
		if (Hud->IsWindowOpen())
		{
			Hud->CloseWindow();
		}
		else
		{
			ClearSelection();
		}
	}
}

void ACozyRealmEstatePlayerController::HandleWorldClick()
{
	if (Hud && Hud->IsWindowOpen())
	{
		return;
	}

	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		if (ACozyFacilityActor* Facility = Cast<ACozyFacilityActor>(Hit.GetActor()))
		{
			SelectFacility(Facility);
			return;
		}
	}
	// 빈 곳 클릭 → 닫기
	ClearSelection();
}

void ACozyRealmEstatePlayerController::SelectFacility(ACozyFacilityActor* Facility)
{
	if (SelectedFacility.Get() == Facility)
	{
		return;
	}
	ClearSelection();
	SelectedFacility = Facility;
	Facility->SetSelected(true);
	if (Hud)
	{
		Hud->ShowFacilityIcons(Facility);
	}
}

void ACozyRealmEstatePlayerController::SelectFacilityByDefinition(FName DefinitionId)
{
	const UCozyEstateSubsystem* Estate = GetWorld() ? GetWorld()->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	if (!Estate)
	{
		return;
	}
	ACozyFacilityActor* Best = nullptr;
	int32 BestLevel = -1;
	for (TActorIterator<ACozyFacilityActor> It(GetWorld()); It; ++It)
	{
		const FCozyFacilityState* State = Estate->FindFacility(It->GetFacilityId());
		if (State && State->DefinitionId == DefinitionId && State->Level > BestLevel)
		{
			Best = *It;
			BestLevel = State->Level;
		}
	}
	if (Best)
	{
		SelectFacility(Best);
	}
}

void ACozyRealmEstatePlayerController::ClearSelection()
{
	if (ACozyFacilityActor* Previous = SelectedFacility.Get())
	{
		Previous->SetSelected(false);
	}
	SelectedFacility.Reset();
	if (Hud)
	{
		Hud->HideFacilityIcons();
	}
}
