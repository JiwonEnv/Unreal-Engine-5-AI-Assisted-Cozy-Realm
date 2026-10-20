#include "UI/CozyTitleWidget.h"
#include "UI/CozyTitleSettings.h"
#include "UI/Kit/CozyUiScreen.h"
#include "UI/Kit/CozyUiTheme.h"
#include "Core/CozyTitleGameMode.h"
#include "Settings/CozySettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"
#include "CozyRealm.h"

#define LOCTEXT_NAMESPACE "CozyTitle"

namespace CozyTitle
{
	static const FName Continue(TEXT("Continue"));
	static const FName NewGame(TEXT("NewGame"));
	static const FName Settings(TEXT("Settings"));
	static const FName Quit(TEXT("Quit"));
	static const FName Cancel(TEXT("Cancel"));
	static const FName ConfirmNew(TEXT("ConfirmNew"));
	static const FName ConfirmQuit(TEXT("ConfirmQuit"));
	static const FName Retry(TEXT("Retry"));
	static const FName Close(TEXT("Close"));
	constexpr float ButtonWidth = 340.f;
	constexpr float ButtonHeight = 64.f;

	/** 왼쪽이 짙고 오른쪽으로 사라지는 그림자 (배경 위에서도 글자가 읽히게) */
	static UTexture2D* MakeGradient()
	{
		constexpr int32 W = 256;
		UTexture2D* Texture = UTexture2D::CreateTransient(W, 1, PF_B8G8R8A8);
		Texture->SRGB = true;
		Texture->AddressX = TA_Clamp; Texture->AddressY = TA_Clamp;
		FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 X = 0; X < W; ++X)
		{
			const float T = float(X) / (W - 1);
			const float A = T < .62f ? FMath::Lerp(.72f, 0.f, FMath::SmoothStep(0.f, 1.f, T / .62f)) : 0.f;
			Pixels[X] = FColor(38, 24, 26, uint8(A * 255));
		}
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

bool UCozyTitleWidget::Initialize()
{
	const bool bOk = Super::Initialize();
	SetIsFocusable(true);
	if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
	return bOk;
}

void UCozyTitleWidget::FocusRoot()
{
	SetFocus();
}

FSlateFontInfo UCozyTitleWidget::Font(float Size) const
{
	FSlateFontInfo Info = Theme ? Theme->Fonts.Button : FSlateFontInfo();
	if (!Info.FontObject && Theme) Info = Theme->Fonts.Title;
	Info.Size = FMath::RoundToInt(Size);
	return Info;
}

FLinearColor UCozyTitleWidget::ThemeColor(ECozyUiColor Role, const FLinearColor& Fallback) const
{
	return Theme ? Theme->Colors.Get(Role) : Fallback;
}

UButton* UCozyTitleWidget::MakeButton(FName Style, UTextBlock*& OutLabel, const FText& Text, float FontSize)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	if (const FButtonStyle* Found = Theme ? Theme->FindButtonStyle(Style) : nullptr) Button->SetStyle(*Found);
	OutLabel = WidgetTree->ConstructWidget<UTextBlock>();
	OutLabel->SetText(Text);
	OutLabel->SetFont(Font(FontSize));
	OutLabel->SetJustification(ETextJustify::Center);
	OutLabel->SetColorAndOpacity(FSlateColor(ThemeColor(ECozyUiColor::Ink, FLinearColor(.2f, .14f, .1f))));
	Button->SetContent(OutLabel);
	return Button;
}

void UCozyTitleWidget::BuildTree()
{
	Theme = UCozyUiSettings::LoadDefaultTheme();
	const UCozyTitleSettings* Config = UCozyTitleSettings::Get();
	AccentTexture = Config->AccentIcon.LoadSynchronous();
	GradientTexture = CozyTitle::MakeGradient();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TitleRoot"));
	WidgetTree->RootWidget = Root;

	// 0) 배경 맵이 보일 때까지 덮는 종이색 바탕 (배경이 없으면 그대로 남음)
	PaperCover = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PaperCover"));
	PaperCover->SetColorAndOpacity(ThemeColor(ECozyUiColor::Paper, FLinearColor(.93f, .88f, .78f)));
	PaperCover->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* S = Root->AddChildToOverlay(PaperCover)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }

	// 1) 왼쪽 그림자 (로고·버튼 뒤)
	UImage* Shade = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LeftShade"));
	Shade->SetBrushFromTexture(GradientTexture, false);
	Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* S = Root->AddChildToOverlay(Shade)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }

	// 2) 왼쪽 세로 줄: 로고 · 부제 · 버튼 패널 · 상태 글
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleColumn"));
	if (UOverlaySlot* S = Root->AddChildToOverlay(Column)) { S->SetHorizontalAlignment(HAlign_Left); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(110.f, 40.f, 40.f, 40.f)); }

	UHorizontalBox* LogoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LogoRow"));
	Column->AddChildToVerticalBox(LogoRow);
	if (UTexture2D* LogoImage = Config->LogoImage.LoadSynchronous())
	{
		UImage* Logo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LogoImage"));
		Logo->SetBrushFromTexture(LogoImage, true);
		LogoRow->AddChildToHorizontalBox(Logo);
	}
	else
	{
		if (AccentTexture)
		{
			USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
			IconSize->SetWidthOverride(84.f); IconSize->SetHeightOverride(84.f);
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LogoIcon"));
			Icon->SetBrushFromTexture(AccentTexture, false);
			IconSize->SetContent(Icon);
			if (UHorizontalBoxSlot* S = LogoRow->AddChildToHorizontalBox(IconSize)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f)); }
		}
		UTextBlock* Logo = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LogoText"));
		Logo->SetText(FText::FromString(Config->LogoText));
		Logo->SetFont(Font(88.f));
		Logo->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, .96f, .9f)));
		Logo->SetShadowOffset(FVector2D(3.f, 4.f));
		Logo->SetShadowColorAndOpacity(FLinearColor(.25f, .08f, .1f, .75f));
		if (UHorizontalBoxSlot* S = LogoRow->AddChildToHorizontalBox(Logo)) S->SetVerticalAlignment(VAlign_Center);
	}
	if (!Config->Subtitle.IsEmpty())
	{
		UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Subtitle"));
		Sub->SetText(FText::FromString(Config->Subtitle));
		Sub->SetFont(Font(28.f));
		Sub->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, .9f, .92f)));
		Sub->SetShadowOffset(FVector2D(2.f, 2.f));
		Sub->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .6f));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Sub)) S->SetPadding(FMargin(AccentTexture ? 100.f : 4.f, 0.f, 0.f, 0.f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ButtonPanel"));
	if (const FSlateBrush* Brush = Theme ? Theme->FindImage(Config->PanelImage) : nullptr) Panel->SetBrush(*Brush);
	else Panel->SetBrushColor(FLinearColor(.95f, .9f, .82f, .92f));
	Panel->SetPadding(FMargin(22.f, 20.f, 30.f, 20.f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Panel)) { S->SetPadding(FMargin(0.f, 36.f, 0.f, 0.f)); S->SetHorizontalAlignment(HAlign_Left); }
	ButtonList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ButtonList"));
	Panel->SetContent(ButtonList);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetFont(Font(22.f));
	StatusText->SetAutoWrapText(true);
	StatusText->SetShadowOffset(FVector2D(2.f, 2.f));
	StatusText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .7f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StatusText)) S->SetPadding(FMargin(6.f, 14.f, 0.f, 0.f));

	// 3) 오른쪽 아래 버전
	VersionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VersionText"));
	FString Version;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
	VersionText->SetText(FText::FromString(FString::Printf(TEXT("v%s %s"), Version.IsEmpty() ? TEXT("0.0.0") : *Version, *Config->VersionSuffix).TrimEnd()));
	VersionText->SetFont(Font(18.f));
	VersionText->SetVisibility(ESlateVisibility::HitTestInvisible);
	VersionText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, .85f)));
	VersionText->SetShadowOffset(FVector2D(1.f, 1.f));
	VersionText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .8f));
	if (UOverlaySlot* S = Root->AddChildToOverlay(VersionText)) { S->SetHorizontalAlignment(HAlign_Right); S->SetVerticalAlignment(VAlign_Bottom); S->SetPadding(FMargin(16.f, 12.f)); }

	// 4) 설정 창 자리 (게임 안 설정 화면을 그대로 띄움)
	SettingsLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsLayer"));
	SettingsLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, .55f));
	SettingsLayer->SetHorizontalAlignment(HAlign_Center);
	SettingsLayer->SetVerticalAlignment(VAlign_Center);
	SettingsLayer->SetPadding(FMargin(20.f));
	SettingsSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SettingsSize"));
	SettingsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SettingsScroll"));
	SettingsSize->SetContent(SettingsScroll);
	SettingsLayer->SetContent(SettingsSize);
	SettingsLayer->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* S = Root->AddChildToOverlay(SettingsLayer)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }

	// 5) 확인 창 · 불러오는 중 (화면 전체를 덮어 뒤 버튼을 누를 수 없게)
	ModalLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ModalLayer"));
	ModalLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, .5f));
	ModalLayer->SetHorizontalAlignment(HAlign_Center);
	ModalLayer->SetVerticalAlignment(VAlign_Center);
	ModalLayer->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* S = Root->AddChildToOverlay(ModalLayer)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	USizeBox* ModalSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ModalSize"));
	ModalSize->SetWidthOverride(560.f);
	ModalLayer->SetContent(ModalSize);
	UBorder* Window = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ModalWindow"));
	if (const FSlateBrush* Brush = Theme ? Theme->FindImage(Config->WindowImage) : nullptr) Window->SetBrush(*Brush);
	else Window->SetBrushColor(FLinearColor(.96f, .92f, .84f));
	Window->SetPadding(FMargin(36.f, 28.f));
	ModalSize->SetContent(Window);
	UVerticalBox* ModalBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Window->SetContent(ModalBox);
	ModalTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ModalTitle"));
	ModalTitle->SetFont(Font(32.f));
	ModalTitle->SetColorAndOpacity(FSlateColor(ThemeColor(ECozyUiColor::Point, FLinearColor(.75f, .25f, .2f))));
	ModalTitle->SetJustification(ETextJustify::Center);
	ModalBox->AddChildToVerticalBox(ModalTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	ModalBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ModalBody"));
	ModalBody->SetFont(Font(22.f));
	ModalBody->SetAutoWrapText(true);
	ModalBody->SetJustification(ETextJustify::Center);
	ModalBody->SetColorAndOpacity(FSlateColor(ThemeColor(ECozyUiColor::Ink, FLinearColor(.2f, .14f, .1f))));
	ModalBox->AddChildToVerticalBox(ModalBody)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));
	ModalButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ModalButtons"));
	if (UVerticalBoxSlot* S = ModalBox->AddChildToVerticalBox(ModalButtonRow)) S->SetHorizontalAlignment(HAlign_Center);
	for (int32 I = 0; I < 2; ++I)
	{
		UTextBlock* Label = nullptr;
		UButton* Button = MakeButton(Config->ButtonStyle, Label, FText::GetEmpty(), 24.f);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(210.f); Size->SetHeightOverride(56.f);
		Size->SetContent(Button);
		if (UHorizontalBoxSlot* S = ModalButtonRow->AddChildToHorizontalBox(Size)) S->SetPadding(FMargin(10.f, 0.f));
		ModalButtons.Add(Button); ModalLabels.Add(Label);
	}
	ModalButtons[0]->OnClicked.AddDynamic(this, &UCozyTitleWidget::OnModalClicked0);
	ModalButtons[1]->OnClicked.AddDynamic(this, &UCozyTitleWidget::OnModalClicked1);
	ModalButtons[0]->OnHovered.AddDynamic(this, &UCozyTitleWidget::OnModalHovered0);
	ModalButtons[1]->OnHovered.AddDynamic(this, &UCozyTitleWidget::OnModalHovered1);
}

void UCozyTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshSaveState();
	UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 화면 표시 · 저장 상태 %d (%s) · 영지 시작 안 함"), int32(SaveState), *SaveDetail);
}

void UCozyTitleWidget::RefreshSaveState()
{
	SavedUtc = FDateTime();
	SaveState = UCozyEstateSubsystem::ProbeSaveFile(SavedUtc, SaveDetail);
	const FDateTime Local = SavedUtc + (FDateTime::Now() - FDateTime::UtcNow());
	const FString When = SavedUtc.GetTicks() > 0 ? FString::Printf(TEXT("%d월 %d일 %02d:%02d"), Local.GetMonth(), Local.GetDay(), Local.GetHour(), Local.GetMinute()) : FString();
	FText Status;
	FLinearColor Color(1.f, .96f, .9f);
	switch (SaveState)
	{
	case ECozySaveProbe::Valid: Status = FText::Format(LOCTEXT("SavedAt", "마지막 저장 · {0}"), FText::FromString(When)); break;
	case ECozySaveProbe::Recovered:
		Status = FText::Format(LOCTEXT("Recovered", "{0} · 마지막 저장 {1}"), FText::FromString(SaveDetail), FText::FromString(When));
		Color = FLinearColor(1.f, .85f, .55f); break;
	case ECozySaveProbe::Invalid:
		Status = FText::Format(LOCTEXT("Invalid", "{0} · 파일은 지우지 않고 그대로 두었습니다"), FText::FromString(SaveDetail));
		Color = FLinearColor(1.f, .72f, .62f); break;
	default: break;
	}
	StatusText->SetText(Status);
	StatusText->SetColorAndOpacity(FSlateColor(Color));
	StatusText->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	// 버튼 구성(이어하기 유무)이 바뀔 때만 다시 만든다 · 누르는 중인 버튼을 지우지 않게
	const bool bWantContinue = SaveState != ECozySaveProbe::None;
	const bool bHasContinue = !Buttons.IsEmpty() && Buttons[0].Id == CozyTitle::Continue;
	if (Buttons.IsEmpty() || bWantContinue != bHasContinue) RebuildButtons();
}

void UCozyTitleWidget::RebuildButtons()
{
	const UCozyTitleSettings* Config = UCozyTitleSettings::Get();
	ButtonList->ClearChildren();
	Buttons.Reset();
	// 저장이 있으면(읽을 수 없는 경우 포함) 이어하기를 맨 위 · 가장 눈에 띄는 기본 선택. 없으면 새 게임이 기본.
	const bool bHasSave = SaveState != ECozySaveProbe::None;
	TArray<TPair<FName, FText>> Items;
	if (bHasSave) Items.Add({ CozyTitle::Continue, LOCTEXT("BtnContinue", "이어하기") });
	Items.Add({ CozyTitle::NewGame, LOCTEXT("BtnNew", "새 게임") });
	Items.Add({ CozyTitle::Settings, LOCTEXT("BtnSettings", "설정") });
	Items.Add({ CozyTitle::Quit, LOCTEXT("BtnQuit", "게임 종료") });
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		FCozyTitleButton Entry;
		Entry.Id = Items[I].Key;
		Entry.bPrimary = I == 0;
		Entry.Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Entry.Marker = WidgetTree->ConstructWidget<UImage>();
		USizeBox* MarkerSize = WidgetTree->ConstructWidget<USizeBox>();
		MarkerSize->SetWidthOverride(34.f); MarkerSize->SetHeightOverride(34.f);
		if (AccentTexture) Entry.Marker->SetBrushFromTexture(AccentTexture, false);
		else Entry.Marker->SetColorAndOpacity(ThemeColor(ECozyUiColor::Point, FLinearColor::Red));
		MarkerSize->SetContent(Entry.Marker);
		if (UHorizontalBoxSlot* S = Entry.Row->AddChildToHorizontalBox(MarkerSize)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f)); }
		UTextBlock* Label = nullptr;
		Entry.Button = MakeButton(Entry.bPrimary ? Config->PrimaryButtonStyle : Config->ButtonStyle, Label, Items[I].Value, Entry.bPrimary ? 32.f : 28.f);
		Entry.Label = Label;
		Entry.Size = WidgetTree->ConstructWidget<USizeBox>();
		Entry.Size->SetWidthOverride(CozyTitle::ButtonWidth);
		Entry.Size->SetHeightOverride(Entry.bPrimary ? CozyTitle::ButtonHeight + 10.f : CozyTitle::ButtonHeight);
		Entry.Size->SetContent(Entry.Button);
		Entry.Size->SetRenderTransformPivot(FVector2D(0.f, .5f));
		Entry.Row->AddChildToHorizontalBox(Entry.Size);
		if (UVerticalBoxSlot* S = ButtonList->AddChildToVerticalBox(Entry.Row)) S->SetPadding(FMargin(0.f, I == 0 ? 0.f : 12.f, 0.f, 0.f));
		Buttons.Add(Entry);
	}
	for (int32 I = 0; I < Buttons.Num(); ++I)
	{
		switch (I)
		{
		case 0: Buttons[I].Button->OnClicked.AddDynamic(this, &UCozyTitleWidget::OnButtonClicked0); Buttons[I].Button->OnHovered.AddDynamic(this, &UCozyTitleWidget::OnButtonHovered0); break;
		case 1: Buttons[I].Button->OnClicked.AddDynamic(this, &UCozyTitleWidget::OnButtonClicked1); Buttons[I].Button->OnHovered.AddDynamic(this, &UCozyTitleWidget::OnButtonHovered1); break;
		case 2: Buttons[I].Button->OnClicked.AddDynamic(this, &UCozyTitleWidget::OnButtonClicked2); Buttons[I].Button->OnHovered.AddDynamic(this, &UCozyTitleWidget::OnButtonHovered2); break;
		case 3: Buttons[I].Button->OnClicked.AddDynamic(this, &UCozyTitleWidget::OnButtonClicked3); Buttons[I].Button->OnHovered.AddDynamic(this, &UCozyTitleWidget::OnButtonHovered3); break;
		default: break;
		}
	}
	SetSelected(0);
}

void UCozyTitleWidget::SetSelected(int32 Index)
{
	if (Buttons.IsEmpty()) return;
	Selected = (Index % Buttons.Num() + Buttons.Num()) % Buttons.Num();
	UpdateButtonVisuals();
}

void UCozyTitleWidget::UpdateButtonVisuals()
{
	// 선택: 벚꽃 표시 + 조금 크게 + 강조색 글자 · 마우스 올림·누름은 버튼 모양(테마)이 따로 보여 줌
	for (int32 I = 0; I < Buttons.Num(); ++I)
	{
		const bool bSel = I == Selected;
		Buttons[I].Marker->SetRenderOpacity(bSel ? 1.f : 0.f);
		Buttons[I].Size->SetRenderScale(bSel ? FVector2D(1.05f, 1.05f) : FVector2D(1.f, 1.f));
		Buttons[I].Label->SetColorAndOpacity(FSlateColor(bSel ? ThemeColor(ECozyUiColor::Point, FLinearColor(.75f, .25f, .2f)) : ThemeColor(ECozyUiColor::Ink, FLinearColor(.2f, .14f, .1f))));
		Buttons[I].Button->SetIsEnabled(!bEntering);
	}
}

void UCozyTitleWidget::ClickMain(int32 Index)
{
	if (!Buttons.IsValidIndex(Index) || Modal != EModal::None || SettingsScreen && SettingsLayer->IsVisible()) return;
	SetSelected(Index);
	FocusRoot();
	Activate(Buttons[Index].Id);
}

void UCozyTitleWidget::OnButtonClicked0() { ClickMain(0); }
void UCozyTitleWidget::OnButtonClicked1() { ClickMain(1); }
void UCozyTitleWidget::OnButtonClicked2() { ClickMain(2); }
void UCozyTitleWidget::OnButtonClicked3() { ClickMain(3); }
void UCozyTitleWidget::OnButtonHovered0() { if (Modal == EModal::None) SetSelected(0); }
void UCozyTitleWidget::OnButtonHovered1() { if (Modal == EModal::None) SetSelected(1); }
void UCozyTitleWidget::OnButtonHovered2() { if (Modal == EModal::None) SetSelected(2); }
void UCozyTitleWidget::OnButtonHovered3() { if (Modal == EModal::None) SetSelected(3); }

void UCozyTitleWidget::ActivateSelected()
{
	if (Buttons.IsValidIndex(Selected)) Activate(Buttons[Selected].Id);
}

void UCozyTitleWidget::Activate(FName Id)
{
	if (bEntering) return;
	UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 버튼: %s"), *Id.ToString());
	if (Id == CozyTitle::Continue) StartContinue();
	else if (Id == CozyTitle::NewGame) StartNewGame();
	else if (Id == CozyTitle::Settings) OpenSettings();
	else if (Id == CozyTitle::Quit)
	{
		ShowModal(EModal::QuitConfirm, LOCTEXT("QuitTitle", "게임을 종료할까요?"), LOCTEXT("QuitBody", "타이틀 화면에서는 저장할 내용이 없어 바로 종료합니다."),
			{ { CozyTitle::Cancel, LOCTEXT("Cancel", "취소") }, { CozyTitle::ConfirmQuit, LOCTEXT("QuitOk", "게임 종료") } }, 0);
	}
}

void UCozyTitleWidget::StartContinue()
{
	// 누른 순간 다시 확인 (그 사이 파일이 바뀌었을 수 있음) · 읽을 수 없으면 새 게임으로 덮어쓰지 않고 오류·재시도 안내
	RefreshSaveState();
	if (SaveState == ECozySaveProbe::Valid || SaveState == ECozySaveProbe::Recovered)
	{
		EnterEstate(TEXT("이어하기"));
	}
	else if (SaveState == ECozySaveProbe::Invalid)
	{
		ShowModal(EModal::LoadError, LOCTEXT("LoadErrTitle", "저장을 불러오지 못했습니다"),
			FText::Format(LOCTEXT("LoadErrBody", "{0}\n\n저장 파일은 지우거나 덮어쓰지 않았습니다.\n파일을 되돌린 뒤 '다시 시도'를 누르거나, 새 게임에서 백업 후 새로 시작할 수 있습니다."), FText::FromString(SaveDetail)),
			{ { CozyTitle::Retry, LOCTEXT("Retry", "다시 시도") }, { CozyTitle::Close, LOCTEXT("Close", "닫기") } }, 0);
	}
}

void UCozyTitleWidget::StartNewGame()
{
	RefreshSaveState();
	if (SaveState == ECozySaveProbe::None)
	{
		EnterEstate(TEXT("새 게임 (저장 없음)"));
		return;
	}
	// 기존 저장을 덮어쓰게 되므로 반드시 확인 · 확인해도 지우지 않고 백업 폴더로 옮긴다
	const FText Body = SaveState == ECozySaveProbe::Invalid
		? LOCTEXT("NewBodyBroken", "읽을 수 없는 지금 저장 파일은 지우지 않고 백업 폴더(SaveGames/Backup_NewGame_…)로 옮겨 둡니다.\n그 뒤 영지를 처음부터 시작합니다.")
		: LOCTEXT("NewBody", "지금 영지는 지우지 않고 백업 폴더(SaveGames/Backup_NewGame_…)로 옮겨 둡니다.\n그 뒤 영지를 처음부터 시작합니다. 이어하기로는 돌아갈 수 없습니다.");
	ShowModal(EModal::NewGameConfirm, LOCTEXT("NewTitle", "새 게임을 시작할까요?"), Body,
		{ { CozyTitle::Cancel, LOCTEXT("Cancel2", "취소") }, { CozyTitle::ConfirmNew, LOCTEXT("NewOk", "백업하고 새로 시작") } }, 0);
}

void UCozyTitleWidget::EnterEstate(const FString& Reason)
{
	const UCozyTitleSettings* Config = UCozyTitleSettings::Get();
	if (Config->EstateMap.IsNull())
	{
		ShowModal(EModal::EnterFailed, LOCTEXT("NoMapTitle", "마을에 들어갈 수 없습니다"), LOCTEXT("NoMapBody", "프로젝트 설정 'Cozy Title'에 영지 맵이 지정되지 않았습니다."),
			{ { CozyTitle::Close, LOCTEXT("Close2", "닫기") } }, 0);
		return;
	}
	// 중복 입력 방지: 버튼을 끄고 화면 전체를 덮는 '불러오는 중' 창을 띄운 뒤 다음 순간에 맵을 연다
	bEntering = true;
	EnterStartTime = FPlatformTime::Seconds();
	UpdateButtonVisuals();
	LoadingBase = LOCTEXT("Loading", "마을로 가는 중");
	ShowModal(EModal::Loading, LOCTEXT("LoadingTitle", "잠시만 기다려 주세요"), LoadingBase, {}, 0);
	UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 마을 입장 시작 (%s) → %s"), *Reason, *Config->EstateMap.ToString());
	FTimerHandle Handle;
	TWeakObjectPtr<UCozyTitleWidget> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid()) UGameplayStatics::OpenLevelBySoftObjectPtr(WeakThis.Get(), UCozyTitleSettings::Get()->EstateMap);
	}), .25f, false);
}

void UCozyTitleWidget::OpenSettings()
{
	UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this);
	if (!SettingsScreen)
	{
		const TSoftClassPtr<UCozyUiScreen>* Soft = UCozyUiSettings::Get()->WindowScreens.Find(TEXT("Settings"));
		UClass* ScreenClass = Soft ? Soft->LoadSynchronous() : nullptr;
		SettingsScreen = ScreenClass ? CreateWidget<UCozyUiScreen>(GetOwningPlayer(), ScreenClass) : nullptr;
		if (!SettingsScreen)
		{
			UE_LOG(LogCozyRealm, Warning, TEXT("[타이틀] 설정 화면을 찾지 못함 (프로젝트 설정 Cozy UI > Window Screens > Settings)"));
			return;
		}
		SettingsScreen->OnCloseRequested.AddUObject(this, &UCozyTitleWidget::HandleSettingsClose);
		SettingsScroll->AddChild(SettingsScreen);
	}
	if (Settings) Settings->OpenSettings();
	const float Scale = FMath::Max(.1f, UWidgetLayoutLibrary::GetViewportScale(this));
	SettingsSize->SetMaxDesiredHeight(FMath::Max(200.f, UWidgetLayoutLibrary::GetViewportSize(this).Y / Scale - 60.f));
	SettingsLayer->SetVisibility(ESlateVisibility::Visible);
	SettingsScreen->RefreshTheme();
	SettingsScreen->RefreshValues();
	FocusRoot();
	UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 설정 창 열기 (게임 안 설정 화면 재사용)"));
}

void UCozyTitleWidget::HandleSettingsClose(UCozyUiScreen*)
{
	CloseSettings();
}

void UCozyTitleWidget::CloseSettings()
{
	if (!SettingsLayer->IsVisible()) return;
	if (UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this)) Settings->CloseSettings();
	SettingsLayer->SetVisibility(ESlateVisibility::Collapsed);
	FocusRoot();
	UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 설정 창 닫기 → 타이틀로"));
}

void UCozyTitleWidget::ShowModal(EModal Kind, const FText& Title, const FText& Body, const TArray<TPair<FName, FText>>& InButtons, int32 DefaultIndex)
{
	Modal = Kind;
	ModalTitle->SetText(Title);
	ModalBody->SetText(Body);
	ModalIds.Reset();
	for (int32 I = 0; I < ModalButtons.Num(); ++I)
	{
		const bool bUsed = InButtons.IsValidIndex(I);
		ModalButtons[I]->GetParent()->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bUsed) { ModalLabels[I]->SetText(InButtons[I].Value); ModalIds.Add(InButtons[I].Key); }
	}
	ModalButtonRow->SetVisibility(InButtons.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ModalSelected = FMath::Clamp(DefaultIndex, 0, FMath::Max(0, ModalIds.Num() - 1));
	ModalLayer->SetVisibility(ESlateVisibility::Visible);
	UpdateModalVisuals();
	FocusRoot();
}

void UCozyTitleWidget::HideModal()
{
	Modal = EModal::None;
	ModalLayer->SetVisibility(ESlateVisibility::Collapsed);
	FocusRoot();
}

void UCozyTitleWidget::UpdateModalVisuals()
{
	for (int32 I = 0; I < ModalLabels.Num(); ++I)
	{
		const bool bSel = I == ModalSelected;
		ModalLabels[I]->SetColorAndOpacity(FSlateColor(bSel ? ThemeColor(ECozyUiColor::Point, FLinearColor(.75f, .25f, .2f)) : ThemeColor(ECozyUiColor::Ink, FLinearColor(.2f, .14f, .1f))));
		ModalButtons[I]->GetParent()->SetRenderScale(bSel ? FVector2D(1.06f, 1.06f) : FVector2D(1.f, 1.f));
		ModalButtons[I]->GetParent()->SetRenderTransformPivot(FVector2D(.5f, .5f));
	}
}

void UCozyTitleWidget::ClickModal(int32 Index)
{
	if (!ModalIds.IsValidIndex(Index)) return;
	ModalSelected = Index;
	FocusRoot();
	ActivateModal(ModalIds[Index]);
}

void UCozyTitleWidget::OnModalClicked0() { ClickModal(0); }
void UCozyTitleWidget::OnModalClicked1() { ClickModal(1); }
void UCozyTitleWidget::OnModalHovered0() { ModalSelected = 0; UpdateModalVisuals(); }
void UCozyTitleWidget::OnModalHovered1() { if (ModalIds.Num() > 1) { ModalSelected = 1; UpdateModalVisuals(); } }

void UCozyTitleWidget::ActivateModal(FName Id)
{
	UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 확인 창 버튼: %s"), *Id.ToString());
	if (Id == CozyTitle::Cancel || Id == CozyTitle::Close)
	{
		const bool bWasNewGame = Modal == EModal::NewGameConfirm;
		HideModal();
		if (bWasNewGame) UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 새 게임 취소 · 저장 파일 그대로"));
		if (Id == CozyTitle::Cancel && !bWasNewGame) UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 게임 종료 취소"));
		return;
	}
	if (Id == CozyTitle::ConfirmQuit)
	{
		HideModal();
		UE_LOG(LogCozyRealm, Log, TEXT("[타이틀] 게임 종료 (저장하지 않음 · 타이틀에는 저장할 영지가 없음)"));
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		return;
	}
	if (Id == CozyTitle::ConfirmNew)
	{
		FString Folder, Error;
		if (!UCozyEstateSubsystem::ArchiveSaveForNewGame(Folder, Error))
		{
			ShowModal(EModal::LoadError, LOCTEXT("ArchiveFail", "새 게임을 시작하지 못했습니다"), FText::Format(LOCTEXT("ArchiveFailBody", "{0}\n기존 저장 파일은 그대로 있습니다."), FText::FromString(Error)),
				{ { CozyTitle::Close, LOCTEXT("Close3", "닫기") } }, 0);
			return;
		}
		HideModal();
		EnterEstate(FString::Printf(TEXT("새 게임 · 기존 저장 백업 %s"), *Folder));
		return;
	}
	if (Id == CozyTitle::Retry)
	{
		RefreshSaveState();
		if (SaveState == ECozySaveProbe::Invalid)
		{
			const FDateTime Now = FDateTime::Now();
			ModalBody->SetText(FText::Format(LOCTEXT("StillBroken", "{0}\n\n아직 읽을 수 없습니다 ({1}:{2} 다시 확인).\n저장 파일은 지우거나 덮어쓰지 않았습니다."),
				FText::FromString(SaveDetail), FText::AsNumber(Now.GetHour()), FText::FromString(FString::Printf(TEXT("%02d"), Now.GetMinute()))));
			return;
		}
		HideModal();
		StatusText->SetText(SaveState == ECozySaveProbe::None ? LOCTEXT("RetryNone", "저장 파일이 없습니다 · 새 게임으로 시작할 수 있습니다") : FText::Format(LOCTEXT("RetryOk", "다시 읽었습니다 · {0}"), StatusText->GetText()));
		return;
	}
}

void UCozyTitleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 배경(실제 영지 풍경)이 보이면 종이 바탕을 천천히 걷는다
	const ACozyTitlePlayerController* Controller = Cast<ACozyTitlePlayerController>(GetOwningPlayer());
	if (CoverAlpha > 0.f && Controller && Controller->IsBackgroundVisible())
	{
		CoverAlpha = FMath::Max(0.f, CoverAlpha - InDeltaTime / .8f);
		PaperCover->SetRenderOpacity(CoverAlpha);
	}
	if (Modal == EModal::Loading)
	{
		const int32 Dots = int32(FPlatformTime::Seconds() * 2.5) % 4;
		ModalBody->SetText(FText::FromString(LoadingBase.ToString() + FString::ChrN(Dots, TEXT('.')) + FString::ChrN(3 - Dots, TEXT(' '))));
		// 맵이 열리지 않고 오래 머물면 안내 후 다시 누를 수 있게
		if (bEntering && FPlatformTime::Seconds() - EnterStartTime > 20.0)
		{
			bEntering = false;
			UpdateButtonVisuals();
			ShowModal(EModal::EnterFailed, LOCTEXT("EnterFailTitle", "마을에 들어가지 못했습니다"), LOCTEXT("EnterFailBody", "영지 맵을 여는 데 실패했습니다. 저장 파일은 바뀌지 않았습니다. 다시 눌러 주세요."),
				{ { CozyTitle::Close, LOCTEXT("Close4", "닫기") } }, 0);
		}
	}
}

FReply UCozyTitleWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FocusRoot();
	return FReply::Unhandled();
}

FReply UCozyTitleWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bUp = Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up;
	const bool bDown = Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down;
	const bool bLeft = Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left;
	const bool bRight = Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right;
	const bool bAccept = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
	const bool bBack = Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right;
	if (Modal == EModal::Loading) return FReply::Handled();
	if (Modal != EModal::None)
	{
		if ((bLeft || bUp) && ModalIds.Num() > 1) { ModalSelected = (ModalSelected + ModalIds.Num() - 1) % ModalIds.Num(); UpdateModalVisuals(); }
		else if ((bRight || bDown) && ModalIds.Num() > 1) { ModalSelected = (ModalSelected + 1) % ModalIds.Num(); UpdateModalVisuals(); }
		else if (bAccept && ModalIds.IsValidIndex(ModalSelected)) ActivateModal(ModalIds[ModalSelected]);
		else if (bBack) ActivateModal(ModalIds.Contains(CozyTitle::Cancel) ? CozyTitle::Cancel : CozyTitle::Close);
		return FReply::Handled();
	}
	if (SettingsLayer->IsVisible())
	{
		if (bBack)
		{
			UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this);
			if (!(Settings && Settings->HasDialog() && Settings->CancelDialog())) CloseSettings();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	if (bEntering) return FReply::Handled();
	if (bUp) { SetSelected(Selected - 1); return FReply::Handled(); }
	if (bDown) { SetSelected(Selected + 1); return FReply::Handled(); }
	if (bAccept) { ActivateSelected(); return FReply::Handled(); }
	if (bBack) { Activate(CozyTitle::Quit); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
