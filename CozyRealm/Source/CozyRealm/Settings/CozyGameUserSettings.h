#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/DeveloperSettings.h"
#include "Data/CozyRealmDataTypes.h"
#include "CozyGameUserSettings.generated.h"

class USoundClass;
class USoundMix;
class USoundBase;

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
	/** 불러온 뒤 예전 설정 파일을 지금 기본값 규칙에 맞춤 (예: 최대 프레임 0(제한 없음)으로 저장된 첫 설정 → 60) */
	virtual void LoadSettings(bool bForceReload = false) override;

	/** 이 게임의 설정 규칙 버전 (1 = 최대 프레임 기본 60 · 출시 수정 2차 2026-10-30) · 낮으면 LoadSettings에서 한 번만 맞춤 */
	UPROPERTY(config)
	int32 CozySettingsVersion = 0;

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
 *  그래픽 부담 줄이기 (프로젝트 설정 → Cozy Performance · DefaultGame.ini [/Script/CozyRealm.CozyPerformanceSettings]).
 *  방치형이라 창을 뒤에 두거나 작은 창으로 켜 두는 시간이 길어서, 그때는 프레임을 낮춰 GPU·전기를 아낀다.
 *  플레이어가 고른 '최대 프레임'(GameUserSettings)보다 높게 올리지는 않는다 (더 낮은 값만 적용).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Cozy Performance (그래픽 부담)"))
class UCozyPerformanceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UCozyPerformanceSettings* Get() { return GetDefault<UCozyPerformanceSettings>(); }

	/** 첫 실행·기본값 복원 때의 최대 프레임 (0 = 제한 없음) */
	UPROPERTY(Config, EditAnywhere, Category = "프레임", meta = (DisplayName = "Default Frame Limit (기본 최대 프레임)", ClampMin = "0"))
	int32 DefaultFrameLimit = 60;

	/** 게임 창이 다른 창 뒤에 있거나 최소화됐을 때 최대 프레임 (0 = 낮추지 않음) */
	UPROPERTY(Config, EditAnywhere, Category = "프레임", meta = (DisplayName = "Background Frame Limit (뒤에 있을 때)", ClampMin = "0"))
	int32 BackgroundFrameLimit = 15;

	/** 작은 창 모드(M)일 때 최대 프레임 (0 = 낮추지 않음) · 창이 뒤에 있으면 Background 값이 우선 */
	UPROPERTY(Config, EditAnywhere, Category = "프레임", meta = (DisplayName = "Compact Frame Limit (작은 창일 때)", ClampMin = "0"))
	int32 CompactFrameLimit = 30;

	/** 화면 탭 '이 탭 기본값 복원'의 그래픽 품질 (0 낮음 · 1 중간 · 2 높음 · 3 최고) */
	UPROPERTY(Config, EditAnywhere, Category = "품질", meta = (DisplayName = "Default Graphics Quality (기본 그래픽 품질)", ClampMin = "0", ClampMax = "3"))
	int32 DefaultGraphicsQuality = 3;
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

	/**
	 *  영지 일 알림 소리 (수확 · 가공 완료 · 업그레이드 완료 · 판매 · 배치 확정 · 구매 · 주민 배치 → 소리 · 출시 수정 2차).
	 *  비어 있거나 빠진 알림은 무음 · 소리 에셋의 Sound Class를 SC_SFX로 두면 효과음 슬라이더를 따른다
	 */
	UPROPERTY(Config, EditAnywhere, Category = "알림 소리", meta = (DisplayName = "Event Sounds (영지 일 알림 소리)"))
	TMap<ECozyEstateEventKind, TSoftObjectPtr<USoundBase>> EventSounds;

	/** 영지 배경음 (비우면 무음 · 반복은 소리 에셋의 Looping · Sound Class는 SC_Music) */
	UPROPERTY(Config, EditAnywhere, Category = "배경음", meta = (DisplayName = "Estate Music (영지 배경음)"))
	TSoftObjectPtr<USoundBase> EstateMusic;

	/** 타이틀 화면 배경음 (비우면 무음) */
	UPROPERTY(Config, EditAnywhere, Category = "배경음", meta = (DisplayName = "Title Music (타이틀 배경음)"))
	TSoftObjectPtr<USoundBase> TitleMusic;
};
