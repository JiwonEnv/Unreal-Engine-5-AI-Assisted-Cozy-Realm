#include "UI/CozyHudWidget.h"
#include "CozyRealm.h"
#include "Estate/CozyEstateSubsystem.h"
#include "UI/Kit/CozyUiScreen.h"
#include "UI/Kit/CozyUiTheme.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "Facilities/CozyFacilityActor.h"
#include "Environment/CozyEstateTerrain.h"
#include "Environment/CozySeasonPreset.h"
#include "Settings/CozySettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "CozyHud"

namespace CozyHud
{
	/** 위쪽 HUD 줄(칩 · 시계 · 창고·주민·배치·메뉴 버튼)이 차지하는 높이 (화면 단위) · 창 덮개 위 여백 · 배치 패널 위치와 같은 값 */
	constexpr float HudTopSafe = 100.f;
	/** 시설 메뉴를 화면 가장자리에서 띄우는 거리 (화면 단위) */
	constexpr float MenuEdgeMargin = 12.f;
	/** 고칠 프로젝트 설정 항목 (WarnMissingScreen 안내 글) */
	const TCHAR* const HudScreenSetting = TEXT("HUD Screen (위쪽 HUD 화면)");
	const TCHAR* const WindowScreensSetting = TEXT("Window Screens (창별 화면)");
	const FLinearColor PanelColor(0.05f, 0.05f, 0.08f, 0.75f);
	const FLinearColor OverlayColor(0.f, 0.f, 0.f, 0.7f);
	const FLinearColor MutedText(0.7f, 0.7f, 0.75f);
	const FLinearColor AccentText(1.f, 0.85f, 0.45f);

	FText TimeScaleText(float Scale)
	{
		return FText::Format(LOCTEXT("TimeScale", "×{0}"), FText::AsNumber(FMath::RoundToInt(Scale)));
	}

	/** 레벨의 첫 '영지 지형' 액터 (없으면 nullptr) */
	ACozyEstateTerrain* FindTerrain(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		TActorIterator<ACozyEstateTerrain> It(World);
		return It ? *It : nullptr;
	}
}

// ---------------------------------------------------------------------------
// 생성

TSharedRef<SWidget> UCozyHudWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UCozyHudWidget::BuildLayout()
{
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	// 편집 가능한 HUD 화면 (Widget Blueprint) · 프로젝트 설정 'Cozy UI' → HUD Screen
	if (UClass* ScreenClass = UCozyUiSettings::Get()->HudScreenClass.LoadSynchronous())
	{
		HudScreen = CreateWidget<UCozyUiScreen>(this, ScreenClass);
		if (HudScreen)
		{
			if (UCanvasPanelSlot* ScreenSlot = RootCanvas->AddChildToCanvas(HudScreen))
			{
				ScreenSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
				ScreenSlot->SetOffsets(FMargin(0.f));
				// 창 덮개(0)보다 위 · 창이 열려 있어도 HUD 버튼(창고·주민·배치·메뉴)이 눌린다
				ScreenSlot->SetZOrder(5);
			}
			HudScreen->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	// 시설 이름표 (3D 글자 기본 글꼴에 한글이 없어 UI 글자로 그림)
	LabelLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LabelLayer"));
	LabelLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* LabelSlot = RootCanvas->AddChildToCanvas(LabelLayer))
	{
		LabelSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		LabelSlot->SetOffsets(FMargin(0.f));
	}

	// 선택한 시설 위 시설 메뉴 화면 (FacilityMenu)
	IconBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("IconBox"));
	IconSlot = RootCanvas->AddChildToCanvas(IconBox);
	if (IconSlot)
	{
		IconSlot->SetZOrder(6); // 새 HUD(5)보다 위
		IconSlot->SetAutoSize(true);
		IconSlot->SetAlignment(FVector2D(0.5f, 1.f));
	}
	IconBox->SetVisibility(ESlateVisibility::Collapsed);

	// 배치 모드 패널 (왼쪽 위 · Placement 화면을 담음) · 미리보기 옆 버튼 상자 (PlacementActions 화면을 담음)
	PlacementPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlacementPanel"));
	PlacementPanel->SetBrushColor(FLinearColor::Transparent);
	PlacementPanel->SetPadding(FMargin(0.f));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PlacementPanel))
	{
		PanelSlot->SetZOrder(6);
		// 새 HUD의 칩 줄(위쪽 약 100) 아래
		PanelSlot->SetPosition(FVector2D(24.f, 110.f));
		PanelSlot->SetAutoSize(true);
	}
	PlacementPanel->SetVisibility(ESlateVisibility::Collapsed);

	PlacementActionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlacementActionBox"));
	PlacementActionSlot = RootCanvas->AddChildToCanvas(PlacementActionBox);
	if (PlacementActionSlot)
	{
		PlacementActionSlot->SetAutoSize(true);
		PlacementActionSlot->SetAlignment(FVector2D(0.5f, 1.f));
	}
	PlacementActionBox->SetVisibility(ESlateVisibility::Collapsed);

	// 단축키 결과 알림 (위쪽 가운데 · 잠깐 보였다 사라짐)
	ToastText = MakeText(FText::GetEmpty(), 17, CozyHud::AccentText);
	if (UCanvasPanelSlot* ToastSlot = RootCanvas->AddChildToCanvas(ToastText))
	{
		ToastSlot->SetZOrder(10);
		ToastSlot->SetAnchors(FAnchors(0.5f, 0.f));
		ToastSlot->SetAlignment(FVector2D(0.5f, 0.f));
		ToastSlot->SetPosition(FVector2D(0.f, 52.f));
		ToastSlot->SetAutoSize(true);
	}
	ToastText->SetVisibility(ESlateVisibility::Collapsed);

	// 화면을 덮는 전용 창 (창 화면을 담음 · 위쪽 HUD 줄 아래 공간 · 넘치면 스크롤)
	WindowOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WindowOverlay"));
	WindowOverlay->SetBrushColor(CozyHud::OverlayColor);
	WindowOverlay->SetHorizontalAlignment(HAlign_Center);
	WindowOverlay->SetVerticalAlignment(VAlign_Center);
	WindowOverlay->SetPadding(FMargin(0.f, 100.f, 0.f, 20.f));
	if (UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(WindowOverlay))
	{
		OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		OverlaySlot->SetOffsets(FMargin(0.f));
	}
	WindowScreenSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WindowScreenSize"));
	WindowScreenScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("WindowScreenScroll"));
	WindowScreenSize->SetContent(WindowScreenScroll);
	WindowOverlay->SetContent(WindowScreenSize);
	WindowOverlay->SetVisibility(ESlateVisibility::Collapsed);

	// 디버그 메뉴 (F1)
	DebugPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DebugPanel"));
	DebugPanel->SetBrushColor(CozyHud::PanelColor);
	DebugPanel->SetPadding(FMargin(12.f));
	DebugContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugContent"));
	// 길어도 화면 안에 들어가게 높이 상한(매 프레임 화면 높이에 맞춤) + 스크롤
	DebugSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DebugSize"));
	UScrollBox* DebugScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DebugScroll"));
	DebugScroll->AddChild(DebugContent);
	DebugSize->SetContent(DebugScroll);
	DebugSize->SetMaxDesiredHeight(700.f);
	DebugSize->SetWidthOverride(500.f);
	DebugPanel->SetContent(DebugSize);
	if (UCanvasPanelSlot* DebugSlot = RootCanvas->AddChildToCanvas(DebugPanel))
	{
		DebugSlot->SetZOrder(6);
		DebugSlot->SetAnchors(FAnchors(1.f, 0.f));
		DebugSlot->SetAlignment(FVector2D(1.f, 0.f));
		// 위쪽 HUD 버튼 줄 아래에서 시작 (56이면 큰 화면에서 제목 줄이 버튼에 가렸음)
		DebugSlot->SetPosition(FVector2D(-12.f, CozyHud::HudTopSafe));
		DebugSlot->SetAutoSize(true);
	}
	DebugPanel->SetVisibility(ESlateVisibility::Collapsed);

	// HUD 화면 연결이 빠졌으면 알림 (알림 글자를 만든 뒤에)
	if (!HudScreen)
	{
		WarnMissingScreen(TEXT("HUD"), CozyHud::HudScreenSetting, true);
	}
}

void UCozyHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UCozyEstateSubsystem* Estate = GetEstate())
	{
		EstateChangedHandle = Estate->OnEstateChanged.AddUObject(this, &UCozyHudWidget::HandleEstateChanged);
	}
	HandleEstateChanged(true);
}

void UCozyHudWidget::NativeDestruct()
{
	if (UCozyEstateSubsystem* Estate = GetEstate())
	{
		Estate->OnEstateChanged.Remove(EstateChangedHandle);
	}
	Super::NativeDestruct();
}

void UCozyHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateNameLabelPositions();

	// 창 화면 높이 상한: 시작 직후에는 화면 크기를 아직 모를 수 있어 매 프레임 이 위젯의 실제 높이로 맞춘다 (HUD 아래 공간)
	if (WindowScreenSize && WindowKind != ECozyWindowKind::None)
	{
		const float MaxHeight = FMath::Max(200.f, MyGeometry.GetLocalSize().Y - 130.f);
		if (MyGeometry.GetLocalSize().Y > 1.f && !FMath::IsNearlyEqual(WindowScreenSize->GetMaxDesiredHeight(), MaxHeight, 1.f))
		{
			WindowScreenSize->SetMaxDesiredHeight(MaxHeight);
		}
		// 화면 안 'Scroll When Tall' 영역이 이 높이에 맞춰 줄어든다 → 아래 고정 버튼 줄이 늘 보임
		// (디자이너에서 창을 띄워 둔 만큼은 영역이 실제 그려진 위치로 한 번 더 맞춤 · UCozyUiArea::NativeTick)
		if (MyGeometry.GetLocalSize().Y > 1.f)
		{
			if (UCozyUiScreen* OpenScreen = GetWindowScreen(WindowKind))
			{
				OpenScreen->MaxScreenHeight = MaxHeight;
			}
		}
	}

	if (DebugSize)
	{
		// HUD 아래부터 화면 끝 12px 전까지 (패널 안쪽 위아래 여백 포함)
		const float DebugMax = FMath::Max(240.f, MyGeometry.GetLocalSize().Y - CozyHud::HudTopSafe - 12.f - DebugPanel->GetPadding().GetTotalSpaceAlong<Orient_Vertical>());
		if (MyGeometry.GetLocalSize().Y > 1.f && !FMath::IsNearlyEqual(DebugSize->GetMaxDesiredHeight(), DebugMax, 1.f))
		{
			DebugSize->SetMaxDesiredHeight(DebugMax);
		}
	}

	if (bPlacementMode || bPlacementPreview)
	{
		UpdatePlacementLive();
	}

	if (ToastRemaining > 0.f)
	{
		ToastRemaining -= InDeltaTime;
		if (ToastRemaining <= 0.f && ToastText)
		{
			ToastText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 메뉴를 선택한 시설 위에 따라 붙임
	if (IconSlot && IconFacility.IsValid() && IconBox->GetVisibility() != ESlateVisibility::Collapsed)
	{
		FVector2D ScreenPosition;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), IconFacility->GetIconAnchorLocation(), ScreenPosition, false))
		{
			// 메뉴(아래 가운데 기준)가 화면 가장자리 밖이나 위쪽 HUD 줄(칩 · 시계 · 버튼)로 넘어가지 않게 화면 안으로 당김
			const FVector2D View = MyGeometry.GetLocalSize();
			const FVector2D Menu = IconBox->GetDesiredSize();
			if (View.X > 1.f && Menu.X > 1.f)
			{
				const float HalfWidth = Menu.X * 0.5f + CozyHud::MenuEdgeMargin;
				ScreenPosition.X = View.X > HalfWidth * 2.f ? FMath::Clamp(ScreenPosition.X, HalfWidth, View.X - HalfWidth) : View.X * 0.5f;
				ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y, CozyHud::HudTopSafe + Menu.Y, FMath::Max(CozyHud::HudTopSafe + Menu.Y, View.Y - CozyHud::MenuEdgeMargin));
			}
			IconSlot->SetPosition(ScreenPosition);
		}
	}
}

UCozyEstateSubsystem* UCozyHudWidget::GetEstate() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
}

FVector2D UCozyHudWidget::GetViewportSlateSize() const
{
	const float Scale = FMath::Max(0.1f, UWidgetLayoutLibrary::GetViewportScale(this));
	return UWidgetLayoutLibrary::GetViewportSize(this) / Scale;
}

void UCozyHudWidget::WarnMissingScreen(FName ScreenName, const TCHAR* SettingName, bool bOnce)
{
	if (bOnce)
	{
		bool bAlreadyWarned = false;
		WarnedMissingScreens.Add(ScreenName, &bAlreadyWarned);
		if (bAlreadyWarned)
		{
			return;
		}
	}
	UE_LOG(LogCozyRealm, Error, TEXT("[UI] '%s' 화면이 연결되지 않아 띄울 수 없습니다. 프로젝트 설정 > Cozy UI > %s 에서 '%s' 항목에 Widget Blueprint를 지정해 주세요."),
		*ScreenName.ToString(), SettingName, *ScreenName.ToString());
	ShowToast(FText::Format(LOCTEXT("ScreenMissing", "'{0}' 화면이 연결되지 않았습니다 · 프로젝트 설정 > Cozy UI > {1}에서 지정해 주세요"),
		FText::FromName(ScreenName), FText::FromString(SettingName)));
}

// ---------------------------------------------------------------------------
// 배치 모드

void UCozyHudWidget::SetPlacementMode(bool bEnable)
{
	bPlacementMode = bEnable;
	HideFacilityIcons();
	if (PlacementPanel)
	{
		PlacementPanel->SetVisibility(bEnable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!bEnable && PlacementActionBox)
	{
		PlacementActionBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshPlacementPanel();
}

void UCozyHudWidget::RefreshPlacementPanel()
{
	if (!PlacementPanel || !GetEstate() || (!bPlacementMode && !bPlacementPreview))
	{
		return;
	}
	// 배치 패널 화면 (배경·안내·보관함 목록 모두 화면 설정에서)
	UCozyUiScreen* Screen = GetScreenByName(TEXT("Placement"));
	if (!Screen)
	{
		WarnMissingScreen(TEXT("Placement"), CozyHud::WindowScreensSetting, true);
		return;
	}
	if (PlacementPanel->GetContent() != Screen)
	{
		PlacementPanel->SetContent(Screen);
	}
	Screen->RefreshTheme();
	Screen->RefreshValues();
}

bool UCozyHudWidget::ShowPlacementActionsScreen()
{
	// 배치 버튼 화면 (상태 글 · 회전·보관·확정·취소 버튼은 화면 설정에서)
	UCozyUiScreen* ActionScreen = GetScreenByName(TEXT("PlacementActions"));
	if (!ActionScreen)
	{
		WarnMissingScreen(TEXT("PlacementActions"), CozyHud::WindowScreensSetting, true);
		return false;
	}
	if (PlacementActionBox->GetChildrenCount() != 1 || PlacementActionBox->GetChildAt(0) != ActionScreen)
	{
		PlacementActionBox->ClearChildren();
		PlacementActionBox->AddChildToVerticalBox(ActionScreen)->SetHorizontalAlignment(HAlign_Center);
		ActionScreen->RefreshTheme();
	}
	ActionScreen->RefreshValues();
	return true;
}

void UCozyHudWidget::UpdatePlacementLive()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	ACozyFacilityActor* Actor = Estate ? Estate->GetPlacementActor() : nullptr;
	if (bPlacementPreview && PlacementActionBox && !(Estate && Estate->IsPlacing()))
	{
		// 미리보기: 실제로 옮기는 시설이 없어도 화면 가운데에 버튼 상자를 보여 줌 (버튼은 알림만)
		if (!ShowPlacementActionsScreen())
		{
			PlacementActionBox->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		if (PlacementActionSlot)
		{
			PlacementActionSlot->SetPosition(GetViewportSlateSize() * 0.5f);
		}
		PlacementActionBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}
	if (!Estate || !Estate->IsPlacing() || !Actor || !PlacementActionBox)
	{
		if (PlacementActionBox)
		{
			PlacementActionBox->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	FVector2D ScreenPosition;
	if (PlacementActionSlot && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Actor->GetIconAnchorLocation(), ScreenPosition, false))
	{
		PlacementActionSlot->SetPosition(ScreenPosition);
	}
	PlacementActionBox->SetVisibility(ShowPlacementActionsScreen() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UCozyHudWidget::ShowToast(const FText& Message)
{
	if (ToastText)
	{
		ToastText->SetText(Message);
		ToastText->SetVisibility(ESlateVisibility::HitTestInvisible);
		ToastRemaining = 3.f;
	}
}

// ---------------------------------------------------------------------------
// 단축키 · 방치 보상

FGuid UCozyHudWidget::FindFirstFacilityWith(ECozyFacilityFunction Function, bool bSkipStored) const
{
	if (UCozyEstateSubsystem* Estate = GetEstate())
	{
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility.DefinitionId);
			if (Def && Def->Functions.Contains(Function) && !(bSkipStored && Facility.bStored))
			{
				return Facility.InstanceId;
			}
		}
	}
	return FGuid();
}

void UCozyHudWidget::ToggleNagayaWindow()
{
	if (WindowKind == ECozyWindowKind::Nagaya)
	{
		CloseWindow();
		return;
	}
	// 보관함에 넣은 나가야도 대상 (예전 동작 그대로)
	const FGuid NagayaId = FindFirstFacilityWith(ECozyFacilityFunction::ResidentHousing, false);
	if (NagayaId.IsValid())
	{
		OpenWindow(ECozyWindowKind::Nagaya, NagayaId);
	}
}

void UCozyHudWidget::ToggleStorageWindow()
{
	if (WindowKind == ECozyWindowKind::Storage)
	{
		CloseWindow();
		return;
	}
	OpenWindow(ECozyWindowKind::Storage, FGuid());
}

void UCozyHudWidget::ShowPendingOfflineReport()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (Estate && Estate->GetPendingOfflineReport() && WindowKind != ECozyWindowKind::OfflineReport)
	{
		// 상태가 바뀔 때마다 불리므로 화면이 없으면 한 번만 알림
		if (!GetWindowScreen(ECozyWindowKind::OfflineReport))
		{
			WarnMissingScreen(GetWindowName(ECozyWindowKind::OfflineReport), CozyHud::WindowScreensSetting, true);
			return;
		}
		OpenWindow(ECozyWindowKind::OfflineReport, FGuid());
	}
}

void UCozyHudWidget::HandleEstateChanged(bool bStructural)
{
	ShowPendingOfflineReport();
	if (bStructural)
	{
		RefreshPlacementPanel();
		RefreshNameLabels();
	}
	// 열린 창 화면이 목록·버튼 상태를 다시 적용
	if (UCozyUiScreen* Screen = WindowKind != ECozyWindowKind::None ? GetWindowScreen(WindowKind) : nullptr)
	{
		ClampProcessingSelection();
		Screen->RefreshTheme();
		Screen->RefreshValues();
	}
	if (!bStructural)
	{
		return;
	}
	// 구조가 바뀌면 다시 그림: 나가야(배치 가능 여부) · 후신소(시설 추가 · 레벨 변경으로 다음 단계의 조건 '이동' 버튼이 달라짐)
	if (WindowKind == ECozyWindowKind::Nagaya || WindowKind == ECozyWindowKind::Upgrade)
	{
		RefreshWindow();
	}
	if (bDebugVisible)
	{
		RefreshDebugPanel();
	}
}

// ---------------------------------------------------------------------------
// 공통 위젯 (이름표 · 디버그 창)

UTextBlock* UCozyHudWidget::MakeText(const FText& Text, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

UButton* UCozyHudWidget::MakeButton(const FText& Label, TFunction<void()> OnClick, bool bEnabled, int32 FontSize)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* LabelText = MakeText(Label, FontSize, FLinearColor(0.08f, 0.08f, 0.1f));
	Button->SetContent(LabelText);
	Button->SetIsEnabled(bEnabled);

	UCozyUiAction* Action = NewObject<UCozyUiAction>(this);
	// 누른 뒤 키보드 포커스를 게임 화면으로 돌림 · 버튼에 남아 있으면 Space 단축키(전부 수확)가 그 버튼을 다시 누름
	Action->Callback = [Click = MoveTemp(OnClick)]()
	{
		Click();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	};
	Button->OnClicked.AddDynamic(Action, &UCozyUiAction::Fire);
	if (ActionSink)
	{
		ActionSink->Add(Action);
	}
	return Button;
}

// ---------------------------------------------------------------------------
// 시설 이름표

void UCozyHudWidget::RefreshNameLabels()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	UWorld* World = GetWorld();
	if (!LabelLayer || !Estate || !World)
	{
		return;
	}

	// 시설 액터가 그대로면 다시 만들지 않음
	TArray<TWeakObjectPtr<ACozyFacilityActor>> Current;
	for (TActorIterator<ACozyFacilityActor> It(World); It; ++It)
	{
		Current.Add(*It);
	}
	if (Current == LabelFacilities)
	{
		return;
	}

	LabelLayer->ClearChildren();
	NameLabels.Reset();
	LabelFacilities = Current;
	for (const TWeakObjectPtr<ACozyFacilityActor>& Facility : LabelFacilities)
	{
		UTextBlock* Label = MakeText(Estate->GetFacilityDisplayName(Facility->GetFacilityId()), 16, FLinearColor(0.12f, 0.1f, 0.09f));
		Label->SetShadowOffset(FVector2D(1.f, 1.f));
		Label->SetShadowColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f));
		if (UCanvasPanelSlot* LabelSlot = LabelLayer->AddChildToCanvas(Label))
		{
			LabelSlot->SetAutoSize(true);
			LabelSlot->SetAlignment(FVector2D(0.5f, 1.f));
		}
		NameLabels.Add(Label);
	}
	UpdateNameLabelPositions();
}

void UCozyHudWidget::UpdateNameLabelPositions()
{
	for (int32 Index = 0; Index < NameLabels.Num() && Index < LabelFacilities.Num(); ++Index)
	{
		UTextBlock* Label = NameLabels[Index];
		const ACozyFacilityActor* Facility = LabelFacilities[Index].Get();
		if (!Label || !Facility)
		{
			continue;
		}
		FVector2D ScreenPosition;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Facility->GetNameAnchorLocation(), ScreenPosition, false))
		{
			if (UCanvasPanelSlot* LabelSlot = Cast<UCanvasPanelSlot>(Label->Slot))
			{
				LabelSlot->SetPosition(ScreenPosition);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// 시설 메뉴

TArray<TPair<FName, FText>> UCozyHudWidget::GetFacilityMenu(const FCozyFacilityRow& Def)
{
	TArray<TPair<FName, FText>> Items;
	for (const ECozyFacilityFunction Function : Def.Functions)
	{
		switch (Function)
		{
		case ECozyFacilityFunction::Production: Items.Add({ TEXT("Info"), LOCTEXT("IconInfo2", "정보") }); break;
		case ECozyFacilityFunction::ResidentHousing: Items.Add({ TEXT("Nagaya"), LOCTEXT("IconNagaya2", "주민 관리") }); break;
		case ECozyFacilityFunction::Processing: Items.Add({ TEXT("Processing"), LOCTEXT("IconProcess2", "가공") }); break;
		case ECozyFacilityFunction::Sales: Items.Add({ TEXT("Sales"), LOCTEXT("IconSell2", "판매") }); break;
		case ECozyFacilityFunction::UpgradeQueue: Items.Add({ TEXT("Upgrade"), LOCTEXT("IconUpgrade2", "업그레이드") }); break;
		case ECozyFacilityFunction::ShrineCore: Items.Add({ TEXT("Shrine"), LOCTEXT("IconShrine2", "신사") }); break;
		case ECozyFacilityFunction::FieldManagement: Items.Add({ TEXT("FieldManagement"), LOCTEXT("IconFieldMgmt2", "밭 관리") }); break;
		default: break;
		}
	}
	if (Def.MaxResidents > 0)
	{
		Items.Add({ TEXT("Resident"), LOCTEXT("IconResident2", "주민") });
	}
	return Items;
}

void UCozyHudWidget::OpenFacilityFunction(FName Function, const FGuid& FacilityId)
{
	if (Function == TEXT("Info")) { OpenWindow(ECozyWindowKind::FacilityInfo, FacilityId); }
	else if (Function == TEXT("Nagaya")) { OpenWindow(ECozyWindowKind::Nagaya, FacilityId); }
	else if (Function == TEXT("Processing")) { OpenWindow(ECozyWindowKind::Processing, FacilityId); }
	else if (Function == TEXT("Sales")) { OpenWindow(ECozyWindowKind::Sales, FacilityId); }
	else if (Function == TEXT("Upgrade")) { OpenWindow(ECozyWindowKind::Upgrade, FacilityId); }
	else if (Function == TEXT("Shrine")) { OpenWindow(ECozyWindowKind::Shrine, FacilityId); }
	else if (Function == TEXT("FieldManagement")) { OpenWindow(ECozyWindowKind::FieldManagement, FacilityId); }
	else if (Function == TEXT("Resident")) { OpenWindow(ECozyWindowKind::Nagaya, FGuid(), FacilityId); }
}

void UCozyHudWidget::OpenUpgradeFor(const FGuid& TargetFacility)
{
	const FGuid UpgradeId = FindFirstFacilityWith(ECozyFacilityFunction::UpgradeQueue, true);
	if (UpgradeId.IsValid())
	{
		OpenWindow(ECozyWindowKind::Upgrade, UpgradeId, TargetFacility);
	}
}

void UCozyHudWidget::ShowFacilityIcons(ACozyFacilityActor* FacilityActor)
{
	IconFacility = FacilityActor;
	RefreshFacilityMenu();
}

void UCozyHudWidget::HideFacilityIcons()
{
	IconFacility.Reset();
	if (IconBox)
	{
		IconBox->ClearChildren();
		IconBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCozyHudWidget::RefreshFacilityMenu()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!IconBox || !Estate || !IconFacility.IsValid())
	{
		HideFacilityIcons();
		return;
	}

	const FGuid FacilityId = IconFacility->GetFacilityId();
	const FCozyFacilityState* Facility = Estate->FindFacility(FacilityId);
	if (!Facility || !Estate->GetFacilityDef(Facility->DefinitionId))
	{
		HideFacilityIcons();
		return;
	}

	// 시설 메뉴 화면 (버튼 글자·이미지·크기·간격은 화면 설정에서 · 버튼 목록은 GetFacilityMenu)
	UCozyUiScreen* MenuScreen = GetScreenByName(TEXT("FacilityMenu"));
	if (!MenuScreen)
	{
		WarnMissingScreen(TEXT("FacilityMenu"), CozyHud::WindowScreensSetting, false);
		HideFacilityIcons();
		return;
	}
	IconBox->ClearChildren();
	MenuScreen->ContextFacility = FacilityId;
	IconBox->AddChildToHorizontalBox(MenuScreen);
	MenuScreen->RefreshTheme();
	MenuScreen->RefreshValues();
	IconBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

// ---------------------------------------------------------------------------
// 전용 창

void UCozyHudWidget::OpenWindow(ECozyWindowKind Kind, const FGuid& FacilityId, const FGuid& TargetFacility)
{
	// 화면 연결이 빠진 창은 열지 않고 알림 (예전 C++ 임시 창은 없앰)
	UCozyUiScreen* Screen = GetWindowScreen(Kind);
	if (!Screen)
	{
		WarnMissingScreen(GetWindowName(Kind), CozyHud::WindowScreensSetting, false);
		return;
	}
	// 시설 메뉴(가공·주민 등)에서 상세 창으로 넘어가면 메뉴와 선택을 지운다
	// → 창과 겹치지 않고, 창을 닫은 뒤 같은 시설을 다시 누르면 메뉴가 새로 뜬다
	if (ACozyRealmEstatePlayerController* Controller = Cast<ACozyRealmEstatePlayerController>(GetOwningPlayer()))
	{
		Controller->ClearSelection();
	}
	HideFacilityIcons();
	// 설정 창 위에 다른 창을 열면(단축키 등) 설정 창을 닫은 것과 같게 정리
	if (WindowKind == ECozyWindowKind::Settings && Kind != ECozyWindowKind::Settings)
	{
		if (UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this))
		{
			Settings->CloseSettings();
		}
	}
	WindowKind = Kind;
	WindowFacility = FacilityId;
	WindowTargetFacility = TargetFacility;
	LastFeedback = FText::GetEmpty();
	if (Kind == ECozyWindowKind::Processing)
	{
		// 처음 열면 첫 레시피 · 1회
		UCozyEstateSubsystem* Estate = GetEstate();
		const TArray<FName> Recipes = Estate ? Estate->GetFacilityRecipes(FacilityId) : TArray<FName>();
		ProcSelectedRecipe = Recipes.Num() > 0 ? Recipes[0] : NAME_None;
		ProcSelectedRuns = 1;
		ProcPendingCancelJob.Invalidate();
	}
	if (Kind == ECozyWindowKind::Upgrade)
	{
		// 업그레이드 창은 열 때마다 시설 목록부터 · 대상 시설을 주면 그 시설 상세 (고른 시설은 화면 상태 · 저장 안 함)
		Screen->SelectedUpgradeFacility = TargetFacility;
	}
	if (Kind == ECozyWindowKind::Sales)
	{
		// 처음 열면 팔 수 있는 첫 재료 · 1개
		UCozyEstateSubsystem* Estate = GetEstate();
		SellSelectedItem = NAME_None;
		SellSelectedAmount = 1;
		if (Estate)
		{
			for (const FName& ItemId : Estate->GetSaleListItems())
			{
				const FCozyItemRow* Item = Estate->GetItemDef(ItemId);
				if (Item && Item->SellPrice > 0)
				{
					SellSelectedItem = ItemId;
					break;
				}
			}
		}
	}
	if (WindowOverlay)
	{
		WindowOverlay->SetVisibility(ESlateVisibility::Visible);
	}
	RefreshWindow();
}

void UCozyHudWidget::SetCompactMode(bool bEnable)
{
	bCompactMode = bEnable;
	if (bEnable)
	{
		CloseWindow();
		HideFacilityIcons();
		if (ACozyRealmEstatePlayerController* Controller = Cast<ACozyRealmEstatePlayerController>(GetOwningPlayer()))
		{
			Controller->ClearSelection();
			if (Controller->IsPlacementMode())
			{
				Controller->SetPlacementMode(false);
			}
		}
		if (bDebugVisible)
		{
			ToggleDebugPanel();
		}
	}
	if (LabelLayer)
	{
		LabelLayer->SetVisibility(bEnable ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (HudScreen)
	{
		HudScreen->SetCompact(bEnable);
	}
}

void UCozyHudWidget::ToggleSettingsWindow()
{
	// 작은 창에서는 설정 창을 열지 않음 (크게 보기 후 열기)
	if (bCompactMode)
	{
		return;
	}
	if (IsSettingsOpen())
	{
		CloseWindow();
		return;
	}
	// 화면이 없으면 설정 서비스를 '열림'으로 두지 않고 알림만
	if (!GetWindowScreen(ECozyWindowKind::Settings))
	{
		WarnMissingScreen(GetWindowName(ECozyWindowKind::Settings), CozyHud::WindowScreensSetting, false);
		return;
	}
	if (UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this))
	{
		Settings->OpenSettings();
	}
	OpenWindow(ECozyWindowKind::Settings, FGuid());
}

void UCozyHudWidget::CloseWindow()
{
	// 설정 창: 적용하지 않은 변경은 버림 (화면 변경 확인 중이면 이전 화면 설정으로) · 진행 데이터는 건드리지 않음
	if (WindowKind == ECozyWindowKind::Settings)
	{
		if (UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this))
		{
			if (Settings->CloseSettings())
			{
				ShowToast(LOCTEXT("SettingsDiscarded", "적용하지 않은 설정 변경은 취소했습니다"));
			}
		}
	}
	// 방치 보상 창은 닫기도 '확인'과 같게 처리 · 안 하면 남은 보고서 때문에 바로 다시 열려 닫기가 안 되는 것처럼 보임
	if (WindowKind == ECozyWindowKind::OfflineReport && !bUiPreview)
	{
		if (UCozyEstateSubsystem* Estate = GetEstate())
		{
			Estate->DismissOfflineReport();
		}
	}
	WindowKind = ECozyWindowKind::None;
	if (WindowOverlay)
	{
		WindowOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCozyHudWidget::RefreshWindow()
{
	UCozyUiScreen* Screen = GetWindowScreen(WindowKind);
	if (!Screen || !WindowScreenSize)
	{
		return;
	}
	Screen->ContextFacility = WindowFacility;
	Screen->ContextTarget = WindowTargetFacility;
	ClampProcessingSelection();
	// 위쪽 HUD(칩 줄·버튼)와 겹치지 않게 그 아래 공간에 맞춤 · 넘치면 스크롤
	WindowScreenSize->SetMaxDesiredHeight(FMath::Max(200.f, GetViewportSlateSize().Y - 130.f));
	if (WindowScreenScroll->GetChildrenCount() != 1 || WindowScreenScroll->GetChildAt(0) != Screen)
	{
		WindowScreenScroll->ClearChildren();
		WindowScreenScroll->AddChild(Screen);
	}
	Screen->RefreshTheme();
	Screen->RefreshValues();
}

FName UCozyHudWidget::GetWindowName(ECozyWindowKind Kind)
{
	switch (Kind)
	{
	case ECozyWindowKind::FacilityInfo: return TEXT("FacilityInfo");
	case ECozyWindowKind::Nagaya: return TEXT("Nagaya");
	case ECozyWindowKind::Storage: return TEXT("Storage");
	case ECozyWindowKind::Processing: return TEXT("Processing");
	case ECozyWindowKind::Sales: return TEXT("Sales");
	case ECozyWindowKind::Upgrade: return TEXT("Upgrade");
	case ECozyWindowKind::FieldManagement: return TEXT("FieldManagement");
	case ECozyWindowKind::OfflineReport: return TEXT("OfflineReport");
	case ECozyWindowKind::Shrine: return TEXT("Shrine");
	case ECozyWindowKind::Settings: return TEXT("Settings");
	default: return NAME_None;
	}
}

void UCozyHudWidget::ClampProcessingSelection()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (WindowKind != ECozyWindowKind::Processing || !Estate)
	{
		return;
	}
	const FCozyRecipeQuote Quote = Estate->GetRecipeQuote(WindowFacility, ProcSelectedRecipe, ProcSelectedRuns);
	if (ProcSelectedRuns > FMath::Max(1, Quote.MaxRuns))
	{
		ProcSelectedRuns = FMath::Max(1, Quote.MaxRuns);
	}
	if (ProcPendingCancelJob.IsValid())
	{
		const TArray<FCozyProcessingJobView> Jobs = Estate->GetProcessingJobs(WindowFacility);
		if (!Jobs.ContainsByPredicate([this](const FCozyProcessingJobView& Job) { return Job.JobId == ProcPendingCancelJob; }))
		{
			ProcPendingCancelJob.Invalidate();
		}
	}
}

UCozyUiScreen* UCozyHudWidget::GetWindowScreen(ECozyWindowKind Kind)
{
	return GetScreenByName(GetWindowName(Kind));
}

UCozyUiScreen* UCozyHudWidget::GetScreenByName(FName Name)
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (TObjectPtr<UCozyUiScreen>* Cached = WindowScreenCache.Find(Name))
	{
		return *Cached;
	}
	const TSoftClassPtr<UCozyUiScreen>* Soft = UCozyUiSettings::Get()->WindowScreens.Find(Name);
	UClass* ScreenClass = Soft ? Soft->LoadSynchronous() : nullptr;
	UCozyUiScreen* Screen = ScreenClass ? CreateWidget<UCozyUiScreen>(this, ScreenClass) : nullptr;
	if (Screen)
	{
		WindowScreenCache.Add(Name, Screen);
		ApplyPreview(Screen);
	}
	return Screen;
}

// ---------------------------------------------------------------------------
// UI 미리보기

void UCozyHudWidget::ApplyPreview(UCozyUiScreen* Screen) const
{
	if (Screen && (Screen->bPreview != bUiPreview || Screen->PreviewState != UiPreviewState))
	{
		Screen->PreviewState = UiPreviewState;
		Screen->SetPreview(bUiPreview);
	}
}

void UCozyHudWidget::ApplyPreviewAll() const
{
	ApplyPreview(HudScreen);
	for (const TPair<FName, TObjectPtr<UCozyUiScreen>>& Each : WindowScreenCache)
	{
		ApplyPreview(Each.Value);
	}
}

void UCozyHudWidget::SetUiPreview(bool bEnable)
{
	bUiPreview = bEnable;
	if (UCozyEstateSubsystem* Estate = GetEstate())
	{
		Estate->SetAutosavePaused(bEnable);
	}
	if (!bEnable && bPlacementPreview)
	{
		TogglePlacementPreview();
	}
	ApplyPreviewAll();
	RefreshFacilityMenu();
	RefreshDebugPanel();
}

void UCozyHudWidget::SetUiPreviewState(ECozyUiPreviewState State)
{
	UiPreviewState = State;
	ApplyPreviewAll();
	RefreshDebugPanel();
}

void UCozyHudWidget::OpenPreviewWindow(ECozyWindowKind Kind)
{
	if (!GetEstate())
	{
		return;
	}
	switch (Kind)
	{
	case ECozyWindowKind::FacilityInfo: OpenWindow(Kind, FindFirstFacilityWith(ECozyFacilityFunction::Production, true)); break;
	case ECozyWindowKind::Processing: OpenWindow(Kind, FindFirstFacilityWith(ECozyFacilityFunction::Processing, true)); break;
	case ECozyWindowKind::Sales: OpenWindow(Kind, FindFirstFacilityWith(ECozyFacilityFunction::Sales, true)); break;
	case ECozyWindowKind::Upgrade: OpenWindow(Kind, FindFirstFacilityWith(ECozyFacilityFunction::UpgradeQueue, true)); break;
	case ECozyWindowKind::FieldManagement: OpenWindow(Kind, FindFirstFacilityWith(ECozyFacilityFunction::FieldManagement, true)); break;
	default: OpenWindow(Kind, FGuid()); break;
	}
}

void UCozyHudWidget::TogglePlacementPreview()
{
	bPlacementPreview = !bPlacementPreview;
	if (PlacementPanel)
	{
		PlacementPanel->SetVisibility((bPlacementPreview || bPlacementMode) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!bPlacementPreview && PlacementActionBox && !bPlacementMode)
	{
		PlacementActionBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshPlacementPanel();
	UpdatePlacementLive();
}

// ---------------------------------------------------------------------------
// 창 화면이 쓰는 공통 처리 (방금 한 일 · 수령)

void UCozyHudWidget::SetFeedback(const FText& Message)
{
	LastFeedback = Message;
}

void UCozyHudWidget::HandleCollectClicked(const FGuid& FacilityId)
{
	if (UCozyEstateSubsystem* Estate = GetEstate())
	{
		// 클릭 결과는 '방금 한 일'로 따로 · 현재 상태 줄은 알림을 받아 최신 값으로 바뀜
		SetFeedback(Estate->CollectUnclaimed(FacilityId).Message);
	}
}

// ---------------------------------------------------------------------------
// 디버그 메뉴

void UCozyHudWidget::ToggleDebugPanel()
{
	bDebugVisible = !bDebugVisible;
	if (DebugPanel)
	{
		DebugPanel->SetVisibility(bDebugVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (bDebugVisible)
	{
		RefreshDebugPanel();
	}
}

void UCozyHudWidget::RefreshDebugPanel()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!DebugContent || !Estate)
	{
		return;
	}
	DebugContent->ClearChildren();
	DebugActions.Reset();
	ActionSink = &DebugActions;

	// 공통 테마 (HUD 화면의 색·글꼴 덮어쓰기까지 같이 씀) · HUD 화면이 없으면 예전 모습
	UCozyUiTheme* Theme = HudScreen ? HudScreen->GetTheme() : nullptr;
	if (DebugPanel && Theme)
	{
		if (const FSlateBrush* Brush = Theme->Images.Find(TEXT("Window")))
		{
			DebugPanel->SetBrush(*Brush);
			DebugPanel->SetBrushColor(FLinearColor::White);
		}
		DebugPanel->SetPadding(FMargin(18.f, 14.f));
	}
	const FButtonStyle* ButtonStyle = Theme ? Theme->ButtonStyles.Find(TEXT("Default")) : nullptr;

	UHorizontalBox* CurrentRow = nullptr;
	auto Text = [this, Theme](const FText& Value, ECozyUiTextRole Role, ECozyUiColor Color, bool bWrap) -> UTextBlock*
	{
		UTextBlock* Block = MakeText(Value, Role == ECozyUiTextRole::Title ? 18 : 13, Role == ECozyUiTextRole::Small ? CozyHud::MutedText : FLinearColor::White);
		if (Theme)
		{
			Block->SetFont(HudScreen->GetFont(Role));
			Block->SetColorAndOpacity(FSlateColor(HudScreen->GetColor(Color)));
		}
		if (bWrap)
		{
			Block->SetAutoWrapText(true);
			Block->SetWrapTextAt(440.f);
		}
		return Block;
	};
	auto Section = [this, &Text, &CurrentRow](const FText& Title, const FText& Help)
	{
		CurrentRow = nullptr;
		DebugContent->AddChildToVerticalBox(Text(Title, ECozyUiTextRole::Body, ECozyUiColor::Point, true))->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		DebugContent->AddChildToVerticalBox(Text(Help, ECozyUiTextRole::Small, ECozyUiColor::InkMuted, true))->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	};
	auto Row = [this, &CurrentRow]()
	{
		CurrentRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		DebugContent->AddChildToVerticalBox(CurrentRow)->SetPadding(FMargin(0.f, 2.f));
	};
	auto EndRow = [&CurrentRow]() { CurrentRow = nullptr; };
	// 버튼 하나 (툴팁 = 초보자용 설명) · 줄을 시작했으면 그 줄에, 아니면 한 줄 전체
	auto Btn = [this, &CurrentRow, ButtonStyle, Theme](const FText& Label, const FText& Tip, TFunction<void()> OnClick, bool bEnabled = true) -> UButton*
	{
		UButton* Button = MakeButton(Label, MoveTemp(OnClick), bEnabled, 13);
		Button->SetToolTipText(Tip);
		if (ButtonStyle)
		{
			Button->SetStyle(*ButtonStyle);
		}
		if (UTextBlock* LabelText = Theme ? Cast<UTextBlock>(Button->GetContent()) : nullptr)
		{
			LabelText->SetFont(HudScreen->GetFont(ECozyUiTextRole::Small));
			LabelText->SetColorAndOpacity(FSlateColor(HudScreen->GetColor(ECozyUiColor::Ink)));
		}
		if (CurrentRow)
		{
			CurrentRow->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
		}
		else
		{
			DebugContent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 2.f));
		}
		return Button;
	};

	DebugContent->AddChildToVerticalBox(Text(LOCTEXT("DebugTitle2", "디버그 · 시험 도구 (F1)"), ECozyUiTextRole::Title, ECozyUiColor::Point, false));
	DebugContent->AddChildToVerticalBox(Text(LOCTEXT("DebugIntro", "개발과 검증에 쓰는 도구입니다. 제목에 '저장 데이터가 바뀜'이 붙은 버튼은 결과가 실제 저장 파일에 남으니, 시험 전에 저장 파일(Saved/SaveGames)을 복사해 두세요. 버튼에 마우스를 올리면 설명이 나옵니다."),
		ECozyUiTextRole::Small, ECozyUiColor::InkMuted, true))->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));

	// ① 게임 속도
	Section(FText::Format(LOCTEXT("DbgSpeed", "① 게임 속도 — 지금 {0}"), CozyHud::TimeScaleText(Estate->GetTimeScale())),
		LOCTEXT("DbgSpeedHelp", "게임 시간이 흐르는 빠르기를 바꿉니다. 생산·가공·업그레이드가 모두 그만큼 빨라집니다. 시험이 끝나면 ×1로 돌려 두세요."));
	Row();
	for (const float Scale : { 1.f, 10.f, 100.f })
	{
		Btn(CozyHud::TimeScaleText(Scale), FText::Format(LOCTEXT("DbgSpeedTip", "게임 시간을 {0} 빠르기로"), CozyHud::TimeScaleText(Scale)), [this, Scale]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->SetTimeScale(Scale);
				RefreshDebugPanel();
			}
		}, !FMath::IsNearlyEqual(Estate->GetTimeScale(), Scale));
	}
	EndRow();

	// ② 재료 넣기 · 바꾸기
	Section(LOCTEXT("DbgRes", "② 재료 넣기 · 바꾸기 (저장 데이터가 바뀜)"),
		LOCTEXT("DbgResHelp", "창고 수량을 늘리거나 정해진 값으로 바꿉니다. 수령·판매·가공 '최대' 계산이나 창고가 가득 찼을 때를 시험할 때 씁니다."));
	auto AddRes = [this, &Btn](const FText& Label, FName ItemId, int32 Amount)
	{
		Btn(Label, FText::Format(LOCTEXT("DbgAddTip", "창고에 {0} {1}개를 더합니다 (한도를 넘지 않음)"), FText::FromName(ItemId), FText::AsNumber(Amount)), [this, ItemId, Amount]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->DebugAddResource(ItemId, Amount);
			}
		});
	};
	auto SetRes = [this, &Btn](const FText& Label, FName ItemId, int32 Amount)
	{
		Btn(Label, FText::Format(LOCTEXT("DbgSetTip", "창고의 {0}을(를) 정확히 {1}개로 맞춥니다"), FText::FromName(ItemId), FText::AsNumber(Amount)), [this, ItemId, Amount]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->DebugSetResource(ItemId, Amount);
			}
		});
	};
	Row();
	AddRes(LOCTEXT("AddGold", "골드 +100"), TEXT("Gold"), 100);
	AddRes(LOCTEXT("AddWheat", "밀 +10"), TEXT("Wheat"), 10);
	AddRes(LOCTEXT("AddFlour", "밀가루 +10"), TEXT("Flour"), 10);
	EndRow();
	Row();
	AddRes(LOCTEXT("AddTalisman", "시간 부적 +5"), TEXT("TimeTalisman"), 5);
	AddRes(LOCTEXT("AddRice", "쌀 +10 (테스트 레시피용)"), TEXT("Rice"), 10);
	EndRow();
	Row();
	SetRes(LOCTEXT("SetWheat10", "창고 밀 10개로"), TEXT("Wheat"), 10);
	SetRes(LOCTEXT("SetWheat90", "밀 90"), TEXT("Wheat"), 90);
	EndRow();
	Row();
	SetRes(LOCTEXT("SetWheat97", "밀 97"), TEXT("Wheat"), 97);
	SetRes(LOCTEXT("SetWheat98", "밀 98"), TEXT("Wheat"), 98);
	SetRes(LOCTEXT("SetWheat100", "밀 100 (가득)"), TEXT("Wheat"), 100);
	EndRow();
	Row();
	SetRes(LOCTEXT("SetFlour98", "창고 밀가루 98개로"), TEXT("Flour"), 98);
	SetRes(LOCTEXT("SetFlour100", "밀가루 100 (가득)"), TEXT("Flour"), 100);
	EndRow();

	// ③ 시험용 시설 · 레시피 · 보상
	Section(LOCTEXT("DbgTest", "③ 시험용 시설 · 레시피 · 보상 (저장 데이터가 바뀜)"),
		LOCTEXT("DbgTestHelp", "기획이 아직 정해지지 않은 시설이나 레시피를 검증할 때만 꺼냅니다. 추가한 시설은 배치 모드에서 보관함에 넣어 치울 수 있습니다."));
	Btn(Estate->GetShowTestRecipes() ? LOCTEXT("TestRecipesOn", "테스트 레시피: 보임") : LOCTEXT("TestRecipesOff", "테스트 레시피: 숨김"),
		LOCTEXT("DbgTestRecipeTip", "가공 창에 '(테스트)' 레시피를 보이거나 숨깁니다"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->SetShowTestRecipes(!EstateNow->GetShowTestRecipes());
			if (WindowKind == ECozyWindowKind::Processing)
			{
				RefreshWindow();
			}
			RefreshDebugPanel();
		}
	});
	Row();
	Btn(LOCTEXT("AddTestField", "테스트용 밭 추가"), LOCTEXT("DbgAddFieldTip", "빈 자리에 밭을 하나 더 놓습니다 (같은 재료 시설이 둘일 때 시험)"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugAddFacility(TEXT("Field"));
		}
	});
	Btn(LOCTEXT("AddFieldOffice", "테스트용 밭 관리 시설 추가"), LOCTEXT("DbgAddOfficeTip", "밭 관리 시설을 하나 놓습니다 (시작 배치가 아직 미정이라 시험용)"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugAddFacility(TEXT("FieldOffice"));
		}
	});
	EndRow();
	Btn(LOCTEXT("GrantTutorial", "튜토리얼 보상 받기 (한 번만)"), LOCTEXT("DbgTutorialTip", "한 번만 받는 보상이 두 번 들어오지 않는지 시험합니다"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			FText Message;
			EstateNow->GrantOneTimeReward(TEXT("Tutorial_FirstSpeedup"), Message);
			ShowToast(Message);
		}
	});

	// ④ 규칙 검사
	Section(LOCTEXT("DbgCheck", "④ 규칙 검사"),
		LOCTEXT("DbgCheckHelp", "'자체 검사'는 지금 상태를 복사해 규칙을 시험한 뒤 원래대로 되돌립니다 (저장 안 바뀜). '재검사 시험'은 실제 상태를 바꿉니다. '데이터 검사'는 출력 로그에만 결과를 씁니다."));
	Btn(LOCTEXT("SelfCheckAppend", "자체 검사: 가공 제작 추가 (상태 되돌림)"), LOCTEXT("DbgSelfCheckTip", "제작 추가·업그레이드 반환 규칙 30개 항목 · 결과는 알림과 출력 로그 [자체 검사]"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			ShowToast(FText::FromString(EstateNow->DebugRunProcessingAppendCheck()));
		}
	});
	Btn(LOCTEXT("RecheckTest", "재검사 시험: 밀 100 직후 제분소 업그레이드"),
		LOCTEXT("DbgRecheckTip", "(저장 데이터가 바뀜) 버튼이 켜진 뒤 조건이 바뀌었을 때 시작 순간 다시 확인하는지 시험합니다 · 실패하면 비용·가공·예약 공간이 그대로여야 합니다"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			for (const FGuid& FacilityId : EstateNow->GetUpgradableFacilities())
			{
				const FCozyFacilityState* Facility = EstateNow->FindFacility(FacilityId);
				if (Facility && Facility->DefinitionId == TEXT("Mill"))
				{
					EstateNow->DebugSetResource(TEXT("Wheat"), 100);
					FText Message;
					EstateNow->StartUpgrade(FacilityId, Message);
					ShowToast(Message);
					break;
				}
			}
		}
	});
	Btn(LOCTEXT("Validate", "데이터 검사 다시 실행 (로그)"), LOCTEXT("DbgValidateTip", "데이터 표(시설·재료·레시피 등)에 빠진 값이 없는지 검사해 출력 로그에 씁니다"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->ValidateData();
		}
	});

	// ⑤ 낮 · 밤
	const ECozyNightOverride Current = Estate->GetNightOverride();
	const FText NightLabel = Current == ECozyNightOverride::Auto ? LOCTEXT("NightAuto", "낮/밤: 자동 (게임 시계)")
		: Current == ECozyNightOverride::ForceDay ? LOCTEXT("NightDay", "낮/밤: 낮 고정")
		: LOCTEXT("NightNight", "낮/밤: 밤 고정");
	Section(LOCTEXT("DbgNight", "⑤ 낮 · 밤 · 계절"), LOCTEXT("DbgNightHelp", "화면 밝기(낮/밤)를 게임 시계(하루 30분)와 상관없이 고정해 봅니다. 누를 때마다 자동 → 낮 → 밤 순서로 바뀝니다."));
	Btn(NightLabel, LOCTEXT("DbgNightTip", "자동 → 낮 고정 → 밤 고정 → 자동"), [this, Current]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			const ECozyNightOverride Next = Current == ECozyNightOverride::Auto ? ECozyNightOverride::ForceDay
				: Current == ECozyNightOverride::ForceDay ? ECozyNightOverride::ForceNight
				: ECozyNightOverride::Auto;
			EstateNow->SetNightOverride(Next);
			RefreshDebugPanel();
		}
	});
	// 계절: 레벨의 '영지 지형' 액터에 있는 계절 목록을 차례로 (지형 모습만 바뀜 · 저장 안 됨)
	const ACozyEstateTerrain* Terrain = CozyHud::FindTerrain(GetWorld());
	if (Terrain && Terrain->Seasons.Num() > 0)
	{
		const FText SeasonName = Terrain->CurrentSeason ? Terrain->CurrentSeason->DisplayName : LOCTEXT("SeasonNone", "없음");
		Btn(FText::Format(LOCTEXT("DbgSeason", "계절: {0} (누르면 다음 계절)"), SeasonName), LOCTEXT("DbgSeasonTip", "영지 지형의 계절 목록 순서대로 바꿉니다 · 지형 모습만 바뀌고 저장되지 않습니다"), [this]()
		{
			if (ACozyEstateTerrain* TerrainNow = CozyHud::FindTerrain(GetWorld()))
			{
				TerrainNow->CycleSeason();
			}
			RefreshDebugPanel();
		});
	}

	// ⑥ 저장 · 불러오기 · 방치 시간
	Section(LOCTEXT("DbgSave", "⑥ 저장 · 불러오기 · 방치 시간 (저장 데이터가 바뀜)"),
		LOCTEXT("DbgSaveHelp", "'저장 시각 당기기'는 다음에 게임을 켤 때 그만큼 자리를 비운 것으로 방치 보상을 계산하게 합니다. '건너뛰기'는 지금 바로 그 시간이 지난 것으로 정산하고 방치 보상 창을 띄웁니다."));
	Row();
	Btn(LOCTEXT("SaveNow", "저장"), LOCTEXT("DbgSaveTip", "지금 상태를 저장 파일에 씁니다"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->SaveEstate(TEXT("디버그"));
		}
	});
	Btn(LOCTEXT("ReloadSave", "불러오기"), LOCTEXT("DbgLoadTip", "저장 파일에서 다시 읽어 옵니다 (마지막 저장 이후 바뀐 것은 사라짐)"), [this]()
	{
		HideFacilityIcons();
		CloseWindow();
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugReloadFromSave();
		}
	});
	EndRow();
	Row();
	Btn(LOCTEXT("ShiftSave1h", "저장 시각 -1시간"), LOCTEXT("DbgShift1Tip", "저장된 시각을 1시간 앞으로 당깁니다"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugShiftSaveTime(3600.0);
		}
	});
	Btn(LOCTEXT("ShiftSave13h", "-13시간"), LOCTEXT("DbgShift13Tip", "저장된 시각을 13시간 앞으로 당깁니다 (방치 보상 최대 12시간 시험)"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugShiftSaveTime(13.0 * 3600.0);
		}
	});
	EndRow();
	Row();
	for (const float Hours : { 1.f, 12.f })
	{
		Btn(FText::Format(LOCTEXT("SkipOffline", "방치 {0}시간 건너뛰기"), FText::AsNumber(Hours)),
			FText::Format(LOCTEXT("DbgSkipTip", "지금 {0}시간이 지난 것으로 정산합니다 (게임을 꺼 둔 것과 같은 계산)"), FText::AsNumber(Hours)), [this, Hours]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->DebugSkipOffline(Hours * 3600.0);
			}
		});
	}
	EndRow();

	// ⑦ UI 미리보기
	Section(bUiPreview ? FText::Format(LOCTEXT("DbgPreviewOn", "⑦ UI 미리보기 — 켜짐 · 상태 {0}"), UCozyUiScreen::GetPreviewStateName(UiPreviewState)) : LOCTEXT("DbgPreview", "⑦ UI 미리보기 (재료·저장 안 바뀜)"),
		LOCTEXT("DbgPreviewHelp", "모든 편집 가능한 화면을 가짜 값으로 바꿔 글자·버튼 배치를 확인합니다. 버튼은 실제로 동작하지 않고 알림만 띄우며, 미리보기 중에는 자동 저장도 멈춥니다. 상태 버튼: 잠김 = 버튼이 꺼진 모습 · 비어 있음 = 목록이 0줄인 모습."));
	Btn(bUiPreview ? LOCTEXT("UiPreviewOff", "UI 미리보기 끄기") : LOCTEXT("UiPreviewOn", "UI 미리보기 켜기 (재료 안 씀)"),
		LOCTEXT("DbgPreviewTip", "HUD · 창 9개 · 배치 패널 · 시설 메뉴 · 배치 버튼을 한꺼번에 미리보기로"), [this]()
	{
		SetUiPreview(!bUiPreview);
		ShowToast(bUiPreview ? LOCTEXT("UiPreviewOnToast", "UI 미리보기 켜짐 · 버튼은 알림만 띄웁니다") : LOCTEXT("UiPreviewOffToast", "UI 미리보기 꺼짐 · 실제 값으로 돌아갑니다"));
	});
	if (bUiPreview)
	{
		int32 StateButtonIndex = 0;
		for (const ECozyUiPreviewState State : { ECozyUiPreviewState::Progress, ECozyUiPreviewState::Paused, ECozyUiPreviewState::Full, ECozyUiPreviewState::Locked, ECozyUiPreviewState::Claimable, ECozyUiPreviewState::Empty })
		{
			if (StateButtonIndex++ % 3 == 0) { Row(); }
			Btn(UCozyUiScreen::GetPreviewStateName(State), FText::Format(LOCTEXT("DbgStateTip", "모든 화면을 '{0}' 상태로"), UCozyUiScreen::GetPreviewStateName(State)), [this, State]()
			{
				SetUiPreviewState(State);
				ShowToast(FText::Format(LOCTEXT("UiPreviewState", "미리보기 상태: {0}"), UCozyUiScreen::GetPreviewStateName(State)));
			}, UiPreviewState != State);
		}
		EndRow();
		DebugContent->AddChildToVerticalBox(Text(LOCTEXT("DbgOpenWindows", "창 열어 보기 (그 기능이 있는 첫 시설로)"), ECozyUiTextRole::Small, ECozyUiColor::InkMuted, false))->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		struct FPreviewWindow { ECozyWindowKind Kind; FText Label; };
		const FPreviewWindow Windows[] = {
			{ ECozyWindowKind::Storage, LOCTEXT("PvWinStorage", "창고") },
			{ ECozyWindowKind::FacilityInfo, LOCTEXT("PvWinInfo", "시설 정보") },
			{ ECozyWindowKind::Processing, LOCTEXT("PvWinProc", "가공") },
			{ ECozyWindowKind::Sales, LOCTEXT("PvWinSales", "판매") },
			{ ECozyWindowKind::Upgrade, LOCTEXT("PvWinUpgrade", "업그레이드") },
			{ ECozyWindowKind::FieldManagement, LOCTEXT("PvWinField", "밭 관리") },
			{ ECozyWindowKind::Nagaya, LOCTEXT("PvWinNagaya", "나가야") },
			{ ECozyWindowKind::OfflineReport, LOCTEXT("PvWinOffline", "방치 보상") },
		};
		int32 Count = 0;
		for (const FPreviewWindow& Each : Windows)
		{
			if (Count++ % 4 == 0)
			{
				Row();
			}
			const ECozyWindowKind Kind = Each.Kind;
			Btn(Each.Label, FText::Format(LOCTEXT("DbgOpenTip", "'{0}' 창을 미리보기로 엽니다 (닫기는 창의 닫기 버튼)"), Each.Label), [this, Kind]() { OpenPreviewWindow(Kind); });
		}
		EndRow();
		Btn(bPlacementPreview ? LOCTEXT("PvPlaceOff", "배치 패널·버튼 상자 미리보기 끄기") : LOCTEXT("PvPlaceOn", "배치 패널·버튼 상자 미리보기 (시설을 옮기지 않음)"),
			LOCTEXT("DbgPlaceTip", "왼쪽 위 배치 패널과 화면 가운데 배치 버튼 상자를 보여 줍니다 · 시설 메뉴는 미리보기 중에 시설을 누르면 모든 버튼이 보입니다"), [this]()
		{
			TogglePlacementPreview();
			RefreshDebugPanel();
		});
	}

	// ⑧ 위험
	Section(LOCTEXT("DbgDanger", "⑧ 처음부터 다시 (저장 데이터가 바뀜 · 되돌릴 수 없음)"),
		LOCTEXT("DbgDangerHelp", "지금 진행을 버리고 새 게임 상태로 시작합니다. 되돌리려면 미리 복사해 둔 저장 파일이 필요합니다."));
	Btn(LOCTEXT("Restart", "새 게임 다시 시작"), LOCTEXT("DbgRestartTip", "지금 진행을 버리고 처음 상태로 (되돌릴 수 없음)"), [this]()
	{
		HideFacilityIcons();
		CloseWindow();
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugRestartNewGame();
		}
	});

	ActionSink = nullptr;
}

#undef LOCTEXT_NAMESPACE
