#include "UI/CozyHudWidget.h"
#include "Estate/CozyEstateSubsystem.h"
#include "UI/Kit/CozyUiScreen.h"
#include "UI/Kit/CozyUiTheme.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "Facilities/CozyFacilityActor.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
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
	const FLinearColor PanelColor(0.05f, 0.05f, 0.08f, 0.75f);
	const FLinearColor OverlayColor(0.f, 0.f, 0.f, 0.7f);
	const FLinearColor WindowColor(0.12f, 0.11f, 0.14f, 0.97f);
	const FLinearColor MutedText(0.7f, 0.7f, 0.75f);
	const FLinearColor AccentText(1.f, 0.85f, 0.45f);
	const FLinearColor WarningText(1.f, 0.55f, 0.45f);

	FText TimeScaleText(float Scale)
	{
		return FText::Format(LOCTEXT("TimeScale", "×{0}"), FText::AsNumber(FMath::RoundToInt(Scale)));
	}

	FText MultiplierText(float Value)
	{
		FNumberFormattingOptions Fmt;
		Fmt.MinimumFractionalDigits = 1;
		Fmt.MaximumFractionalDigits = 2;
		return FText::AsNumber(Value, &Fmt);
	}

	/** 초 → "45초" / "1분 30초" */
	FText DurationText(double Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt32(Seconds));
		if (Total < 60)
		{
			return FText::Format(LOCTEXT("DurSec", "{0}초"), FText::AsNumber(Total));
		}
		return Total % 60 == 0
			? FText::Format(LOCTEXT("DurMin", "{0}분"), FText::AsNumber(Total / 60))
			: FText::Format(LOCTEXT("DurMinSec", "{0}분 {1}초"), FText::AsNumber(Total / 60), FText::AsNumber(Total % 60));
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

	// 위쪽 재화 표시줄
	UBorder* TopBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopBar"));
	TopBar->SetBrushColor(CozyHud::PanelColor);
	TopBar->SetPadding(FMargin(16.f, 8.f));
	UHorizontalBox* TopBarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopBarRow"));
	TopBar->SetContent(TopBarRow);
	// 재료 숫자는 매초 다시 그리고, 창고 버튼은 다시 만들지 않음 (버튼을 다시 만들면 클릭이 끊김)
	// 창고 버튼은 맨 왼쪽에 고정 → 재료 숫자 자릿수가 바뀌어도 움직이지 않음
	ActionSink = &FrameActions;
	UButton* StorageButton = MakeButton(LOCTEXT("StorageButton", "창고"), [this]() { OpenWindow(ECozyWindowKind::Storage, FGuid()); }, true, 15);
	ActionSink = nullptr;
	TopBarRow->AddChildToHorizontalBox(StorageButton)->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
	TopBarBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopBarBox"));
	TopBarRow->AddChildToHorizontalBox(TopBarBox)->SetVerticalAlignment(VAlign_Center);
	ClockText = MakeText(FText::GetEmpty(), 16, CozyHud::MutedText);
	UHorizontalBoxSlot* ClockSlot = TopBarRow->AddChildToHorizontalBox(ClockText);
	ClockSlot->SetHorizontalAlignment(HAlign_Right);
	ClockSlot->SetVerticalAlignment(VAlign_Center);
	ClockSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	if (UCanvasPanelSlot* TopSlot = RootCanvas->AddChildToCanvas(TopBar))
	{
		TopSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
		TopSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 44.f));
	}
	LegacyTopBar = TopBar;

	// 편집 가능한 HUD 화면 (Widget Blueprint) · 프로젝트 설정 'Cozy UI'에 지정돼 있으면 기존 위쪽 바 대신 씀
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
			TopBar->SetVisibility(ESlateVisibility::Collapsed);
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

	// 선택한 시설 위 기능 아이콘
	IconBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("IconBox"));
	IconSlot = RootCanvas->AddChildToCanvas(IconBox);
	IconSlot->SetZOrder(6); // 새 HUD(5)보다 위
	if (IconSlot)
	{
		IconSlot->SetAutoSize(true);
		IconSlot->SetAlignment(FVector2D(0.5f, 1.f));
	}
	IconBox->SetVisibility(ESlateVisibility::Collapsed);

	// 배치 모드 패널 (왼쪽 위 · 안내 + 보관함 목록) · 미리보기 옆 버튼 (회전 · 보관 · 확정 · 취소)
	PlacementPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlacementPanel"));
	PlacementPanel->SetBrushColor(CozyHud::PanelColor);
	PlacementPanel->SetPadding(FMargin(12.f));
	UVerticalBox* PlacementBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PlacementPanel->SetContent(PlacementBody);
	PlacementBody->AddChildToVerticalBox(MakeText(LOCTEXT("PlaceTitle", "배치 모드 (B로 끝내기)"), 18, CozyHud::AccentText));
	PlacementBody->AddChildToVerticalBox(MakeText(LOCTEXT("PlaceHelp", "시설을 누른 채 끌어서 옮기기 · R 회전 · Esc 취소"), 14, CozyHud::MutedText))->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
	PlacementStoredBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PlacementBody->AddChildToVerticalBox(PlacementStoredBox);
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PlacementPanel))
	{
		PanelSlot->SetZOrder(6);
		// 새 HUD의 칩 줄(위쪽 약 100) 아래
		PanelSlot->SetPosition(FVector2D(24.f, 110.f));
		PanelSlot->SetAutoSize(true);
	}
	PlacementPanel->SetVisibility(ESlateVisibility::Collapsed);

	PlacementActionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlacementActionBox"));
	PlacementStatusText = MakeText(FText::GetEmpty(), 14);
	PlacementActionBox->AddChildToVerticalBox(PlacementStatusText)->SetHorizontalAlignment(HAlign_Center);
	UHorizontalBox* PlacementButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ActionSink = &FrameActions;
	PlacementButtons->AddChildToHorizontalBox(MakeButton(LOCTEXT("PlaceRotate", "회전 (R)"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->RotatePlacement();
		}
	}, true, 14))->SetPadding(FMargin(2.f, 0.f));
	PlacementStoreButton = MakeButton(LOCTEXT("PlaceStore", "보관"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			const FGuid Id = EstateNow->GetPlacementId();
			FText Message;
			EstateNow->StoreFacility(Id, Message);
			ShowToast(Message);
		}
	}, true, 14);
	PlacementButtons->AddChildToHorizontalBox(PlacementStoreButton)->SetPadding(FMargin(2.f, 0.f));
	PlacementConfirmButton = MakeButton(LOCTEXT("PlaceConfirm", "확정"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			FText Message;
			EstateNow->ConfirmPlacement(Message);
			ShowToast(Message);
		}
	}, true, 14);
	PlacementButtons->AddChildToHorizontalBox(PlacementConfirmButton)->SetPadding(FMargin(2.f, 0.f));
	PlacementButtons->AddChildToHorizontalBox(MakeButton(LOCTEXT("PlaceCancel", "취소"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->CancelPlacement();
		}
	}, true, 14))->SetPadding(FMargin(2.f, 0.f));
	ActionSink = nullptr;
	PlacementActionBox->AddChildToVerticalBox(PlacementButtons)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
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

	// 화면을 덮는 전용 창
	WindowOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WindowOverlay"));
	WindowOverlay->SetBrushColor(CozyHud::OverlayColor);
	WindowOverlay->SetHorizontalAlignment(HAlign_Center);
	WindowOverlay->SetVerticalAlignment(VAlign_Center);
	if (UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(WindowOverlay))
	{
		OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		OverlaySlot->SetOffsets(FMargin(0.f));
	}

	UBorder* WindowFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WindowFrame"));
	WindowFrameWidget = WindowFrame;
	WindowFrame->SetBrushColor(CozyHud::WindowColor);
	WindowFrame->SetPadding(FMargin(32.f, 24.f));
	WindowOverlay->SetContent(WindowFrame);

	UVerticalBox* WindowBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WindowBody"));
	WindowFrame->SetContent(WindowBody);

	WindowTitle = MakeText(FText::GetEmpty(), 26, CozyHud::AccentText);
	WindowBody->AddChildToVerticalBox(WindowTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	// 내용이 길면(후신소 창처럼 시설이 늘어나는 창) 화면 안에서 스크롤
	WindowContentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WindowContentSize"));
	UScrollBox* WindowScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("WindowScroll"));
	WindowContentSize->SetContent(WindowScroll);
	WindowContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WindowContent"));
	WindowScroll->AddChild(WindowContent);
	WindowBody->AddChildToVerticalBox(WindowContentSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));

	ActionSink = &FrameActions; // 닫기 버튼은 창을 다시 그려도 유지
	UButton* CloseButton = MakeButton(LOCTEXT("Close", "닫기 (Esc)"), [this]() { CloseWindow(); });
	ActionSink = nullptr;
	if (UVerticalBoxSlot* CloseSlot = WindowBody->AddChildToVerticalBox(CloseButton))
	{
		CloseSlot->SetHorizontalAlignment(HAlign_Right);
	}
	WindowOverlay->SetVisibility(ESlateVisibility::Collapsed);

	// 디버그 메뉴 (F1)
	DebugPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DebugPanel"));
	DebugPanel->SetBrushColor(CozyHud::PanelColor);
	DebugPanel->SetPadding(FMargin(12.f));
	DebugContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugContent"));
	DebugPanel->SetContent(DebugContent);
	if (UCanvasPanelSlot* DebugSlot = RootCanvas->AddChildToCanvas(DebugPanel))
	{
		DebugSlot->SetZOrder(6);
		DebugSlot->SetAnchors(FAnchors(1.f, 0.f));
		DebugSlot->SetAlignment(FVector2D(1.f, 0.f));
		DebugSlot->SetPosition(FVector2D(-12.f, 56.f));
		DebugSlot->SetAutoSize(true);
	}
	DebugPanel->SetVisibility(ESlateVisibility::Collapsed);
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

	if (bPlacementMode)
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

	// 아이콘을 선택한 시설 위에 따라 붙임
	if (IconSlot && IconFacility.IsValid() && IconBox->GetVisibility() != ESlateVisibility::Collapsed)
	{
		FVector2D ScreenPosition;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), IconFacility->GetIconAnchorLocation(), ScreenPosition, false))
		{
			IconSlot->SetPosition(ScreenPosition);
		}
	}
}

UCozyEstateSubsystem* UCozyHudWidget::GetEstate() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
}

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
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!PlacementStoredBox || !Estate || !bPlacementMode)
	{
		return;
	}
	const TArray<FGuid> Stored = Estate->GetStoredFacilities();
	if (Stored == PlacementStoredIds && PlacementStoredBox->GetChildrenCount() > 0)
	{
		return;
	}
	PlacementStoredIds = Stored;
	PlacementStoredBox->ClearChildren();
	PlacementActions.Reset();
	PlacementStoredBox->AddChildToVerticalBox(MakeText(FText::Format(LOCTEXT("StoredTitle", "보관함 ({0})"), FText::AsNumber(Stored.Num())), 15));
	if (Stored.Num() == 0)
	{
		PlacementStoredBox->AddChildToVerticalBox(MakeText(LOCTEXT("StoredEmpty", "비어 있음"), 14, CozyHud::MutedText));
	}
	ActionSink = &PlacementActions;
	for (const FGuid& Id : Stored)
	{
		const FCozyFacilityState* Facility = Estate->FindFacility(Id);
		FString Label = FString::Printf(TEXT("꺼내기: %s Lv%d"), *Estate->GetFacilityDisplayName(Id).ToString(), Facility ? Facility->Level : 0);
		if (Facility && !Facility->SelectedCropId.IsNone())
		{
			if (const FCozyCropRow* Crop = Estate->GetCropDef(Facility->SelectedCropId))
			{
				Label += FString::Printf(TEXT(" · %s"), *Crop->DisplayName.ToString());
			}
		}
		PlacementStoredBox->AddChildToVerticalBox(MakeButton(FText::FromString(Label), [this, Id]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->BeginPlacement(Id);
			}
		}, true, 14))->SetPadding(FMargin(0.f, 2.f));
	}
	ActionSink = nullptr;
}

void UCozyHudWidget::UpdatePlacementLive()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	ACozyFacilityActor* Actor = Estate ? Estate->GetPlacementActor() : nullptr;
	if (!Estate || !Estate->IsPlacing() || !Actor || !PlacementActionBox)
	{
		if (PlacementActionBox)
		{
			PlacementActionBox->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	FText PlaceReason;
	const bool bCanPlace = Estate->CanPlaceFacility(Estate->GetPlacementId(), Estate->GetPlacementCoord(), Estate->GetPlacementRotation(), PlaceReason);
	FText StoreReason;
	const bool bCanStore = !Estate->IsPlacementFromStorage() && Estate->CanStoreFacility(Estate->GetPlacementId(), StoreReason);
	FString Status = bCanPlace ? TEXT("놓을 수 있습니다") : PlaceReason.ToString();
	if (!Estate->IsPlacementFromStorage() && !bCanStore)
	{
		Status += TEXT("\n보관 불가: ") + StoreReason.ToString();
	}
	PlacementStatusText->SetText(FText::FromString(Status));
	PlacementStatusText->SetColorAndOpacity(FSlateColor(bCanPlace ? FLinearColor(0.55f, 1.f, 0.6f) : CozyHud::WarningText));
	PlacementConfirmButton->SetIsEnabled(bCanPlace);
	PlacementStoreButton->SetIsEnabled(bCanStore);
	PlacementActionBox->SetVisibility(ESlateVisibility::Visible);
	FVector2D ScreenPosition;
	if (PlacementActionSlot && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Actor->GetIconAnchorLocation(), ScreenPosition, false))
	{
		PlacementActionSlot->SetPosition(ScreenPosition);
	}
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

void UCozyHudWidget::ToggleNagayaWindow()
{
	if (WindowKind == ECozyWindowKind::Nagaya)
	{
		CloseWindow();
		return;
	}
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate)
	{
		return;
	}
	for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
	{
		const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility.DefinitionId);
		if (Def && Def->Functions.Contains(ECozyFacilityFunction::ResidentHousing))
		{
			HideFacilityIcons();
			OpenWindow(ECozyWindowKind::Nagaya, Facility.InstanceId);
			return;
		}
	}
}

void UCozyHudWidget::ToggleStorageWindow()
{
	if (WindowKind == ECozyWindowKind::Storage)
	{
		CloseWindow();
		return;
	}
	HideFacilityIcons();
	OpenWindow(ECozyWindowKind::Storage, FGuid());
}

void UCozyHudWidget::ShowPendingOfflineReport()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (Estate && Estate->GetPendingOfflineReport() && WindowKind != ECozyWindowKind::OfflineReport)
	{
		HideFacilityIcons();
		OpenWindow(ECozyWindowKind::OfflineReport, FGuid());
	}
}

void UCozyHudWidget::BuildOfflineReportContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	const FCozyOfflineReport* Report = Estate ? Estate->GetPendingOfflineReport() : nullptr;
	WindowTitle->SetText(LOCTEXT("OfflineTitle", "방치 보상"));
	if (!Report)
	{
		WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("OfflineNone", "새로 받은 방치 보상이 없습니다"), 15, CozyHud::MutedText));
		return;
	}
	WindowContent->AddChildToVerticalBox(MakeText(FText::Format(LOCTEXT("OfflineAway", "자리를 비운 동안: {0}{1}"),
		CozyHud::DurationText(Report->AwaySeconds),
		Report->bClamped ? FText::Format(LOCTEXT("OfflineClamp", " · 최대 {0}까지만 정산"), CozyHud::DurationText(Report->AppliedSeconds)) : FText::GetEmpty()), 16))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	if (Report->Lines.Num() == 0)
	{
		WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("OfflineNothing", "그동안 쌓인 것이 없습니다 (주민 배치·미수령 공간을 확인해 주세요)"), 15, CozyHud::MutedText));
	}
	for (const FText& Line : Report->Lines)
	{
		WindowContent->AddChildToVerticalBox(MakeText(Line, 15))->SetPadding(FMargin(0.f, 2.f));
	}
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("OfflineNote", "생산·가공품은 각 시설의 미수령분에 쌓였습니다 · 재화는 이미 받았습니다"), 13, CozyHud::MutedText))->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	WindowContent->AddChildToVerticalBox(MakeButton(LOCTEXT("OfflineOk", "확인"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DismissOfflineReport();
		}
		CloseWindow();
	}))->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
}

void UCozyHudWidget::HandleEstateChanged(bool bStructural)
{
	RefreshTopBar();
	ShowPendingOfflineReport();
	if (bStructural)
	{
		RefreshPlacementPanel();
	}
	if (bStructural)
	{
		RefreshNameLabels();
	}
	// 숫자만 바뀌는 창(시설 정보 · 창고)은 다시 만들지 않고 글자만 바꿈 → 버튼 클릭이 끊기지 않음
	if (WindowKind == ECozyWindowKind::FacilityInfo)
	{
		UpdateFacilityInfoLive();
	}
	else if (WindowKind == ECozyWindowKind::Storage)
	{
		UpdateStorageLive();
	}
	else if (WindowKind == ECozyWindowKind::Processing)
	{
		UpdateProcessingLive();
	}
	else if (WindowKind == ECozyWindowKind::Sales)
	{
		UpdateSalesLive();
	}
	else if (WindowKind == ECozyWindowKind::Upgrade)
	{
		UpdateUpgradeLive();
	}
	else if (WindowKind == ECozyWindowKind::FieldManagement)
	{
		UpdateFieldManagementLive();
	}
	if (!bStructural)
	{
		return;
	}
	// 업그레이드 완료로 레벨이 바뀌면 열린 창의 제목(Lv)도 바꿈 · 창은 다시 만들지 않음
	if (WindowKind == ECozyWindowKind::Processing || WindowKind == ECozyWindowKind::Sales || WindowKind == ECozyWindowKind::FacilityInfo)
	{
		UCozyEstateSubsystem* EstateNow = GetEstate();
		const FCozyFacilityState* Facility = EstateNow ? EstateNow->FindFacility(WindowFacility) : nullptr;
		const FCozyFacilityRow* Def = Facility ? EstateNow->GetFacilityDef(Facility->DefinitionId) : nullptr;
		if (Def && WindowTitle)
		{
			if (WindowKind == ECozyWindowKind::Processing)
			{
				WindowTitle->SetText(FText::Format(LOCTEXT("ProcTitle", "{0}  Lv.{1} — 가공"), Def->DisplayName, FText::AsNumber(Facility->Level)));
			}
			else if (WindowKind == ECozyWindowKind::Sales)
			{
				WindowTitle->SetText(FText::Format(LOCTEXT("SellTitle", "{0}  Lv.{1} — 판매"), Def->DisplayName, FText::AsNumber(Facility->Level)));
			}
			else if (Def->ManagerFacilityId.IsNone())
			{
				WindowTitle->SetText(FText::Format(LOCTEXT("InfoTitle", "{0}  Lv.{1}"), Def->DisplayName, FText::AsNumber(Facility->Level)));
			}
		}
	}
	// 버튼 구성이 바뀌는 창(나가야: 배치 가능 여부)만 다시 그림
	if (WindowKind == ECozyWindowKind::Nagaya || WindowKind == ECozyWindowKind::Placeholder)
	{
		RefreshWindow();
	}
	// 후신소 창은 구조가 바뀐 경우(시설 추가 · 레벨 변경으로 다음 단계의 조건 '이동' 버튼이 달라짐)에 다시 그림
	if (WindowKind == ECozyWindowKind::Upgrade)
	{
		RefreshWindow();
	}
	if (bDebugVisible)
	{
		RefreshDebugPanel();
	}
}

void UCozyHudWidget::UpdateFacilityInfoLive()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate)
	{
		return;
	}
	const FCozyProductionView View = Estate->GetProductionView(WindowFacility);

	// 작물 · 공통 성장 효과 · 작물 선택 버튼 (해금·변경 가능 여부는 서비스 판단)
	if (const FCozyFacilityState* CropFacility = Estate->FindFacility(WindowFacility))
	{
		const FCozyCropRow* Crop = Estate->GetCropDef(CropFacility->SelectedCropId);
		if (InfoCropText)
		{
			InfoCropText->SetText(FText::Format(LOCTEXT("CropLine", "키우는 작물: {0}"), Crop ? Crop->DisplayName : LOCTEXT("NoneCrop", "없음")));
		}
		FString Reasons;
		for (int32 Index = 0; Index < InfoCropIds.Num() && Index < InfoCropButtons.Num(); ++Index)
		{
			const FName CropId = InfoCropIds[Index];
			const FCozyCropRow* Option = Estate->GetCropDef(CropId);
			FText Reason;
			const bool bCan = Estate->CanSelectCrop(WindowFacility, CropId, Reason);
			const bool bCurrent = CropFacility->SelectedCropId == CropId;
			const bool bUnlocked = Estate->IsCropUnlocked(CropId);
			UButton* Button = InfoCropButtons[Index];
			Button->SetIsEnabled(bCan);
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
			{
				const FText Name = Option ? Option->DisplayName : FText::FromName(CropId);
				Label->SetText(bCurrent ? FText::Format(LOCTEXT("CropBtnCurrent", "{0} (키우는 중)"), Name)
					: bUnlocked ? Name : FText::Format(LOCTEXT("CropBtnLocked", "{0} (잠김)"), Name));
			}
			if (!bCurrent && !bCan)
			{
				Reasons += (Reasons.IsEmpty() ? TEXT("") : TEXT("\n")) + Reason.ToString();
			}
		}
		if (InfoCropReasonText)
		{
			InfoCropReasonText->SetText(FText::FromString(Reasons));
			InfoCropReasonText->SetVisibility(Reasons.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		}
	}
	if (InfoGrowthText)
	{
		FNumberFormattingOptions Fmt;
		Fmt.MinimumFractionalDigits = 1;
		Fmt.MaximumFractionalDigits = 2;
		const FText Multiplier = FText::AsNumber(View.SpeedMultiplier, &Fmt);
		InfoGrowthText->SetText(!View.bHasGrowthSource ? FText::GetEmpty()
			: View.GrowthSourceLevel > 0
				? FText::Format(LOCTEXT("GrowthLine", "공통 관리 효과: 생산 속도 ×{0} ({1} Lv{2}) · 단계가 오르면 다음 주기부터 적용"), Multiplier, View.GrowthSourceName, FText::AsNumber(View.GrowthSourceLevel))
				: FText::Format(LOCTEXT("GrowthLineNone", "공통 관리 효과: 기본 속도 ×{0} ({1}이 아직 없습니다)"), Multiplier, View.GrowthSourceName));
		InfoGrowthText->SetVisibility(View.bHasGrowthSource ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (InfoResidentText)
	{
		const FCozyFacilityState* Facility = Estate->FindFacility(WindowFacility);
		const FCozyFacilityRow* Def = Facility ? Estate->GetFacilityDef(Facility->DefinitionId) : nullptr;
		if (Facility && Def)
		{
			FString Names;
			for (const FGuid& ResidentId : Facility->AssignedResidents)
			{
				Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Estate->GetResidentDisplayName(ResidentId).ToString();
			}
			InfoResidentText->SetText(FText::Format(LOCTEXT("ResidentLine", "주민: {0}  ({1}/{2})"),
				Names.IsEmpty() ? LOCTEXT("NoneResident", "없음") : FText::FromString(Names),
				FText::AsNumber(Facility->AssignedResidents.Num()), FText::AsNumber(Def->MaxResidents)));
		}
	}
	if (InfoStatusText)
	{
		InfoStatusText->SetText(View.Status);
		InfoStatusText->SetColorAndOpacity(FSlateColor(View.bWorking ? FLinearColor::White : CozyHud::WarningText));
	}
	if (InfoProgressBar)
	{
		InfoProgressBar->SetPercent(View.Progress01);
		InfoProgressBar->SetVisibility(View.bWorking ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (InfoUnclaimedText)
	{
		InfoUnclaimedText->SetText(FText::Format(LOCTEXT("UnclaimedLine", "미수령 {0}: {1} / {2}"), View.UnclaimedItemName, FText::AsNumber(View.UnclaimedAmount), FText::AsNumber(View.UnclaimedCapacity)));
	}
	if (InfoCollectButton)
	{
		InfoCollectButton->SetIsEnabled(View.UnclaimedAmount > 0);
	}
	if (InfoStorageText)
	{
		const bool bNoSpace = View.StorageCap > 0 && View.StoredAmount >= View.StorageCap;
		const FText StorageLine = FText::Format(LOCTEXT("InfoStorage", "창고 {0} {1}/{2}"), View.UnclaimedItemName, FText::AsNumber(View.StoredAmount), FText::AsNumber(View.StorageCap));
		// 현재 상태: 공간이 생기면 '공간 없음' 안내는 바로 사라지고 수령 가능 수량으로 바뀜
		const bool bWarn = bNoSpace && View.UnclaimedAmount > 0;
		InfoStorageText->SetText(bNoSpace
			? FText::Format(LOCTEXT("InfoStorageFull", "현재 {0} — 받을 수 있는 공간이 없습니다"), StorageLine)
			: FText::Format(LOCTEXT("InfoStorageSpace", "현재 {0} · 지금 수령 가능 {1}개"), StorageLine, FText::AsNumber(View.CollectableNow)));
		InfoStorageText->SetColorAndOpacity(FSlateColor(bWarn ? CozyHud::WarningText : CozyHud::MutedText));
	}
	if (InfoRemainingText)
	{
		InfoRemainingText->SetText(View.CycleSeconds > 0.f
			? FText::Format(LOCTEXT("NextHarvestCycle", "다음 생산 완료까지 {0}초 · 이번 주기 {1}초"), FText::AsNumber(FMath::CeilToInt(View.RemainingSeconds)), CozyHud::MultiplierText(View.CycleSeconds))
			: FText::Format(LOCTEXT("NextHarvest", "다음 생산 완료까지 {0}초"), FText::AsNumber(FMath::CeilToInt(View.RemainingSeconds))));
		InfoRemainingText->SetVisibility(View.bWorking ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------------------
// 공통 위젯

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
// 위쪽 재화 표시줄

void UCozyHudWidget::RefreshTopBar()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!TopBarBox || !Estate)
	{
		return;
	}
	TopBarBox->ClearChildren();

	for (const FName& ItemId : Estate->GetHudItems())
	{
		const FCozyItemRow* Item = Estate->GetItemDef(ItemId);
		const FText Line = FText::Format(LOCTEXT("HudItem", "{0} {1}"), Item ? Item->DisplayName : FText::FromName(ItemId), FText::AsNumber(Estate->GetAmount(ItemId)));
		TopBarBox->AddChildToHorizontalBox(MakeText(Line, 18))->SetPadding(FMargin(0.f, 0.f, 28.f, 0.f));
	}

	// 게임 시간 · 낮/밤 (PC 현지 시각 기준)
	const int32 Total = FMath::FloorToInt32(Estate->GetGameSeconds());
	const FText Clock = FText::Format(LOCTEXT("Clock", "PC {0} · {1} · 게임 시간 {2}:{3} {4}"),
		FText::FromString(FDateTime::Now().ToString(TEXT("%H:%M"))),
		Estate->IsNight() ? LOCTEXT("Night", "밤") : LOCTEXT("Day", "낮"),
		FText::AsNumber(Total / 60),
		FText::FromString(FString::Printf(TEXT("%02d"), Total % 60)),
		CozyHud::TimeScaleText(Estate->GetTimeScale()));
	if (ClockText)
	{
		ClockText->SetText(Clock);
	}
}

// ---------------------------------------------------------------------------
// 기능 아이콘

void UCozyHudWidget::ShowFacilityIcons(ACozyFacilityActor* FacilityActor)
{
	IconFacility = FacilityActor;
	RefreshIcons();
}

void UCozyHudWidget::HideFacilityIcons()
{
	IconFacility.Reset();
	if (IconBox)
	{
		IconBox->ClearChildren();
		IconBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	IconActions.Reset();
}

void UCozyHudWidget::RefreshIcons()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!IconBox || !Estate || !IconFacility.IsValid())
	{
		HideFacilityIcons();
		return;
	}

	const FGuid FacilityId = IconFacility->GetFacilityId();
	const FCozyFacilityState* Facility = Estate->FindFacility(FacilityId);
	const FCozyFacilityRow* Def = Facility ? Estate->GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		HideFacilityIcons();
		return;
	}

	IconBox->ClearChildren();
	IconActions.Reset();
	ActionSink = &IconActions;

	auto AddIcon = [this](const FText& Label, TFunction<void()> OnClick)
	{
		IconBox->AddChildToHorizontalBox(MakeButton(Label, MoveTemp(OnClick), true, 14))->SetPadding(FMargin(4.f, 0.f));
	};

	// 기능 아이콘은 시설 정의의 기능 조합에서 만든다 (시설 이름으로 분기하지 않음)
	for (const ECozyFacilityFunction Function : Def->Functions)
	{
		switch (Function)
		{
		case ECozyFacilityFunction::Production:
			AddIcon(LOCTEXT("IconInfo", "정보"), [this, FacilityId]() { OpenWindow(ECozyWindowKind::FacilityInfo, FacilityId); });
			break;
		case ECozyFacilityFunction::ResidentHousing:
			AddIcon(LOCTEXT("IconNagaya", "주민 관리"), [this, FacilityId]() { OpenWindow(ECozyWindowKind::Nagaya, FacilityId); });
			break;
		case ECozyFacilityFunction::Processing:
			AddIcon(LOCTEXT("IconProcess", "가공"), [this, FacilityId]() { OpenWindow(ECozyWindowKind::Processing, FacilityId); });
			break;
		case ECozyFacilityFunction::Sales:
			AddIcon(LOCTEXT("IconSell", "판매"), [this, FacilityId]() { OpenWindow(ECozyWindowKind::Sales, FacilityId); });
			break;
		case ECozyFacilityFunction::UpgradeQueue:
			AddIcon(LOCTEXT("IconUpgrade", "업그레이드"), [this, FacilityId]() { OpenWindow(ECozyWindowKind::Upgrade, FacilityId); });
			break;
		case ECozyFacilityFunction::ShrineCore:
			AddIcon(LOCTEXT("IconShrine", "신사"), [this, FacilityId]() { PlaceholderLabel = LOCTEXT("PhShrine", "신사"); OpenWindow(ECozyWindowKind::Placeholder, FacilityId); });
			break;
		case ECozyFacilityFunction::FieldManagement:
			AddIcon(LOCTEXT("IconFieldMgmt", "밭 관리"), [this, FacilityId]() { OpenWindow(ECozyWindowKind::FieldManagement, FacilityId); });
			break;
		default:
			break;
		}
	}

	// 주민 배치 칸이 있는 시설은 주민 아이콘 → 나가야 창으로 바로 감 (클릭 수 줄이기)
	if (Def->MaxResidents > 0)
	{
		AddIcon(LOCTEXT("IconResident", "주민"), [this, FacilityId]() { OpenWindow(ECozyWindowKind::Nagaya, FGuid(), FacilityId); });
	}

	ActionSink = nullptr;
	IconBox->SetVisibility(ESlateVisibility::Visible);
}

// ---------------------------------------------------------------------------
// 전용 창

void UCozyHudWidget::OpenWindow(ECozyWindowKind Kind, const FGuid& FacilityId, const FGuid& TargetFacility)
{
	// 시설 메뉴(가공·주민 등)에서 상세 창으로 넘어가면 메뉴 아이콘과 선택을 지운다
	// → 창과 겹치지 않고, 창을 닫은 뒤 같은 시설을 다시 누르면 메뉴가 새로 뜬다
	if (ACozyRealmEstatePlayerController* Controller = Cast<ACozyRealmEstatePlayerController>(GetOwningPlayer()))
	{
		Controller->ClearSelection();
	}
	HideFacilityIcons();
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

void UCozyHudWidget::CloseWindow()
{
	// 방치 보상 창은 닫기도 '확인'과 같게 처리 · 안 하면 남은 보고서 때문에 바로 다시 열려 닫기가 안 되는 것처럼 보임
	if (WindowKind == ECozyWindowKind::OfflineReport)
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
	if (WindowContent)
	{
		WindowContent->ClearChildren();
	}
	WindowActions.Reset();
}

void UCozyHudWidget::RefreshWindow()
{
	if (!WindowContent)
	{
		return;
	}
	WindowContent->ClearChildren();
	WindowActions.Reset();

	// 편집 가능한 창 화면이 지정돼 있으면 예전 창 틀 대신 그 화면을 띄운다
	if (UCozyUiScreen* Screen = GetWindowScreen(WindowKind))
	{
		Screen->ContextFacility = WindowFacility;
		if (WindowOverlay->GetContent() != Screen)
		{
			WindowOverlay->SetContent(Screen);
		}
		Screen->RefreshTheme();
		Screen->RefreshValues();
		return;
	}
	if (WindowFrameWidget && WindowOverlay->GetContent() != WindowFrameWidget)
	{
		WindowOverlay->SetContent(WindowFrameWidget);
	}
	if (WindowContentSize)
	{
		// 제목·닫기 버튼·여백을 뺀 높이까지만 (화면 단위 = 픽셀 / DPI 배율)
		const float Scale = FMath::Max(0.1f, UWidgetLayoutLibrary::GetViewportScale(this));
		const float ViewHeight = UWidgetLayoutLibrary::GetViewportSize(this).Y / Scale;
		WindowContentSize->SetMaxDesiredHeight(FMath::Max(200.f, ViewHeight - 190.f));
	}
	InfoProgressBar = nullptr;
	InfoStatusText = nullptr;
	InfoRemainingText = nullptr;
	InfoUnclaimedText = nullptr;
	InfoStorageText = nullptr;
	InfoResidentText = nullptr;
	InfoFeedbackText = nullptr;
	InfoCollectButton = nullptr;
	StorageAmountTexts.Reset();
	StorageSpaceTexts.Reset();
	StorageItemIds.Reset();
	ProcSelectedText = nullptr;
	ProcRunsText = nullptr;
	ProcMinusButton = nullptr;
	ProcPlusButton = nullptr;
	ProcMaxButton = nullptr;
	ProcMaxText = nullptr;
	ProcSummaryText = nullptr;
	ProcBlockText = nullptr;
	ProcStorageNoteText = nullptr;
	ProcStartButton = nullptr;
	ProcStartLabel = nullptr;
	ProcQueueRow = nullptr;
	ProcQueueText = nullptr;
	ProcSlotStatusTexts.Reset();
	ProcSlotBars.Reset();
	ProcSlotTimeTexts.Reset();
	ProcSlotCancelButtons.Reset();
	ProcConfirmBox = nullptr;
	ProcConfirmText = nullptr;
	SellListTexts.Reset();
	SellListButtons.Reset();
	SellListItemIds.Reset();
	SellSelectedText = nullptr;
	SellAmountText = nullptr;
	SellMinusButton = nullptr;
	SellPlusButton = nullptr;
	SellMaxButton = nullptr;
	SellSummaryText = nullptr;
	SellBlockText = nullptr;
	SellButton = nullptr;
	UpgradeSlotText = nullptr;
	UpgradeJobTexts.Reset();
	UpgradeJobBars.Reset();
	UpgradeTitleTexts.Reset();
	UpgradeDetailTexts.Reset();
	UpgradeBlockTexts.Reset();
	UpgradeButtons.Reset();
	UpgradeFacilityIds.Reset();
	FieldMgmtText = nullptr;
	InfoCropText = nullptr;
	InfoGrowthText = nullptr;
	InfoCropButtons.Reset();
	InfoCropReasonText = nullptr;
	InfoCropIds.Reset();
	ActionSink = &WindowActions;

	switch (WindowKind)
	{
	case ECozyWindowKind::FacilityInfo:
		BuildFacilityInfoContent();
		break;
	case ECozyWindowKind::Nagaya:
		BuildNagayaContent();
		break;
	case ECozyWindowKind::Placeholder:
		BuildPlaceholderContent();
		break;
	case ECozyWindowKind::Storage:
		BuildStorageContent();
		break;
	case ECozyWindowKind::Processing:
		BuildProcessingContent();
		break;
	case ECozyWindowKind::Sales:
		BuildSalesContent();
		break;
	case ECozyWindowKind::Upgrade:
		BuildUpgradeContent();
		break;
	case ECozyWindowKind::FieldManagement:
		BuildFieldManagementContent();
		break;
	case ECozyWindowKind::OfflineReport:
		BuildOfflineReportContent();
		break;
	default:
		break;
	}

	if (!LastFeedback.IsEmpty() && WindowKind != ECozyWindowKind::FacilityInfo && WindowKind != ECozyWindowKind::Processing && WindowKind != ECozyWindowKind::Sales && WindowKind != ECozyWindowKind::Upgrade)
	{
		WindowContent->AddChildToVerticalBox(MakeText(LastFeedback, 16, CozyHud::WarningText))->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
	ActionSink = nullptr;
}

FName UCozyHudWidget::GetWindowName(ECozyWindowKind Kind)
{
	switch (Kind)
	{
	case ECozyWindowKind::FacilityInfo: return TEXT("FacilityInfo");
	case ECozyWindowKind::Nagaya: return TEXT("Nagaya");
	case ECozyWindowKind::Placeholder: return TEXT("Placeholder");
	case ECozyWindowKind::Storage: return TEXT("Storage");
	case ECozyWindowKind::Processing: return TEXT("Processing");
	case ECozyWindowKind::Sales: return TEXT("Sales");
	case ECozyWindowKind::Upgrade: return TEXT("Upgrade");
	case ECozyWindowKind::FieldManagement: return TEXT("FieldManagement");
	case ECozyWindowKind::OfflineReport: return TEXT("OfflineReport");
	default: return NAME_None;
	}
}

UCozyUiScreen* UCozyHudWidget::GetWindowScreen(ECozyWindowKind Kind)
{
	const FName Name = GetWindowName(Kind);
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
	}
	return Screen;
}

void UCozyHudWidget::BuildFacilityInfoContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	const FCozyFacilityState* Facility = Estate ? Estate->FindFacility(WindowFacility) : nullptr;
	const FCozyFacilityRow* Def = Facility ? Estate->GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		CloseWindow();
		return;
	}

	// 관리 시설의 효과를 받는 시설(밭)은 개별 레벨이 없음 (D39)
	WindowTitle->SetText(Def->ManagerFacilityId.IsNone()
		? FText::Format(LOCTEXT("InfoTitle", "{0}  Lv.{1}"), Def->DisplayName, FText::AsNumber(Facility->Level))
		: Def->DisplayName);

	// 키우는 작물 · 작물 선택 (해금된 작물만 고를 수 있음 · D37) · UpdateFacilityInfoLive가 갱신
	InfoCropText = MakeText(FText::GetEmpty());
	WindowContent->AddChildToVerticalBox(InfoCropText);
	if (Def->bProductionItemSelectable && Def->ProductionItems.Num() > 0)
	{
		UHorizontalBox* CropRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		CropRow->AddChildToHorizontalBox(MakeText(LOCTEXT("CropPick", "작물 바꾸기:"), 14, CozyHud::MutedText))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		const FGuid FacilityId = WindowFacility;
		for (const FName& CropId : Def->ProductionItems)
		{
			UButton* CropButton = MakeButton(FText::FromName(CropId), [this, FacilityId, CropId]()
			{
				if (UCozyEstateSubsystem* EstateNow = GetEstate())
				{
					FText Message;
					EstateNow->SelectCrop(FacilityId, CropId, Message);
					SetFeedback(Message);
				}
			}, false, 14);
			CropRow->AddChildToHorizontalBox(CropButton)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			InfoCropIds.Add(CropId);
			InfoCropButtons.Add(CropButton);
		}
		WindowContent->AddChildToVerticalBox(CropRow)->SetPadding(FMargin(0.f, 4.f));
		InfoCropReasonText = MakeText(FText::GetEmpty(), 13, CozyHud::MutedText);
		WindowContent->AddChildToVerticalBox(InfoCropReasonText);
	}
	InfoGrowthText = MakeText(FText::GetEmpty(), 14, CozyHud::AccentText);
	WindowContent->AddChildToVerticalBox(InfoGrowthText)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	// 배치된 주민 (UpdateFacilityInfoLive가 최신 값으로 갱신)
	InfoResidentText = MakeText(FText::GetEmpty());
	WindowContent->AddChildToVerticalBox(InfoResidentText)->SetPadding(FMargin(0.f, 6.f));

	// 생산 상태 · 다음 수확까지 (1초마다 UpdateFacilityInfoLive가 숫자만 갱신)
	const FCozyProductionView View = Estate->GetProductionView(WindowFacility);
	if (View.bHasProduction)
	{
		InfoStatusText = MakeText(View.Status, 18);
		WindowContent->AddChildToVerticalBox(InfoStatusText)->SetPadding(FMargin(0.f, 10.f, 0.f, 4.f));
		InfoProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		InfoProgressBar->SetFillColorAndOpacity(CozyHud::AccentText);
		WindowContent->AddChildToVerticalBox(InfoProgressBar)->SetPadding(FMargin(0.f, 4.f));
		InfoRemainingText = MakeText(FText::GetEmpty(), 15, CozyHud::MutedText);
		WindowContent->AddChildToVerticalBox(InfoRemainingText);

		// 미수령 생산물 · 수확(수령) 버튼 (🙋 D27: 수령해야 창고 재료가 됨)
		if (View.UnclaimedCapacity > 0)
		{
			UHorizontalBox* CollectRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			InfoUnclaimedText = MakeText(FText::GetEmpty(), 18, CozyHud::AccentText);
			CollectRow->AddChildToHorizontalBox(InfoUnclaimedText)->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
			const FGuid FacilityId = WindowFacility;
			const FText CollectLabel = View.CollectButtonLabel.IsEmpty() ? LOCTEXT("CollectDefault", "수령") : View.CollectButtonLabel;
			InfoCollectButton = MakeButton(CollectLabel, [this, FacilityId]() { HandleCollectClicked(FacilityId); }, View.UnclaimedAmount > 0, 16);
			CollectRow->AddChildToHorizontalBox(InfoCollectButton);
			WindowContent->AddChildToVerticalBox(CollectRow)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
			// 그 생산물의 창고 상태 · 지금 받을 수 있는 수량 (미수령분은 창고에 포함하지 않음)
			InfoStorageText = MakeText(FText::GetEmpty(), 15, CozyHud::MutedText);
			WindowContent->AddChildToVerticalBox(InfoStorageText)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		UpdateFacilityInfoLive();
	}

	if (Def->MaxResidents > 0)
	{
		const FGuid FacilityId = WindowFacility;
		WindowContent->AddChildToVerticalBox(MakeButton(LOCTEXT("GoNagaya", "주민 배치 (나가야로)"), [this, FacilityId]()
		{
			OpenWindow(ECozyWindowKind::Nagaya, FGuid(), FacilityId);
		}))->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	}

	// 방금 한 일 (클릭 결과) · 현재 상태와 구분해 아래에 따로
	InfoFeedbackText = MakeText(LastFeedback.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("JustDid", "방금 한 일 — {0}"), LastFeedback), 15, CozyHud::AccentText);
	InfoFeedbackText->SetVisibility(LastFeedback.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	WindowContent->AddChildToVerticalBox(InfoFeedbackText)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	UpdateFacilityInfoLive();
}

void UCozyHudWidget::BuildNagayaContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate)
	{
		return;
	}

	WindowTitle->SetText(LOCTEXT("NagayaTitle", "나가야 — 주민 관리"));

	// 바로가기로 열었으면 배치할 시설을 위에 표시
	if (WindowTargetFacility.IsValid())
	{
		WindowContent->AddChildToVerticalBox(MakeText(FText::Format(LOCTEXT("TargetLine", "배치할 시설: {0}"), Estate->GetFacilityDisplayName(WindowTargetFacility)), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	// 주민을 받을 수 있는 시설 목록 (배치 칸이 있는 시설)
	TArray<FGuid> SlotFacilities;
	for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
	{
		const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility.DefinitionId);
		if (Def && Def->MaxResidents > 0 && (!WindowTargetFacility.IsValid() || Facility.InstanceId == WindowTargetFacility))
		{
			SlotFacilities.Add(Facility.InstanceId);
		}
	}

	for (const FCozyResidentState& Resident : Estate->GetState().Residents)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->AddChildToHorizontalBox(MakeText(Estate->GetResidentDisplayName(Resident.InstanceId), 18))->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));

		const bool bAssigned = Resident.AssignedFacility.IsValid();
		const FText Where = bAssigned
			? FText::Format(LOCTEXT("AssignedAt", "배치: {0}"), Estate->GetFacilityDisplayName(Resident.AssignedFacility))
			: LOCTEXT("Unassigned", "미배치 (나가야)");
		Row->AddChildToHorizontalBox(MakeText(Where, 16, CozyHud::MutedText))->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));

		const FGuid ResidentId = Resident.InstanceId;
		for (const FGuid& FacilityId : SlotFacilities)
		{
			if (FacilityId == Resident.AssignedFacility)
			{
				continue;
			}
			FText Reason;
			const bool bCanAccept = Estate->CanAcceptResident(FacilityId, Reason);
			const FText FacilityName = Estate->GetFacilityDisplayName(FacilityId);
			const FText Label = bAssigned
				? (WindowTargetFacility.IsValid() ? LOCTEXT("MoveHere", "이 시설로 옮기기") : FText::Format(LOCTEXT("MoveTo", "{0}(으)로 옮기기"), FacilityName))
				: (WindowTargetFacility.IsValid() ? LOCTEXT("AssignHere", "이 시설에 배치") : FText::Format(LOCTEXT("AssignTo", "{0}에 배치"), FacilityName));
			Row->AddChildToHorizontalBox(MakeButton(Label, [this, ResidentId, FacilityId]()
			{
				if (UCozyEstateSubsystem* EstateNow = GetEstate())
				{
					FText FailReason;
					LastFeedback = EstateNow->AssignResident(ResidentId, FacilityId, FailReason) ? FText::GetEmpty() : FailReason;
					RefreshWindow();
				}
			}, bCanAccept))->SetPadding(FMargin(4.f, 0.f));
		}

		if (bAssigned)
		{
			// 🙋 필요 인원이 부족해지면 진행 중인 생산 주기는 취소되고 0초부터 다시 시작
			Row->AddChildToHorizontalBox(MakeButton(LOCTEXT("Unassign", "배치 해제"), [this, ResidentId]()
			{
				if (UCozyEstateSubsystem* EstateNow = GetEstate())
				{
					FText FailReason;
					LastFeedback = EstateNow->UnassignResident(ResidentId, FailReason) ? FText::GetEmpty() : FailReason;
					RefreshWindow();
				}
			}))->SetPadding(FMargin(4.f, 0.f));
		}

		WindowContent->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 6.f));
	}
}

void UCozyHudWidget::BuildPlaceholderContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	const FText FacilityName = Estate ? Estate->GetFacilityDisplayName(WindowFacility) : FText::GetEmpty();
	WindowTitle->SetText(FText::Format(LOCTEXT("PhTitle", "{0} — {1}"), FacilityName, PlaceholderLabel));
	WindowContent->AddChildToVerticalBox(MakeText(FText::Format(LOCTEXT("PhBody", "{0} 창은 해당 기능을 만들 때 채웁니다. (지금은 전용 창 틀)"), PlaceholderLabel), 16, CozyHud::MutedText));
}

void UCozyHudWidget::BuildStorageContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate)
	{
		return;
	}
	WindowTitle->SetText(LOCTEXT("StorageTitle", "창고"));
	WindowContent->AddChildToVerticalBox(MakeText(FText::Format(LOCTEXT("StorageRule", "재료마다 따로 최대 {0}개까지 보관합니다 (모든 재료 합계가 아님) · 시설의 미수령 생산물은 포함하지 않습니다"),
		FText::AsNumber(Estate->GetConfig().StorageCapPerItem)), 14, CozyHud::MutedText))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	for (const FName& ItemId : Estate->GetStorageItems())
	{
		const FCozyItemRow* Item = Estate->GetItemDef(ItemId);
		if (!Item)
		{
			continue;
		}
		const int32 Amount = Estate->GetAmount(ItemId);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(MakeText(Item->DisplayName, 17));
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetPadding(FMargin(0.f, 0.f, 30.f, 0.f));

		UTextBlock* AmountText = MakeText(FText::AsNumber(Amount), 17);
		Row->AddChildToHorizontalBox(AmountText)->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		UTextBlock* SpaceText = MakeText(FText::GetEmpty(), 15, CozyHud::MutedText);
		Row->AddChildToHorizontalBox(SpaceText);
		StorageItemIds.Add(ItemId);
		StorageAmountTexts.Add(AmountText);
		StorageSpaceTexts.Add(SpaceText);
		WindowContent->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));
	}
	UpdateStorageLive();
}

void UCozyHudWidget::UpdateStorageLive()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate)
	{
		return;
	}
	const int32 Cap = Estate->GetConfig().StorageCapPerItem;
	for (int32 Index = 0; Index < StorageItemIds.Num(); ++Index)
	{
		const FName ItemId = StorageItemIds[Index];
		const FCozyItemRow* Item = Estate->GetItemDef(ItemId);
		UTextBlock* AmountText = StorageAmountTexts.IsValidIndex(Index) ? StorageAmountTexts[Index].Get() : nullptr;
		UTextBlock* SpaceText = StorageSpaceTexts.IsValidIndex(Index) ? StorageSpaceTexts[Index].Get() : nullptr;
		if (!Item || !AmountText || !SpaceText)
		{
			continue;
		}
		const int32 Amount = Estate->GetAmount(ItemId);
		if (Item->Category == ECozyItemCategory::Material)
		{
			const int32 Space = Estate->GetStorageSpace(ItemId);
			AmountText->SetText(FText::Format(LOCTEXT("StorageAmount", "{0} / {1}"), FText::AsNumber(Amount), FText::AsNumber(Cap)));
			SpaceText->SetText(Space > 0
				? FText::Format(LOCTEXT("StorageSpace", "더 받을 수 있음 {0}개"), FText::AsNumber(Space))
				: LOCTEXT("StorageNoSpace", "가득 참 — 받을 수 있는 공간이 없습니다"));
			SpaceText->SetColorAndOpacity(FSlateColor(Space > 0 ? CozyHud::MutedText : CozyHud::WarningText));
		}
		else
		{
			AmountText->SetText(FText::AsNumber(Amount));
			SpaceText->SetText(LOCTEXT("StorageCurrency", "재화 · 한도 없음"));
		}
	}
}

// ---------------------------------------------------------------------------
// 공통 가공 창 (모든 제작 시설이 같은 창 · 레시피·재료·완료품·시간은 데이터 · D32)

void UCozyHudWidget::SetFeedback(const FText& Message)
{
	LastFeedback = Message;
	if (InfoFeedbackText)
	{
		InfoFeedbackText->SetText(FText::Format(LOCTEXT("JustDid", "방금 한 일 — {0}"), LastFeedback));
		InfoFeedbackText->SetVisibility(LastFeedback.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UCozyHudWidget::HandleCollectClicked(const FGuid& FacilityId)
{
	if (UCozyEstateSubsystem* Estate = GetEstate())
	{
		// 클릭 결과는 '방금 한 일'로 따로 · 현재 상태 줄은 알림을 받아 최신 값으로 바뀜
		SetFeedback(Estate->CollectUnclaimed(FacilityId).Message);
	}
}

void UCozyHudWidget::BuildProcessingContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	const FCozyFacilityState* Facility = Estate ? Estate->FindFacility(WindowFacility) : nullptr;
	const FCozyFacilityRow* Def = Facility ? Estate->GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		CloseWindow();
		return;
	}
	const FGuid FacilityId = WindowFacility;
	WindowTitle->SetText(FText::Format(LOCTEXT("ProcTitle", "{0}  Lv.{1} — 가공"), Def->DisplayName, FText::AsNumber(Facility->Level)));

	// 배치된 주민 (UpdateProcessingLive가 갱신)
	InfoResidentText = MakeText(FText::GetEmpty());
	WindowContent->AddChildToVerticalBox(InfoResidentText)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// ① 레시피 선택
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("ProcStep1", "① 레시피 선택"), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 4.f));
	UHorizontalBox* RecipeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const FName& RecipeId : Estate->GetFacilityRecipes(FacilityId))
	{
		const FCozyRecipeRow* Recipe = Estate->GetRecipeDef(RecipeId);
		if (!Recipe)
		{
			continue;
		}
		const FCozyItemRow* Output = Estate->GetItemDef(Recipe->OutputItem);
		const FText Label = FText::Format(Recipe->bTestOnly ? LOCTEXT("RecipeBtnTest", "{0} ×{1} (테스트)") : LOCTEXT("RecipeBtn", "{0} ×{1}"),
			Output ? Output->DisplayName : FText::FromName(Recipe->OutputItem), FText::AsNumber(Recipe->OutputAmount));
		RecipeRow->AddChildToHorizontalBox(MakeButton(Label, [this, RecipeId]()
		{
			ProcSelectedRecipe = RecipeId;
			ProcSelectedRuns = 1;
			UpdateProcessingLive();
		}, true, 14))->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
	WindowContent->AddChildToVerticalBox(RecipeRow)->SetPadding(FMargin(0.f, 2.f));
	ProcSelectedText = MakeText(FText::GetEmpty(), 15);
	WindowContent->AddChildToVerticalBox(ProcSelectedText)->SetPadding(FMargin(0.f, 4.f));

	// ② 제작 수량 (레시피 실행 횟수)
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("ProcStep2", "② 제작 횟수"), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 8.f, 0.f, 2.f));
	UHorizontalBox* RunsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ProcMinusButton = MakeButton(LOCTEXT("RunsMinus", " − "), [this]()
	{
		ProcSelectedRuns = FMath::Max(1, ProcSelectedRuns - 1);
		UpdateProcessingLive();
	}, true, 16);
	RunsRow->AddChildToHorizontalBox(ProcMinusButton)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	ProcRunsText = MakeText(FText::GetEmpty(), 18);
	RunsRow->AddChildToHorizontalBox(ProcRunsText)->SetVerticalAlignment(VAlign_Center);
	ProcPlusButton = MakeButton(LOCTEXT("RunsPlus", " + "), [this]()
	{
		++ProcSelectedRuns;
		UpdateProcessingLive();
	}, true, 16);
	RunsRow->AddChildToHorizontalBox(ProcPlusButton)->SetPadding(FMargin(8.f, 0.f, 8.f, 0.f));
	ProcMaxButton = MakeButton(LOCTEXT("RunsMax", "최대"), [this, FacilityId]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			ProcSelectedRuns = FMath::Max(1, EstateNow->GetRecipeQuote(FacilityId, ProcSelectedRecipe, 1).MaxRuns);
			UpdateProcessingLive();
		}
	}, true, 14);
	RunsRow->AddChildToHorizontalBox(ProcMaxButton);
	WindowContent->AddChildToVerticalBox(RunsRow)->SetPadding(FMargin(0.f, 2.f));
	ProcMaxText = MakeText(FText::GetEmpty(), 14, CozyHud::MutedText);
	WindowContent->AddChildToVerticalBox(ProcMaxText)->SetPadding(FMargin(0.f, 2.f));

	// ③ 재료·시간 확인 → 시작
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("ProcStep3", "③ 재료·시간 확인"), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 8.f, 0.f, 2.f));
	ProcSummaryText = MakeText(FText::GetEmpty(), 15);
	WindowContent->AddChildToVerticalBox(ProcSummaryText)->SetPadding(FMargin(0.f, 2.f));
	ProcBlockText = MakeText(FText::GetEmpty(), 15, CozyHud::WarningText);
	WindowContent->AddChildToVerticalBox(ProcBlockText)->SetPadding(FMargin(0.f, 2.f));
	ProcStorageNoteText = MakeText(FText::GetEmpty(), 15, CozyHud::AccentText);
	WindowContent->AddChildToVerticalBox(ProcStorageNoteText)->SetPadding(FMargin(0.f, 2.f));
	ProcStartButton = MakeButton(LOCTEXT("ProcStart", "제작 시작"), [this, FacilityId]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			// 시작 직전에 서비스가 조건을 다시 확인 · 실패하면 아무것도 바뀌지 않음
			FText Message;
			EstateNow->StartProcessing(FacilityId, ProcSelectedRecipe, ProcSelectedRuns, Message);
			SetFeedback(Message);
			UpdateProcessingLive();
		}
	}, false, 16);
	ProcStartLabel = Cast<UTextBlock>(ProcStartButton->GetContent());
	WindowContent->AddChildToVerticalBox(ProcStartButton)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	// 가공 칸 (데이터의 동시 가공 수만큼 · 슬롯이 늘어도 같은 코드)
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("ProcSlots", "진행 중인 가공"), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 14.f, 0.f, 2.f));
	for (int32 SlotIndex = 0; SlotIndex < FMath::Max(1, Def->ProcessingSlots); ++SlotIndex)
	{
		UHorizontalBox* SlotHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* StatusText = MakeText(FText::GetEmpty(), 16);
		UHorizontalBoxSlot* StatusSlot = SlotHeader->AddChildToHorizontalBox(StatusText);
		StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StatusSlot->SetVerticalAlignment(VAlign_Center);
		UButton* CancelButton = MakeButton(LOCTEXT("ProcCancel", "취소"), [this, FacilityId, SlotIndex]()
		{
			UCozyEstateSubsystem* EstateNow = GetEstate();
			TArray<FCozyProcessingJobView> Jobs = EstateNow ? EstateNow->GetProcessingJobs(FacilityId) : TArray<FCozyProcessingJobView>();
			Jobs.RemoveAll([](const FCozyProcessingJobView& Each) { return Each.bQueued; });
			if (!Jobs.IsValidIndex(SlotIndex))
			{
				return;
			}
			// 🙋 취소 전 안내와 확인 (D29) · 안내 숫자는 UpdateProcessingLive가 최신 값으로 갱신
			ProcPendingCancelJob = Jobs[SlotIndex].JobId;
			UpdateProcessingLive();
		}, true, 14);
		SlotHeader->AddChildToHorizontalBox(CancelButton)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		WindowContent->AddChildToVerticalBox(SlotHeader)->SetPadding(FMargin(0.f, 4.f, 0.f, 2.f));

		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Bar->SetFillColorAndOpacity(CozyHud::AccentText);
		WindowContent->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0.f, 2.f));
		UTextBlock* TimeText = MakeText(FText::GetEmpty(), 14, CozyHud::MutedText);
		WindowContent->AddChildToVerticalBox(TimeText);

		ProcSlotStatusTexts.Add(StatusText);
		ProcSlotBars.Add(Bar);
		ProcSlotTimeTexts.Add(TimeText);
		ProcSlotCancelButtons.Add(CancelButton);
	}

	// 제작 추가로 기다리는 작업 (가공 칸을 차지하지 않음 · 앞 작업이 끝나면 이어서)
	ProcQueueRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ProcQueueText = MakeText(FText::GetEmpty(), 15, CozyHud::MutedText);
	UHorizontalBoxSlot* QueueTextSlot = ProcQueueRow->AddChildToHorizontalBox(ProcQueueText);
	QueueTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	QueueTextSlot->SetVerticalAlignment(VAlign_Center);
	ProcQueueRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("ProcQueueCancel", "추가분 취소"), [this, FacilityId]()
	{
		UCozyEstateSubsystem* EstateNow = GetEstate();
		const TArray<FCozyProcessingJobView> Jobs = EstateNow ? EstateNow->GetProcessingJobs(FacilityId) : TArray<FCozyProcessingJobView>();
		// 가장 나중에 추가한 대기 작업부터 취소 (확인 안내는 기존 취소와 같음)
		for (int32 Index = Jobs.Num() - 1; Index >= 0; --Index)
		{
			if (Jobs[Index].bQueued)
			{
				ProcPendingCancelJob = Jobs[Index].JobId;
				break;
			}
		}
		UpdateProcessingLive();
	}, true, 14))->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	WindowContent->AddChildToVerticalBox(ProcQueueRow)->SetPadding(FMargin(0.f, 4.f));

	// 취소 확인 (평소에는 숨김)
	ProcConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ProcConfirmText = MakeText(FText::GetEmpty(), 15, CozyHud::WarningText);
	ProcConfirmBox->AddChildToVerticalBox(ProcConfirmText)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	UHorizontalBox* ConfirmRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ConfirmRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("ProcConfirmYes", "취소 확정"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			FText Message;
			EstateNow->CancelProcessing(ProcPendingCancelJob, Message);
			ProcPendingCancelJob.Invalidate();
			SetFeedback(Message);
			UpdateProcessingLive();
		}
	}, true, 14))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	ConfirmRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("ProcConfirmNo", "계속 제작"), [this]()
	{
		ProcPendingCancelJob.Invalidate();
		UpdateProcessingLive();
	}, true, 14));
	ProcConfirmBox->AddChildToVerticalBox(ConfirmRow);
	WindowContent->AddChildToVerticalBox(ProcConfirmBox)->SetPadding(FMargin(0.f, 6.f));

	// 미수령 완료품 · 수령 (생산과 같은 수령 규칙 · D31)
	UHorizontalBox* CollectRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	InfoUnclaimedText = MakeText(FText::GetEmpty(), 17, CozyHud::AccentText);
	CollectRow->AddChildToHorizontalBox(InfoUnclaimedText)->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	const FText CollectLabel = Def->CollectButtonLabel.IsEmpty() ? LOCTEXT("CollectDefault", "수령") : Def->CollectButtonLabel;
	InfoCollectButton = MakeButton(CollectLabel, [this, FacilityId]() { HandleCollectClicked(FacilityId); }, false, 16);
	CollectRow->AddChildToHorizontalBox(InfoCollectButton);
	WindowContent->AddChildToVerticalBox(CollectRow)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	InfoStorageText = MakeText(FText::GetEmpty(), 14, CozyHud::MutedText);
	WindowContent->AddChildToVerticalBox(InfoStorageText)->SetPadding(FMargin(0.f, 2.f));

	if (Def->MaxResidents > 0)
	{
		WindowContent->AddChildToVerticalBox(MakeButton(LOCTEXT("GoNagaya", "주민 배치 (나가야로)"), [this, FacilityId]()
		{
			OpenWindow(ECozyWindowKind::Nagaya, FGuid(), FacilityId);
		}, true, 14))->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}

	// 방금 한 일 (클릭 결과) · 현재 상태와 구분
	InfoFeedbackText = MakeText(FText::GetEmpty(), 15, CozyHud::AccentText);
	WindowContent->AddChildToVerticalBox(InfoFeedbackText)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	SetFeedback(LastFeedback);

	UpdateProcessingLive();
}

void UCozyHudWidget::UpdateProcessingLive()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	const FCozyFacilityState* Facility = Estate ? Estate->FindFacility(WindowFacility) : nullptr;
	const FCozyFacilityRow* Def = Facility ? Estate->GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def || !ProcRunsText)
	{
		return;
	}

	// 주민
	if (InfoResidentText)
	{
		FString Names;
		for (const FGuid& ResidentId : Facility->AssignedResidents)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Estate->GetResidentDisplayName(ResidentId).ToString();
		}
		InfoResidentText->SetText(FText::Format(LOCTEXT("ProcResidentLine", "주민: {0}  ({1}/{2} · 필요 {3}명)"),
			Names.IsEmpty() ? LOCTEXT("NoneResident", "없음") : FText::FromString(Names),
			FText::AsNumber(Facility->AssignedResidents.Num()), FText::AsNumber(Def->MaxResidents), FText::AsNumber(FMath::Max(1, Def->MinResidents))));
	}

	// 선택한 레시피 · 수량 · 견적 (계산은 서비스의 공통 함수)
	FCozyRecipeQuote Quote = Estate->GetRecipeQuote(WindowFacility, ProcSelectedRecipe, ProcSelectedRuns);
	// 창을 연 동안 재고·수령·작업 완료로 최대가 줄면 선택 수량을 새 최대로 낮춘다 (최대 0이면 1로 두고 시작은 막힘 · 이유 표시)
	if (ProcSelectedRuns > FMath::Max(1, Quote.MaxRuns))
	{
		ProcSelectedRuns = FMath::Max(1, Quote.MaxRuns);
		Quote = Estate->GetRecipeQuote(WindowFacility, ProcSelectedRecipe, ProcSelectedRuns);
	}
	if (const FCozyRecipeRow* Recipe = Estate->GetRecipeDef(ProcSelectedRecipe))
	{
		FString InputsText;
		for (const TPair<FName, int32>& Input : Recipe->Inputs)
		{
			const FCozyItemRow* Item = Estate->GetItemDef(Input.Key);
			InputsText += FString::Printf(TEXT("%s%s %d"), InputsText.IsEmpty() ? TEXT("") : TEXT(" + "), *(Item ? Item->DisplayName : FText::FromName(Input.Key)).ToString(), Input.Value);
		}
		ProcSelectedText->SetText(FText::Format(LOCTEXT("ProcSelected", "선택: {0} → {1} {2}개 · 1회 {3}"),
			FText::FromString(InputsText), Quote.OutputName, FText::AsNumber(Quote.OutputPerRun), CozyHud::DurationText(Quote.SecondsPerRun)));
	}
	else
	{
		ProcSelectedText->SetText(LOCTEXT("ProcNoRecipe", "이 시설에서 만들 수 있는 레시피가 없습니다"));
	}

	ProcRunsText->SetText(Quote.bAppend
		? FText::Format(LOCTEXT("ProcRunsAppend", "추가 {0}회  →  {1} {2}개   (현재 남은 {3}회 뒤에 이어서)"), FText::AsNumber(ProcSelectedRuns), Quote.OutputName, FText::AsNumber(Quote.TotalOutput), FText::AsNumber(Quote.CurrentRemainingRuns))
		: FText::Format(LOCTEXT("ProcRuns", "{0}회  →  {1} {2}개"), FText::AsNumber(ProcSelectedRuns), Quote.OutputName, FText::AsNumber(Quote.TotalOutput)));
	ProcMinusButton->SetIsEnabled(ProcSelectedRuns > 1);
	ProcPlusButton->SetIsEnabled(ProcSelectedRuns < Quote.MaxRuns);
	ProcMaxButton->SetIsEnabled(Quote.MaxRuns > 0 && ProcSelectedRuns != Quote.MaxRuns);
	ProcMaxText->SetText(FText::Format(Quote.bAppend
			? LOCTEXT("ProcMaxAppend", "지금 최대 추가 {0}회  (재료로 {1}회분 · 진행 중·대기 작업이 확보한 공간을 뺀 남은 미수령 공간으로 {2}회분)")
			: LOCTEXT("ProcMax", "지금 최대 {0}회  (재료로 {1}회분 · 남은 미수령 공간으로 {2}회분)"),
		FText::AsNumber(Quote.MaxRuns), FText::AsNumber(Quote.MaxByMaterials), FText::AsNumber(Quote.MaxBySpace)));

	FString InputsNeed;
	for (const FCozyRecipeQuote::FInput& Input : Quote.Inputs)
	{
		const FCozyItemRow* Item = Estate->GetItemDef(Input.ItemId);
		InputsNeed += FString::Printf(TEXT("%s%s %d개 (보유 %d개)"), InputsNeed.IsEmpty() ? TEXT("") : TEXT(", "), *(Item ? Item->DisplayName : FText::FromName(Input.ItemId)).ToString(), Input.Need, Input.Have);
	}
	ProcSummaryText->SetText(FText::Format(LOCTEXT("ProcSummary", "필요 재료: {0}\n완료품: {1} {2}개 (1회마다 {3}개씩 시설에 쌓임)\n예상 시간: {4} (주민 부족으로 멈춘 시간은 제외)"),
		FText::FromString(InputsNeed), Quote.OutputName, FText::AsNumber(Quote.TotalOutput), FText::AsNumber(Quote.OutputPerRun), CozyHud::DurationText(Quote.TotalSeconds)));
	ProcBlockText->SetText(Quote.BlockReason);
	ProcBlockText->SetVisibility(Quote.BlockReason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ProcStartButton->SetIsEnabled(Quote.bCanStart);
	if (ProcStartLabel)
	{
		ProcStartLabel->SetText(Quote.bAppend ? LOCTEXT("ProcAppend", "제작 추가") : LOCTEXT("ProcStart", "제작 시작"));
	}

	// 🙋 완료품의 공용 창고가 가득해도 시작은 허용하고 안내만 (D35 · 확인 팝업 없음)
	if (ProcStorageNoteText)
	{
		const FCozyRecipeRow* SelectedRecipe = Estate->GetRecipeDef(ProcSelectedRecipe);
		const bool bStorageFull = SelectedRecipe && Estate->GetStorageSpace(SelectedRecipe->OutputItem) <= 0;
		ProcStorageNoteText->SetText(bStorageFull
			? FText::Format(LOCTEXT("ProcStorageFullNote", "창고에 {0} 공간이 없습니다. 완성품은 시설에 보관되며, 수령하려면 창고 공간이 필요합니다"), Quote.OutputName)
			: FText::GetEmpty());
		ProcStorageNoteText->SetVisibility(bStorageFull ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 가공 칸 (진행·일시 정지 작업만) · 대기 작업은 아래 대기 줄에
	const TArray<FCozyProcessingJobView> AllJobs = Estate->GetProcessingJobs(WindowFacility);
	TArray<FCozyProcessingJobView> Jobs;
	FString QueueLine;
	int32 QueueRuns = 0;
	for (const FCozyProcessingJobView& Each : AllJobs)
	{
		if (Each.bQueued)
		{
			QueueLine += (QueueLine.IsEmpty() ? TEXT("") : TEXT(" · ")) + FString::Printf(TEXT("%s %d회(%d개)"), *Each.OutputName.ToString(), Each.TotalRuns, Each.TotalRuns * Each.OutputPerRun);
			QueueRuns += Each.TotalRuns;
		}
		else
		{
			Jobs.Add(Each);
		}
	}
	if (ProcQueueRow && ProcQueueText)
	{
		ProcQueueText->SetText(FText::Format(LOCTEXT("ProcQueueLine", "제작 추가 대기 {0}회: {1} — 지금 작업이 끝나면 이어서 시작 (가공 칸을 차지하지 않음)"), FText::AsNumber(QueueRuns), FText::FromString(QueueLine)));
		ProcQueueRow->SetVisibility(QueueRuns > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	for (int32 SlotIndex = 0; SlotIndex < ProcSlotStatusTexts.Num(); ++SlotIndex)
	{
		UTextBlock* StatusText = ProcSlotStatusTexts[SlotIndex];
		UProgressBar* Bar = ProcSlotBars[SlotIndex];
		UTextBlock* TimeText = ProcSlotTimeTexts[SlotIndex];
		UButton* CancelButton = ProcSlotCancelButtons[SlotIndex];
		if (Jobs.IsValidIndex(SlotIndex))
		{
			const FCozyProcessingJobView& Job = Jobs[SlotIndex];
			StatusText->SetText(Job.bPaused
				? FText::Format(LOCTEXT("SlotPaused", "{0} — {1} · {2}/{3}회 완성 · 현재 회차 진행도 유지"), Job.Status, Job.OutputName, FText::AsNumber(Job.CompletedRuns), FText::AsNumber(Job.TotalRuns))
				: Job.Status);
			StatusText->SetColorAndOpacity(FSlateColor(Job.bPaused ? CozyHud::WarningText : FLinearColor::White));
			Bar->SetPercent(Job.RunProgress01);
			Bar->SetVisibility(ESlateVisibility::Visible);
			TimeText->SetText(FText::Format(LOCTEXT("SlotTime", "이번 회 남은 {0} · 전체 남은 {1} · 완성 {2}/{3}회 ({4}개)"),
				CozyHud::DurationText(Job.RunRemainingSeconds), CozyHud::DurationText(Job.TotalRemainingSeconds),
				FText::AsNumber(Job.CompletedRuns), FText::AsNumber(Job.TotalRuns), FText::AsNumber(Job.CompletedRuns * Job.OutputPerRun)));
			TimeText->SetVisibility(ESlateVisibility::Visible);
			CancelButton->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			StatusText->SetText(FText::Format(LOCTEXT("SlotEmpty", "가공 칸 {0}: 비어 있음"), FText::AsNumber(SlotIndex + 1)));
			StatusText->SetColorAndOpacity(FSlateColor(CozyHud::MutedText));
			Bar->SetVisibility(ESlateVisibility::Collapsed);
			TimeText->SetVisibility(ESlateVisibility::Collapsed);
			CancelButton->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 취소 확인: 안내를 띄운 동안 회차가 끝나도 숫자가 맞도록 매번 다시 씀 · 대상 작업이 이미 끝났으면 닫음
	const FCozyProcessingJobView* PendingJob = ProcPendingCancelJob.IsValid()
		? AllJobs.FindByPredicate([this](const FCozyProcessingJobView& Job) { return Job.JobId == ProcPendingCancelJob; })
		: nullptr;
	const bool bPendingAlive = PendingJob != nullptr;
	if (!bPendingAlive)
	{
		ProcPendingCancelJob.Invalidate();
	}
	else if (ProcConfirmText)
	{
		ProcConfirmText->SetText(FText::Format(LOCTEXT("ProcConfirm", "취소하면 투입한 재료를 돌려받을 수 없습니다.\n완성된 {0}회분({1}개)은 시설에 남고, 남은 {2}회는 완료품 없이 종료됩니다. 취소할까요?"),
			FText::AsNumber(PendingJob->CompletedRuns), FText::AsNumber(PendingJob->CompletedRuns * PendingJob->OutputPerRun), FText::AsNumber(PendingJob->TotalRuns - PendingJob->CompletedRuns)));
	}
	if (ProcConfirmBox)
	{
		ProcConfirmBox->SetVisibility(bPendingAlive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 미수령 · 창고 상태 (생산과 같은 공통 계산)
	const FCozyUnclaimedView Unclaimed = Estate->GetUnclaimedView(WindowFacility);
	if (InfoUnclaimedText)
	{
		InfoUnclaimedText->SetText(Unclaimed.Reserved > 0
			? FText::Format(LOCTEXT("ProcUnclaimedReserved", "미수령 {0}: {1} / {2}  · 제작 중 확보 {3}"), Unclaimed.ItemName, FText::AsNumber(Unclaimed.Amount), FText::AsNumber(Unclaimed.Capacity), FText::AsNumber(Unclaimed.Reserved))
			: FText::Format(LOCTEXT("ProcUnclaimed", "미수령 {0}: {1} / {2}"), Unclaimed.ItemName, FText::AsNumber(Unclaimed.Amount), FText::AsNumber(Unclaimed.Capacity)));
	}
	if (InfoCollectButton)
	{
		InfoCollectButton->SetIsEnabled(Unclaimed.Amount > 0);
	}
	if (InfoStorageText)
	{
		if (Unclaimed.Amount <= 0)
		{
			InfoStorageText->SetText(LOCTEXT("ProcStorageEmpty", "현재 수령할 완료품이 없습니다"));
			InfoStorageText->SetColorAndOpacity(FSlateColor(CozyHud::MutedText));
		}
		else
		{
			const bool bNoSpace = Unclaimed.StorageCap > 0 && Unclaimed.StoredAmount >= Unclaimed.StorageCap;
			const FText StorageLine = FText::Format(LOCTEXT("InfoStorage", "창고 {0} {1}/{2}"), Unclaimed.ItemName, FText::AsNumber(Unclaimed.StoredAmount), FText::AsNumber(Unclaimed.StorageCap));
			InfoStorageText->SetText(bNoSpace
				? FText::Format(LOCTEXT("InfoStorageFull", "현재 {0} — 받을 수 있는 공간이 없습니다"), StorageLine)
				: FText::Format(LOCTEXT("InfoStorageSpace", "현재 {0} · 지금 수령 가능 {1}개"), StorageLine, FText::AsNumber(Unclaimed.CollectableNow)));
			InfoStorageText->SetColorAndOpacity(FSlateColor(bNoSpace ? CozyHud::WarningText : CozyHud::MutedText));
		}
	}
}

// ---------------------------------------------------------------------------
// 판매소 창 (재료 선택 → 수량 → 받을 재화 확인 → 판매)

void UCozyHudWidget::BuildSalesContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	const FCozyFacilityState* Facility = Estate ? Estate->FindFacility(WindowFacility) : nullptr;
	const FCozyFacilityRow* Def = Facility ? Estate->GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		CloseWindow();
		return;
	}
	const FGuid FacilityId = WindowFacility;
	WindowTitle->SetText(FText::Format(LOCTEXT("SellTitle", "{0}  Lv.{1} — 판매"), Def->DisplayName, FText::AsNumber(Facility->Level)));
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("SellRule", "창고에 있는 재료를 팔아 골드를 받습니다 · 시설의 미수령분은 수령한 뒤에 팔 수 있습니다"), 14, CozyHud::MutedText))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// ① 재료 선택 (판매가 0인 재료는 '판매 불가'로 버튼이 꺼짐)
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("SellStep1", "① 팔 재료"), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 4.f));
	for (const FName& ItemId : Estate->GetSaleListItems())
	{
		const FCozyItemRow* Item = Estate->GetItemDef(ItemId);
		if (!Item)
		{
			continue;
		}
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UButton* PickButton = MakeButton(Item->DisplayName, [this, ItemId]()
		{
			SellSelectedItem = ItemId;
			SellSelectedAmount = 1;
			UpdateSalesLive();
		}, Item->SellPrice > 0, 14);
		Row->AddChildToHorizontalBox(PickButton)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		UTextBlock* LineText = MakeText(FText::GetEmpty(), 14, CozyHud::MutedText);
		Row->AddChildToHorizontalBox(LineText)->SetVerticalAlignment(VAlign_Center);
		WindowContent->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
		SellListItemIds.Add(ItemId);
		SellListTexts.Add(LineText);
		SellListButtons.Add(PickButton);
	}
	SellSelectedText = MakeText(FText::GetEmpty(), 15);
	WindowContent->AddChildToVerticalBox(SellSelectedText)->SetPadding(FMargin(0.f, 4.f));

	// ② 수량
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("SellStep2", "② 판매 수량"), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 8.f, 0.f, 2.f));
	UHorizontalBox* AmountRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SellMinusButton = MakeButton(LOCTEXT("SellMinus", " − "), [this]()
	{
		SellSelectedAmount = FMath::Max(1, SellSelectedAmount - 1);
		UpdateSalesLive();
	}, true, 16);
	AmountRow->AddChildToHorizontalBox(SellMinusButton)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	SellAmountText = MakeText(FText::GetEmpty(), 18);
	AmountRow->AddChildToHorizontalBox(SellAmountText)->SetVerticalAlignment(VAlign_Center);
	SellPlusButton = MakeButton(LOCTEXT("SellPlus", " + "), [this]()
	{
		++SellSelectedAmount;
		UpdateSalesLive();
	}, true, 16);
	AmountRow->AddChildToHorizontalBox(SellPlusButton)->SetPadding(FMargin(8.f, 0.f, 8.f, 0.f));
	SellMaxButton = MakeButton(LOCTEXT("SellMax", "전부"), [this, FacilityId]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			SellSelectedAmount = FMath::Max(1, EstateNow->GetSellQuote(FacilityId, SellSelectedItem, 1).MaxAmount);
			UpdateSalesLive();
		}
	}, true, 14);
	AmountRow->AddChildToHorizontalBox(SellMaxButton);
	WindowContent->AddChildToVerticalBox(AmountRow)->SetPadding(FMargin(0.f, 2.f));

	// ③ 확인 → 판매
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("SellStep3", "③ 받을 골드 확인"), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 8.f, 0.f, 2.f));
	SellSummaryText = MakeText(FText::GetEmpty(), 15);
	WindowContent->AddChildToVerticalBox(SellSummaryText)->SetPadding(FMargin(0.f, 2.f));
	SellBlockText = MakeText(FText::GetEmpty(), 15, CozyHud::WarningText);
	WindowContent->AddChildToVerticalBox(SellBlockText)->SetPadding(FMargin(0.f, 2.f));
	SellButton = MakeButton(LOCTEXT("SellDo", "판매"), [this, FacilityId]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			// 판매 직전에 서비스가 조건을 다시 확인 · 실패하면 아무것도 바뀌지 않음
			FText Message;
			EstateNow->SellItem(FacilityId, SellSelectedItem, SellSelectedAmount, Message);
			SetFeedback(Message);
			UpdateSalesLive();
		}
	}, false, 16);
	WindowContent->AddChildToVerticalBox(SellButton)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	// 방금 한 일 (클릭 결과) · 현재 상태와 구분
	InfoFeedbackText = MakeText(FText::GetEmpty(), 15, CozyHud::AccentText);
	WindowContent->AddChildToVerticalBox(InfoFeedbackText)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	SetFeedback(LastFeedback);

	UpdateSalesLive();
}

void UCozyHudWidget::UpdateSalesLive()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate || !SellAmountText)
	{
		return;
	}

	// 재료 목록: 창고 보유량 · 개당 가격
	for (int32 Index = 0; Index < SellListItemIds.Num(); ++Index)
	{
		const FCozyItemRow* Item = Estate->GetItemDef(SellListItemIds[Index]);
		UTextBlock* LineText = SellListTexts.IsValidIndex(Index) ? SellListTexts[Index].Get() : nullptr;
		if (!Item || !LineText)
		{
			continue;
		}
		const int32 Have = Estate->GetAmount(SellListItemIds[Index]);
		LineText->SetText(Item->SellPrice > 0
			? FText::Format(LOCTEXT("SellLine", "창고 {0}개 · 1개 {1}골드{2}"), FText::AsNumber(Have), FText::AsNumber(Item->SellPrice),
				SellListItemIds[Index] == SellSelectedItem ? LOCTEXT("SellLineSelected", "  ◀ 선택") : FText::GetEmpty())
			: FText::Format(LOCTEXT("SellLineBlocked", "창고 {0}개 · 판매 불가"), FText::AsNumber(Have)));
	}

	const FCozySellQuote Quote = Estate->GetSellQuote(WindowFacility, SellSelectedItem, SellSelectedAmount);
	SellSelectedText->SetText(SellSelectedItem.IsNone()
		? LOCTEXT("SellNothing", "팔 수 있는 재료가 없습니다")
		: FText::Format(LOCTEXT("SellSelected", "선택: {0} · 1개 {1}{2}"), Quote.ItemName, FText::AsNumber(Quote.UnitPrice), Quote.CurrencyName));
	SellAmountText->SetText(FText::Format(LOCTEXT("SellAmount", "{0}개"), FText::AsNumber(SellSelectedAmount)));
	SellMinusButton->SetIsEnabled(SellSelectedAmount > 1);
	SellPlusButton->SetIsEnabled(SellSelectedAmount < Quote.MaxAmount);
	SellMaxButton->SetIsEnabled(Quote.MaxAmount > 0 && SellSelectedAmount != Quote.MaxAmount);
	SellSummaryText->SetText(FText::Format(LOCTEXT("SellSummary", "{0} {1}개 × {2} = {3} {4}  (창고 보유 {5}개 · 판매 후 {6}개)"),
		Quote.ItemName, FText::AsNumber(SellSelectedAmount), FText::AsNumber(Quote.UnitPrice), FText::AsNumber(Quote.TotalPrice), Quote.CurrencyName,
		FText::AsNumber(Quote.MaxAmount), FText::AsNumber(FMath::Max(0, Quote.MaxAmount - SellSelectedAmount))));
	SellBlockText->SetText(Quote.BlockReason);
	SellBlockText->SetVisibility(Quote.BlockReason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	SellButton->SetIsEnabled(Quote.bCanSell);
}

// ---------------------------------------------------------------------------
// 후신소 창 (신사·시설·밭 관리 시설 업그레이드 · 공통 성장 처리 · D9·D25·D36~D40)

void UCozyHudWidget::BuildUpgradeContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate)
	{
		return;
	}
	WindowTitle->SetText(FText::Format(LOCTEXT("UpTitle", "{0} — 업그레이드"), Estate->GetFacilityDisplayName(WindowFacility)));
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("UpRule", "신사·시설 업그레이드는 여기서 합니다 · 시작 비용은 시작할 때 내고, 시간이 지나면 완료됩니다"), 14, CozyHud::MutedText))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// 진행 중인 업그레이드 (후신소 동시 작업 수만큼)
	UpgradeSlotText = MakeText(FText::GetEmpty(), 17, CozyHud::AccentText);
	WindowContent->AddChildToVerticalBox(UpgradeSlotText)->SetPadding(FMargin(0.f, 2.f));
	UpgradeSpeedRows.Reset();
	UpgradeSpeedCountTexts.Reset();
	UpgradeSpeedPreviewTexts.Reset();
	UpgradeSpeedApplyButtons.Reset();
	const int32 SlotCount = FMath::Max(1, Estate->GetUpgradeSlotCount());
	if (UpgradeSpeedCounts.Num() != SlotCount)
	{
		UpgradeSpeedCounts.Init(1, SlotCount);
	}
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UTextBlock* JobText = MakeText(FText::GetEmpty(), 15);
		WindowContent->AddChildToVerticalBox(JobText)->SetPadding(FMargin(0.f, 2.f));
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Bar->SetFillColorAndOpacity(CozyHud::AccentText);
		WindowContent->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0.f, 2.f));
		UpgradeJobTexts.Add(JobText);
		UpgradeJobBars.Add(Bar);

		// 시간 단축: 부적 수 고르기 → 미리보기(아래 글자) → '단축 확정'
		UHorizontalBox* SpeedRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		SpeedRow->AddChildToHorizontalBox(MakeText(LOCTEXT("SpeedLabel", "시간 부적:"), 14, CozyHud::MutedText))->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		auto ChangeCount = [this, SlotIndex](int32 Delta, bool bToMax)
		{
			UCozyEstateSubsystem* EstateNow = GetEstate();
			const TArray<FCozyUpgradeJobView> JobsNow = EstateNow ? EstateNow->GetUpgradeJobs() : TArray<FCozyUpgradeJobView>();
			if (!JobsNow.IsValidIndex(SlotIndex) || !UpgradeSpeedCounts.IsValidIndex(SlotIndex))
			{
				return;
			}
			const FCozySpeedupQuote Quote = EstateNow->GetSpeedupQuote(JobsNow[SlotIndex].JobId, 1);
			const int32 Limit = FMath::Max(1, FMath::Max(Quote.Owned, Quote.MaxUseful));
			UpgradeSpeedCounts[SlotIndex] = bToMax ? FMath::Max(1, FMath::Min(Quote.Owned, Quote.MaxUseful))
				: FMath::Clamp(UpgradeSpeedCounts[SlotIndex] + Delta, 1, Limit);
			UpdateUpgradeLive();
		};
		SpeedRow->AddChildToHorizontalBox(MakeButton(FText::FromString(TEXT("-")), [ChangeCount]() { ChangeCount(-1, false); }, true, 13))->SetPadding(FMargin(2.f, 0.f));
		UTextBlock* CountText = MakeText(FText::GetEmpty(), 14);
		SpeedRow->AddChildToHorizontalBox(CountText)->SetPadding(FMargin(6.f, 0.f));
		SpeedRow->AddChildToHorizontalBox(MakeButton(FText::FromString(TEXT("+")), [ChangeCount]() { ChangeCount(1, false); }, true, 13))->SetPadding(FMargin(2.f, 0.f));
		SpeedRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("SpeedFit", "딱 맞게"), [ChangeCount]() { ChangeCount(0, true); }, true, 13))->SetPadding(FMargin(2.f, 0.f));
		UButton* ApplyButton = MakeButton(LOCTEXT("SpeedApply", "단축 확정"), [this, SlotIndex]()
		{
			UCozyEstateSubsystem* EstateNow = GetEstate();
			const TArray<FCozyUpgradeJobView> JobsNow = EstateNow ? EstateNow->GetUpgradeJobs() : TArray<FCozyUpgradeJobView>();
			if (JobsNow.IsValidIndex(SlotIndex) && UpgradeSpeedCounts.IsValidIndex(SlotIndex))
			{
				// 확정할 때 서비스가 다시 계산 · 실패하면 부적·시간 그대로
				FText Message;
				EstateNow->ApplySpeedup(JobsNow[SlotIndex].JobId, UpgradeSpeedCounts[SlotIndex], Message);
				UpgradeSpeedCounts[SlotIndex] = 1;
				SetFeedback(Message);
				UpdateUpgradeLive();
			}
		}, false, 13);
		SpeedRow->AddChildToHorizontalBox(ApplyButton)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		WindowContent->AddChildToVerticalBox(SpeedRow)->SetPadding(FMargin(0.f, 2.f));
		UTextBlock* PreviewText = MakeText(FText::GetEmpty(), 13, CozyHud::MutedText);
		WindowContent->AddChildToVerticalBox(PreviewText);
		UpgradeSpeedRows.Add(SpeedRow);
		UpgradeSpeedCountTexts.Add(CountText);
		UpgradeSpeedPreviewTexts.Add(PreviewText);
		UpgradeSpeedApplyButtons.Add(ApplyButton);
	}

	// 업그레이드할 시설 (성장 설정표에 행이 있는 시설)
	WindowContent->AddChildToVerticalBox(MakeText(LOCTEXT("UpList", "업그레이드할 시설"), 17, CozyHud::AccentText))->SetPadding(FMargin(0.f, 12.f, 0.f, 2.f));
	UpgradeFacilityIds = Estate->GetUpgradableFacilities();
	for (const FGuid& FacilityId : UpgradeFacilityIds)
	{
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* TitleText = MakeText(FText::GetEmpty(), 16);
		UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(TitleText);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		UButton* StartButton = MakeButton(LOCTEXT("UpStart", "업그레이드 시작"), [this, FacilityId]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				// 시작 직전에 서비스가 최신 상태로 다시 검사 · 실패하면 아무것도 바뀌지 않음 (D38)
				FText Message;
				EstateNow->StartUpgrade(FacilityId, Message);
				SetFeedback(Message);
				UpdateUpgradeLive();
			}
		}, false, 14);
		Header->AddChildToHorizontalBox(StartButton)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		WindowContent->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 8.f, 0.f, 2.f));
		UTextBlock* DetailText = MakeText(FText::GetEmpty(), 14, CozyHud::MutedText);
		WindowContent->AddChildToVerticalBox(DetailText);
		// 조건 시설로 '이동' 버튼 (조건 목록은 데이터라 창을 만들 때 한 번만 · 충족 여부 글자는 UpdateUpgradeLive가 갱신)
		UHorizontalBox* MoveRow = nullptr;
		for (const FCozyUpgradeQuote::FCondition& Condition : Estate->GetUpgradeQuote(FacilityId).Conditions)
		{
			const FCozyFacilityRow* TargetDef = Condition.FacilityId.IsNone() ? nullptr : Estate->GetFacilityDef(Condition.FacilityId);
			if (!TargetDef)
			{
				continue;
			}
			if (!MoveRow)
			{
				MoveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				MoveRow->AddChildToHorizontalBox(MakeText(LOCTEXT("UpMoveLabel", "조건 시설로 이동:"), 13, CozyHud::MutedText))->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			}
			const FName TargetId = Condition.FacilityId;
			MoveRow->AddChildToHorizontalBox(MakeButton(TargetDef->DisplayName, [this, TargetId]()
			{
				if (ACozyRealmEstatePlayerController* Controller = Cast<ACozyRealmEstatePlayerController>(GetOwningPlayer()))
				{
					CloseWindow();
					Controller->SelectFacilityByDefinition(TargetId);
				}
			}, true, 13))->SetPadding(FMargin(2.f, 0.f));
		}
		if (MoveRow)
		{
			WindowContent->AddChildToVerticalBox(MoveRow)->SetPadding(FMargin(0.f, 2.f));
		}
		UTextBlock* BlockText = MakeText(FText::GetEmpty(), 14, CozyHud::WarningText);
		WindowContent->AddChildToVerticalBox(BlockText);
		UpgradeTitleTexts.Add(TitleText);
		UpgradeDetailTexts.Add(DetailText);
		UpgradeBlockTexts.Add(BlockText);
		UpgradeButtons.Add(StartButton);
	}

	// 방금 한 일 (클릭 결과) · 현재 상태와 구분
	InfoFeedbackText = MakeText(FText::GetEmpty(), 15, CozyHud::AccentText);
	WindowContent->AddChildToVerticalBox(InfoFeedbackText)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	SetFeedback(LastFeedback);

	UpdateUpgradeLive();
}

void UCozyHudWidget::UpdateUpgradeLive()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate || !UpgradeSlotText)
	{
		return;
	}

	const TArray<FCozyUpgradeJobView> Jobs = Estate->GetUpgradeJobs();
	UpgradeSlotText->SetText(FText::Format(LOCTEXT("UpSlots", "진행 중인 업그레이드 {0} / {1}"), FText::AsNumber(Jobs.Num()), FText::AsNumber(Estate->GetUpgradeSlotCount())));
	for (int32 Index = 0; Index < UpgradeJobTexts.Num(); ++Index)
	{
		if (Jobs.IsValidIndex(Index))
		{
			UpgradeJobTexts[Index]->SetText(FText::Format(LOCTEXT("UpJob", "{0} → Lv{1} · 남은 {2}"), Jobs[Index].FacilityName, FText::AsNumber(Jobs[Index].ToLevel), CozyHud::DurationText(Jobs[Index].RemainingSeconds)));
			UpgradeJobTexts[Index]->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			UpgradeJobBars[Index]->SetPercent(Jobs[Index].Progress01);
			UpgradeJobBars[Index]->SetVisibility(ESlateVisibility::Visible);
			if (UpgradeSpeedRows.IsValidIndex(Index))
			{
				const FCozySpeedupQuote Speed = Estate->GetSpeedupQuote(Jobs[Index].JobId, UpgradeSpeedCounts[Index]);
				UpgradeSpeedRows[Index]->SetVisibility(ESlateVisibility::Visible);
				UpgradeSpeedCountTexts[Index]->SetText(FText::Format(LOCTEXT("SpeedCount", "{0}장 (보유 {1}장)"), FText::AsNumber(Speed.Count), FText::AsNumber(Speed.Owned)));
				FString Preview = FString::Printf(TEXT("미리보기: 1장 = %s 단축 · %d장 → %s 단축 · 남은 시간 %s → %s"),
					*CozyHud::DurationText(Speed.SecondsPerItem).ToString(), Speed.Count, *CozyHud::DurationText(Speed.Reduce).ToString(),
					*CozyHud::DurationText(Speed.RemainingBefore).ToString(), *CozyHud::DurationText(Speed.RemainingAfter).ToString());
				if (Speed.Wasted > 0.f)
				{
					Preview += FString::Printf(TEXT(" · 남은 시간보다 많아 %s은 버려집니다"), *CozyHud::DurationText(Speed.Wasted).ToString());
				}
				if (!Speed.BlockReason.IsEmpty())
				{
					Preview += TEXT(" · ") + Speed.BlockReason.ToString();
				}
				UpgradeSpeedPreviewTexts[Index]->SetText(FText::FromString(Preview));
				UpgradeSpeedPreviewTexts[Index]->SetVisibility(ESlateVisibility::Visible);
				UpgradeSpeedApplyButtons[Index]->SetIsEnabled(Speed.bCanApply);
			}
		}
		else
		{
			UpgradeJobTexts[Index]->SetText(FText::Format(LOCTEXT("UpJobEmpty", "작업 칸 {0}: 비어 있음"), FText::AsNumber(Index + 1)));
			UpgradeJobTexts[Index]->SetColorAndOpacity(FSlateColor(CozyHud::MutedText));
			UpgradeJobBars[Index]->SetVisibility(ESlateVisibility::Collapsed);
			if (UpgradeSpeedRows.IsValidIndex(Index))
			{
				UpgradeSpeedRows[Index]->SetVisibility(ESlateVisibility::Collapsed);
				UpgradeSpeedPreviewTexts[Index]->SetVisibility(ESlateVisibility::Collapsed);
				UpgradeSpeedCounts[Index] = 1;
			}
		}
	}

	for (int32 Index = 0; Index < UpgradeFacilityIds.Num(); ++Index)
	{
		const FCozyUpgradeQuote Quote = Estate->GetUpgradeQuote(UpgradeFacilityIds[Index]);
		const FText Name = Estate->GetFacilityDisplayName(UpgradeFacilityIds[Index]);
		UpgradeTitleTexts[Index]->SetText(Quote.bValid
			? FText::Format(LOCTEXT("UpRowTitle", "{0}   Lv{1} → Lv{2}"), Name, FText::AsNumber(Quote.FromLevel), FText::AsNumber(Quote.ToLevel))
			: FText::Format(LOCTEXT("UpRowTitleMax", "{0}   Lv{1}"), Name, FText::AsNumber(Quote.FromLevel)));

		FString Detail;
		if (Quote.bValid)
		{
			FString Costs;
			for (const FCozyUpgradeQuote::FAmount& Cost : Quote.Costs)
			{
				const FCozyItemRow* Item = Estate->GetItemDef(Cost.ItemId);
				Costs += FString::Printf(TEXT("%s%s %d개 (보유 %d)"), Costs.IsEmpty() ? TEXT("") : TEXT(" · "), *(Item ? Item->DisplayName : FText::FromName(Cost.ItemId)).ToString(), Cost.Amount, Cost.Have);
			}
			Detail = FString::Printf(TEXT("비용: %s · 시간: %s"), Costs.IsEmpty() ? TEXT("없음") : *Costs, *CozyHud::DurationText(Quote.Seconds).ToString());
			// 조건마다 충족 여부 (신사 상한 · 선행 시설 D41)
			if (Quote.Conditions.Num() > 0)
			{
				FString Conditions;
				for (const FCozyUpgradeQuote::FCondition& Condition : Quote.Conditions)
				{
					Conditions += FString::Printf(TEXT("%s%s %s"), Conditions.IsEmpty() ? TEXT("") : TEXT(" · "), *Condition.Label.ToString(),
						Condition.bMet ? TEXT("(충족)") : *FString::Printf(TEXT("(부족 · 지금 Lv%d)"), Condition.Current));
				}
				Detail += FString::Printf(TEXT("\n조건: %s"), *Conditions);
			}
			if (Quote.UnlockFacilityNames.Num() > 0)
			{
				FString Facilities;
				for (const FText& Facility : Quote.UnlockFacilityNames)
				{
					Facilities += (Facilities.IsEmpty() ? TEXT("") : TEXT(", ")) + Facility.ToString();
				}
				Detail += FString::Printf(TEXT("\n해금 시설: %s"), *Facilities);
			}
			if (Quote.NextFieldSpeedMultiplier > 0.f)
			{
				Detail += FString::Printf(TEXT("\n효과: 모든 밭 생산 속도 ×%s (진행 중인 주기는 그대로, 다음 주기부터)"), *CozyHud::MultiplierText(Quote.NextFieldSpeedMultiplier).ToString());
			}
			if (Quote.UnlockCropNames.Num() > 0)
			{
				FString Crops;
				for (const FText& Crop : Quote.UnlockCropNames)
				{
					Crops += (Crops.IsEmpty() ? TEXT("") : TEXT(", ")) + Crop.ToString();
				}
				Detail += FString::Printf(TEXT("\n해금 작물: %s (각 밭의 작물 선택에 표시 · 자동으로 바뀌지 않음)"), *Crops);
			}
			// D36: 시작하면 종료될 가공과 반환될 재료
			if (Quote.EndingJobs.Num() > 0)
			{
				FString Ending;
				for (const FText& Line : Quote.EndingJobs)
				{
					Ending += (Ending.IsEmpty() ? TEXT("") : TEXT(" / ")) + Line.ToString();
				}
				FString Refunds;
				for (const FCozyUpgradeQuote::FAmount& Refund : Quote.Refunds)
				{
					const FCozyItemRow* Item = Estate->GetItemDef(Refund.ItemId);
					Refunds += FString::Printf(TEXT("%s%s %d개"), Refunds.IsEmpty() ? TEXT("") : TEXT(", "), *(Item ? Item->DisplayName : FText::FromName(Refund.ItemId)).ToString(), Refund.Amount);
				}
				Detail += FString::Printf(TEXT("\n시작하면 진행 중인 가공이 종료됩니다: %s\n미완료 회차 재료 반환: %s · 완성된 가공품은 시설에 남습니다"), *Ending, Refunds.IsEmpty() ? TEXT("없음") : *Refunds);
			}
		}
		UpgradeDetailTexts[Index]->SetText(FText::FromString(Detail));
		UpgradeDetailTexts[Index]->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		UpgradeBlockTexts[Index]->SetText(Quote.BlockReason);
		UpgradeBlockTexts[Index]->SetVisibility(Quote.BlockReason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		UpgradeButtons[Index]->SetIsEnabled(Quote.bCanStart);
	}
}

// ---------------------------------------------------------------------------
// 밭 관리 창 (관리 단계 · 모든 밭 공통 효과 · 해금 작물 · 업그레이드는 후신소에서)

void UCozyHudWidget::BuildFieldManagementContent()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	const FCozyFacilityState* Facility = Estate ? Estate->FindFacility(WindowFacility) : nullptr;
	const FCozyFacilityRow* Def = Facility ? Estate->GetFacilityDef(Facility->DefinitionId) : nullptr;
	if (!Def)
	{
		CloseWindow();
		return;
	}
	WindowTitle->SetText(FText::Format(LOCTEXT("FieldMgmtTitle", "{0} — 밭 관리"), Def->DisplayName));
	FieldMgmtText = MakeText(FText::GetEmpty(), 16);
	WindowContent->AddChildToVerticalBox(FieldMgmtText)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	// 업그레이드는 후신소에서 (역할 구분 · D37)
	FGuid QueueFacility;
	for (const FCozyFacilityState& Other : Estate->GetState().Facilities)
	{
		const FCozyFacilityRow* OtherDef = Estate->GetFacilityDef(Other.DefinitionId);
		if (OtherDef && OtherDef->Functions.Contains(ECozyFacilityFunction::UpgradeQueue))
		{
			QueueFacility = Other.InstanceId;
			break;
		}
	}
	WindowContent->AddChildToVerticalBox(MakeButton(LOCTEXT("GoUpgrade", "업그레이드 (후신소로)"), [this, QueueFacility]()
	{
		OpenWindow(ECozyWindowKind::Upgrade, QueueFacility);
	}, QueueFacility.IsValid(), 14));
	UpdateFieldManagementLive();
}

void UCozyHudWidget::UpdateFieldManagementLive()
{
	UCozyEstateSubsystem* Estate = GetEstate();
	if (!Estate || !FieldMgmtText)
	{
		return;
	}
	const FCozyFieldManagementView View = Estate->GetFieldManagementView(WindowFacility);
	auto Join = [](const TArray<FText>& Items)
	{
		FString Out;
		for (const FText& Item : Items)
		{
			Out += (Out.IsEmpty() ? TEXT("") : TEXT(", ")) + Item.ToString();
		}
		return Out.IsEmpty() ? FString(TEXT("없음")) : Out;
	};
	FString Text = FString::Printf(TEXT("관리 단계 Lv%d%s\n모든 밭 생산 속도 ×%s (관리하는 밭 %d개 · 새로 지은 밭도 같은 효과)\n고를 수 있는 작물: %s"),
		View.Level, View.bUpgrading ? TEXT(" (업그레이드 중 · 밭은 지금 효과로 계속 생산)") : TEXT(""),
		*CozyHud::MultiplierText(View.CurrentMultiplier).ToString(), View.ManagedFacilities, *Join(View.UnlockedCrops));
	if (View.NextMultiplier > 0.f)
	{
		Text += FString::Printf(TEXT("\n다음 단계: 속도 ×%s · 해금 작물 %s"), *CozyHud::MultiplierText(View.NextMultiplier).ToString(), *Join(View.NextUnlockCrops));
	}
	else
	{
		Text += TEXT("\n다음 단계: 아직 없음");
	}
	FieldMgmtText->SetText(FText::FromString(Text));
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

	DebugContent->AddChildToVerticalBox(MakeText(LOCTEXT("DebugTitle", "디버그 (F1)"), 18, CozyHud::AccentText))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// 시간 배속
	UHorizontalBox* SpeedRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SpeedRow->AddChildToHorizontalBox(MakeText(FText::Format(LOCTEXT("SpeedNow", "배속 {0}"), CozyHud::TimeScaleText(Estate->GetTimeScale())), 15))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	for (const float Scale : { 1.f, 10.f, 100.f })
	{
		SpeedRow->AddChildToHorizontalBox(MakeButton(CozyHud::TimeScaleText(Scale), [this, Scale]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->SetTimeScale(Scale);
			}
		}, true, 13))->SetPadding(FMargin(2.f, 0.f));
	}
	DebugContent->AddChildToVerticalBox(SpeedRow)->SetPadding(FMargin(0.f, 3.f));

	// 재료·재화 추가 (아이디는 Items.csv 행 이름)
	auto AddResourceButton = [this](const FText& Label, FName ItemId, int32 Amount)
	{
		DebugContent->AddChildToVerticalBox(MakeButton(Label, [this, ItemId, Amount]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->DebugAddResource(ItemId, Amount);
			}
		}, true, 13))->SetPadding(FMargin(0.f, 3.f));
	};
	AddResourceButton(LOCTEXT("AddGold", "골드 +100"), TEXT("Gold"), 100);
	AddResourceButton(LOCTEXT("AddWheat", "밀 +10"), TEXT("Wheat"), 10);
	AddResourceButton(LOCTEXT("AddFlour", "밀가루 +10"), TEXT("Flour"), 10);
	AddResourceButton(LOCTEXT("AddTalisman", "시간 부적 +5"), TEXT("TimeTalisman"), 5);
	// 튜토리얼 일회성 보상 구조 검증용 (지급 시점은 튜토리얼 기능에서 연결 · ❓)
	DebugContent->AddChildToVerticalBox(MakeButton(LOCTEXT("GrantTutorial", "튜토리얼 보상 받기 (한 번만)"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			FText Message;
			EstateNow->GrantOneTimeReward(TEXT("Tutorial_FirstSpeedup"), Message);
			SetFeedback(Message);
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));

	// 수령 검증용: 창고 거의 참 / 가득 참 · 같은 재료 시설 하나 더
	auto SetResourceButton = [this](const FText& Label, FName ItemId, int32 Amount)
	{
		DebugContent->AddChildToVerticalBox(MakeButton(Label, [this, ItemId, Amount]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->DebugSetResource(ItemId, Amount);
			}
		}, true, 13))->SetPadding(FMargin(0.f, 3.f));
	};
	AddResourceButton(LOCTEXT("AddRice", "쌀 +10 (테스트 레시피용)"), TEXT("Rice"), 10);
	DebugContent->AddChildToVerticalBox(MakeButton(Estate->GetShowTestRecipes() ? LOCTEXT("TestRecipesOn", "테스트 레시피: 보임") : LOCTEXT("TestRecipesOff", "테스트 레시피: 숨김"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->SetShowTestRecipes(!EstateNow->GetShowTestRecipes());
			if (WindowKind == ECozyWindowKind::Processing)
			{
				RefreshWindow();
			}
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));
	// 가공 창을 연 채 재료가 줄 때 선택 횟수가 새 최대로 내려가는지 검증용
	SetResourceButton(LOCTEXT("SetWheat10", "창고 밀 10개로"), TEXT("Wheat"), 10);
	SetResourceButton(LOCTEXT("SetWheat90", "창고 밀 90개로"), TEXT("Wheat"), 90);
	// 업그레이드 반환 공간 경계값 검증용 (D38: 비용을 뺀 뒤의 최종 재고 기준)
	SetResourceButton(LOCTEXT("SetWheat97", "창고 밀 97개로"), TEXT("Wheat"), 97);
	SetResourceButton(LOCTEXT("SetWheat98", "창고 밀 98개로"), TEXT("Wheat"), 98);
	SetResourceButton(LOCTEXT("SetFlour98", "창고 밀가루 98개로"), TEXT("Flour"), 98);
	SetResourceButton(LOCTEXT("SetFlour100", "창고 밀가루 100개로 (가득)"), TEXT("Flour"), 100);
	SetResourceButton(LOCTEXT("SetWheat100", "창고 밀 100개로 (가득)"), TEXT("Wheat"), 100);
	// 시작 직전 재검사 검증용: 화면에서 버튼이 켜진 뒤 조건이 바뀐 상황을 한 번에 만든다
	// (창고 밀을 가득 채운 직후 제분소 업그레이드 시작을 시도 · 실패하면 비용·가공·예약 공간이 그대로여야 함)
	DebugContent->AddChildToVerticalBox(MakeButton(LOCTEXT("RecheckTest", "재검사 시험: 밀 100으로 바꾼 직후 제분소 업그레이드 시작"), [this]()
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
					SetFeedback(Message);
					break;
				}
			}
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));
	DebugContent->AddChildToVerticalBox(MakeButton(LOCTEXT("AddTestField", "테스트용 밭 추가"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugAddFacility(TEXT("Field"));
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));
	// 밭 관리 시설은 시작 배치가 미정(❓)이라 검증할 때만 디버그로 놓음
	DebugContent->AddChildToVerticalBox(MakeButton(LOCTEXT("AddFieldOffice", "테스트용 밭 관리 시설 추가"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugAddFacility(TEXT("FieldOffice"));
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));

	// 낮/밤 전환
	const ECozyNightOverride Current = Estate->GetNightOverride();
	const FText NightLabel = Current == ECozyNightOverride::Auto ? LOCTEXT("NightAuto", "낮/밤: 자동 (PC 시각)")
		: Current == ECozyNightOverride::ForceDay ? LOCTEXT("NightDay", "낮/밤: 낮 고정")
		: LOCTEXT("NightNight", "낮/밤: 밤 고정");
	DebugContent->AddChildToVerticalBox(MakeButton(NightLabel, [this, Current]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			const ECozyNightOverride Next = Current == ECozyNightOverride::Auto ? ECozyNightOverride::ForceDay
				: Current == ECozyNightOverride::ForceDay ? ECozyNightOverride::ForceNight
				: ECozyNightOverride::Auto;
			EstateNow->SetNightOverride(Next);
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));

	DebugContent->AddChildToVerticalBox(MakeButton(LOCTEXT("Validate", "데이터 검사 다시 실행 (로그)"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->ValidateData();
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));

	// 저장 · 불러오기 (2주차 기능 3) · 저장 시각 당기기는 방치 보상 검증용
	UHorizontalBox* SaveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SaveRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("SaveNow", "저장"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->SaveEstate(TEXT("디버그"));
		}
	}, true, 13))->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
	SaveRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("ReloadSave", "불러오기"), [this]()
	{
		HideFacilityIcons();
		CloseWindow();
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugReloadFromSave();
		}
	}, true, 13))->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
	SaveRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("ShiftSave1h", "저장 시각 -1시간"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugShiftSaveTime(3600.0);
		}
	}, true, 13))->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
	SaveRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("ShiftSave13h", "-13시간"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugShiftSaveTime(13.0 * 3600.0);
		}
	}, true, 13));
	DebugContent->AddChildToVerticalBox(SaveRow)->SetPadding(FMargin(0.f, 3.f));
	// 방치 시간 건너뛰기 (기획서 제작자 도구 ② · 같은 정산 처리)
	UHorizontalBox* SkipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const float Hours : { 1.f, 12.f })
	{
		SkipRow->AddChildToHorizontalBox(MakeButton(FText::Format(LOCTEXT("SkipOffline", "방치 {0}시간 건너뛰기"), FText::AsNumber(Hours)), [this, Hours]()
		{
			if (UCozyEstateSubsystem* EstateNow = GetEstate())
			{
				EstateNow->DebugSkipOffline(Hours * 3600.0);
			}
		}, true, 13))->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
	}
	DebugContent->AddChildToVerticalBox(SkipRow)->SetPadding(FMargin(0.f, 3.f));
	DebugContent->AddChildToVerticalBox(MakeButton(LOCTEXT("SelfCheckAppend", "자체 검사: 가공 제작 추가 (상태 되돌림)"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			ShowToast(FText::FromString(EstateNow->DebugRunProcessingAppendCheck()));
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));

	// UI 미리보기 (편집 가능한 화면만 · 실제 재료를 쓰지 않음 · 상태를 바꿔 가며 확인)
	if (HudScreen)
	{
		DebugContent->AddChildToVerticalBox(MakeButton(HudScreen->bPreview ? LOCTEXT("UiPreviewOff", "UI 미리보기 끄기") : LOCTEXT("UiPreviewOn", "UI 미리보기 켜기 (재료 안 씀)"), [this]()
		{
			if (HudScreen)
			{
				HudScreen->SetPreview(!HudScreen->bPreview);
				ShowToast(HudScreen->bPreview ? LOCTEXT("UiPreviewOnToast", "UI 미리보기 켜짐 · 버튼은 알림만 띄웁니다") : LOCTEXT("UiPreviewOffToast", "UI 미리보기 꺼짐 · 실제 값으로 돌아갑니다"));
				RefreshDebugPanel();
			}
		}, true, 13))->SetPadding(FMargin(0.f, 3.f));
		if (HudScreen->bPreview)
		{
			UHorizontalBox* StateRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			for (const ECozyUiPreviewState State : { ECozyUiPreviewState::Progress, ECozyUiPreviewState::Paused, ECozyUiPreviewState::Full, ECozyUiPreviewState::Locked, ECozyUiPreviewState::Claimable, ECozyUiPreviewState::Empty })
			{
				StateRow->AddChildToHorizontalBox(MakeButton(UCozyUiScreen::GetPreviewStateName(State), [this, State]()
				{
					if (HudScreen)
					{
						HudScreen->SetPreviewState(State);
						ShowToast(FText::Format(LOCTEXT("UiPreviewState", "미리보기 상태: {0}"), UCozyUiScreen::GetPreviewStateName(State)));
					}
				}, HudScreen->PreviewState != State, 12))->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
			}
			DebugContent->AddChildToVerticalBox(StateRow)->SetPadding(FMargin(0.f, 3.f));
		}
	}

	DebugContent->AddChildToVerticalBox(MakeButton(LOCTEXT("Restart", "새 게임 다시 시작"), [this]()
	{
		HideFacilityIcons();
		CloseWindow();
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugRestartNewGame();
		}
	}, true, 13))->SetPadding(FMargin(0.f, 3.f));

	ActionSink = nullptr;
}

#undef LOCTEXT_NAMESPACE
