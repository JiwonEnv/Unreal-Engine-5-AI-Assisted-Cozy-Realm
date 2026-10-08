#include "Core/CozyRealmEstatePlayerController.h"
#include "Facilities/CozyFacilityActor.h"
#include "UI/CozyHudWidget.h"
#include "Estate/CozyEstateSubsystem.h"
#include "EngineUtils.h"
#include "Camera/CozyRealmCameraPawn.h"
#include "CozyRealm.h"

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

	if (WasInputKeyJustPressed(EKeys::B))
	{
		SetPlacementMode(!bPlacementMode);
	}
	// UI 위젯이 먼저 클릭을 받으면 여기까지 오지 않음 (GameAndUI 입력 모드)
	if (bPlacementMode)
	{
		TickPlacement();
	}
	else if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
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

	if (WasInputKeyJustPressed(EKeys::Escape) && Hud && bPlacementMode)
	{
		// 배치 중이면 미리보기 취소, 아니면 배치 모드 끝내기
		UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
		if (Estate && Estate->IsPlacing())
		{
			Estate->CancelPlacement();
		}
		else
		{
			SetPlacementMode(false);
		}
	}
	else if (WasInputKeyJustPressed(EKeys::Escape) && Hud)
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
		UE_LOG(LogCozyRealm, Log, TEXT("[입력] 월드 클릭 무시: 창이 열려 있음"));
		return;
	}

	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		if (ACozyFacilityActor* Facility = Cast<ACozyFacilityActor>(Hit.GetActor()))
		{
			UE_LOG(LogCozyRealm, Log, TEXT("[입력] 시설 클릭: %s"), *Facility->GetName());
			SelectFacility(Facility);
			return;
		}
	}
	UE_LOG(LogCozyRealm, Log, TEXT("[입력] 빈 곳 클릭 → 선택 해제"));
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

void ACozyRealmEstatePlayerController::SetPlacementMode(bool bEnable)
{
	UCozyEstateSubsystem* Estate = GetWorld() ? GetWorld()->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	if (!bEnable && Estate && Estate->IsPlacing())
	{
		Estate->CancelPlacement();
	}
	bPlacementMode = bEnable;
	bDraggingPlacement = false;
	ClearSelection();
	if (Hud)
	{
		Hud->CloseWindow();
		Hud->SetPlacementMode(bEnable);
	}
}

void ACozyRealmEstatePlayerController::TickPlacement()
{
	UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	if (!Estate || (Hud && Hud->IsWindowOpen()))
	{
		return;
	}
	if (WasInputKeyJustPressed(EKeys::R) && Estate->IsPlacing())
	{
		Estate->RotatePlacement();
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		FHitResult Hit;
		ACozyFacilityActor* Clicked = GetHitResultUnderCursor(ECC_Visibility, false, Hit) ? Cast<ACozyFacilityActor>(Hit.GetActor()) : nullptr;
		if (Clicked && Estate->IsPlacing() && Clicked == Estate->GetPlacementActor())
		{
			bDraggingPlacement = true;
		}
		else if (Clicked && !Estate->IsPlacing())
		{
			bDraggingPlacement = Estate->BeginPlacement(Clicked->GetFacilityId());
		}
		else if (Clicked && Hud)
		{
			Hud->ShowToast(NSLOCTEXT("CozyRealm", "PlaceFinishFirst", "먼저 지금 옮기는 시설을 확정하거나 취소해 주세요"));
		}
	}
	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		bDraggingPlacement = false;
	}
	if (bDraggingPlacement && Estate->IsPlacing())
	{
		// 커서 광선이 영지 바닥 평면과 만나는 칸 → 시설 가운데가 그 칸에 오게
		FVector RayOrigin;
		FVector RayDirection;
		if (DeprojectMousePositionToWorld(RayOrigin, RayDirection) && !FMath::IsNearlyZero(RayDirection.Z))
		{
			const float T = (Estate->GetGroundZ() - RayOrigin.Z) / RayDirection.Z;
			const FIntPoint Cell = Estate->WorldToCell(RayOrigin + RayDirection * T);
			const FIntPoint Size = Estate->GetFootprint(Estate->GetPlacementId(), Estate->GetPlacementRotation());
			const FIntPoint Coord(Cell.X - (Size.X - 1) / 2, Cell.Y - (Size.Y - 1) / 2);
			if (Coord != Estate->GetPlacementCoord())
			{
				Estate->UpdatePlacement(Coord, Estate->GetPlacementRotation());
			}
		}
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
