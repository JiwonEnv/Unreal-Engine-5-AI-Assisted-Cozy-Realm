#pragma once
#include "CoreMinimal.h"
#include "CozyPaintMask.generated.h"

/** RGBA: 바탕과 세 가지 덧칠 비율. 합계는 항상 255이며 CPU 데이터가 저장 원본이다. */
USTRUCT()
struct FCozyPaintMask
{
	GENERATED_BODY()
	UPROPERTY() FGuid DomainId;
	UPROPERTY() int32 PaletteVersion = 1;
	UPROPERTY() TArray<FName> MaterialIds = { TEXT("GroundBase"), TEXT("GroundDirt"), TEXT("GroundStone"), TEXT("GroundSand") };
	UPROPERTY() int32 Resolution = 128;
	UPROPERTY() TArray<FColor> Pixels;
	void Initialize(int32 InResolution);
	bool IsValid() const;
	bool Stamp(FVector2D UV, FVector2D RadiusUV, int32 Layer, float Strength);
};

/** 한 드래그를 한 작업으로 묶고, 최대 16회만 보관한다. */
class FCozyPaintHistory
{
public:
	void Begin(const FCozyPaintMask& Mask);
	void Commit(const FCozyPaintMask& Mask);
	bool Undo(FCozyPaintMask& Mask);
	bool Redo(FCozyPaintMask& Mask);
private:
	TArray<FCozyPaintMask> UndoStack, RedoStack;
	TOptional<FCozyPaintMask> Before;
};
