#include "Core/CozyRealmEstatePlayerController.h"
#include "Environment/CozySurfaceComponent.h"
#include "Facilities/CozyFacilityActor.h"
#include "UI/CozyHudWidget.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Settings/CozySettingsSubsystem.h"
#include "EngineUtils.h"
#include "Camera/CozyRealmCameraPawn.h"
#include "Hero/CozyHeroCharacter.h"
#include "Hero/CozyHeroSettings.h"
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

	// 저장된 음량(설정 창 · GameUserSettings.ini)을 이 레벨에 적용
	if (IsLocalController())
	{
		if (UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this))
		{
			Settings->ApplySavedAudio(this);
		}
		SpawnHero();
	}
}

void ACozyRealmEstatePlayerController::SpawnHero()
{
	UCozyHeroSettings* HeroSettings = UCozyHeroProjectSettings::Get()->HeroSettings.LoadSynchronous();
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	if (!HeroSettings || !Estate)
	{
		UE_LOG(LogCozyRealm, Log, TEXT("[주인공] 프로젝트 설정 Cozy Hero에 설정 에셋이 없어 주인공을 만들지 않음"));
		return;
	}
	// 영지 가운데 칸 (시설이 덮고 있으면 주인공이 가장 가까운 빈 칸으로 스스로 옮겨 감)
	const FIntPoint Center(Estate->GetGridSize().X / 2, Estate->GetGridSize().Y / 2);
	const FVector Location = Estate->GetCellCenter(Center) + FVector(0.f, 0.f, HeroSettings->Config.Height * 0.5f + 5.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Hero = GetWorld()->SpawnActor<ACozyHeroCharacter>(Location, FRotator(0.f, 225.f, 0.f), Params);
	if (!Hero)
	{
		return;
	}
	Hero->SetSettings(HeroSettings);
	if (ACozyRealmCameraPawn* CameraPawn = Cast<ACozyRealmCameraPawn>(GetPawn()))
	{
		CameraPawn->SetFollowTarget(Hero);
	}
}

ACozyFacilityActor* ACozyRealmEstatePlayerController::FindFacilityActor(const FGuid& FacilityId) const
{
	for (TActorIterator<ACozyFacilityActor> It(GetWorld()); FacilityId.IsValid() && It; ++It)
	{
		if (It->GetFacilityId() == FacilityId)
		{
			return *It;
		}
	}
	return nullptr;
}

void ACozyRealmEstatePlayerController::TickHero(bool bCompact)
{
	if (!Hero)
	{
		return;
	}
	// 조작이 있으면 산책 대기 시간을 처음부터 (키·마우스 버튼·휠)
	static const FKey ActivityKeys[] = { EKeys::LeftMouseButton, EKeys::RightMouseButton, EKeys::MiddleMouseButton, EKeys::MouseScrollUp, EKeys::MouseScrollDown,
		EKeys::E, EKeys::SpaceBar, EKeys::Tab, EKeys::I, EKeys::B, EKeys::M, EKeys::F1, EKeys::Escape };
	for (const FKey& Key : ActivityKeys)
	{
		if (WasInputKeyJustPressed(Key))
		{
			Hero->NotifyPlayerInput();
			break;
		}
	}

	const bool bWindowOpen = Hud && Hud->IsWindowOpen();
	const bool bCanMove = !bPlacementMode && !bWindowOpen;

	// WASD: 카메라가 바라보는 방향 기준 (W = 화면 위쪽)
	FVector2D Input(0.f, 0.f);
	if (bCanMove)
	{
		Input.X = (IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
		Input.Y = (IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f);
	}
	const ACozyRealmCameraPawn* CameraPawn = Cast<ACozyRealmCameraPawn>(GetPawn());
	const FRotator Yaw(0.f, CameraPawn ? CameraPawn->GetCurrentYaw() : 45.f, 0.f);
	const FVector Direction = Yaw.RotateVector(FVector::ForwardVector) * Input.X + Yaw.RotateVector(FVector::RightVector) * Input.Y;
	Hero->SetKeyboardInput(Direction.GetSafeNormal(), IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift));

	// 오른쪽 클릭: 시설이면 이동하지 않고 메뉴 · 땅이면 그 자리로 걸어감 (🙋 2026-10-18)
	if (bCanMove && !bCompact && WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			if (ACozyFacilityActor* Facility = Cast<ACozyFacilityActor>(Hit.GetActor()))
			{
				UE_LOG(LogCozyRealm, Log, TEXT("[주인공] 오른쪽 클릭 시설 → 메뉴 (이동 안 함): %s"), *Facility->GetName());
				SelectFacility(Facility);
				return;
			}
		}
		const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
		FVector RayOrigin;
		FVector RayDirection;
		if (Estate && DeprojectMousePositionToWorld(RayOrigin, RayDirection) && !FMath::IsNearlyZero(RayDirection.Z))
		{
			FCozySurfaceHit Surface;
			if (!CozySurface::Trace(GetWorld(), RayOrigin, RayOrigin + RayDirection * 100000.f, Surface)) return;
			const FVector Ground = Surface.Position;
			const bool bMoving = Hero->MoveToLocation(Ground);
			UE_LOG(LogCozyRealm, Log, TEXT("[주인공] 오른쪽 클릭 땅 → 이동 %s (%.0f, %.0f)"), bMoving ? TEXT("시작") : TEXT("불가"), Ground.X, Ground.Y);
		}
	}

	// E: 상호작용 거리 안의 가장 가까운 시설 메뉴
	if (WasInputKeyJustPressed(EKeys::E) && (bPlacementMode || bWindowOpen || bCompact))
	{
		UE_LOG(LogCozyRealm, Log, TEXT("[주인공] E 무시: %s"), bPlacementMode ? TEXT("배치 모드") : bWindowOpen ? TEXT("창이 열려 있음") : TEXT("작은 창 모드"));
	}
	else if (WasInputKeyJustPressed(EKeys::E))
	{
		if (ACozyFacilityActor* Facility = FindFacilityActor(Hero->FindInteractFacility()))
		{
			UE_LOG(LogCozyRealm, Log, TEXT("[주인공] E 상호작용 → %s"), *Facility->GetName());
			SelectFacility(Facility);
		}
		else if (Hud)
		{
			UE_LOG(LogCozyRealm, Log, TEXT("[주인공] E: 상호작용 거리 안에 시설 없음"));
			Hud->ShowToast(NSLOCTEXT("CozyRealm", "HeroNoFacility", "가까운 시설이 없습니다 (시설 옆으로 가서 E)"));
		}
	}
}

void ACozyRealmEstatePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>())
	{
		const FString& Notice = Estate->GetRecoveryNotice();
		if (Hud && !Notice.IsEmpty() && Notice != LastRecoveryNotice) { Hud->ShowToast(FText::FromString(Notice)); LastRecoveryNotice = Notice; }
	}

	// 작은 창 모드: 영지를 보며 수확만 (시설 메뉴 · 창 · 배치는 크게 보기 후)
	const bool bCompact = Hud && Hud->IsCompactMode();
	if (!bCompact && WasInputKeyJustPressed(EKeys::G)) CozyGroundMode((GroundMode + 1) % 3);
	if (GroundMode != 0)
	{
		if (bCompact || WasInputKeyJustPressed(EKeys::Escape)) CozyGroundMode(0);
		else { TickGroundEditing(); return; }
	}
	if (!bCompact && WasInputKeyJustPressed(EKeys::B))
	{
		SetPlacementMode(!bPlacementMode);
	}
	// UI 위젯이 먼저 클릭을 받으면 여기까지 오지 않음 (GameAndUI 입력 모드)
	if (bPlacementMode)
	{
		TickPlacement();
	}
	else if (!bCompact && WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		HandleWorldClick();
	}
	TickHero(bCompact);
	if (WasInputKeyJustPressed(EKeys::F1) && Hud && !Hud->IsCompactMode())
	{
		Hud->ToggleDebugPanel();
	}
	// 작은 창 모드 (M · 화면 구석의 작은 창 ↔ 원래 크기)
	if (Hud && WasInputKeyJustPressed(EKeys::M))
	{
		if (UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this))
		{
			Settings->Run(ECozyUiAction::ToggleCompactWindow, NAME_None, this);
		}
	}
	// 단축키 (기획서 4 조작 · 3주차 기능 3) · 창 안의 글자 입력은 아직 없어 그대로 받음
	if (Hud && WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		if (UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>())
		{
			Hud->ShowToast(Estate->CollectAllProduction());
		}
	}
	if (Hud && !bCompact && WasInputKeyJustPressed(EKeys::Tab))
	{
		ClearSelection();
		Hud->ToggleNagayaWindow();
	}
	if (Hud && !bCompact && WasInputKeyJustPressed(EKeys::I))
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
		// 설정 창: 화면 변경 확인 · 종료 확인이 떠 있으면 그 확인부터 취소 (화면 변경은 이전 설정으로) · 없으면 창 닫기
		UCozySettingsSubsystem* Settings = Hud->IsSettingsOpen() ? UCozySettingsSubsystem::Get(this) : nullptr;
		const bool bCanceledDialog = Settings && Settings->CancelDialog();
		if (!bCanceledDialog)
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
	if (Hero)
	{
		// 배치 중에는 주인공을 세워 둠 (산책도 안 함) · 끝나면 시설에 덮였을 때 빈 칸으로 옮겨 감
		Hero->SetFrozen(bEnable);
	}
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
		if (Estate->IsPlacing() && (!Clicked || Clicked->GetFacilityId() == Estate->GetPlacementId()))
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
		// 실제 지면을 가리킨 칸 → 시설 가운데가 그 칸에 오게
		FVector RayOrigin;
		FVector RayDirection;
		if (DeprojectMousePositionToWorld(RayOrigin, RayDirection) && !FMath::IsNearlyZero(RayDirection.Z))
		{
			FCozySurfaceHit Surface;
			if (!CozySurface::Trace(GetWorld(), RayOrigin, RayOrigin + RayDirection * 100000.f, Surface)) return;
			const FIntPoint Cell = Estate->WorldToCell(Surface.Position);
			const FIntPoint Size = Estate->GetFootprint(Estate->GetPlacementId(), Estate->GetPlacementRotation());
			const FIntPoint Coord(Cell.X - (Size.X - 1) / 2, Cell.Y - (Size.Y - 1) / 2);
			if (Coord != Estate->GetPlacementCoord() || Surface.SurfaceId != Estate->GetPlacementSurface())
			{
				Estate->SetPlacementSurface(Surface.SurfaceId);
				Estate->UpdatePlacement(Coord, Estate->GetPlacementRotation());
			}
		}
	}
}

void ACozyRealmEstatePlayerController::CozySetResource(FName ItemId, int32 Amount)
{
	if (UCozyEstateSubsystem* Estate = GetWorld() ? GetWorld()->GetSubsystem<UCozyEstateSubsystem>() : nullptr)
	{
		const int32 Before = Estate->GetAmount(ItemId);
		Estate->DebugSetResource(ItemId, Amount);
		UE_LOG(LogCozyRealm, Log, TEXT("[개발] 수량 맞춤: %s %d → %d"), *ItemId.ToString(), Before, Estate->GetAmount(ItemId));
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
