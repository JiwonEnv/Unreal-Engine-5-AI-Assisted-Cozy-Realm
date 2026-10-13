#include "Environment/CozyDayNightLighting.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Estate/CozyEstateState.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Facilities/CozyFacilityActor.h"

namespace CozyDayNight
{
	/** 새로 생긴 밤 빛(시설 배치 등)을 찾는 간격 (초) */
	constexpr float RescanSeconds = 0.5f;
	/** 이 정도보다 어두우면 밤 빛을 아예 끈다 */
	constexpr float LightOffAlpha = 0.01f;
	/** 시설에 주민(오니비)이 있을 때만 켜는 빛의 이름표 */
	const FName OnibiLightTag(TEXT("CozyOnibiLight"));
}

ACozyDayNightLighting::ACozyDayNightLighting()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 노출 보정만 덮어쓰는 전역 후처리 (다른 후처리 설정은 건드리지 않음)
	Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));
	Exposure->SetupAttachment(RootComponent);
	Exposure->bUnbound = true;
	Exposure->Settings.bOverride_AutoExposureBias = true;
	Exposure->Settings.AutoExposureBias = 0.f;
}

void ACozyDayNightLighting::BeginPlay()
{
	Super::BeginPlay();
	FindLights();
	// 시작할 때는 전환 없이 지금 상태로
	NightAlpha = TargetNightAlpha();
	RefreshNightLights();
	Apply(NightAlpha);
}

void ACozyDayNightLighting::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = TargetNightAlpha();
	if (!FMath::IsNearlyEqual(NightAlpha, Target, 0.0005f))
	{
		// 게임 시각에 따른 서서히 바뀌는 변화는 그대로 따르고, 디버그 고정처럼 갑자기 바뀔 때만 전환 시간에 걸쳐 바꿈
		NightAlpha = TransitionSeconds <= 0.f ? Target : FMath::FInterpConstantTo(NightAlpha, Target, DeltaSeconds, 1.f / TransitionSeconds);
		Apply(NightAlpha);
	}

	RescanTimer += DeltaSeconds;
	if (RescanTimer >= CozyDayNight::RescanSeconds)
	{
		RescanTimer = 0.f;
		RefreshNightLights();
	}
}

float ACozyDayNightLighting::TargetNightAlpha() const
{
	const UWorld* World = GetWorld();
	const UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	if (!Estate)
	{
		return 0.f;
	}
	switch (Estate->GetNightOverride())
	{
	case ECozyNightOverride::ForceDay: return 0.f;
	case ECozyNightOverride::ForceNight: return 1.f;
	default: break;
	}
	if (TwilightGameHours <= 0.f)
	{
		return Estate->IsNight() ? 1.f : 0.f;
	}
	// 밤 시작·끝 시각의 앞뒤 TwilightGameHours/2 안에서만 서서히 바뀌고, 그 밖은 낮 0 · 밤 1
	const float Hour = static_cast<float>(Estate->GetDayHour());
	auto Signed = [](float Delta) { return FMath::Fmod(Delta + 36.f, 24.f) - 12.f; }; // -12~12시로 접음
	const float Half = TwilightGameHours * 0.5f;
	const float ToDusk = Signed(Hour - Estate->GetNightStartHour());
	const float ToDawn = Signed(Hour - Estate->GetNightEndHour());
	if (FMath::Abs(ToDusk) < Half)
	{
		return FMath::SmoothStep(-Half, Half, ToDusk);
	}
	if (FMath::Abs(ToDawn) < Half)
	{
		return 1.f - FMath::SmoothStep(-Half, Half, ToDawn);
	}
	return Estate->IsNight() ? 1.f : 0.f;
}

void ACozyDayNightLighting::FindLights()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!Sun)
	{
		TActorIterator<ADirectionalLight> It(World);
		Sun = It ? *It : nullptr;
	}
	if (!Sky)
	{
		TActorIterator<ASkyLight> It(World);
		Sky = It ? *It : nullptr;
	}
}

void ACozyDayNightLighting::Apply(float Alpha)
{
	if (Sun)
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetIntensity(FMath::Lerp(DaySunIntensity, NightSunIntensity, Alpha));
			Light->SetLightColor(FLinearColor::LerpUsingHSV(DaySunColor, NightSunColor, Alpha));
			Light->SetTemperature(FMath::Lerp(DaySunTemperature, NightSunTemperature, Alpha));
		}
	}
	if (Sky)
	{
		if (USkyLightComponent* Light = Sky->GetLightComponent())
		{
			Light->SetIntensity(FMath::Lerp(DaySkyIntensity, NightSkyIntensity, Alpha));
			Light->SetLightColor(FLinearColor::LerpUsingHSV(DaySkyColor, NightSkyColor, Alpha));
		}
	}
	Exposure->Settings.AutoExposureBias = NightExposureBias * Alpha;

	const UWorld* World = GetWorld();
	const UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	for (auto It = NightLightBase.CreateIterator(); It; ++It)
	{
		ULightComponent* Light = It.Key().Get();
		if (!Light)
		{
			It.RemoveCurrent();
			continue;
		}
		bool bOn = Alpha > CozyDayNight::LightOffAlpha;
		// 오니비 빛: 그 시설에 주민이 배치돼 있을 때만
		if (bOn && Light->ComponentHasTag(CozyDayNight::OnibiLightTag))
		{
			const ACozyFacilityActor* Facility = Cast<ACozyFacilityActor>(Light->GetOwner());
			const FCozyFacilityState* State = (Facility && Estate) ? Estate->FindFacility(Facility->GetFacilityId()) : nullptr;
			bOn = State && State->AssignedResidents.Num() > 0;
		}
		Light->SetVisibility(bOn);
		Light->SetIntensity(It.Value() * Alpha);
	}
}

void ACozyDayNightLighting::RefreshNightLights()
{
	UWorld* World = GetWorld();
	if (!World || NightLightTag.IsNone())
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<ULightComponent*> Lights(*It);
		for (ULightComponent* Light : Lights)
		{
			if (Light && Light->ComponentHasTag(NightLightTag) && !NightLightBase.Contains(Light))
			{
				NightLightBase.Add(Light, Light->Intensity);
			}
		}
	}
	// 새로 찾은 빛과 주민 배치 변화를 지금 밤 정도에 맞춤
	Apply(NightAlpha);
}
