#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CozyDemoSettings.generated.h"

/**
 *  무료 데모 끝 안내 (프로젝트 설정 → Cozy Demo · DefaultGame.ini [/Script/CozyRealm.CozyDemoSettings] · 출시 수정 2차 2026-10-30).
 *  신사가 데모 마지막 레벨(성장 표의 신사 최대 레벨)에 닿으면 한 번 띄운다 (영지 저장의 bDemoEndShown).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Cozy Demo (데모 끝 안내)"))
class UCozyDemoSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UCozyDemoSettings* Get() { return GetDefault<UCozyDemoSettings>(); }

	/** 안내 제목 */
	UPROPERTY(Config, EditAnywhere, Category = "데모 끝", meta = (DisplayName = "End Title (제목)"))
	FText EndTitle;

	/** 안내 본문 (줄바꿈은 \n 대신 Shift+Enter) */
	UPROPERTY(Config, EditAnywhere, Category = "데모 끝", meta = (DisplayName = "End Body (본문)", MultiLine = true))
	FText EndBody;

	/** 위시리스트 버튼 글자 */
	UPROPERTY(Config, EditAnywhere, Category = "데모 끝", meta = (DisplayName = "Wishlist Label (위시리스트 버튼)"))
	FText WishlistLabel;

	/** 닫기 버튼 글자 */
	UPROPERTY(Config, EditAnywhere, Category = "데모 끝", meta = (DisplayName = "Continue Label (계속 버튼)"))
	FText ContinueLabel;

	/** ❓ Steam 상점 페이지 주소 (비우면 위시리스트 버튼이 꺼지고 '준비 중' 안내) */
	UPROPERTY(Config, EditAnywhere, Category = "데모 끝", meta = (DisplayName = "Wishlist Url (Steam 상점 주소)"))
	FString WishlistUrl;
};
