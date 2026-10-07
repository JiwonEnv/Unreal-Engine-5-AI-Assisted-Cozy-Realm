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
		if (bUseThemeIconSize)
		{
			Copy.ImageSize = Theme->Metrics.IconSize;
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
// 버튼

void UCozyUiButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button)
	{
		Button->OnClicked.AddUniqueDynamic(this, &UCozyUiButton::HandleClicked);
	}
}

void UCozyUiButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (UCozyUiScreen* Screen = GetTypedOuter<UCozyUiScreen>())
	{
		ApplyCozyTheme(*Screen);
	}
}

void UCozyUiButton::HandleClicked()
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

void UCozyUiButton::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	// 내용: 영역이 준 줄 > 화면 설정의 같은 ID 줄 > 디자이너 기본값
	if (!bFromArea)
	{
		const FCozyUiButtonEntry* Found = Screen.FindButtonEntry(ButtonId);
		Entry = Found ? *Found : Defaults;
		if (Entry.Id.IsNone())
		{
			Entry.Id = ButtonId;
		}
	}
	const UCozyUiTheme* Theme = Screen.GetTheme();
	if (!Theme || !Button)
	{
		return;
	}
	if (const FButtonStyle* Style = Theme->FindButtonStyle(Entry.Style))
	{
		Button->SetStyle(*Style);
	}
	if (Icon)
	{
		const FSlateBrush* Brush = Theme->FindImage(Entry.Icon);
		if (Brush)
		{
			FSlateBrush Copy = *Brush;
			Copy.ImageSize = Theme->Metrics.IconSize;
			Icon->SetBrush(Copy);
		}
		Icon->SetVisibility(Brush ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Label)
	{
		Label->SetText(Entry.Label);
		Label->SetVisibility(Entry.Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		const FSlateFontInfo Font = Screen.GetFont(ECozyUiTextRole::Button);
		if (Font.HasValidFont())
		{
			Label->SetFont(Font);
		}
		Label->SetColorAndOpacity(FSlateColor(Button->GetIsEnabled() ? Screen.GetColor(ECozyUiColor::Ink) : Theme->DisabledTextColor));
	}
	// 크기: 줄의 크기 > 테마 기본 크기 (0은 내용에 맞춤) · 위치는 디자이너·영역이 정함
	if (USizeBox* Size = Cast<USizeBox>(WidgetTree ? WidgetTree->FindWidget(TEXT("SizeBox")) : nullptr))
	{
		const FVector2D Want = Entry.Size.IsNearlyZero() ? Theme->Metrics.ButtonSize : Entry.Size;
		if (Want.X > 0.f) { Size->SetWidthOverride(Want.X); } else { Size->ClearWidthOverride(); }
		if (Want.Y > 0.f) { Size->SetHeightOverride(Want.Y); } else { Size->ClearHeightOverride(); }
	}
	Button->SetToolTipText(UCozyUiScreen::GetActionName(Entry.Action));
	SetVisibility(Entry.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// 버튼 영역

void UCozyUiButtonArea::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	if (!Box || !ButtonClass)
	{
		return;
	}
	const TArray<FCozyUiButtonEntry> Entries = Screen.GetButtonsForArea(AreaId);
	const UCozyUiTheme* Theme = Screen.GetTheme();
	const float Gap = Spacing >= 0.f ? Spacing : (Theme ? Theme->Metrics.Gap : 8.f);

	// 버튼 구성이 바뀐 경우에만 다시 만듦 (매번 만들면 클릭이 끊김)
	FString Signature = FString::Printf(TEXT("%.1f|"), Gap);
	for (const FCozyUiButtonEntry& Each : Entries)
	{
		Signature += FString::Printf(TEXT("%s:%d:%d;"), *Each.Id.ToString(), Each.Order, Each.bVisible ? 1 : 0);
	}
	if (Signature != BuiltSignature || Spawned.Num() != Box->GetChildrenCount())
	{
		BuiltSignature = Signature;
		Box->ClearChildren();
		Spawned.Reset();
		for (const FCozyUiButtonEntry& Each : Entries)
		{
			if (!Each.bVisible)
			{
				continue;
			}
			UCozyUiButton* NewButton = CreateWidget<UCozyUiButton>(this, ButtonClass);
			if (!NewButton)
			{
				continue;
			}
			NewButton->bFromArea = true;
			NewButton->SetEntry(Each);
			UPanelSlot* NewSlot = Box->AddChild(NewButton);
			const bool bFirst = Spawned.Num() == 0;
			if (UHorizontalBoxSlot* H = Cast<UHorizontalBoxSlot>(NewSlot))
			{
				H->SetPadding(FMargin(bFirst ? 0.f : Gap, 0.f, 0.f, 0.f));
				H->SetVerticalAlignment(VAlign_Center);
			}
			else if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(NewSlot))
			{
				V->SetPadding(FMargin(0.f, bFirst ? 0.f : Gap, 0.f, 0.f));
			}
			Spawned.Add(NewButton);
		}
		if (UWrapBox* Wrap = Cast<UWrapBox>(Box))
		{
			Wrap->SetInnerSlotPadding(FVector2D(Gap, Gap));
		}
	}
	else
	{
		// 같은 구성이면 내용(글자·아이콘·동작)만 새 값으로
		int32 Index = 0;
		for (const FCozyUiButtonEntry& Each : Entries)
		{
			if (Each.bVisible && Spawned.IsValidIndex(Index))
			{
				Spawned[Index++]->SetEntry(Each);
			}
		}
	}
	for (UCozyUiButton* Each : Spawned)
	{
		Each->ApplyCozyTheme(Screen);
	}
}

// ---------------------------------------------------------------------------
// 게이지

void UCozyUiGauge::ApplyCozyTheme(const UCozyUiScreen& Screen)
{
	const UCozyUiTheme* Theme = Screen.GetTheme();
	if (!Theme || !Bar)
	{
		return;
	}
	if (const FCozyUiGaugeStyle* GaugeStyle = Theme->FindGaugeStyle(Style))
	{
		Bar->SetWidgetStyle(GaugeStyle->Style);
		bTintByState = GaugeStyle->bTintByState;
	}
	Bar->SetBarFillType(FillType);
	if (ValueText)
	{
		const FSlateFontInfo Font = Screen.GetFont(ECozyUiTextRole::Small);
		if (Font.HasValidFont())
		{
			ValueText->SetFont(Font);
		}
		ValueText->SetColorAndOpacity(FSlateColor(Screen.GetColor(ECozyUiColor::Ink)));
	}
	if (IsDesignTime())
	{
		Bar->SetPercent(DesignPercent);
	}
}

void UCozyUiGauge::UpdateCozyValue(const UCozyUiScreen& Screen)
{
	if (!Bar)
	{
		return;
	}
	const FCozyUiValueResult R = Screen.GetValue(Value, ValueParam);
	if (!R.bValid)
	{
		return;
	}
	Bar->SetPercent(R.Ratio());
	if (FillColor != ECozyUiColor::None)
	{
		Bar->SetFillColorAndOpacity(Screen.GetColor(FillColor));
	}
	else if (bTintByState)
	{
		Bar->SetFillColorAndOpacity(Screen.GetColor(R.StateColor));
	}
	else
	{
		Bar->SetFillColorAndOpacity(FLinearColor::White);
	}
	if (ValueText)
	{
		TArray<FString> Parts;
		if (bShowNumber)
		{
			Parts.Add(R.Max > 0.f ? FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(R.Current), FMath::RoundToInt(R.Max)) : FString::FromInt(FMath::RoundToInt(R.Current)));
		}
		if (bShowPercent)
		{
			Parts.Add(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(R.Ratio() * 100.f)));
		}
		if (bShowRemaining && R.RemainingSeconds > 0.f)
		{
			Parts.Add(CozyUiFormat::Duration(R.RemainingSeconds).ToString());
		}
		if (!R.Text.IsEmpty() && (R.StateColor == ECozyUiColor::Paused || R.StateColor == ECozyUiColor::Locked))
		{
			Parts.Add(R.Text.ToString());
		}
		ValueText->SetText(FText::FromString(FString::Join(Parts, TEXT(" · "))));
		ValueText->SetVisibility(Parts.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

#undef LOCTEXT_NAMESPACE
