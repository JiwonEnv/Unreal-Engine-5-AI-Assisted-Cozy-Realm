#include "Core/CozyRealmEstatePlayerController.h"
#include "Facilities/CozyFacilityActor.h"
#include "UI/CozyHudWidget.h"
#include "Estate/CozyEstateSubsystem.h"
#include "EngineUtils.h"

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
