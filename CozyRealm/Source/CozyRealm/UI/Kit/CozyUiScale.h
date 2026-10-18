#pragma once

#include "CoreMinimal.h"
#include "Engine/DPICustomScalingRule.h"
#include "CozyUiScale.generated.h"

/** 화면 배율 한 점: 화면의 짧은 쪽(세로) 픽셀 → UI 배율 */
USTRUCT(BlueprintType)
struct FCozyUiScalePoint
{
	GENERATED_BODY()

	FCozyUiScalePoint() = default;
	FCozyUiScalePoint(int32 InShortSide, float InScale) : ShortSide(InShortSide), Scale(InScale) {}

	/** 화면의 짧은 쪽 길이 (보통 세로 · 예: 1280×720이면 720) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Screen Short Side (화면 짧은 쪽 픽셀)", ClampMin = "1"))
	int32 ShortSide = 1080;

	/** 이 크기에서 UI 배율 (1 = 1920×1080 기준 크기 그대로 · 글자·버튼·창이 함께 커지고 작아짐) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "UI Scale (UI 배율)", ClampMin = "0.1"))
	float Scale = 1.f;
};

/**
 *  UI 배율 규칙 (엔진 'DPI 배율'을 대신함).
 *  프로젝트 설정 → Cozy UI (UI 설정) → '3 화면 배율'의 점 목록을 보고, 그 사이는 직선으로 이어 계산한다.
 *  예: 720 → 0.8 · 1080 → 1.0 이면 1280×720에서 글자·창이 기준의 0.8배 (엔진 기본은 0.67배).
 */
UCLASS()
class UCozyDpiScalingRule : public UDPICustomScalingRule
{
	GENERATED_BODY()

public:
	virtual float GetDPIScaleBasedOnSize(FIntPoint Size) const override;

	/** 점 목록으로 배율 계산 (점이 없으면 짧은 쪽 / 1080) */
	static float Evaluate(const TArray<FCozyUiScalePoint>& Points, int32 ShortSide);
};
