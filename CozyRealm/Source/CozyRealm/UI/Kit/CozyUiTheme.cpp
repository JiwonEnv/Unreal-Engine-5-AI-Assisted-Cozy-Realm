#include "UI/Kit/CozyUiTheme.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Texture2D.h"

const FSlateBrush* UCozyUiTheme::FindImage(FName Name) const
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (const FCozyUiAtlasImage* Entry = AtlasImages.Find(Name))
	{
		const UTexture2D* Texture = Entry->Texture;
		const FVector2D Dimensions = Entry->TextureSize.X > 0.f && Entry->TextureSize.Y > 0.f
			? Entry->TextureSize
			: Texture ? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : FVector2D::ZeroVector;
		if (Texture && Dimensions.X > 0.f && Dimensions.Y > 0.f &&
			Entry->Origin.X >= 0.f && Entry->Origin.Y >= 0.f && Entry->Size.X > 0.f && Entry->Size.Y > 0.f &&
			Entry->Origin.X + Entry->Size.X <= Dimensions.X &&
			Entry->Origin.Y + Entry->Size.Y <= Dimensions.Y)
		{
			FSlateBrush& Brush = AtlasBrushCache.FindOrAdd(Name);
			Brush = FSlateBrush();
			Brush.SetResourceObject(Entry->Texture);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = Entry->Size;
			Brush.TintColor = Entry->Tint;
			Brush.SetUVRegion(FBox2d(Entry->Origin / Dimensions, (Entry->Origin + Entry->Size) / Dimensions));
			return &Brush;
		}
	}
	return Images.Find(Name);
}

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

namespace CozyUiThemeStyles
{
	/** 이름으로 모양을 찾고, 없거나 비어 있으면 'Default' */
	template <typename TStyle>
	const TStyle* FindOrDefault(const TMap<FName, TStyle>& Styles, FName Name)
	{
		const FName DefaultName(TEXT("Default"));
		if (const TStyle* Found = Styles.Find(Name.IsNone() ? DefaultName : Name))
		{
			return Found;
		}
		return Styles.Find(DefaultName);
	}
}

const FButtonStyle* UCozyUiTheme::FindButtonStyle(FName Name) const
{
	return CozyUiThemeStyles::FindOrDefault(ButtonStyles, Name);
}

const FCozyUiGaugeStyle* UCozyUiTheme::FindGaugeStyle(FName Name) const
{
	return CozyUiThemeStyles::FindOrDefault(GaugeStyles, Name);
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
	return CozyUiThemeOptions::Collect([](const UCozyUiTheme& Theme, TSet<FName>& Out)
	{
		TSet<FName> ImageNames;
		for (const auto& Pair : Theme.Images) { ImageNames.Add(Pair.Key); }
		for (const auto& Pair : Theme.AtlasImages) { ImageNames.Add(Pair.Key); }
		for (const FName ImageName : ImageNames)
		{
			Out.Add(ImageName);
			// 'Menu.Info'처럼 점이 있는 이름표가 있으면 'Menu.@Row'도 고를 수 있게 한다
			// (반복 목록 안 요소에서 @Row가 줄 ID로 바뀌어 줄마다 다른 그림 · 예: Menu.@Row → Menu.Processing)
			FString Key = ImageName.ToString();
			int32 Dot = INDEX_NONE;
			if (Key.FindChar(TEXT('.'), Dot) && Dot > 0)
			{
				FString Prefix = Key.Left(Dot);
				Prefix[0] = FChar::ToUpper(Prefix[0]);
				Out.Add(FName(*(Prefix + TEXT(".@Row"))));
			}
		}
	});
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
