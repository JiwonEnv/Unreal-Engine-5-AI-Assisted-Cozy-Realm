#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CozyTitleSettings.generated.h"

class UTexture2D;
class UWorld;

/**
 *  프로젝트 설정 'Cozy Title (타이틀 화면)' · 타이틀 화면에서 바꿀 수 있는 값 (코드에 에셋 경로를 두지 않음).
 *  저장 위치: Config/DefaultGame.ini [/Script/CozyRealm.CozyTitleSettings]
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Cozy Title (타이틀 화면)"))
class UCozyTitleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** 이어하기 · 새 게임을 누르면 들어갈 영지 맵 */
	UPROPERTY(Config, EditAnywhere, Category = "1 이동 (Maps)", meta = (DisplayName = "Estate Map (영지 맵)"))
	TSoftObjectPtr<UWorld> EstateMap;

	/** 배경으로 불러올 맵 (실제 영지 풍경 · 시설은 영지를 시작해야 생기므로 풍경만 보임) · 비우면 종이색 바탕 */
	UPROPERTY(Config, EditAnywhere, Category = "2 배경 (Background)", meta = (DisplayName = "Background Level (배경 맵)"))
	TSoftObjectPtr<UWorld> BackgroundLevel;

	/** 배경 카메라 위치 · 방향 · 화각 */
	UPROPERTY(Config, EditAnywhere, Category = "2 배경 (Background)", meta = (DisplayName = "Camera Location (카메라 위치)"))
	FVector CameraLocation = FVector(-1700.f, -2000.f, 800.f);

	UPROPERTY(Config, EditAnywhere, Category = "2 배경 (Background)", meta = (DisplayName = "Camera Rotation (카메라 방향)"))
	FRotator CameraRotation = FRotator(-22.f, 48.f, 0.f);

	UPROPERTY(Config, EditAnywhere, Category = "2 배경 (Background)", meta = (DisplayName = "Camera FOV (카메라 화각)", ClampMin = "20", ClampMax = "120"))
	float CameraFOV = 55.f;

	/** 카메라가 천천히 좌우로 흔들리는 폭 (cm · 0이면 고정) */
	UPROPERTY(Config, EditAnywhere, Category = "2 배경 (Background)", meta = (DisplayName = "Camera Drift (카메라 흔들림 폭)", ClampMin = "0"))
	float CameraDrift = 120.f;

	/** 글자 로고 (로고 그림이 없을 때) · 정식 제목이 정해지면 바꾼다 */
	UPROPERTY(Config, EditAnywhere, Category = "3 로고 (Logo)", meta = (DisplayName = "Logo Text (글자 로고)"))
	FString LogoText = TEXT("CozyRealm");

	/** 로고 아래 작은 글 */
	UPROPERTY(Config, EditAnywhere, Category = "3 로고 (Logo)", meta = (DisplayName = "Subtitle (부제)"))
	FString Subtitle;

	/** 로고 그림 (지정하면 글자 로고 대신 이 그림) */
	UPROPERTY(Config, EditAnywhere, Category = "3 로고 (Logo)", meta = (DisplayName = "Logo Image (로고 그림)"))
	TSoftObjectPtr<UTexture2D> LogoImage;

	/** 글자 로고 옆 · 선택한 버튼 옆에 붙는 작은 그림 (벚꽃) */
	UPROPERTY(Config, EditAnywhere, Category = "3 로고 (Logo)", meta = (DisplayName = "Accent Icon (벚꽃 아이콘)"))
	TSoftObjectPtr<UTexture2D> AccentIcon;

	/** 테마 이름표: 가장 눈에 띄는 버튼 · 나머지 버튼 · 버튼 묶음 패널 · 확인 창 */
	UPROPERTY(Config, EditAnywhere, Category = "4 모양 (Style)", meta = (DisplayName = "Primary Button Style (기본 버튼 모양)"))
	FName PrimaryButtonStyle = TEXT("Main");

	UPROPERTY(Config, EditAnywhere, Category = "4 모양 (Style)", meta = (DisplayName = "Button Style (버튼 모양)"))
	FName ButtonStyle = TEXT("Default");

	UPROPERTY(Config, EditAnywhere, Category = "4 모양 (Style)", meta = (DisplayName = "Panel Image (패널 이미지)"))
	FName PanelImage = TEXT("Panel");

	UPROPERTY(Config, EditAnywhere, Category = "4 모양 (Style)", meta = (DisplayName = "Window Image (확인 창 이미지)"))
	FName WindowImage = TEXT("Window");

	/** 버전 뒤에 붙는 글 (번호는 프로젝트 설정 > Project > Description > Project Version) */
	UPROPERTY(Config, EditAnywhere, Category = "5 버전 (Version)", meta = (DisplayName = "Version Suffix (버전 꼬리말)"))
	FString VersionSuffix = TEXT("개발판");

	static const UCozyTitleSettings* Get() { return GetDefault<UCozyTitleSettings>(); }
};
