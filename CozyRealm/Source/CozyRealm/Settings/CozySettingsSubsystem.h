#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "GenericPlatform/GenericWindow.h"
#include "UI/Kit/CozyUiTypes.h"
#include "CozySettingsSubsystem.generated.h"

class APlayerController;
class UWorld;

/**
 *  설정 창의 상태와 동작 (사운드 · 화면 · 게임 종료).
 *  - 값 저장은 UCozyGameUserSettings (GameUserSettings.ini) · 영지 진행 저장과 섞지 않는다
 *  - 설정 창(WBP_Settings · DA_UiScreen_Settings)은 UI 키트의 값(Settings*) · 목록 · 동작으로 이 클래스를 부른다
 *  - 창을 열고 닫기만 해서는 아무것도 저장하지 않는다 (적용 · 유지 · 종료를 눌렀을 때만)
 */
UCLASS()
class UCozySettingsSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	static UCozySettingsSubsystem* Get(const UObject* WorldContext);

	/** 화면 모드·해상도를 바꾼 뒤 유지할지 묻는 시간 (초) · 지나면 이전 설정으로 */
	static constexpr float ConfirmSeconds = 15.f;

	// 창 열기·닫기
	void OpenSettings();
	/** 닫기 (적용하지 않은 변경은 버림 · 화면 변경 확인 중이면 이전 설정으로) · 버린 것이 있으면 true */
	bool CloseSettings();
	/** 확인 창(화면 유지 · 게임 종료 · 저장 실패)이 떠 있는지 */
	bool HasDialog() const { return bConfirmActive || QuitState != EQuit::None; }
	/** Esc: 떠 있는 확인 창을 먼저 취소 (화면 변경 확인은 '되돌리기') · 취소한 것이 있으면 true */
	bool CancelDialog();

	// UI 키트 연결
	FCozyUiValueResult GetUiValue(ECozyUiValue Value, FName Param) const;
	bool GetListRows(ECozyUiListSource Source, TArray<struct FCozyUiListRow>& OutRows) const;
	bool CanRun(ECozyUiAction Action, FName Param, FText& OutReason) const;
	/** 설정 동작이면 실행하고 true (Ratio는 슬라이더 값 · 0~1) */
	bool Run(ECozyUiAction Action, FName Param, APlayerController* Controller, float Ratio = -1.f);
	static bool IsSettingsAction(ECozyUiAction Action);
	static bool IsSettingsValue(ECozyUiValue Value);
	static bool IsSettingsList(ECozyUiListSource Source);
	/** 화면을 다시 그려야 할 때마다 1씩 오름 (시간 초과 복원 등 버튼 밖에서 바뀐 경우 포함) */
	int32 GetRevision() const { return Revision; }

	/**
	 *  창 모드인데 해상도가 모니터만 하면 '최대화된 창'으로 (제목 줄·테두리·작업 표시줄이 보이게 · 그림은 남은 공간에 맞춤).
	 *  모니터보다 작으면 그 크기의 일반 창. 게임 시작 · 레벨 시작 · 화면 설정 적용 뒤에 부른다 (실행 파일에서만).
	 */
	void ApplyWindowState(const UObject* WorldContext);

	/** 저장된 음량을 지금 월드에 적용 (게임 시작 · 레벨 시작 때) */
	void ApplySavedAudio(const UObject* WorldContext);

	/** 작은 창 크기 (게임 화면 · 화면 구석에 붙음) */
	static constexpr int32 CompactWidth = 640;
	static constexpr int32 CompactHeight = 360;
	/** 작은 창 모드 (화면 설정 · GameUserSettings는 바꾸지 않고 창만 바꿨다가 되돌림) */
	bool IsCompact() const { return bCompact; }
	/** 작은 창 ↔ 원래 크기 · 바꿨으면 true (할 수 없으면 OutReason) */
	bool ToggleCompact(APlayerController* Controller, FText& OutReason);

	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCozySettingsSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bConfirmActive || CompactMoveFrames > 0 || MaximizeFrames > 0 || WindowStateFrames > 0; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional; }

private:

	enum class ETab : uint8 { Sound, Display };
	enum class EQuit : uint8 { None, Asking, SaveFailed };

	struct FDisplayChoice
	{
		EWindowMode::Type Mode = EWindowMode::Windowed;
		FIntPoint Resolution = FIntPoint(1280, 720);
		bool bVSync = false;
		int32 FrameLimit = 0;
		bool bCompactTop = false;
		bool operator==(const FDisplayChoice& O) const { return Mode == O.Mode && Resolution == O.Resolution && bVSync == O.bVSync && FrameLimit == O.FrameLimit && bCompactTop == O.bCompactTop; }
	};

	TSharedPtr<class SWindow> GetGameWindow(const UWorld* World) const;
	/** 창 모드 + 모니터 크기(세로는 작업 표시줄·제목 줄 높이만큼 여유)인지 */
	static bool IsFullSizeWindow(EWindowMode::Type Mode, const FIntPoint& Resolution, const FIntPoint& Desktop);
	/** 해상도를 바꾸기 전: 최대화돼 있으면 원래 크기로 풀어 둠 (최대화된 채로는 창 크기가 바뀌지 않음) */
	void RestoreFromMaximize(const UWorld* World);
	/** 최대화하면 엔진이 줄어든 창 크기를 해상도 설정에 써 넣음 → 몇 프레임 뒤 고른 값으로 되돌림 */
	int32 MaximizeFrames = 0;
	/** 레벨 시작 직후에는 창이 아직 자리를 잡기 전일 수 있어 몇 프레임 뒤에 다시 적용 */
	int32 WindowStateFrames = 0;
	void ApplyWindowStateNow(UWorld* World);
	FIntPoint MaximizeResolution = FIntPoint::ZeroValue;
	TWeakObjectPtr<UWorld> MaximizeWorld;
	void PlaceCompactWindow();
	void ApplyCompactTopmost(bool bTop);
	void RestoreSettingsValues();
	void SaveRestoredSettings();

	FDisplayChoice ReadAppliedDisplay() const;
	void LoadPendingFromSaved();
	bool HasPendingSound() const;
	bool HasPendingDisplay() const;
	TArray<FIntPoint> GetResolutionOptions(EWindowMode::Type Mode) const;
	void StepResolution(int32 Direction);
	void ApplySound();
	void ApplyDisplay();
	void FinishConfirm(bool bKeep, bool bTimedOut);
	void RevertTab();
	void ResetTab();
	void ApplyAudioToWorld(const UObject* WorldContext, float Master, float Music, float Sfx);
	void Touch(const FText& Message = FText::GetEmpty());

	static FText ModeName(EWindowMode::Type Mode);
	static FText FrameLimitName(int32 Limit);
	static FText ResolutionName(const FIntPoint& Resolution);

	ETab Tab = ETab::Sound;
	float PendingMaster = 1.f;
	float PendingMusic = 1.f;
	float PendingSfx = 1.f;
	FDisplayChoice PendingDisplay;

	bool bConfirmActive = false;
	float ConfirmRemaining = 0.f;
	FDisplayChoice ConfirmBefore;

	EQuit QuitState = EQuit::None;
	FText Feedback;
	int32 Revision = 0;

	/** 음량 믹스를 올린 월드 (같은 월드에 두 번 올리지 않게) */
	TWeakObjectPtr<UWorld> MixWorld;
	/** 화면 변경 확인 · 시간 초과 복원 · 종료에 쓸 월드 (설정 창을 연 월드) */
	TWeakObjectPtr<UWorld> SettingsWorld;

	// 작은 창 모드: 들어가기 전 상태 (나올 때 그대로 되돌림 · 엔진이 창 크기를 해상도 설정에 써 넣어도 원래 값으로)
	bool bCompact = false;
	TWeakPtr<class SWindow> CompactWindow;
	EWindowMode::Type SavedMode = EWindowMode::Windowed;
	FIntPoint SavedResolution = FIntPoint::ZeroValue;
	FVector2D SavedClientSize = FVector2D::ZeroVector;
	FVector2D SavedPosition = FVector2D::ZeroVector;
	/** 창 크기가 바뀐 뒤 위치를 맞출 남은 프레임 (해상도 변경은 한두 프레임 늦게 적용됨) */
	int32 CompactMoveFrames = 0;
	bool bMoveToCorner = false;
};
