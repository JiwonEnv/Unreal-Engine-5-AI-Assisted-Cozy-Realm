#include "UI/Kit/CozyUiTheme.h"
#include "UObject/UObjectIterator.h"

const FSlateFontInfo& FCozyUiFonts::Get(ECozyUiTextRole Role) const
{
	switch (Role)
	{
	case ECozyUiTextRole::Title: return Title;
	case ECozyUiTextRole::Number: return Number;
	case ECozyUiTextRole::Small: return Small;
	case ECozyUiTextRole::Button: return Button;
	default: return Body;
	}
}

FLinearColor FCozyUiColors::Get(ECozyUiColor Role) const
{
	switch (Role)
	{
	case ECozyUiColor::Paper: return Paper;
	case ECozyUiColor::PaperDark: return PaperDark;
	case ECozyUiColor::Ink: return Ink;
	case ECozyUiColor::InkMuted: return InkMuted;
	case ECozyUiColor::Border: return Border;
	case ECozyUiColor::Point: return Point;
	case ECozyUiColor::OnPoint: return OnPoint;
	case ECozyUiColor::Progress: return Progress;
	case ECozyUiColor::Paused: return Paused;
	case ECozyUiColor::Ok: return Ok;
	case ECozyUiColor::Locked: return Locked;
	case ECozyUiColor::Warning: return Warning;
	case ECozyUiColor::Claimable: return Claimable;
	default: return FLinearColor::White;
	}
}

const FButtonStyle* UCozyUiTheme::FindButtonStyle(FName Name) const
{
	if (const FButtonStyle* Found = ButtonStyles.Find(Name.IsNone() ? FName(TEXT("Default")) : Name))
	{
		return Found;
	}
	return ButtonStyles.Find(TEXT("Default"));
}

const FCozyUiGaugeStyle* UCozyUiTheme::FindGaugeStyle(FName Name) const
{
	if (const FCozyUiGaugeStyle* Found = GaugeStyles.Find(Name.IsNone() ? FName(TEXT("Default")) : Name))
	{
		return Found;
	}
	return GaugeStyles.Find(TEXT("Default"));
}

namespace CozyUiThemeOptions
{
	template <typename FGetKeys>
	TArray<FName> Collect(FGetKeys GetKeys)
	{
		TSet<FName> Names;
		for (TObjectIterator<UCozyUiTheme> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject))
			{
				GetKeys(**It, Names);
			}
		}
		TArray<FName> Result = Names.Array();
		Result.Sort(FNameLexicalLess());
		Result.Insert(NAME_None, 0);
		return Result;
	}
}

TArray<FName> UCozyUiTheme::GetImageNameOptions()
{
	return CozyUiThemeOptions::Collect([](const UCozyUiTheme& Theme, TSet<FName>& Out) { for (const auto& Pair : Theme.Images) { Out.Add(Pair.Key); } });
}

TArray<FName> UCozyUiTheme::GetButtonStyleOptions()
{
	return CozyUiThemeOptions::Collect([](const UCozyUiTheme& Theme, TSet<FName>& Out) { for (const auto& Pair : Theme.ButtonStyles) { Out.Add(Pair.Key); } });
}

TArray<FName> UCozyUiTheme::GetGaugeStyleOptions()
{
	return CozyUiThemeOptions::Collect([](const UCozyUiTheme& Theme, TSet<FName>& Out) { for (const auto& Pair : Theme.GaugeStyles) { Out.Add(Pair.Key); } });
}

#if WITH_EDITOR
void UCozyUiTheme::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnChanged.Broadcast(this);
}
#endif

UCozyUiTheme* UCozyUiSettings::LoadDefaultTheme()
{
	return Get()->DefaultTheme.LoadSynchronous();
}
