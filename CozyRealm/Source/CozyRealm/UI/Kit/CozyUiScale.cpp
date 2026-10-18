#include "UI/Kit/CozyUiScale.h"
#include "UI/Kit/CozyUiTheme.h"

float UCozyDpiScalingRule::GetDPIScaleBasedOnSize(FIntPoint Size) const
{
	const UCozyUiSettings* Settings = UCozyUiSettings::Get();
	return Evaluate(Settings ? Settings->ScalePoints : TArray<FCozyUiScalePoint>(), FMath::Min(Size.X, Size.Y));
}

float UCozyDpiScalingRule::Evaluate(const TArray<FCozyUiScalePoint>& Points, int32 ShortSide)
{
	if (ShortSide <= 0)
	{
		return 1.f;
	}
	TArray<FCozyUiScalePoint> Sorted = Points.FilterByPredicate([](const FCozyUiScalePoint& P) { return P.ShortSide > 0 && P.Scale > 0.f; });
	if (Sorted.IsEmpty())
	{
		return ShortSide / 1080.f;
	}
	Sorted.Sort([](const FCozyUiScalePoint& A, const FCozyUiScalePoint& B) { return A.ShortSide < B.ShortSide; });

	// 목록보다 작거나 크면 끝 점의 비율을 그대로 늘이거나 줄임 (화면 크기에 비례)
	if (ShortSide <= Sorted[0].ShortSide)
	{
		return Sorted[0].Scale * ShortSide / Sorted[0].ShortSide;
	}
	if (ShortSide >= Sorted.Last().ShortSide)
	{
		return Sorted.Last().Scale * ShortSide / Sorted.Last().ShortSide;
	}
	for (int32 i = 1; i < Sorted.Num(); ++i)
	{
		if (ShortSide <= Sorted[i].ShortSide)
		{
			const float Alpha = float(ShortSide - Sorted[i - 1].ShortSide) / float(Sorted[i].ShortSide - Sorted[i - 1].ShortSide);
			return FMath::Lerp(Sorted[i - 1].Scale, Sorted[i].Scale, Alpha);
		}
	}
	return 1.f;
}
