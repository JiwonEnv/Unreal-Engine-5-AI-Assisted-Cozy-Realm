#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CozyHudWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UCozyEstateSubsystem;
class ACozyFacilityActor;

/** 버튼 클릭을 C++ 람다로 넘기는 작은 연결 객체 */
UCLASS()
class UCozyUiAction : public UObject
{
	GENERATED_BODY()

public:

	TFunction<void()> Callback;

	UFUNCTION()
	void Fire()
	{
		if (Callback)
		{
			Callback();
		}
	}
};

/** 화면을 덮는 전용 창의 종류 */
enum class ECozyWindowKind : uint8
{
	None,
	/** 시설 정보 (생산 상태) */
	FacilityInfo,
	/** 나가야: 주민 목록 · 배치 */
	Nagaya,
	/** 아직 내용이 없는 기능 창 틀 (가공 · 판매 · 업그레이드 · 신사) */
	Placeholder,
	/** 창고: 재료별 보유량 / 한도 / 받을 수 있는 수량 */
	Storage,
	/** 공통 가공: 레시피 → 제작 횟수 → 재료·시간 확인 → 시작 (모든 제작 시설 공통 · D32) */
	Processing,
	/** 판매소: 재료 선택 → 수량 → 받을 재화 확인 → 판매 */
	Sales,
	/** 후신소: 신사·시설·관리 시설 업그레이드 (공통 성장 처리 · D9) */
	Upgrade,
	/** 밭 관리 시설: 관리 단계 · 모든 밭 효과 · 해금 작물 (D37·D39) */
	FieldManagement,
	/** 방치 보상: 꺼 둔 동안 정산한 결과 (D44) */
	OfflineReport
};

/**
 *  영지 화면 UI (임시 글자 화면 · 디자인은 나중에).
 *  위쪽 재화 표시 · 선택한 시설 위 기능 아이콘 · 화면을 덮는 전용 창 · 디버그 메뉴(F1).
 *  상태는 영지 서비스에서 읽고, 버튼은 서비스에 요청만 보낸다 (정리 6-7).
 */
UCLASS()
class UCozyHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** 시설을 선택해 기능 아이콘을 띄운다 */
	void ShowFacilityIcons(ACozyFacilityActor* FacilityActor);

	/** 선택 해제 (아이콘 숨김) */
	void HideFacilityIcons();

	/** 전용 창을 연다 · TargetFacility는 나가야 창에서 배치할 시설 */
	void OpenWindow(ECozyWindowKind Kind, const FGuid& FacilityId, const FGuid& TargetFacility = FGuid());

	void CloseWindow();
	bool IsWindowOpen() const { return WindowKind != ECozyWindowKind::None; }
	/** 배치 모드 안내·보관함 패널과 미리보기 옆 버튼 (D45) */
	void SetPlacementMode(bool bEnable);

	/** 화면 위쪽에 잠깐 보이는 알림 (단축키 결과 등) */
	void ShowToast(const FText& Message);
	/** 단축키: Tab 주민 목록(나가야) · I 창고 · 이미 열려 있으면 닫음 */
	void ToggleNagayaWindow();
	void ToggleStorageWindow();

	void ToggleDebugPanel();

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	void BuildLayout();
	void HandleEstateChanged(bool bStructural);
	/** 시설 정보 창의 진행 바·상태 글자만 갱신 (창을 다시 만들지 않음) */
	void UpdateFacilityInfoLive();
	/** 창고 창의 숫자만 갱신 (창을 다시 만들지 않음) */
	void UpdateStorageLive();
	/** 가공 창의 글자·버튼 상태만 갱신 (창을 다시 만들지 않음) */
	void UpdateProcessingLive();
	/** 판매소 창의 글자·버튼 상태만 갱신 (창을 다시 만들지 않음) */
	void UpdateSalesLive();
	/** 후신소 창의 글자·버튼 상태만 갱신 */
	void UpdateUpgradeLive();
	/** 밭 관리 창의 글자만 갱신 */
	void UpdateFieldManagementLive();
	/** 미수령분 수령 버튼 공통 처리 (생산·가공) · 결과는 '방금 한 일'로 */
	void HandleCollectClicked(const FGuid& FacilityId);
	void SetFeedback(const FText& Message);

	void RefreshTopBar();
	/** 시설마다 이름표를 만든다 (시설 액터가 바뀌면 다시) */
	void RefreshNameLabels();
	void UpdateNameLabelPositions();
	void RefreshIcons();
	void RefreshWindow();
	void RefreshDebugPanel();

	void BuildFacilityInfoContent();
	void BuildNagayaContent();
	void BuildPlaceholderContent();
	void BuildStorageContent();
	void BuildProcessingContent();
	void BuildSalesContent();
	void BuildUpgradeContent();
	void BuildFieldManagementContent();
	void BuildOfflineReportContent();
	/** 보여 주지 않은 방치 보상이 있으면 창을 연다 */
	void ShowPendingOfflineReport();

	UTextBlock* MakeText(const FText& Text, int32 FontSize = 16, const FLinearColor& Color = FLinearColor::White);
	UButton* MakeButton(const FText& Label, TFunction<void()> OnClick, bool bEnabled = true, int32 FontSize = 15);

	UCozyEstateSubsystem* GetEstate() const;

	// --- 위젯 ---
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> TopBarBox;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> LabelLayer;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> NameLabels;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> IconBox;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> IconSlot;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowOverlay;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WindowTitle;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> WindowContent;
	// --- 배치 모드 ---
	void RefreshPlacementPanel();
	void UpdatePlacementLive();
	bool bPlacementMode = false;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PlacementPanel;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PlacementStoredBox;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PlacementActionBox;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> PlacementActionSlot;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlacementStatusText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PlacementStoreButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PlacementConfirmButton;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiAction>> PlacementActions;
	TArray<FGuid> PlacementStoredIds;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToastText;
	float ToastRemaining = 0.f;
	/** 창 내용이 화면보다 길면 스크롤 (높이는 화면 크기에 맞춰 RefreshWindow 때 정함) */
	UPROPERTY(Transient)
	TObjectPtr<class USizeBox> WindowContentSize;
	UPROPERTY(Transient)
	TObjectPtr<class UProgressBar> InfoProgressBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoStatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoRemainingText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoUnclaimedText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoStorageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoResidentText;
	/** 클릭 결과 (방금 한 일) · 현재 상태 줄과 구분 */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoFeedbackText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StorageAmountTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StorageSpaceTexts;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> InfoCollectButton;
	// --- 가공 창 (UpdateProcessingLive가 글자만 바꿈) ---
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcSelectedText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcRunsText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ProcMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ProcPlusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ProcMaxButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcMaxText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcSummaryText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcBlockText;
	/** 완료품의 공용 창고가 가득일 때 안내 (시작은 막지 않음 · D35) */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcStorageNoteText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ProcStartButton;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ProcSlotStatusTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UProgressBar>> ProcSlotBars;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ProcSlotTimeTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ProcSlotCancelButtons;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ProcConfirmBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcConfirmText;
	// --- 판매소 창 (UpdateSalesLive가 글자만 바꿈) ---
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SellListTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> SellListButtons;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SellSelectedText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SellAmountText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SellMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SellPlusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SellMaxButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SellSummaryText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SellBlockText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SellButton;
	// --- 후신소 창 ---
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeSlotText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeJobTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UProgressBar>> UpgradeJobBars;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeTitleTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeDetailTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeBlockTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> UpgradeButtons;
	// 작업 칸마다 시간 단축 (부적 수 선택 → 미리보기 → 확정 · D10~D12)
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UHorizontalBox>> UpgradeSpeedRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeSpeedCountTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeSpeedPreviewTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> UpgradeSpeedApplyButtons;
	/** 작업 칸마다 고른 부적 수 (창을 다시 그려도 유지) */
	TArray<int32> UpgradeSpeedCounts;
	// --- 밭 관리 창 ---
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FieldMgmtText;
	// --- 시설 정보 창: 작물 선택 · 공통 성장 효과 ---
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoCropText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoGrowthText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> InfoCropButtons;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoCropReasonText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> DebugPanel;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> DebugContent;

	/** 버튼 연결 객체 (가비지 컬렉션 방지) · 창을 다시 그릴 때 비움 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiAction>> FrameActions;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiAction>> WindowActions;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiAction>> IconActions;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiAction>> DebugActions;

	// --- 상태 ---
	TWeakObjectPtr<ACozyFacilityActor> IconFacility;
	TArray<TWeakObjectPtr<ACozyFacilityActor>> LabelFacilities;
	TArray<FName> StorageItemIds;
	ECozyWindowKind WindowKind = ECozyWindowKind::None;
	FGuid WindowFacility;
	FGuid WindowTargetFacility;
	FText PlaceholderLabel;
	FText LastFeedback;
	/** 가공 창에서 고른 레시피 · 제작 횟수 (레시피 실행 횟수) */
	FName ProcSelectedRecipe;
	int32 ProcSelectedRuns = 1;
	/** 취소 확인 중인 작업 */
	FGuid ProcPendingCancelJob;
	/** 판매소 창에서 고른 재료 · 수량 */
	TArray<FName> SellListItemIds;
	FName SellSelectedItem;
	int32 SellSelectedAmount = 1;
	/** 후신소 창에 보이는 시설 (목록이 바뀌면 창을 다시 그림) */
	TArray<FGuid> UpgradeFacilityIds;
	/** 시설 정보 창의 작물 버튼 순서 */
	TArray<FName> InfoCropIds;
	bool bDebugVisible = false;
	FDelegateHandle EstateChangedHandle;
	TArray<TObjectPtr<UCozyUiAction>>* ActionSink = nullptr;
};
