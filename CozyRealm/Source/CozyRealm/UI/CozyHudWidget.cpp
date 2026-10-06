#include "UI/CozyHudWidget.h"
#include "Estate/CozyEstateSubsystem.h"
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
#include "Styling/CoreStyle.h"
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
	if (IconSlot)
	{
		IconSlot->SetAutoSize(true);
		IconSlot->SetAlignment(FVector2D(0.5f, 1.f));
	}
	IconBox->SetVisibility(ESlateVisibility::Collapsed);

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
	WindowFrame->SetBrushColor(CozyHud::WindowColor);
	WindowFrame->SetPadding(FMargin(32.f, 24.f));
	WindowOverlay->SetContent(WindowFrame);

	UVerticalBox* WindowBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WindowBody"));
	WindowFrame->SetContent(WindowBody);

	WindowTitle = MakeText(FText::GetEmpty(), 26, CozyHud::AccentText);
	WindowBody->AddChildToVerticalBox(WindowTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	WindowContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WindowContent"));
	WindowBody->AddChildToVerticalBox(WindowContent)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));

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

void UCozyHudWidget::HandleEstateChanged(bool bStructural)
{
	RefreshTopBar();
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
	if (!bStructural)
	{
		return;
	}
	// 버튼 구성이 바뀌는 창(나가야: 배치 가능 여부)만 다시 그림
	if (WindowKind == ECozyWindowKind::Nagaya || WindowKind == ECozyWindowKind::Placeholder)
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
		InfoRemainingText->SetText(FText::Format(LOCTEXT("NextHarvest", "다음 생산 완료까지 {0}초"), FText::AsNumber(FMath::CeilToInt(View.RemainingSeconds))));
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
	Action->Callback = MoveTemp(OnClick);
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
			AddIcon(LOCTEXT("IconSell", "판매"), [this, FacilityId]() { PlaceholderLabel = LOCTEXT("PhSell", "판매"); OpenWindow(ECozyWindowKind::Placeholder, FacilityId); });
			break;
		case ECozyFacilityFunction::UpgradeQueue:
			AddIcon(LOCTEXT("IconUpgrade", "업그레이드"), [this, FacilityId]() { PlaceholderLabel = LOCTEXT("PhUpgrade", "업그레이드"); OpenWindow(ECozyWindowKind::Placeholder, FacilityId); });
			break;
		case ECozyFacilityFunction::ShrineCore:
			AddIcon(LOCTEXT("IconShrine", "신사"), [this, FacilityId]() { PlaceholderLabel = LOCTEXT("PhShrine", "신사"); OpenWindow(ECozyWindowKind::Placeholder, FacilityId); });
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
	if (WindowOverlay)
	{
		WindowOverlay->SetVisibility(ESlateVisibility::Visible);
	}
	RefreshWindow();
}

void UCozyHudWidget::CloseWindow()
{
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
	ProcStartButton = nullptr;
	ProcSlotStatusTexts.Reset();
	ProcSlotBars.Reset();
	ProcSlotTimeTexts.Reset();
	ProcSlotCancelButtons.Reset();
	ProcConfirmBox = nullptr;
	ProcConfirmText = nullptr;
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
	default:
		break;
	}

	if (!LastFeedback.IsEmpty() && WindowKind != ECozyWindowKind::FacilityInfo && WindowKind != ECozyWindowKind::Processing)
	{
		WindowContent->AddChildToVerticalBox(MakeText(LastFeedback, 16, CozyHud::WarningText))->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
	ActionSink = nullptr;
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

	WindowTitle->SetText(FText::Format(LOCTEXT("InfoTitle", "{0}  Lv.{1}"), Def->DisplayName, FText::AsNumber(Facility->Level)));

	const FCozyCropRow* Crop = Estate->GetCropDef(Facility->SelectedCropId);
	WindowContent->AddChildToVerticalBox(MakeText(FText::Format(LOCTEXT("CropLine", "키우는 작물: {0}"), Crop ? Crop->DisplayName : LOCTEXT("NoneCrop", "없음"))));

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
			const TArray<FCozyProcessingJobView> Jobs = EstateNow ? EstateNow->GetProcessingJobs(FacilityId) : TArray<FCozyProcessingJobView>();
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
	const FCozyRecipeQuote Quote = Estate->GetRecipeQuote(WindowFacility, ProcSelectedRecipe, ProcSelectedRuns);
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

	ProcRunsText->SetText(FText::Format(LOCTEXT("ProcRuns", "{0}회  →  {1} {2}개"), FText::AsNumber(ProcSelectedRuns), Quote.OutputName, FText::AsNumber(Quote.TotalOutput)));
	ProcMinusButton->SetIsEnabled(ProcSelectedRuns > 1);
	ProcPlusButton->SetIsEnabled(ProcSelectedRuns < Quote.MaxRuns);
	ProcMaxButton->SetIsEnabled(Quote.MaxRuns > 0 && ProcSelectedRuns != Quote.MaxRuns);
	ProcMaxText->SetText(FText::Format(LOCTEXT("ProcMax", "지금 최대 {0}회  (재료로 {1}회분 · 남은 미수령 공간으로 {2}회분)"),
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

	// 가공 칸
	const TArray<FCozyProcessingJobView> Jobs = Estate->GetProcessingJobs(WindowFacility);
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
		? Jobs.FindByPredicate([this](const FCozyProcessingJobView& Job) { return Job.JobId == ProcPendingCancelJob; })
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
	SetResourceButton(LOCTEXT("SetWheat95", "창고 밀 95개로"), TEXT("Wheat"), 95);
	SetResourceButton(LOCTEXT("SetWheat100", "창고 밀 100개로 (가득)"), TEXT("Wheat"), 100);
	DebugContent->AddChildToVerticalBox(MakeButton(LOCTEXT("AddTestField", "테스트용 밭 추가"), [this]()
	{
		if (UCozyEstateSubsystem* EstateNow = GetEstate())
		{
			EstateNow->DebugAddFacility(TEXT("Field"));
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
