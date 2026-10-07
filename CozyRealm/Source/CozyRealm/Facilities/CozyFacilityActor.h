#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CozyFacilityActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
struct FCozyFacilityRow;

/**
 *  영지에 놓인 시설 1개의 모습 (에셋이 생기기 전에는 회색 상자).
 *  상태는 영지 서비스가 가지고, 이 액터는 고유 ID로 그 상태를 가리키며 보여 주기만 한다.
 */
UCLASS()
class ACozyFacilityActor : public AActor
{
	GENERATED_BODY()

public:

	ACozyFacilityActor();

	/** 시설 정의로 크기·색·이름을 맞춘다 (영지 서비스가 생성 직후 호출) */
	void InitFacility(const FGuid& InFacilityId, const FCozyFacilityRow& Def, float CellSize);

	/** 선택 표시 (바닥 오라 + 테두리 외곽선) */
	void SetSelected(bool bInSelected);

	/** 배치 모드 미리보기: 시설이 차지하는 칸 바닥을 초록(놓을 수 있음) / 빨강(놓을 수 없음)으로 */
	void SetPlacementPreview(bool bActive, bool bValid);

	const FGuid& GetFacilityId() const { return FacilityId; }

	/** 기능 아이콘을 띄울 월드 위치 (상자 위쪽) */
	FVector GetIconAnchorLocation() const;

	/** 시설 이름 표시를 띄울 월드 위치 (상자 바로 위) · 이름은 HUD가 한글 글꼴로 그림 */
	FVector GetNameAnchorLocation() const;

protected:

	/** 회색 상자 */
	UPROPERTY(VisibleAnywhere, Category = "Facility")
	TObjectPtr<UStaticMeshComponent> GreyboxMesh;

	/** 선택했을 때 시설이 차지하는 칸 바닥에 깔리는 오라 */
	UPROPERTY(VisibleAnywhere, Category = "Facility")
	TObjectPtr<UStaticMeshComponent> AuraMesh;

	/** 선택했을 때 상자 모서리를 따라 그리는 테두리 외곽선 (모서리 12개) */
	UPROPERTY(VisibleAnywhere, Category = "Facility")
	TArray<TObjectPtr<UStaticMeshComponent>> OutlineEdges;

	/** 선택 시 바닥 오라 색 */
	UPROPERTY(EditAnywhere, Category = "Selection")
	FLinearColor AuraColor = FLinearColor(1.f, 0.62f, 0.05f);

	/** 바닥 오라가 상자 밖으로 보이는 폭 (cm) · 클수록 둘레가 넓게 보임 */
	UPROPERTY(EditAnywhere, Category = "Selection")
	float AuraMargin = 45.f;

	/** 바닥 오라를 지면에서 띄우는 높이 (cm) · 지면에 묻히지 않을 만큼만 */
	UPROPERTY(EditAnywhere, Category = "Selection")
	float AuraLift = 2.f;

	/** 상자 밑면을 지면에서 띄우는 높이 (cm) · 오라보다 높아야 오라가 상자 밑으로 이어져 보임 */
	UPROPERTY(EditAnywhere, Category = "Selection")
	float BoxLift = 4.f;

	/** 테두리 외곽선 색 */
	UPROPERTY(EditAnywhere, Category = "Selection")
	FLinearColor OutlineColor = FLinearColor(1.f, 0.8f, 0.2f);

	/** 테두리 외곽선 두께 (cm) */
	UPROPERTY(EditAnywhere, Category = "Selection")
	float OutlineThickness = 7.f;

private:

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AuraMaterial;

	FGuid FacilityId;
	float BoxHeight = 150.f;
	FLinearColor BaseColor = FLinearColor::Gray;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GreyboxMaterial;
};
