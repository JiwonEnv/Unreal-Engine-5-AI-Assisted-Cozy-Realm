#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Kit/CozyUiTypes.h"
#include "Data/CozyRealmDataTypes.h"
#include "CozyHudWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UImage;
class UProgressBar;
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

/** 화면을 덮는 전용 창의 종류 (창마다 프로젝트 설정 'Cozy UI' → Window Screens의 화면을 띄움 · 이름은 GetWindowName) */
enum class ECozyWindowKind : uint8
{
	None,
	/** 시설 정보 (생산 상태) */
	FacilityInfo,
	/** 나가야: 주민 목록 · 배치 */
	Nagaya,
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
	OfflineReport,
	/** 신사: 영지 레벨 · 시설 레벨 상한 · 다음 성장 조건 */
	Shrine,
	/** 설정: 사운드 · 화면 탭 · 게임 종료 (Window Screens의 Settings 화면 · 개발용 F1 디버그 창과 별개) */
	Settings,
	/** 꾸미기 상점: 장식 목록 · 구매 (보관함으로) · 꾸미기 점수·생산 보너스 (4주차 기능 2) */
	DecorShop
};

/**
 *  영지 화면 UI.
 *  편집 가능한 HUD 화면 · 시설 이름표 · 선택한 시설 위 메뉴 화면 · 화면을 덮는 창 화면 · 배치 패널·버튼 화면 · 알림 · 디버그 메뉴(F1).
 *  화면은 프로젝트 설정 'Cozy UI'(HUD Screen · Window Screens)에서 고름 · 연결이 빠지면 오류 로그와 알림으로 알려 줌.
 *  상태는 영지 서비스에서 읽고, 버튼은 서비스에 요청만 보낸다 (정리 6-7).
 */
UCLASS()
class UCozyHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** 시설을 선택해 시설 메뉴를 띄운다 */
	void ShowFacilityIcons(ACozyFacilityActor* FacilityActor);

	/** 선택 해제 (메뉴 숨김) */
	void HideFacilityIcons();

	/** 전용 창을 연다 · TargetFacility는 나가야 창에서 배치할 시설 */
	void OpenWindow(ECozyWindowKind Kind, const FGuid& FacilityId, const FGuid& TargetFacility = FGuid());

	void CloseWindow();
	bool IsWindowOpen() const { return WindowKind != ECozyWindowKind::None; }
	bool IsSettingsOpen() const { return WindowKind == ECozyWindowKind::Settings; }
	/** 설정 창 열기 · 열려 있으면 닫기 (HUD 설정 버튼) */
	void ToggleSettingsWindow();
	/** 작은 창 모드: 간단한 HUD만 (시계 · 전부 수확 · 크게 보기) · 열린 창 · 시설 메뉴 · 이름표 · 디버그 창 숨김 */
	void SetCompactMode(bool bEnable);
	bool IsCompactMode() const { return bCompactMode; }
	/** 배치 모드 패널과 미리보기 옆 버튼 (D45) */
	void SetPlacementMode(bool bEnable);

	/** 화면 위쪽에 잠깐 보이는 알림 (단축키 결과 등) */
	void ShowToast(const FText& Message);
	/** 단축키: Tab 주민 목록(나가야) · I 창고 · 이미 열려 있으면 닫음 */
	void ToggleNagayaWindow();
	void ToggleStorageWindow();
	/** 단축키 K · HUD '꾸미기' 버튼: 꾸미기 상점 창 (열려 있으면 닫음) */
	void ToggleDecorShopWindow();

	void ToggleDebugPanel();
	/** 첫 실행 안내를 처음 단계부터 (새 게임 시작 · 설정 '안내 다시 보기') */
	void StartTutorial();
	/** 안내 진행 확인 · 화살표 위치 (플레이어 컨트롤러가 매 프레임 · 위젯이 그려지지 않는 시험 실행에서도 진행되게) */
	void UpdateTutorial(float DeltaTime);
	/** UI 미리보기: HUD · 모든 창 화면 · 배치 패널 · 시설 메뉴 · 배치 버튼을 함께 가짜 값으로 (게임 상태는 바꾸지 않음) */
	void SetUiPreview(bool bEnable);
	void SetUiPreviewState(ECozyUiPreviewState State);
	/** 미리보기용으로 창 열기 (그 기능이 있는 첫 시설 · 재료를 쓰지 않음) */
	void OpenPreviewWindow(ECozyWindowKind Kind);
	/** 미리보기용 배치 패널·배치 버튼 상자 보이기 (실제 배치 모드가 아님 · 시설을 옮기지 않음) */
	void TogglePlacementPreview();

	/** 그 기능이 있는 첫 시설 (bSkipStored면 보관함에 넣은 시설은 건너뜀 · 없으면 빈 ID) */
	FGuid FindFirstFacilityWith(ECozyFacilityFunction Function, bool bSkipStored) const;

	/** 편집 가능한 창 화면이 쓰는 기존 기능 (수령 · 방금 한 일) */
	void CollectFromWindow(const FGuid& FacilityId) { HandleCollectClicked(FacilityId); }
	void SetFeedbackText(const FText& Message) { SetFeedback(Message); }
	const FText& GetLastFeedback() const { return LastFeedback; }
	/** 판매 창 선택값 (편집 가능한 판매 화면이 읽고 바꿈) */
	FName GetSellItem() const { return SellSelectedItem; }
	int32 GetSellAmount() const { return SellSelectedAmount; }
	void SetSellSelection(FName ItemId, int32 Amount) { SellSelectedItem = ItemId; SellSelectedAmount = FMath::Max(1, Amount); RefreshWindow(); }
	/** 가공 창 선택값 (편집 가능한 가공 화면이 읽고 바꿈) */
	FName GetProcRecipe() const { return ProcSelectedRecipe; }
	int32 GetProcRuns() const { return ProcSelectedRuns; }
	const FGuid& GetProcPendingCancel() const { return ProcPendingCancelJob; }
	void SetProcSelection(FName RecipeId, int32 Runs) { ProcSelectedRecipe = RecipeId; ProcSelectedRuns = FMath::Max(1, Runs); RefreshWindow(); }
	void SetProcPendingCancel(const FGuid& JobId) { ProcPendingCancelJob = JobId; RefreshWindow(); }
	/** 후신소 창: 작업 칸마다 고른 시간 부적 장수 (편집 가능한 업그레이드 화면이 읽고 바꿈) */
	int32 GetUpgradeSpeedCount(int32 SlotIndex) const { return UpgradeSpeedCounts.IsValidIndex(SlotIndex) ? FMath::Max(1, UpgradeSpeedCounts[SlotIndex]) : 1; }
	/** 시설 메뉴 버튼 목록 (기능 조합에서 만듦 · 줄 ID와 글자) · 편집 가능한 메뉴 화면이 씀 */
	static TArray<TPair<FName, FText>> GetFacilityMenu(const struct FCozyFacilityRow& Def);
	/** 시설 메뉴 버튼 하나 실행 (줄 ID: GetFacilityMenu 참고) */
	void OpenFacilityFunction(FName Function, const FGuid& FacilityId);
	/** 후신소 업그레이드 창을 열고 그 시설의 상세를 바로 보여 줌 (예: 신사 창의 '후신소에서 업그레이드') */
	void OpenUpgradeFor(const FGuid& TargetFacility);
	/** 창 화면을 최신 상태로 다시 그림 (게임 상태가 바뀌지 않은 버튼 결과 안내 등) */
	void RefreshWindowScreen() { RefreshWindow(); }
	void SetUpgradeSpeedCount(int32 SlotIndex, int32 Count)
	{
		if (SlotIndex < 0)
		{
			return;
		}
		while (UpgradeSpeedCounts.Num() <= SlotIndex)
		{
			UpgradeSpeedCounts.Add(1);
		}
		UpgradeSpeedCounts[SlotIndex] = FMath::Max(1, Count);
		RefreshWindow();
	}

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	void BuildLayout();
	void HandleEstateChanged(bool bStructural);
	/** 미수령분 수령 버튼 공통 처리 (생산·가공) · 결과는 '방금 한 일'로 */
	void HandleCollectClicked(const FGuid& FacilityId);
	void SetFeedback(const FText& Message);

	/** 시설마다 이름표를 만든다 (시설 액터가 바뀌면 다시) */
	void RefreshNameLabels();
	void UpdateNameLabelPositions();
	/** 선택한 시설 위에 시설 메뉴 화면(FacilityMenu)을 붙임 */
	void RefreshFacilityMenu();
	void RefreshWindow();
	/** 가공 화면용: 최대가 줄면 선택 횟수를 낮추고, 끝난 작업의 취소 확인은 닫음 */
	void ClampProcessingSelection();
	void RefreshDebugPanel();

	/** 보여 주지 않은 방치 보상이 있으면 창을 연다 */
	void ShowPendingOfflineReport();

	UTextBlock* MakeText(const FText& Text, int32 FontSize = 16, const FLinearColor& Color = FLinearColor::White);
	UButton* MakeButton(const FText& Label, TFunction<void()> OnClick, bool bEnabled = true, int32 FontSize = 15);

	UCozyEstateSubsystem* GetEstate() const;
	/** 화면 크기 (화면 단위 = 픽셀 / DPI 배율) */
	FVector2D GetViewportSlateSize() const;

	/**
	 *  화면 연결이 빠졌을 때 오류 로그 + 알림 (예전 C++ 임시 창은 없앰).
	 *  SettingName: 고칠 프로젝트 설정 항목 · bOnce면 같은 화면은 한 번만 (매 프레임·상태 변경마다 부르는 곳)
	 */
	void WarnMissingScreen(FName ScreenName, const TCHAR* SettingName, bool bOnce);

	// --- 위젯 ---
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> LabelLayer;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> NameLabels;
	/** 시설마다 이름표 위 수령 배지 (미수령분이 있을 때만 · 누르면 그 시설만 수령) · 이름표와 같은 순서 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CollectBadges;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> BadgeIcons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BadgeCounts;
	/** 배지 클릭 동작 객체 (여기서 붙잡아 두지 않으면 메모리 정리로 사라져 클릭이 안 됨) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiAction>> BadgeActions;
	/** 배지 아이콘이 지금 보여 주는 품목 (바뀔 때만 그림을 다시 찾음) */
	TArray<FName> BadgeItems;
	/** 시설마다 진행 막대 아래 상태 칩 (주민 없음·일시 정지 · 가득 참·창고 부족 · 업그레이드 중) · 색은 창 안 상태색과 같음 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> StateChips;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StateTexts;
	/** 시설마다 이름표 아래 진행 막대 (생산·가공 진행 중일 때만) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> ProgressBars;
	/** 수령 배지 하나를 만든다 (테마 둥근 버튼 · 아이콘 + 개수) */
	UButton* MakeCollectBadge(const FGuid& FacilityId, UImage*& OutIcon, UTextBlock*& OutCount);
	/** 배지·진행 막대의 표시와 값 (매 프레임 · 이름표 위치와 함께) */
	void UpdateFacilityBadges(int32 Index, const FGuid& FacilityId, const FVector2D& ScreenPosition);
	/** 시설 메뉴 화면을 담는 상자 (선택한 시설 위를 따라다님) */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> IconBox;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> IconSlot;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowOverlay;
	// --- 배치 모드 ---
	void RefreshPlacementPanel();
	void UpdatePlacementLive();
	// --- 첫 실행 안내 (Tutorial.csv · 새 게임에서 한 번 · 설정 '안내 다시 보기') ---
	/** 안내 칸·화살표를 만든다 (처음 시작할 때 한 번) */
	void BuildTutorialWidgets();
	/** Index 단계를 보여 주고 그 단계의 기준값(창고 밀·밀가루·골드)을 기록 */
	void ShowTutorialStep(int32 Index);
	/** 안내를 끝냄 (bCompleted면 마지막 단계까지 · 아니면 건너뜀) · 다시 띄우지 않게 저장 */
	void FinishTutorial(bool bCompleted);
	bool IsTutorialGoalDone(FName Goal) const;
	int32 GetStoredAmount(FName ItemId) const;
	struct FTutorialStep { FName Goal; FName Target; FText Title; FText Body; };
	TArray<FTutorialStep> TutorialSteps;
	int32 TutorialIndex = INDEX_NONE;
	float TutorialTime = 0.f;
	/** 단계 시작 때 기준값 (수확 · 가공 · 판매 확인용) */
	int32 TutorialWheatStart = 0;
	int32 TutorialFlourStart = 0;
	int32 TutorialGoldStart = 0;
	bool bTutorialChecked = false;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TutorialPanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TutorialTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TutorialBody;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TutorialArrow;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiAction>> TutorialActions;

	/** 배치 버튼 화면(PlacementActions)을 버튼 상자에 붙이고 갱신 · 화면이 없으면 false */
	bool ShowPlacementActionsScreen();
	bool bPlacementMode = false;
	/** 배치 패널 화면(Placement)을 담는 틀 (왼쪽 위) */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PlacementPanel;
	/** 배치 버튼 화면(PlacementActions)을 담는 상자 (옮기는 시설 위) */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PlacementActionBox;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> PlacementActionSlot;
	bool bUiPreview = false;
	ECozyUiPreviewState UiPreviewState = ECozyUiPreviewState::Progress;
	bool bPlacementPreview = false;
	void ApplyPreview(class UCozyUiScreen* Screen) const;
	/** HUD 화면과 지금까지 만든 모든 창 화면에 미리보기 상태를 적용 */
	void ApplyPreviewAll() const;

	UPROPERTY(Transient)
	TObjectPtr<class UCozyUiScreen> HudScreen;

	// --- 편집 가능한 창 화면 (프로젝트 설정 'Cozy UI' → Window Screens) ---
	/** 창 이름 (Storage · FacilityInfo …) */
	static FName GetWindowName(ECozyWindowKind Kind);
	/** 그 창에 지정된 화면 (없으면 nullptr) */
	class UCozyUiScreen* GetWindowScreen(ECozyWindowKind Kind);
	/** 이름으로 화면 찾기 (Window Screens에 지정된 화면 · 한 번 만들면 재사용 · 예: 배치 패널 'Placement') */
	class UCozyUiScreen* GetScreenByName(FName Name);
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<class UCozyUiScreen>> WindowScreenCache;
	/** 연결이 빠졌다고 이미 알린 화면 (WarnMissingScreen bOnce) */
	TSet<FName> WarnedMissingScreens;
	/** 창 화면이 화면보다 길면 스크롤 (높이 상한은 RefreshWindow 때 화면 크기에 맞춤 · HUD 아래부터) */
	UPROPERTY(Transient)
	TObjectPtr<class USizeBox> WindowScreenSize;
	UPROPERTY(Transient)
	TObjectPtr<class UScrollBox> WindowScreenScroll;
	bool bCompactMode = false;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToastText;
	float ToastRemaining = 0.f;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> DebugPanel;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> DebugContent;
	/** 디버그 창 높이 상한 (화면 높이에 맞춤 · 넘치면 스크롤) */
	UPROPERTY(Transient)
	TObjectPtr<class USizeBox> DebugSize;

	/** 버튼 연결 객체 (가비지 컬렉션 방지) · 디버그 창을 다시 그릴 때 비움 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiAction>> DebugActions;

	// --- 상태 ---
	TWeakObjectPtr<ACozyFacilityActor> IconFacility;
	TArray<TWeakObjectPtr<ACozyFacilityActor>> LabelFacilities;
	ECozyWindowKind WindowKind = ECozyWindowKind::None;
	FGuid WindowFacility;
	FGuid WindowTargetFacility;
	FText LastFeedback;
	/** 가공 창에서 고른 레시피 · 제작 횟수 (레시피 실행 횟수) */
	FName ProcSelectedRecipe;
	int32 ProcSelectedRuns = 1;
	/** 취소 확인 중인 작업 */
	FGuid ProcPendingCancelJob;
	/** 판매소 창에서 고른 재료 · 수량 */
	FName SellSelectedItem;
	int32 SellSelectedAmount = 1;
	/** 후신소 창: 작업 칸마다 고른 부적 수 (창을 다시 그려도 유지) */
	TArray<int32> UpgradeSpeedCounts;
	bool bDebugVisible = false;
	FDelegateHandle EstateChangedHandle;
	/** MakeButton이 만든 연결 객체를 넣을 곳 (디버그 창을 그리는 동안 DebugActions) */
	TArray<TObjectPtr<UCozyUiAction>>* ActionSink = nullptr;
};
