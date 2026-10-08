#include "UI/Kit/CozyUiWidgets.h"
#include "UI/Kit/CozyUiScreen.h"
#include "UI/Kit/CozyUiTheme.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "CozyUi"

FText CozyUiFormat::Duration(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	return Total >= 3600
		? FText::FromString(FString::Printf(TEXT("%d:%02d:%02d"), Total / 3600, (Total / 60) % 60, Total % 60))
		: FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}

namespace
{
	FMargin PaddingFor(const UCozyUiTheme& Theme, int32 Role, const FMargin& Current)
	{
		switch (Role)
		{
		case 0: return Theme.Metrics.WindowPadding;
		case 1: return Theme.Metrics.PanelPadding;
		case 2: return Theme.Metrics.ButtonPadding;
		default: return Current;
		}
	}

	FText FormatNumber(float Value)
	{
		return FText::AsNumber(FMath::RoundToInt(Value));
	}
}

// ---------------------------------------------------------------------------
// 글자

void UCozyUiText::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	if (!Screen.GetTheme())
	{
		return;
	}
	FSlateFontInfo FontInfo = Screen.GetFont(Role);
	if (SizeOverride > 0)
	{
		FontInfo.Size = SizeOverride;
	}
	if (FontInfo.HasValidFont())
	{
		SetFont(FontInfo);
	}
	if (ColorRole != ECozyUiColor::None)
	{
		SetColorAndOpacity(FSlateColor(Screen.GetColor(ColorRole)));
	}
}

void UCozyUiText::UpdateCozyValue(const UCozyUiScreen& Screen)
{
	if (Value == ECozyUiValue::None)
	{
		return;
	}
	const FCozyUiValueResult R = Screen.GetValue(Value, ValueParam);
	if (!R.bValid)
	{
		return;
	}
	FFormatOrderedArguments Args;
	Args.Add(FormatNumber(R.Current));
	Args.Add(FormatNumber(R.Max));
	Args.Add(FText::Format(LOCTEXT("Pct", "{0}%"), FText::AsNumber(FMath::RoundToInt(R.Ratio() * 100.f))));
	Args.Add(CozyUiFormat::Duration(R.RemainingSeconds));
	Args.Add(R.Text);
	SetText(FText::Format(Format, Args));
}

// ---------------------------------------------------------------------------
// 이미지 · 배경 상자

void UCozyUiImage::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	const UCozyUiTheme* Theme = Screen.GetTheme();
	if (!Theme)
	{
		return;
	}
	if (const FSlateBrush* ThemeBrush = Theme->FindImage(ImageName))
	{
		FSlateBrush Copy = *ThemeBrush;
		const FVector2D Box = bUseThemeIconSize ? Theme->Metrics.IconSize : FitBox;
		if (!Box.IsNearlyZero())
		{
			Copy.ImageSize = CozyUiFit::Resolve(*ThemeBrush, Box, Fit);
		}
		SetBrush(Copy);
	}
	SetColorAndOpacity(TintRole == ECozyUiColor::None ? FLinearColor::White : Screen.GetColor(TintRole));
}

void UCozyUiBorder::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	const UCozyUiTheme* Theme = Screen.GetTheme();
	if (!Theme)
	{
		return;
	}
	if (const FSlateBrush* ThemeBrush = Theme->FindImage(ImageName))
	{
		SetBrush(*ThemeBrush);
	}
	SetBrushColor(TintRole == ECozyUiColor::None ? FLinearColor::White : Screen.GetColor(TintRole));
	SetPadding(PaddingFor(*Theme, PaddingRole, GetPadding()));
}

// ---------------------------------------------------------------------------
// 이미지 맞춤

FVector2D CozyUiFit::Resolve(const FSlateBrush& Brush, const FVector2D& Box, ECozyUiImageFit Fit)
{
	// 원래 크기: 텍스처 크기 (없으면 테마에 적은 이미지 크기)
	FVector2D Native = Brush.ImageSize;
	if (const UTexture2D* Texture = Cast<UTexture2D>(Brush.GetResourceObject()))
	{
		if (Texture->GetSizeX() > 0 && Texture->GetSizeY() > 0)
		{
			Native = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		}
	}
	if (Fit == ECozyUiImageFit::Original || Box.X <= 0.f || Box.Y <= 0.f || Native.X <= 0.f || Native.Y <= 0.f)
	{
		return Native;
	}
	if (Fit == ECozyUiImageFit::Stretch)
	{
		return Box;
	}
	const float Scale = FMath::Min(Box.X / Native.X, Box.Y / Native.Y);
	return Native * Scale;
}

namespace
{
	EHorizontalAlignment ToHAlign(ECozyUiAlign Align)
	{
		switch (Align)
		{
		case ECozyUiAlign::Start: return HAlign_Left;
		case ECozyUiAlign::Center: return HAlign_Center;
		case ECozyUiAlign::End: return HAlign_Right;
		default: return HAlign_Fill;
		}
	}

	EVerticalAlignment ToVAlign(ECozyUiAlign Align)
	{
		switch (Align)
		{
		case ECozyUiAlign::Start: return VAlign_Top;
		case ECozyUiAlign::Center: return VAlign_Center;
		case ECozyUiAlign::End: return VAlign_Bottom;
		default: return VAlign_Fill;
		}
	}

	FText FormatValue(const FText& Format, const FCozyUiValueResult& R)
	{
		FFormatOrderedArguments Args;
		Args.Add(FormatNumber(R.Current));
		Args.Add(FormatNumber(R.Max));
		Args.Add(FText::Format(LOCTEXT("Pct2", "{0}%"), FText::AsNumber(FMath::RoundToInt(R.Ratio() * 100.f))));
		Args.Add(CozyUiFormat::Duration(R.RemainingSeconds));
		Args.Add(R.Text);
		return FText::Format(Format, Args);
	}
}

// ---------------------------------------------------------------------------
// 화면 구성 요소

void UCozyUiElementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button)
	{
		Button->OnClicked.AddUniqueDynamic(this, &UCozyUiElementWidget::HandleClicked);
	}
}

void UCozyUiElementWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (UCozyUiScreen* Screen = GetTypedOuter<UCozyUiScreen>())
	{
		ApplyCozyTheme(*Screen);
		UpdateCozyValue(*Screen);
	}
}

void UCozyUiElementWidget::HandleClicked()
{
	if (UCozyUiScreen* Screen = GetTypedOuter<UCozyUiScreen>())
	{
		Screen->RunAction(Entry);
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void UCozyUiElementWidget::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	// 내용: 영역이 준 줄 > 화면 설정의 같은 Id 줄 > 디자이너 기본값
	if (!bFromArea)
	{
		const FCozyUiElementEntry* Found = Screen.FindElement(ElementId);
		Entry = Found ? *Found : Defaults;
		if (Entry.Id.IsNone())
		{
			Entry.Id = ElementId;
		}
	}
	const UCozyUiTheme* Theme = Screen.GetTheme();
	if (!Theme)
	{
		return;
	}

	if (Button)
	{
		if (const FButtonStyle* Style = Theme->FindButtonStyle(Entry.Style))
		{
			Button->SetStyle(*Style);
		}
		Button->SetToolTipText(Entry.Action == ECozyUiAction::None ? FText::GetEmpty() : UCozyUiScreen::GetActionName(Entry.Action));
	}
	if (Background)
	{
		if (const FSlateBrush* Brush = Theme->FindImage(Entry.Background))
		{
			Background->SetBrush(*Brush);
			Background->SetBrushColor(FLinearColor::White);
		}
		else
		{
			Background->SetBrushColor(FLinearColor::Transparent);
		}
		Background->SetPadding(Entry.Kind == ECozyUiElementKind::Panel ? Theme->Metrics.WindowPadding : Theme->Metrics.PanelPadding);
	}

	// 이미지: 칸 크기는 고정, 이미지는 칸 안에 맞춤 방법대로 (비율이 다른 이미지로 바꿔도 줄이 흐트러지지 않음)
	const FSlateBrush* ImageBrush = Theme->FindImage(Entry.Image);
	const FVector2D Box = Entry.ImageBox.IsNearlyZero() ? Theme->Metrics.IconSize : Entry.ImageBox;
	if (Icon)
	{
		if (ImageBrush)
		{
			FSlateBrush Copy = *ImageBrush;
			Copy.ImageSize = CozyUiFit::Resolve(*ImageBrush, Box, Entry.ImageFit);
			Copy.DrawAs = ESlateBrushDrawType::Image;
			Icon->SetBrush(Copy);
			Icon->SetColorAndOpacity(Entry.ImageTint == ECozyUiColor::None ? FLinearColor::White : Screen.GetColor(Entry.ImageTint));
		}
		Icon->SetVisibility(ImageBrush ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (USizeBoxSlot* IconSlot = Cast<USizeBoxSlot>(Icon->Slot))
		{
			const bool bFill = Entry.ImageFit == ECozyUiImageFit::Stretch;
			IconSlot->SetHorizontalAlignment(bFill ? HAlign_Fill : HAlign_Center);
			IconSlot->SetVerticalAlignment(bFill ? VAlign_Fill : VAlign_Center);
		}
	}
	if (IconBox)
	{
		if (Entry.ImageFit == ECozyUiImageFit::Original)
		{
			IconBox->ClearWidthOverride();
			IconBox->ClearHeightOverride();
		}
		else
		{
			IconBox->SetWidthOverride(Box.X);
			IconBox->SetHeightOverride(Box.Y);
		}
		IconBox->SetVisibility(ImageBrush ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Label)
	{
		const ECozyUiTextRole Role = Entry.Kind == ECozyUiElementKind::Button ? ECozyUiTextRole::Button : Entry.TextRole;
		const FSlateFontInfo Font = Screen.GetFont(Role);
		if (Font.HasValidFont())
		{
			Label->SetFont(Font);
		}
		const bool bEnabled = !Button || Button->GetIsEnabled();
		Label->SetColorAndOpacity(FSlateColor(bEnabled ? Screen.GetColor(Entry.TextColor) : Theme->DisabledTextColor));
		Label->SetAutoWrapText(Entry.bWrapText);
		if (Entry.Value == ECozyUiValue::None)
		{
			Label->SetText(Entry.Label);
		}
		Label->SetVisibility(Entry.Label.IsEmpty() && Entry.Value == ECozyUiValue::None ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Bar)
	{
		if (const FCozyUiGaugeStyle* GaugeStyle = Theme->FindGaugeStyle(Entry.Style))
		{
			Bar->SetWidgetStyle(GaugeStyle->Style);
			bTintByState = GaugeStyle->bTintByState;
		}
		Bar->SetBarFillType(Entry.FillType);
		if (IsDesignTime())
		{
			Bar->SetPercent(0.6f);
		}
	}
	if (ValueText)
	{
		const FSlateFontInfo Font = Screen.GetFont(ECozyUiTextRole::Small);
		if (Font.HasValidFont())
		{
			ValueText->SetFont(Font);
		}
		ValueText->SetColorAndOpacity(FSlateColor(Screen.GetColor(ECozyUiColor::Ink)));
	}

	if (ChildArea)
	{
		ChildArea->AreaId = Entry.ChildArea;
		ChildArea->ApplyCozyTheme(Screen);
	}

	// 크기: 고정이면 SizeBox로 · 내용에 맞춤이면 비움 (자유 배치 요소는 디자이너 슬롯 크기도 함께 작동)
	if (SizeBox)
	{
		FVector2D Want = Entry.SizeMode == ECozyUiSizeMode::Fixed ? Entry.Size : FVector2D::ZeroVector;
		if (Entry.SizeMode == ECozyUiSizeMode::Content && Entry.Kind == ECozyUiElementKind::Button)
		{
			Want = Theme->Metrics.ButtonSize;
		}
		if (Want.X > 0.f) { SizeBox->SetWidthOverride(Want.X); } else { SizeBox->ClearWidthOverride(); }
		if (Want.Y > 0.f) { SizeBox->SetHeightOverride(Want.Y); } else { SizeBox->ClearHeightOverride(); }
	}
	// 요소 자신은 마우스를 통과시키고, 안의 Button만 클릭을 받는다
	SetVisibility(Entry.bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UCozyUiElementWidget::UpdateCozyValue(const UCozyUiScreen& Screen)
{
	if (Entry.Value == ECozyUiValue::None)
	{
		return;
	}
	const FCozyUiValueResult R = Screen.GetValue(Entry.Value, Entry.ValueParam);
	if (!R.bValid)
	{
		return;
	}
	if (Label)
	{
		Label->SetText(Entry.Label.IsEmpty() ? FormatNumber(R.Current) : FormatValue(Entry.Label, R));
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Bar)
	{
		Bar->SetPercent(R.Ratio());
		if (Entry.FillColor != ECozyUiColor::None)
		{
			Bar->SetFillColorAndOpacity(Screen.GetColor(Entry.FillColor));
		}
		else
		{
			Bar->SetFillColorAndOpacity(bTintByState ? Screen.GetColor(R.StateColor) : FLinearColor::White);
		}
	}
	if (ValueText)
	{
		// 막대 안에는 짧은 숫자만 · 긴 상태 설명은 제목 줄({4})에서 줄바꿈으로 보여 준다
		TArray<FString> Parts;
		if (Entry.bShowNumber)
		{
			Parts.Add(R.Max > 0.f ? FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(R.Current), FMath::RoundToInt(R.Max)) : FString::FromInt(FMath::RoundToInt(R.Current)));
		}
		if (Entry.bShowPercent)
		{
			Parts.Add(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(R.Ratio() * 100.f)));
		}
		if (Entry.bShowRemaining && R.RemainingSeconds > 0.f)
		{
			Parts.Add(CozyUiFormat::Duration(R.RemainingSeconds).ToString());
		}
		ValueText->SetText(FText::FromString(FString::Join(Parts, TEXT(" · "))));
		ValueText->SetVisibility(Parts.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------------------
// 자동 정렬 영역

void UCozyUiArea::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	if (!Host || !WidgetTree)
	{
		return;
	}
	Host->SetBrushColor(FLinearColor::Transparent);
	// 영역의 빈 곳은 클릭을 통과시킨다 (안의 버튼만 입력)
	Host->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	FCozyUiAreaLayout Layout;
	if (const FCozyUiAreaLayout* Found = Screen.FindArea(AreaId))
	{
		Layout = *Found;
	}
	const UCozyUiTheme* Theme = Screen.GetTheme();
	const float Gap = Layout.Gap >= 0.f ? Layout.Gap : (Theme ? Theme->Metrics.Gap : 8.f);
	const TArray<FCozyUiElementEntry> Entries = Screen.GetElementsForArea(AreaId);

	// 구성(요소 목록 · 정렬 방식)이 바뀐 경우에만 다시 만듦 · 내용만 바뀌면 기존 요소에 다시 적용
	FString Signature = FString::Printf(TEXT("%d|%d|%d|%d|%.1f|%s|"), (int32)Layout.Flow, (int32)Layout.Spread, (int32)Layout.PackAlign, (int32)Layout.ItemAlign, Gap,
		*FString::Printf(TEXT("%.0f,%.0f,%.0f,%.0f"), Layout.Padding.Left, Layout.Padding.Top, Layout.Padding.Right, Layout.Padding.Bottom));
	for (const FCozyUiElementEntry& Each : Entries)
	{
		if (Each.bVisible)
		{
			Signature += FString::Printf(TEXT("%s:%d:%s;"), *Each.Id.ToString(), (int32)Each.Kind, *Each.TemplateOverride.ToString());
		}
	}
	if (Signature == BuiltSignature && Spawned.Num() > 0)
	{
		int32 Index = 0;
		for (const FCozyUiElementEntry& Each : Entries)
		{
			if (Each.bVisible && Spawned.IsValidIndex(Index) && Spawned[Index])
			{
				Spawned[Index]->SetEntry(Each, true);
				Spawned[Index]->ApplyCozyTheme(Screen);
				++Index;
			}
		}
		return;
	}
	BuiltSignature = Signature;
	Spawned.Reset();
	Host->SetPadding(Layout.Padding);

	// 정렬 상자: 가로 · 세로 · 줄바꿈
	UPanelWidget* Box = nullptr;
	if (Layout.Flow == ECozyUiFlow::Wrap)
	{
		UWrapBox* Wrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		Wrap->SetInnerSlotPadding(FVector2D(Gap, Gap));
		Wrap->SetHorizontalAlignment(ToHAlign(Layout.PackAlign == ECozyUiAlign::Fill ? ECozyUiAlign::Start : Layout.PackAlign));
		Box = Wrap;
	}
	else if (Layout.Spread == ECozyUiSpread::EqualSize)
	{
		// 같은 크기: 균일 격자 (모든 칸이 가장 큰 요소 크기 · 영역이 좁아도 글자가 겹치지 않음)
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		Grid->SetSlotPadding(FMargin(Gap * 0.5f));
		Box = Grid;
	}
	else if (Layout.Flow == ECozyUiFlow::Vertical)
	{
		Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	}
	else
	{
		Box = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	}
	Host->SetContent(Box);

	// 묶음 위치: 붙여서 놓을 때만 영역 안에서 앞·가운데·뒤 · 같은 크기·균등 간격·줄바꿈은 영역을 꽉 채움
	const bool bHorizontal = Layout.Flow != ECozyUiFlow::Vertical;
	const bool bFillMain = Layout.Spread == ECozyUiSpread::EqualGap || Layout.Flow == ECozyUiFlow::Wrap
		|| (Layout.Spread == ECozyUiSpread::EqualSize && Layout.PackAlign == ECozyUiAlign::Fill);
	if (bHorizontal)
	{
		Host->SetHorizontalAlignment(bFillMain ? HAlign_Fill : ToHAlign(Layout.PackAlign));
		Host->SetVerticalAlignment(ToVAlign(Layout.ItemAlign));
	}
	else
	{
		Host->SetVerticalAlignment(bFillMain ? VAlign_Fill : ToVAlign(Layout.PackAlign));
		Host->SetHorizontalAlignment(ToHAlign(Layout.ItemAlign));
	}

	for (const FCozyUiElementEntry& Each : Entries)
	{
		if (!Each.bVisible)
		{
			continue;
		}
		TSubclassOf<UCozyUiElementWidget> Class = Each.TemplateOverride.LoadSynchronous();
		if (!Class && Theme)
		{
			if (const TSoftClassPtr<UCozyUiElementWidget>* Template = Theme->ElementTemplates.Find(Each.Kind))
			{
				Class = Template->LoadSynchronous();
			}
		}
		if (!Class)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UI] 영역 %s: 요소 %s의 모양 틀이 테마에 없습니다"), *AreaId.ToString(), *Each.Id.ToString());
			continue;
		}
		UCozyUiElementWidget* NewElement = CreateWidget<UCozyUiElementWidget>(this, Class);
		if (!NewElement)
		{
			continue;
		}
		NewElement->SetEntry(Each, true);

		// 균등 간격: 요소 사이에 늘어나는 빈칸
		if (Layout.Spread == ECozyUiSpread::EqualGap && Spawned.Num() > 0 && Layout.Flow != ECozyUiFlow::Wrap)
		{
			USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
			UPanelSlot* SpacerSlot = Box->AddChild(Spacer);
			if (UHorizontalBoxSlot* H = Cast<UHorizontalBoxSlot>(SpacerSlot)) { H->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
			if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(SpacerSlot)) { V->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
		}

		UPanelSlot* NewSlot = Box->AddChild(NewElement);
		const bool bFirst = Spawned.Num() == 0;
		const float Lead = (bFirst || Layout.Spread == ECozyUiSpread::EqualGap) ? 0.f : Gap;
		if (UHorizontalBoxSlot* H = Cast<UHorizontalBoxSlot>(NewSlot))
		{
			H->SetPadding(FMargin(Lead, 0.f, 0.f, 0.f));
			H->SetVerticalAlignment(ToVAlign(Layout.ItemAlign));
			H->SetHorizontalAlignment(HAlign_Fill);
			H->SetSize(FSlateChildSize(Layout.Spread == ECozyUiSpread::EqualSize ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		}
		else if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(NewSlot))
		{
			V->SetPadding(FMargin(0.f, Lead, 0.f, 0.f));
			V->SetHorizontalAlignment(ToHAlign(Layout.ItemAlign));
			V->SetVerticalAlignment(VAlign_Fill);
			V->SetSize(FSlateChildSize(Layout.Spread == ECozyUiSpread::EqualSize ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		}
		else if (UWrapBoxSlot* W = Cast<UWrapBoxSlot>(NewSlot))
		{
			W->SetVerticalAlignment(ToVAlign(Layout.ItemAlign));
		}
		else if (UUniformGridSlot* G = Cast<UUniformGridSlot>(NewSlot))
		{
			const int32 Index = Spawned.Num();
			G->SetRow(bHorizontal ? 0 : Index);
			G->SetColumn(bHorizontal ? Index : 0);
			G->SetHorizontalAlignment(HAlign_Fill);
			G->SetVerticalAlignment(VAlign_Fill);
		}
		Spawned.Add(NewElement);
		NewElement->ApplyCozyTheme(Screen);
		NewElement->UpdateCozyValue(Screen);
	}
}

#undef LOCTEXT_NAMESPACE
