#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Estate/CozyEstateState.h"
#include "CozyEstateSaveGame.generated.h"

/**
 * 영지 저장 파일 (2주차 기능 3 · 슬롯 1개)
 * 상태는 FCozyEstateState 그대로 저장한다 (시설·주민·작업 기록·재화·지급한 보상 · ID로 서로 참조 · 정리 6-6).
 */
UCLASS()
class COZYREALM_API UCozyEstateSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/** 저장 파일 형식 버전 (구조가 바뀌면 올리고 불러올 때 변환) */
	UPROPERTY()
	int32 FileVersion = 1;

	/** 저장한 실제 시각 (UTC) · 방치 보상은 이 시각부터 계산하고, 정산 직후 다시 저장해 같은 시간을 두 번 받지 않게 한다 */
	UPROPERTY()
	FDateTime SavedUtc;

	UPROPERTY()
	FCozyEstateState State;
};
