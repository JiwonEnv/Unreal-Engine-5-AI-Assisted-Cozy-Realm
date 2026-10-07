#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Kit/CozyUiTypes.h"
#include "CozyUiScreen.generated.h"

class UCozyUiTheme;
class UCozyUiScreenConfig;
struct FCozyUiButtonEntry;

/**
 *  편집 가능한 화면의 부모 (WBP_화면이름의 부모 클래스).
 *  - 테마·화면 설정을 안의 모든 Cozy 부품에 적용 (설정 에셋을 고치면 바로 다시 적용)
 *  - 글자·게이지가 고른 게임 값을 0.25초마다 갱신
 *  - 버튼 동작을 이미 있는 게임 기능으로 연결
 *  - 미리보기: 실제 값 대신 가짜 상태를 보여 주고, 버튼은 알림만 (재료를 쓰지 않음)
 *  게임 규칙은 이 클래스에 없다. 값을 읽고 기존 기능을 부르기만 한다.
 */
UCLASS(Abstract)
class UCozyUiScreen : public UUserWidget
{
	GENERATED_BODY()

public:

	/** 이 화면의 설정 (버튼 목록 · 테마 덮어쓰기) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy UI")
	TObjectPtr<UCozyUiScreenConfig> Config;

	/** 미리보기 (실제 값 대신 가짜 상태 · 버튼은 동작하지 않고 알림만) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy UI|미리보기")
	bool bPreview = false;

	/** 미리보기 상태 (디자이너에서도 이 상태로 보임) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy UI|미리보기")
	ECozyUiPreviewState PreviewState = ECozyUiPreviewState::Progress;

	UFUNCTION(BlueprintCallable, Category = "Cozy UI")
	void SetPreview(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Cozy UI")
	void SetPreviewState(ECozyUiPreviewState NewState);

	/** 테마·설정을 모든 부품에 다시 적용 */
	UFUNCTION(BlueprintCallable, Category = "Cozy UI")
	void RefreshTheme();

	/** 연결된 값을 지금 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Cozy UI")
	void RefreshValues();

	// --- 부품이 쓰는 조회 ---
	UCozyUiTheme* GetTheme() const;
	FLinearColor GetColor(ECozyUiColor Role) const;
	FSlateFontInfo GetFont(ECozyUiTextRole Role) const;
	const FCozyUiButtonEntry* FindButtonEntry(FName ButtonId) const;
	TArray<FCozyUiButtonEntry> GetButtonsForArea(FName AreaId) const;
	FCozyUiValueResult GetValue(ECozyUiValue Value, FName Param) const;

	/** 버튼 클릭 → 기존 기능 (미리보기면 알림만) */
	void RunAction(const FCozyUiButtonEntry& Entry);

	static FText GetActionName(ECozyUiAction Action);
	static FText GetPreviewStateName(ECozyUiPreviewState State);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void ForEachElement(TFunctionRef<void(class ICozyUiElement&)> Visit) const;
	void BindAssetEvents();
	void UnbindAssetEvents();
	void HandleAssetChanged(const UObject* Asset);
	FCozyUiValueResult GetPreviewValue(ECozyUiValue Value) const;
	void ShowMessage(const FText& Message) const;

	TWeakObjectPtr<UCozyUiTheme> BoundTheme;
	TWeakObjectPtr<UCozyUiScreenConfig> BoundConfig;
	FDelegateHandle ThemeHandle;
	FDelegateHandle ConfigHandle;
	float ValueTimer = 0.f;
};
