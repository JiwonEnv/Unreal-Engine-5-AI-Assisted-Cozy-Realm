#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/DeveloperSettings.h"
#include "CozyGameUserSettings.generated.h"

class USoundClass;
class USoundMix;

/**
 *  플레이어 설정 (엔진 GameUserSettings + 음량 3개).
 *  - 저장 위치: Saved/Config/<플랫폼>/GameUserSettings.ini (영지 진행 저장 SaveGames/*.sav 와 따로)
 *  - 화면 모드 · 해상도 · 수직 동기화 · 최대 프레임은 엔진 기능을 그대로 쓴다
 *  - DefaultEngine.ini [/Script/Engine.Engine] GameUserSettingsClassName 으로 이 클래스를 쓰게 지정
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class UCozyGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:

	static UCozyGameUserSettings* Get();

	virtual void SetToDefaults() override;

	/** 전체 음량 (0~1) */
	UPROPERTY(config)
	float MasterVolume = 1.f;

	/** 배경음 음량 (0~1) */
	UPROPERTY(config)
	float MusicVolume = 1.f;

	/** 효과음 음량 (0~1) */
	UPROPERTY(config)
	float SfxVolume = 1.f;

	/** 작은 창 모드일 때 다른 프로그램 창보다 항상 위에 */
	UPROPERTY(config)
	bool bCompactAlwaysOnTop = false;

	/** 첫 실행 안내를 끝냈거나 건너뜀 (다시 띄우지 않음 · 설정 창 '안내 다시 보기'로 다시) · 기본값 복원(SetToDefaults)과 상관없이 유지 */
	UPROPERTY(config)
	bool bTutorialDone = false;
};

/**
 *  소리 연결 (프로젝트 설정 → Cozy Audio).
 *  음량 슬라이더는 이 믹스 하나로 사운드 클래스 3개의 음량을 바꾼다.
 *  새 배경음은 Music Class, 새 효과음은 Sfx Class를 사운드(또는 사운드 큐)의 Sound Class에 지정하면 슬라이더를 따른다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Cozy Audio (소리 연결)"))
class UCozyAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UCozyAudioSettings* Get() { return GetDefault<UCozyAudioSettings>(); }

	/** 음량 슬라이더가 쓰는 사운드 믹스 (클래스별 음량 덮어쓰기) */
	UPROPERTY(Config, EditAnywhere, Category = "소리", meta = (DisplayName = "Volume Mix (음량 믹스)"))
	TSoftObjectPtr<USoundMix> VolumeMix;

	/** 전체 음량 · 배경음과 효과음의 부모 */
	UPROPERTY(Config, EditAnywhere, Category = "소리", meta = (DisplayName = "Master Class (전체)"))
	TSoftObjectPtr<USoundClass> MasterClass;

	/** 배경음 (새 음악은 이 클래스로) */
	UPROPERTY(Config, EditAnywhere, Category = "소리", meta = (DisplayName = "Music Class (배경음)"))
	TSoftObjectPtr<USoundClass> MusicClass;

	/** 효과음 (새 효과음은 이 클래스로 · 프로젝트 기본 사운드 클래스로도 지정) */
	UPROPERTY(Config, EditAnywhere, Category = "소리", meta = (DisplayName = "Sfx Class (효과음)"))
	TSoftObjectPtr<USoundClass> SfxClass;
};
