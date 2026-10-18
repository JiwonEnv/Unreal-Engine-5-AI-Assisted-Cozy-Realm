#include "UI/Kit/CozyUiWidgets.h"
#include "UI/Kit/CozyUiScreen.h"
#include "UI/Kit/CozyUiTheme.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/WrapBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
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
			// 공유 시트에서는 전체 시트가 아니라 해당 아이콘 칸의 비율과 원래 크기를 사용한다.
			const FBox2f UVRegion = Brush.GetUVRegion();
			Native = UVRegion.bIsValid
				? FVector2D(Brush.ImageSize)
				: FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
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

FCozyUiElementEntry CozyUiRow::Resolve(const FCozyUiElementEntry& In, FName RowId, const FText& RowName)
{
	if (RowId.IsNone() && RowName.IsEmpty())
	{
		return In;
	}
	FCozyUiElementEntry Out = In;
	const FString Id = RowId.ToString();
	auto Swap = [&Id](FName& Name)
	{
		const FString Text = Name.ToString();
		if (Text.Contains(TEXT("@Row")))
		{
			Name = FName(*Text.Replace(TEXT("@Row"), *Id));
		}
	};
	Swap(Out.ValueParam);
	Swap(Out.ActionParam);
	Swap(Out.Image);
	Swap(Out.Background);
	const FString Label = In.Label.ToString();
	if (Label.Contains(TEXT("{Row}")))
	{
		Out.Label = FText::FromString(Label.Replace(TEXT("{Row}"), *RowName.ToString()));
	}
	return Out;
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
		// 지금 할 수 없는 동작이면 버튼을 끄고 이유를 툴팁으로 (예: 수령할 것이 없음 · 잠긴 작물)
		FText Why;
		const bool bCan = Screen.CanRunAction(Entry, Why);
		Button->SetIsEnabled(bCan);
		Button->SetToolTipText(!bCan && !Why.IsEmpty() ? Why : (Entry.Action == ECozyUiAction::None ? FText::GetEmpty() : UCozyUiScreen::GetActionName(Entry.Action)));
	}
	if (Background)
	{
		// 반복 목록의 Background는 각 줄의 배경 → 목록 자신은 투명
		const FSlateBrush* Brush = Entry.Kind == ECozyUiElementKind::List ? nullptr : Theme->FindImage(Entry.Background);
		if (Brush)
		{
			Background->SetBrush(*Brush);
			Background->SetBrushColor(FLinearColor::White);
		}
		else
		{
			Background->SetBrushColor(FLinearColor::Transparent);
		}
		ECozyUiPadding Role = Entry.PaddingRole;
		if (Role == ECozyUiPadding::Auto)
		{
			Role = !Brush ? ECozyUiPadding::None : (Entry.Kind == ECozyUiElementKind::Panel ? ECozyUiPadding::Window : ECozyUiPadding::Panel);
		}
		switch (Role)
		{
		case ECozyUiPadding::Window: Background->SetPadding(Theme->Metrics.WindowPadding); break;
		case ECozyUiPadding::Panel: Background->SetPadding(Theme->Metrics.PanelPadding); break;
		case ECozyUiPadding::Button: Background->SetPadding(Theme->Metrics.ButtonPadding); break;
		default: Background->SetPadding(FMargin(0.f)); break;
		}
	}

	// 이미지: 칸 크기는 고정, 이미지는 칸 안에 맞춤 방법대로 (비율이 다른 이미지로 바꿔도 줄이 흐트러지지 않음)
	const FSlateBrush* ImageBrush = Screen.FindImage(Entry.Image);
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
		// 이미지 이름은 있는데 테마에 없으면 Hidden (칸 유지) · SizeBox는 안쪽이 Collapsed면 크기 0이 되어 줄이 밀림
		Icon->SetVisibility(ImageBrush ? ESlateVisibility::HitTestInvisible : (Entry.Image.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::Hidden));
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
		// 이미지 이름은 있는데 테마에 그 이미지가 없으면 칸은 남겨 둔다 (목록 줄의 정렬 유지)
		IconBox->SetVisibility(ImageBrush ? ESlateVisibility::HitTestInvisible : (Entry.Image.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::Hidden));
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
		if (Entry.TextColor != ECozyUiColor::None || !bEnabled)
		{
			Label->SetColorAndOpacity(FSlateColor(bEnabled ? Screen.GetColor(Entry.TextColor) : Theme->DisabledTextColor));
		}
		Label->SetAutoWrapText(Entry.bWrapText);
		// 줄바꿈 글이 가로 상자(칩·버튼 안) 안에 있으면 남은 폭을 다 받게 한다
		// (내용 맞춤이면 직전 폭에 갇혀, 글이 길어질 때 그 좁은 폭에서 접힘 · 예: 미리보기 후 가공 창 '필요 재료' 칩)
		if (Entry.bWrapText)
		{
			if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(Label->Slot))
			{
				LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
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
		ChildArea->SetRowContext(RowId, RowName);
		if (Entry.Kind == ECozyUiElementKind::List)
		{
			ChildArea->SetList(Entry.ListSource, Entry.RowArea, Entry.Background);
		}
		else
		{
			ChildArea->SetList(ECozyUiListSource::None, NAME_None, NAME_None);
		}
		ChildArea->ApplyCozyTheme(Screen);
	}

	// 크기: 고정이면 SizeBox로 · 내용에 맞춤이면 비움 (자유 배치 요소는 디자이너 슬롯 크기도 함께 작동)
	if (SizeBox)
	{
		FVector2D Want = Entry.SizeMode == ECozyUiSizeMode::Fixed ? Entry.Size : FVector2D::ZeroVector;
		// 창 크기 종류: 폭은 테마의 창 폭 (테마에서 숫자 하나로 같은 종류의 창을 모두 바꿈) · 높이는 Size의 Y
		const float WindowWidth = Entry.SizeMode == ECozyUiSizeMode::WindowSmall ? Theme->Metrics.WindowSmallWidth
			: Entry.SizeMode == ECozyUiSizeMode::WindowMedium ? Theme->Metrics.WindowMediumWidth
			: Entry.SizeMode == ECozyUiSizeMode::WindowLarge ? Theme->Metrics.WindowLargeWidth : 0.f;
		if (WindowWidth > 0.f)
		{
			Want = FVector2D(WindowWidth, Entry.Size.Y);
		}
		if (Entry.SizeMode == ECozyUiSizeMode::Content && Entry.Kind == ECozyUiElementKind::Button)
		{
			// 내용 맞춤 버튼은 최소 크기만 유지하고 긴 글자·아이콘에 따라 늘어난다.
			SizeBox->SetMinDesiredWidth(Theme->Metrics.ButtonSize.X);
			SizeBox->SetMinDesiredHeight(Theme->Metrics.ButtonSize.Y);
		}
		else
		{
			SizeBox->ClearMinDesiredWidth();
			SizeBox->ClearMinDesiredHeight();
		}
		if (Want.X > 0.f) { SizeBox->SetWidthOverride(Want.X); } else { SizeBox->ClearWidthOverride(); }
		if (Want.Y > 0.f) { SizeBox->SetHeightOverride(Want.Y); } else { SizeBox->ClearHeightOverride(); }
	}
	// 요소 자신은 마우스를 통과시키고, 안의 Button만 클릭을 받는다
	// 작은 창 모드 규칙은 디자이너에 놓은 요소(자유 배치)에도 적용 (영역이 만든 요소는 영역에서 이미 걸러짐)
	const bool bShown = Entry.bVisible && (bFromArea || Screen.IsElementShown(Entry.Id));
	SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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
	if (Button && Entry.Action != ECozyUiAction::None)
	{
		FText Why;
		Button->SetIsEnabled(Screen.CanRunAction(Entry, Why));
	}
	if (Label)
	{
		const FText Shown = Entry.Label.IsEmpty() ? FormatNumber(R.Current) : FormatValue(Entry.Label, R);
		Label->SetText(Shown);
		// 값 글자가 비면 줄도 접음 (예: 바로가기로 열지 않은 나가야의 '배치할 시설' · 아직 없는 '방금 한 일')
		Label->SetVisibility(Shown.IsEmptyOrWhitespace() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (Entry.TextColor == ECozyUiColor::None)
		{
			// 글자 색 None = 값의 상태색 (예: 창고가 가득 차면 경고색)
			Label->SetColorAndOpacity(FSlateColor(Screen.GetColor(R.StateColor)));
		}
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
// 슬라이더

TSharedRef<SWidget> UCozyUiSlider::RebuildWidget()
{
	// 모양 틀(WBP) 없이 코드로: 크기 상자 → 슬라이더
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox"));
		SliderWidget = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider"));
		SliderWidget->SetMinValue(0.f);
		SliderWidget->SetMaxValue(1.f);
		SliderWidget->SetStepSize(0.01f);
		SliderWidget->SetIndentHandle(true);
		SliderWidget->OnValueChanged.AddUniqueDynamic(this, &UCozyUiSlider::HandleValueChanged);
		SliderWidget->OnMouseCaptureBegin.AddUniqueDynamic(this, &UCozyUiSlider::HandleCaptureBegin);
		SliderWidget->OnMouseCaptureEnd.AddUniqueDynamic(this, &UCozyUiSlider::HandleCaptureEnd);
		SizeBox->SetContent(SliderWidget);
		WidgetTree->RootWidget = SizeBox;
	}
	return Super::RebuildWidget();
}

void UCozyUiSlider::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	Super::ApplyCozyTheme(Screen);
	const FCozyUiElementEntry& Data = GetEntry();
	// 크기를 비우면 기본 크기 · 남은 공간 채우기면 높이만 (폭은 줄의 남은 공간)
	if (SizeBox && Data.SizeMode == ECozyUiSizeMode::Content)
	{
		SizeBox->SetWidthOverride(320.f);
		SizeBox->SetHeightOverride(36.f);
	}
	else if (SizeBox && Data.SizeMode == ECozyUiSizeMode::Fill)
	{
		SizeBox->ClearWidthOverride();
		SizeBox->SetHeightOverride(36.f);
	}
	if (SliderWidget)
	{
		SliderWidget->SetSliderBarColor(Screen.GetColor(ECozyUiColor::Border));
		SliderWidget->SetSliderHandleColor(Screen.GetColor(ECozyUiColor::Point));
		FText Why;
		const bool bCan = Screen.CanRunAction(Data, Why);
		SliderWidget->SetIsEnabled(bCan);
		SliderWidget->SetToolTipText(!bCan && !Why.IsEmpty() ? Why : UCozyUiScreen::GetActionName(Data.Action));
	}
}

void UCozyUiSlider::UpdateCozyValue(const UCozyUiScreen& Screen)
{
	if (!SliderWidget || GetEntry().Value == ECozyUiValue::None)
	{
		return;
	}
	const FCozyUiValueResult R = Screen.GetValue(GetEntry().Value, GetEntry().ValueParam);
	if (R.bValid && !bDragging)
	{
		SliderWidget->SetValue(R.Ratio());
	}
}

void UCozyUiSlider::HandleValueChanged(float Value)
{
	if (UCozyUiScreen* Screen = GetTypedOuter<UCozyUiScreen>())
	{
		Screen->RunSliderAction(GetEntry(), Value);
	}
}

void UCozyUiSlider::HandleCaptureBegin()
{
	bDragging = true;
}

void UCozyUiSlider::HandleCaptureEnd()
{
	bDragging = false;
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
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
	// 항목: 보통 영역은 화면 설정의 요소 줄 · 반복 목록은 출처의 줄마다 '한 줄 상자'(정보 패널 + 한 줄 영역)
	TArray<FCozyUiElementEntry> Entries;
	TArray<FCozyUiListRow> Rows;
	if (ListSource != ECozyUiListSource::None)
	{
		Rows = Screen.GetListRows(ListSource, RowId);
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			FCozyUiElementEntry RowEntry;
			RowEntry.Id = FName(*FString::Printf(TEXT("Row_%s"), *Rows[Index].Id.ToString()));
			RowEntry.Kind = ECozyUiElementKind::Panel;
			RowEntry.Order = Index;
			RowEntry.Background = ListRowBackground;
			RowEntry.PaddingRole = ECozyUiPadding::Panel;
			RowEntry.ChildArea = ListRowArea;
			Entries.Add(RowEntry);
		}
	}
	else
	{
		for (const FCozyUiElementEntry& Each : Screen.GetElementsForArea(AreaId))
		{
			Entries.Add(CozyUiRow::Resolve(Each, RowId, RowName));
			Rows.Add({ RowId, RowName });
		}
	}

	// 구성(요소 목록 · 정렬 방식)이 바뀐 경우에만 다시 만듦 · 내용만 바뀌면 기존 요소에 다시 적용
	// 넘치면 스크롤: 세로로 쌓는 영역에서만 (줄바꿈·같은 크기 격자는 제외)
	const bool bScroll = Layout.bScrollWhenTall && Layout.Flow == ECozyUiFlow::Vertical && Layout.Spread != ECozyUiSpread::EqualSize;
	FString Signature = FString::Printf(TEXT("%d|%d|%d|%d|%.1f|%s|%d|"), (int32)Layout.Flow, (int32)Layout.Spread, (int32)Layout.PackAlign, (int32)Layout.ItemAlign, Gap,
		*FString::Printf(TEXT("%.0f,%.0f,%.0f,%.0f"), Layout.Padding.Left, Layout.Padding.Top, Layout.Padding.Right, Layout.Padding.Bottom), bScroll ? 1 : 0);
	if (bScroll)
	{
		for (const FName& Pinned : Layout.PinnedElements)
		{
			Signature += Pinned.ToString() + TEXT(",");
		}
		Signature += TEXT("|");
	}
	for (const FCozyUiElementEntry& Each : Entries)
	{
		if (Each.bVisible)
		{
			Signature += FString::Printf(TEXT("%s:%d:%s;"), *Each.Id.ToString(), (int32)Each.Kind, *Each.TemplateOverride.ToString());
		}
	}
	// 영역 상자 정렬·여백은 매번 적용 (창을 닫았다 다시 열면 슬레이트 상자가 다시 만들어지며 기본값으로 돌아가는 경우가 있음)
	{
		const bool bHorizontalFlow = Layout.Flow != ECozyUiFlow::Vertical;
		const bool bAnyFillNow = Entries.ContainsByPredicate([](const FCozyUiElementEntry& Each) { return Each.bVisible && Each.SizeMode == ECozyUiSizeMode::Fill; });
		const bool bFillMainNow = Layout.Spread == ECozyUiSpread::EqualGap || Layout.Flow == ECozyUiFlow::Wrap || bAnyFillNow
			|| (Layout.Spread == ECozyUiSpread::EqualSize && Layout.PackAlign == ECozyUiAlign::Fill);
		Host->SetPadding(Layout.Padding);
		if (bHorizontalFlow)
		{
			Host->SetHorizontalAlignment(bFillMainNow ? HAlign_Fill : ToHAlign(Layout.PackAlign));
			Host->SetVerticalAlignment(ToVAlign(Layout.ItemAlign));
		}
		else
		{
			Host->SetVerticalAlignment(bFillMainNow ? VAlign_Fill : ToVAlign(Layout.PackAlign));
			Host->SetHorizontalAlignment(ToHAlign(Layout.ItemAlign));
		}
	}
	if (Signature == BuiltSignature && Spawned.Num() > 0)
	{
		int32 Index = 0;
		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			const FCozyUiElementEntry& Each = Entries[EntryIndex];
			if (Each.bVisible && Spawned.IsValidIndex(Index) && Spawned[Index])
			{
				Spawned[Index]->SetEntry(Each, true);
				Spawned[Index]->SetRow(Rows[EntryIndex].Id, Rows[EntryIndex].Name);
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

	// 넘치면 스크롤: [높이 상한 상자 → 스크롤 → 요소들] + 그 아래 고정 요소 줄 (높이 상한은 NativeTick에서 화면 높이에 맞춤)
	// 고정 요소 중 스크롤 요소보다 앞(Order가 작은 것 · 예: 제목 배경)은 위 줄, 뒤는 아래 줄
	UPanelWidget* PinnedTopBox = nullptr;
	UPanelWidget* PinnedBox = nullptr;
	ScrollSize = nullptr;
	if (bScroll)
	{
		UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PinnedTopBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UVerticalBoxSlot* TopSlot = Outer->AddChildToVerticalBox(PinnedTopBox))
		{
			TopSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->AddChild(Box);
		ScrollSize->SetContent(Scroll);
		if (UVerticalBoxSlot* ScrollSlot = Outer->AddChildToVerticalBox(ScrollSize))
		{
			ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		PinnedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UVerticalBoxSlot* PinnedSlot = Outer->AddChildToVerticalBox(PinnedBox))
		{
			PinnedSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Host->SetContent(Outer);
	}
	else
	{
		Host->SetContent(Box);
	}

	// 묶음 위치: 붙여서 놓을 때만 영역 안에서 앞·가운데·뒤 · 같은 크기·균등 간격·줄바꿈은 영역을 꽉 채움
	const bool bHorizontal = Layout.Flow != ECozyUiFlow::Vertical;
	// 남은 공간 채우기 요소가 있으면 영역을 꽉 채워야 그 요소가 남은 폭을 받는다
	const bool bAnyFill = Entries.ContainsByPredicate([](const FCozyUiElementEntry& Each) { return Each.bVisible && Each.SizeMode == ECozyUiSizeMode::Fill; });
	const bool bFillMain = Layout.Spread == ECozyUiSpread::EqualGap || Layout.Flow == ECozyUiFlow::Wrap || bAnyFill
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

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FCozyUiElementEntry& Each = Entries[EntryIndex];
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
		NewElement->SetRow(Rows[EntryIndex].Id, Rows[EntryIndex].Name);

		// 고정 요소는 스크롤 밖 아래 줄로
		UPanelWidget* Target = Box;
		if (PinnedBox && Layout.PinnedElements.Contains(Each.Id))
		{
			Target = Box->GetChildrenCount() == 0 ? PinnedTopBox : PinnedBox;
		}

		// 균등 간격: 요소 사이에 늘어나는 빈칸
		if (Layout.Spread == ECozyUiSpread::EqualGap && Target->GetChildrenCount() > 0 && Layout.Flow != ECozyUiFlow::Wrap)
		{
			USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
			UPanelSlot* SpacerSlot = Target->AddChild(Spacer);
			if (UHorizontalBoxSlot* H = Cast<UHorizontalBoxSlot>(SpacerSlot)) { H->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
			if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(SpacerSlot)) { V->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
		}

		const bool bFirst = Target->GetChildrenCount() == 0;
		UPanelSlot* NewSlot = Target->AddChild(NewElement);
		const float Lead = (bFirst || Layout.Spread == ECozyUiSpread::EqualGap) ? 0.f : Gap;
		if (UHorizontalBoxSlot* H = Cast<UHorizontalBoxSlot>(NewSlot))
		{
			H->SetPadding(FMargin(Lead, 0.f, 0.f, 0.f));
			H->SetVerticalAlignment(ToVAlign(Layout.ItemAlign));
			H->SetHorizontalAlignment(HAlign_Fill);
			H->SetSize(FSlateChildSize((Layout.Spread == ECozyUiSpread::EqualSize || Each.SizeMode == ECozyUiSizeMode::Fill) ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		}
		else if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(NewSlot))
		{
			V->SetPadding(FMargin(0.f, Lead, 0.f, 0.f));
			V->SetHorizontalAlignment(ToHAlign(Layout.ItemAlign));
			V->SetVerticalAlignment(VAlign_Fill);
			V->SetSize(FSlateChildSize((Layout.Spread == ECozyUiSpread::EqualSize || Each.SizeMode == ECozyUiSizeMode::Fill) ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
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

	// 위·아래 고정 줄과 스크롤 부분 사이 간격 (해당 줄에 요소가 있을 때만 · 원래 세로 목록의 간격과 같게)
	if (ScrollSize)
	{
		if (UVerticalBoxSlot* ScrollSlot = Cast<UVerticalBoxSlot>(ScrollSize->Slot))
		{
			ScrollSlot->SetPadding(FMargin(0.f, PinnedTopBox->GetChildrenCount() > 0 && Box->GetChildrenCount() > 0 ? Gap : 0.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* PinnedSlot = Cast<UVerticalBoxSlot>(PinnedBox->Slot))
		{
			PinnedSlot->SetPadding(FMargin(0.f, PinnedBox->GetChildrenCount() > 0 ? Gap : 0.f, 0.f, 0.f));
		}
	}
}

void UCozyUiArea::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!ScrollSize)
	{
		return;
	}
	// 화면 전체가 HUD가 준 높이 안에 들어가도록 스크롤 부분의 높이만 줄인다
	// (화면 원하는 높이 − 스크롤 부분 높이 = 제목·여백·고정 줄 · 지난 프레임 값이라 한두 프레임 안에 맞춰짐)
	const UCozyUiScreen* Screen = GetTypedOuter<UCozyUiScreen>();
	if (!Screen || Screen->MaxScreenHeight <= 0.f)
	{
		if (ScrollSize->GetMaxDesiredHeight() > 0.f)
		{
			ScrollSize->ClearMaxDesiredHeight();
		}
		return;
	}
	const float Chrome = FMath::Max(0.f, Screen->GetDesiredSize().Y - ScrollSize->GetDesiredSize().Y);
	const float Base = FMath::Max(120.f, Screen->MaxScreenHeight - Chrome);
	float Target = Base;

	// 실제로 그려진 위치로 한 번 더: 창 틀이 원하는 높이보다 아래까지 그려지는 만큼은 위 계산에 잡히지 않으므로
	// 이 영역 아래 끝 + 창 아래 여백이 화면 아래(여백 20)를 넘으면 그만큼 줄이고, 남으면 Base까지 되돌린다
	// (위치는 한 프레임 늦게 들어오므로 넘친 값의 절반씩 · 4 미만은 무시해 흔들리지 않게)
	const FGeometry ViewGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const float Current = ScrollSize->GetMaxDesiredHeight() > 0.f ? ScrollSize->GetMaxDesiredHeight() : Base;
	if (ViewGeometry.GetLocalSize().Y > 1.f && MyGeometry.Scale > 0.f && MyGeometry.GetLocalSize().Y > 1.f)
	{
		const UCozyUiTheme* Theme = Screen->GetTheme();
		const float FrameBottom = (Theme ? Theme->Metrics.WindowPadding.Bottom : 24.f) + 8.f;
		const float LimitAbs = ViewGeometry.LocalToAbsolute(ViewGeometry.GetLocalSize()).Y - 20.f * ViewGeometry.Scale;
		const float BottomAbs = MyGeometry.LocalToAbsolute(MyGeometry.GetLocalSize()).Y;
		const float Overflow = (BottomAbs - LimitAbs) / MyGeometry.Scale + FrameBottom;
		Target = FMath::Abs(Overflow) < 4.f ? Current : Current - Overflow * 0.5f;
		Target = FMath::Clamp(Target, 120.f, Base);
	}
	if (!FMath::IsNearlyEqual(ScrollSize->GetMaxDesiredHeight(), Target, 1.f))
	{
		ScrollSize->SetMaxDesiredHeight(Target);
	}
}

#undef LOCTEXT_NAMESPACE
