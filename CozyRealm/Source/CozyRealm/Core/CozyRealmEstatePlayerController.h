#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CozyRealmEstatePlayerController.generated.h"

class UCozyHudWidget;
class ACozyFacilityActor;
class ACozyHeroCharacter;
struct FCozySurfaceHit;

/**
 *  Player controller for the estate.
 *  Mouse-driven: the cursor stays visible and click/hover events are enabled for selecting facilities.
 */
UCLASS()
class ACozyRealmEstatePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ACozyRealmEstatePlayerController();

	/** 지금 선택된 시설 (없으면 nullptr) */
	ACozyFacilityActor* GetSelectedFacility() const { return SelectedFacility.Get(); }

	/** 화면 UI (편집 가능한 화면이 기존 기능을 부를 때 씀) */
	UCozyHudWidget* GetHud() const { return Hud; }

	/** 이 정의의 시설(같은 시설이 여러 개면 레벨이 가장 높은 것)을 선택 · 업그레이드 조건의 '이동' 버튼 */
	void SelectFacilityByDefinition(FName DefinitionId);

	/** 배치 모드 (B) · 시설을 끌어서 옮기고 회전·보관·확정·취소 (D45) */
	void SetPlacementMode(bool bEnable);
	bool IsPlacementMode() const { return bPlacementMode; }

	/** 시설 선택 해제 (선택 표시 · 기능 아이콘 숨김) · 상세 창을 열 때 HUD도 부름 */
	void ClearSelection();

	/**
	 *  개발용 콘솔 명령 (플레이 중 ` 키 → 입력): 재료·재화 수량을 정확히 맞춘다.
	 *  검증에서 바꾼 저장 데이터를 원래대로 돌릴 때 쓴다. 예: CozySetResource TimeTalisman 1
	 */
	UFUNCTION(Exec)
	void CozySetResource(FName ItemId, int32 Amount);
	/** 0 종료 / 1 브러시 / 2 타일. 제작 검증용 입력 진입점. */
	UFUNCTION(Exec) void CozyGroundMode(int32 Mode);

	/** 브러시를 이 지점까지 끈다 (마우스 드래그 한 프레임과 같음) · 표면이 바뀌어도 한 드래그로 이어 칠함 */
	void PaintBrushTo(const FCozySurfaceHit& Hit);
	/** 지금 드래그를 끝내고 되돌리기 한 단위로 묶음 */
	void FinishGroundStroke();
	/** 마지막 드래그 한 번(여러 표면에 걸쳐도 함께)을 되돌림 / 다시 실행 */
	bool UndoGroundPaint();
	bool RedoGroundPaint();
	void SetBrushForTest(float Radius, int32 Style) { BrushRadius = Radius; GroundStyle = Style; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void FlushPressedKeys() override;

	/** 영지 화면 UI 클래스 (재화 표시 · 기능 아이콘 · 전용 창 · 디버그 메뉴) */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCozyHudWidget> HudClass;

private:
	bool bGroundLeftControl = false;
	bool bGroundRightControl = false;

	/** 왼쪽 클릭: 시설이면 선택하고 기능 아이콘을, 빈 곳이면 선택 해제 */
	void HandleWorldClick();
	void SelectFacility(ACozyFacilityActor* Facility);

	UPROPERTY(Transient)
	TObjectPtr<UCozyHudWidget> Hud;

	TWeakObjectPtr<ACozyFacilityActor> SelectedFacility;

	/** 주인공 (4주차 기능 1 · 프로젝트 설정 Cozy Hero에 설정 에셋이 있을 때만) */
	void SpawnHero();
	/** WASD(카메라 방향 기준) · Shift 달리기 · 오른쪽 클릭 땅 이동 / 시설 메뉴 · E 상호작용 */
	void TickHero(bool bCompact);
	ACozyFacilityActor* FindFacilityActor(const FGuid& FacilityId) const;

	UPROPERTY(Transient)
	TObjectPtr<ACozyHeroCharacter> Hero;

	/** 배치 모드 입력 (누르고 끌기 · R 회전) */
	void TickPlacement();
	bool bPlacementMode = false;
	bool bDraggingPlacement = false;
	void TickGroundEditing();
	/** 지금 칠하는 표면의 기록을 닫음 (드래그는 계속) */
	void EndPaintSegment();
	/** 표면 ID·주인공 발 높이 디버그 표시 (콘솔 Cozy.SurfaceDebug 1) */
	void DrawSurfaceDebug(const struct FCozySurfaceHit* Hit) const;
	int32 GroundMode = 0;
	int32 GroundStyle = 1;
	int32 GroundRotation = 0;
	float BrushRadius = 120.f;
	FVector LastBrushPosition = FVector::ZeroVector;
	TWeakObjectPtr<class UCozySurfaceComponent> StrokeSurface;
	/** 한 드래그 동안 기록이 생긴 표면들 (같은 표면이 여러 번 나올 수 있음) */
	TArray<TWeakObjectPtr<class UCozySurfaceComponent>> DragSurfaces;
	bool bPaintDragActive = false;
	/** 드래그 중 마지막으로 찍은 표면 (벽면을 지나 기록이 닫혀도 이어 칠할 후보로 씀) */
	TWeakObjectPtr<class UCozySurfaceComponent> LastStampSurface;
	/** 드래그 단위 되돌리기 · 다시 실행 (각 표면 기록 16회와 같은 한도) */
	TArray<TArray<TWeakObjectPtr<class UCozySurfaceComponent>>> PaintUndoGroups, PaintRedoGroups;
	bool bFloorPreview = false;
	FVector FloorPreviewPosition = FVector::ZeroVector;
	FGuid FloorPreviewSurface;
	FGuid MovingFloorId;
	FString LastRecoveryNotice;
};
