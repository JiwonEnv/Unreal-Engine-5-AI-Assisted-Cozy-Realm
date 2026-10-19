#include "Environment/CozyPaintMask.h"

void FCozyPaintMask::Initialize(int32 InResolution)
{
	Resolution = FMath::Clamp(InResolution, 16, 512);
	Pixels.Init(FColor(255, 0, 0, 0), Resolution * Resolution);
}

bool FCozyPaintMask::IsValid() const
{
	static const TArray<FName> Supported = { TEXT("GroundBase"), TEXT("GroundDirt"), TEXT("GroundStone"), TEXT("GroundSand") };
	if (PaletteVersion != 1 || MaterialIds != Supported) return false;
	if (Resolution < 16 || Resolution > 512 || Pixels.Num() != Resolution * Resolution) return false;
	for (FColor P : Pixels) if (int32(P.R) + P.G + P.B + P.A != 255) return false;
	return true;
}

bool FCozyPaintMask::Stamp(FVector2D UV, FVector2D RadiusUV, int32 Layer, float Strength)
{
	if (Layer < 0 || Layer > 3 || RadiusUV.X <= 0 || RadiusUV.Y <= 0 || !FMath::IsFinite(Strength)
		|| UV.ContainsNaN() || RadiusUV.ContainsNaN() || Pixels.Num() != Resolution * Resolution || Resolution < 16 || Resolution > 512) return false;
	Strength = FMath::Clamp(Strength, 0.f, 1.f);
	bool Changed = false;
	const int32 X0 = FMath::Clamp(FMath::FloorToInt((UV.X - RadiusUV.X) * Resolution), 0, Resolution - 1);
	const int32 X1 = FMath::Clamp(FMath::CeilToInt((UV.X + RadiusUV.X) * Resolution), 0, Resolution - 1);
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt((UV.Y - RadiusUV.Y) * Resolution), 0, Resolution - 1);
	const int32 Y1 = FMath::Clamp(FMath::CeilToInt((UV.Y + RadiusUV.Y) * Resolution), 0, Resolution - 1);
	for (int32 Y = Y0; Y <= Y1; ++Y)
	for (int32 X = X0; X <= X1; ++X)
	{
		const FVector2D Delta = (FVector2D((X + .5) / Resolution, (Y + .5) / Resolution) - UV) / RadiusUV;
		const float Distance = Delta.Size();
		if (Distance >= 1.f) continue;
		FColor& Pixel = Pixels[Y * Resolution + X];
		uint8 Channels[] = {Pixel.R, Pixel.G, Pixel.B, Pixel.A};
		const float Alpha = Strength * (1.f - Distance);
		int32 Sum = 0;
		for (int32 C = 0; C < 4; ++C)
		{
			if (C != Layer) { Channels[C] = FMath::FloorToInt(Channels[C] * (1.f - Alpha)); Sum += Channels[C]; }
		}
		Channels[Layer] = 255 - Sum;
		const FColor Next(Channels[0], Channels[1], Channels[2], Channels[3]);
		Changed |= Next != Pixel;
		Pixel = Next;
	}
	return Changed;
}

void FCozyPaintHistory::Begin(const FCozyPaintMask& Mask) { if (!Before.IsSet()) Before = Mask; }
void FCozyPaintHistory::Commit(const FCozyPaintMask& Mask)
{
	if (!Before.IsSet()) return;
	if (Before->DomainId == Mask.DomainId && Before->Pixels != Mask.Pixels)
	{
		UndoStack.Add(MoveTemp(Before.GetValue()));
		if (UndoStack.Num() > 16) UndoStack.RemoveAt(0);
		RedoStack.Reset();
	}
	Before.Reset();
}
bool FCozyPaintHistory::Undo(FCozyPaintMask& Mask)
{
	if (Before.IsSet() || UndoStack.IsEmpty() || UndoStack.Last().DomainId != Mask.DomainId) return false;
	RedoStack.Add(Mask); Mask = UndoStack.Pop(); return true;
}
bool FCozyPaintHistory::Redo(FCozyPaintMask& Mask)
{
	if (Before.IsSet() || RedoStack.IsEmpty() || RedoStack.Last().DomainId != Mask.DomainId) return false;
	UndoStack.Add(Mask); Mask = RedoStack.Pop(); return true;
}
