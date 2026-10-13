#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CozyDayNightLighting.generated.h"

class ADirectionalLight;
class ASkyLight;
class ULightComponent;
class UPostProcessComponent;

/**
 *  낮/밤 조명 (영지 서비스의 게임 시계를 따라 조명을 바꾼다 · 하루 30분 = 낮 15분 · 밤 15분).
 *  낮: 따뜻한 오후 햇빛 + 남보라 기운의 하늘빛(그림자 색).
 *  밤(게임 시각 18~6시 · 디버그 '낮/밤'으로 고정 가능): 해·하늘빛·노출을 낮춰 명도만 내리고,
 *  해 질 녘·해 뜰 녘은 게임 시각 기준으로 서서히 바뀐다 (Twilight Game Hours).
 *  이름표 'CozyNightLight'가 붙은 빛(시설 초롱 · 오니비 빛)을 켠다.
 *  해와 하늘빛을 비워 두면 레벨에서 처음 찾은 것을 쓴다.
 */
UCLASS(meta = (DisplayName = "Cozy Day Night Lighting (낮/밤 조명)"))
class ACozyDayNightLighting : public AActor
{
	GENERATED_BODY()

public:

	ACozyDayNightLighting();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 지금 밤 정도 (0 = 낮 · 1 = 밤) */
	float GetNightAlpha() const { return NightAlpha; }

	/** 해 (비우면 레벨의 첫 Directional Light) */
	UPROPERTY(EditAnywhere, Category = "1 연결", meta = (DisplayName = "Sun (해)"))
	TObjectPtr<ADirectionalLight> Sun;

	/** 하늘빛 (비우면 레벨의 첫 Sky Light) */
	UPROPERTY(EditAnywhere, Category = "1 연결", meta = (DisplayName = "Sky Light (하늘빛)"))
	TObjectPtr<ASkyLight> Sky;

	/** 낮 햇빛 세기 (lux) */
	UPROPERTY(EditAnywhere, Category = "2 낮", meta = (DisplayName = "Day Sun Intensity (낮 햇빛 세기)", ClampMin = "0"))
	float DaySunIntensity = 6.f;

	/** 낮 햇빛 색 (따뜻한 오후) */
	UPROPERTY(EditAnywhere, Category = "2 낮", meta = (DisplayName = "Day Sun Color (낮 햇빛 색)"))
	FLinearColor DaySunColor = FLinearColor(FColor(255, 242, 226));

	/** 낮 햇빛 색온도 (K) · 낮을수록 따뜻함 (5200은 화면 전체가 노랗게 보여 5900으로 맞춤) */
	UPROPERTY(EditAnywhere, Category = "2 낮", meta = (DisplayName = "Day Sun Temperature (낮 색온도)", ClampMin = "1700", ClampMax = "12000"))
	float DaySunTemperature = 5900.f;

	/** 낮 하늘빛 세기 · 그림자 밝기 */
	UPROPERTY(EditAnywhere, Category = "2 낮", meta = (DisplayName = "Day Sky Intensity (낮 하늘빛 세기)", ClampMin = "0"))
	float DaySkyIntensity = 1.f;

	/** 낮 하늘빛 색 · 그림자에 남보라 기운 (기획서 그림자 #403E59 방향) */
	UPROPERTY(EditAnywhere, Category = "2 낮", meta = (DisplayName = "Day Sky Color (낮 하늘빛 색)"))
	FLinearColor DaySkyColor = FLinearColor(FColor(214, 206, 242));

	/** 밤 햇빛 세기 (lux) · 달빛 정도 (명도만 낮추고 시설·길이 보이게) */
	UPROPERTY(EditAnywhere, Category = "3 밤", meta = (DisplayName = "Night Sun Intensity (밤 햇빛 세기)", ClampMin = "0"))
	float NightSunIntensity = 3.f;

	/** 밤 햇빛 색 (푸른 달빛) */
	UPROPERTY(EditAnywhere, Category = "3 밤", meta = (DisplayName = "Night Sun Color (밤 햇빛 색)"))
	FLinearColor NightSunColor = FLinearColor(FColor(196, 206, 255));

	/** 밤 햇빛 색온도 (K) */
	UPROPERTY(EditAnywhere, Category = "3 밤", meta = (DisplayName = "Night Sun Temperature (밤 색온도)", ClampMin = "1700", ClampMax = "12000"))
	float NightSunTemperature = 8000.f;

	/** 밤 하늘빛 세기 */
	UPROPERTY(EditAnywhere, Category = "3 밤", meta = (DisplayName = "Night Sky Intensity (밤 하늘빛 세기)", ClampMin = "0"))
	float NightSkyIntensity = 0.75f;

	/** 밤 하늘빛 색 */
	UPROPERTY(EditAnywhere, Category = "3 밤", meta = (DisplayName = "Night Sky Color (밤 하늘빛 색)"))
	FLinearColor NightSkyColor = FLinearColor(FColor(168, 170, 230));

	/** 밤 노출 보정 (EV) · 자동 노출이 밤을 다시 밝히지 않게 낮춘다 (0이면 보정 없음) */
	UPROPERTY(EditAnywhere, Category = "3 밤", meta = (DisplayName = "Night Exposure Bias (밤 노출 보정)", ClampMin = "-6", ClampMax = "0"))
	float NightExposureBias = -0.7f;

	/** 밤에 켜는 빛의 이름표 (빛 컴포넌트의 Component Tags) */
	UPROPERTY(EditAnywhere, Category = "3 밤", meta = (DisplayName = "Night Light Tag (밤 빛 이름표)"))
	FName NightLightTag = TEXT("CozyNightLight");

	/** 해 질 녘·해 뜰 녘 길이 (게임 시간 · 시) · 1이면 17:30~18:30처럼 밤 시작 앞뒤 30분씩 서서히 바뀜 (하루 30분이면 현실 약 75초) */
	UPROPERTY(EditAnywhere, Category = "4 전환", meta = (DisplayName = "Twilight Game Hours (해 질 녘 길이, 게임 시)", ClampMin = "0", ClampMax = "6"))
	float TwilightGameHours = 1.f;

	/** 디버그로 낮/밤을 고정하거나 풀 때 바뀌는 데 걸리는 시간 (현실 초) · 밤 모습 확인은 플레이 중 디버그 창(F1) '낮/밤' 버튼으로 */
	UPROPERTY(EditAnywhere, Category = "4 전환", meta = (DisplayName = "Transition Seconds (전환 시간)", ClampMin = "0"))
	float TransitionSeconds = 2.5f;

protected:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> Exposure;

private:

	void FindLights();
	void Apply(float Alpha);
	void RefreshNightLights();
	/** 지금 게임 시각(또는 디버그 고정)에 맞는 밤 정도 (0~1) */
	float TargetNightAlpha() const;

	float NightAlpha = 0.f;
	float RescanTimer = 0.f;

	/** 이름표 빛의 원래 세기 (처음 찾았을 때 기록 · 밤 정도만큼 곱함) */
	TMap<TWeakObjectPtr<ULightComponent>, float> NightLightBase;
};
