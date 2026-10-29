#include "Settings/CozyGameUserSettings.h"
#include "Engine/Engine.h"

UCozyGameUserSettings* UCozyGameUserSettings::Get()
{
	return GEngine ? Cast<UCozyGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UCozyGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MasterVolume = 1.f;
	MusicVolume = 1.f;
	SfxVolume = 1.f;
	bCompactAlwaysOnTop = false;
	// 🤖 첫 실행 최대 프레임: 엔진 기본(0 = 제한 없음) 대신 DefaultGame.ini의 기본값 (방치형이라 켜 두는 시간이 김)
	FrameRateLimit = static_cast<float>(UCozyPerformanceSettings::Get()->DefaultFrameLimit);
	CozySettingsVersion = 1;
}

void UCozyGameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	if (CozySettingsVersion < 1)
	{
		// 규칙 버전 1 이전 설정 파일: 제한 없음(0)이면 기본 최대 프레임으로 한 번만 바꿈 (그 뒤에 플레이어가 고른 '제한 없음'은 그대로)
		if (FrameRateLimit <= 0.f)
		{
			FrameRateLimit = static_cast<float>(UCozyPerformanceSettings::Get()->DefaultFrameLimit);
		}
		CozySettingsVersion = 1;
	}
}
