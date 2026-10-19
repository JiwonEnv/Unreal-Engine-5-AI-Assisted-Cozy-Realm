#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CozyEstateTerrain.generated.h"

class ALandscape;
class UCozySeasonPreset;
class UMaterialInterface;

/**
 *  영지 지형 · 계절 (랜드스케이프 하나를 만들고 계절 모습을 바꾼다).
 *  - '지형 새로 만들기': 가운데는 평평한 산속 빈 땅(영지 · 앞으로 넓어질 자리까지), 바깥은 산비탈 · 산등성이.
 *    재질 칸은 SourceArt/Landscape/LandscapeLayers.csv 표를 따른다 (칸을 늘리려면 표에 줄을 더하고 '재질 칸 갱신').
 *    만든 뒤에는 랜드스케이프 모드의 붓으로 높낮이·재질을 자유롭게 손질한다 (다시 '새로 만들기'를 누르면 손질한 것을 덮어씀).
 *  - 계절: Seasons 목록에서 Current Season을 고르면 에디터에서 바로 바뀌고, 게임 중에는 SetSeason · 디버그 창으로 바꾼다.
 *    칠해 둔 재질 칸 위치는 그대로이고, 칸마다의 모습(색 · 그림 · 눈 덮임)만 계절 머티리얼을 따른다.
 */
UCLASS(meta = (DisplayName = "Cozy Estate Terrain (영지 지형 · 계절)"))
class ACozyEstateTerrain : public AActor
{
	GENERATED_BODY()

public:

	ACozyEstateTerrain();

	virtual void BeginPlay() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** 이 액터가 만들고 관리하는 랜드스케이프 (지형 새로 만들기로 생김) */
	UPROPERTY(EditAnywhere, Category = "1 지형", meta = (DisplayName = "Landscape (랜드스케이프)"))
	TObjectPtr<ALandscape> Landscape;
	/** 고정 지형을 수정해 배치 가능 영역이 달라지면 증가시킨다. */
	UPROPERTY(EditAnywhere, Category = "1 지형", meta=(ClampMin="1")) int32 TerrainRevision = 1;

	/** 평평한 땅 절반 크기 (cm) · 영지가 가장 넓어질 때의 절반 + 여유 · 지금 영지 절반은 1200 */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Flat Half Size (평평한 땅 절반 크기)", ClampMin = "500"))
	float FlatHalfSize = 2900.f;

	/** 평평한 땅 높이 (cm) · 시설이 놓이는 바닥 높이 */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Ground Z (바닥 높이)"))
	float GroundZ = 20.f;

	/** 산비탈이 끝나고 산등성이가 되는 거리 (영지 중심에서 · cm) */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Ridge Distance (산등성이 거리)", ClampMin = "1000"))
	float RidgeDistance = 6400.f;

	/** 산등성이 높이 (cm) */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Ridge Height (산 높이)", ClampMin = "0"))
	float RidgeHeight = 1800.f;

	/** 울퉁불퉁함 높이 (cm) · 평평한 땅에는 들어가지 않음 */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Noise Height (울퉁불퉁함)", ClampMin = "0"))
	float NoiseHeight = 520.f;

	/** 울퉁불퉁함 크기 (cm) · 클수록 완만한 언덕 */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Noise Size (언덕 크기)", ClampMin = "200"))
	float NoiseSize = 2600.f;

	/** 모양 무작위 씨앗 */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Random Seed (씨앗)"))
	int32 RandomSeed = 7;

	/** 한 변의 구획 수 · 구획 하나 = 63칸 (지금 4 → 252칸) */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Components Per Side (한 변 구획 수)", ClampMin = "1", ClampMax = "16"))
	int32 ComponentsPerSide = 4;

	/** 한 칸 크기 (cm) · 252칸 × 64cm ≈ 161m */
	UPROPERTY(EditAnywhere, Category = "2 모양", meta = (DisplayName = "Quad Size (칸 크기)", ClampMin = "25", ClampMax = "400"))
	float QuadSize = 64.f;

	/** 재질 칸 표 (프로젝트 폴더 기준) · 줄 하나 = 칸 하나 (Name · Label · 계절별 색 · Roughness) */
	UPROPERTY(EditAnywhere, Category = "3 재질 칸", meta = (DisplayName = "Layer Table (재질 칸 표)"))
	FString LayerTablePath = TEXT("SourceArt/Landscape/LandscapeLayers.csv");

	/** 칸마다의 레이어 정보 에셋을 두는 폴더 */
	UPROPERTY(EditAnywhere, Category = "3 재질 칸", meta = (DisplayName = "Layer Info Folder (레이어 정보 폴더)"))
	FString LayerInfoFolder = TEXT("/Game/CozyRealm/Environment/Landscape/LayerInfo");

	/** 고를 수 있는 계절 (DA_Season_*) · 디버그 '계절' 버튼은 이 순서대로 돈다 */
	UPROPERTY(EditAnywhere, Category = "4 계절", meta = (DisplayName = "Seasons (계절 목록)"))
	TArray<TObjectPtr<UCozySeasonPreset>> Seasons;

	/** 지금 계절 · 바꾸면 에디터에서 바로 지형 모습이 바뀜 */
	UPROPERTY(EditAnywhere, Category = "4 계절", meta = (DisplayName = "Current Season (지금 계절)"))
	TObjectPtr<UCozySeasonPreset> CurrentSeason;

	/** 계절 바꾸기 (게임 중에도 됨) */
	UFUNCTION(BlueprintCallable, Category = "Season")
	void SetSeason(UCozySeasonPreset* NewSeason);

	/** 목록의 다음 계절로 (디버그 버튼) */
	UFUNCTION(BlueprintCallable, Category = "Season")
	void CycleSeason();

#if WITH_EDITOR
	/** 표대로 랜드스케이프를 새로 만든다 · 이미 있으면 지우고 다시 만듦 (붓으로 손질한 높낮이·재질은 사라짐) */
	UFUNCTION(CallInEditor, Category = "5 도구", meta = (DisplayName = "지형 새로 만들기 (손질한 것 덮어씀)"))
	void GenerateTerrain();

	/** 표에 새로 더한 재질 칸을 랜드스케이프에 붙인다 (높낮이·칠한 것은 그대로) */
	UFUNCTION(CallInEditor, Category = "5 도구", meta = (DisplayName = "재질 칸 갱신 (표에 더한 칸 붙이기)"))
	void SyncLayers();
#endif

private:

	void ApplySeason();

	/** 표의 칸 이름 목록 (첫 줄은 제목) */
	TArray<FName> ReadLayerNames() const;

#if WITH_EDITOR
	class ULandscapeLayerInfoObject* FindOrCreateLayerInfo(FName LayerName) const;
	float HeightAt(float X, float Y) const;
#endif
};
