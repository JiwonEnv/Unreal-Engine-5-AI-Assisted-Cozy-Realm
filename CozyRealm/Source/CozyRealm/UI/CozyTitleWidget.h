#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Estate/CozyEstateSubsystem.h"
#include "UI/Kit/CozyUiTypes.h"
#include "CozyTitleWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class USizeBox;
class UScrollBox;
class UCozyUiScreen;
class UCozyUiTheme;
class UTexture2D;

/** 타이틀 버튼 하나 (줄 · 표시 · 버튼 · 글자) */
USTRUCT()
struct FCozyTitleButton
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UHorizontalBox> Row;
	UPROPERTY() TObjectPtr<UImage> Marker;
	UPROPERTY() TObjectPtr<USizeBox> Size;
	UPROPERTY() TObjectPtr<UButton> Button;
	UPROPERTY() TObjectPtr<UTextBlock> Label;
	FName Id;
	bool bPrimary = false;
};

/**
 *  타이틀 화면 (로고 · 배경 · 세로 버튼: 이어하기 / 새 게임 / 설정 / 게임 종료 · 구석에 버전).
 *  WBP 없이 코드로 만든다 · 모양은 기본 UI 테마(글꼴 주아 · 버튼 · 패널 · 창 이미지)와 프로젝트 설정 'Cozy Title'을 따른다.
 *  조작: 마우스 · 위/아래(W/S) · Enter/Space · Esc. 확인 창에서는 왼쪽/오른쪽(A/D).
 */
UCLASS()
class UCozyTitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	/** 키보드 입력을 이 화면이 받게 함 */
	void FocusRoot();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EModal : uint8 { None, NewGameConfirm, QuitConfirm, LoadError, Loading, EnterFailed };

	void BuildTree();
	void RefreshSaveState();
	void RebuildButtons();
	void SetSelected(int32 Index);
	void UpdateButtonVisuals();
	void Activate(FName Id);
	void ActivateSelected();

	void StartContinue();
	void StartNewGame();
	void EnterEstate(const FString& Reason);
	void OpenSettings();
	void CloseSettings();
	void HandleSettingsClose(UCozyUiScreen* Screen);

	void ShowModal(EModal Kind, const FText& Title, const FText& Body, const TArray<TPair<FName, FText>>& Buttons, int32 DefaultIndex);
	void HideModal();
	void ActivateModal(FName Id);
	void UpdateModalVisuals();

	UFUNCTION() void OnButtonClicked0();
	UFUNCTION() void OnButtonClicked1();
	UFUNCTION() void OnButtonClicked2();
	UFUNCTION() void OnButtonClicked3();
	UFUNCTION() void OnButtonHovered0();
	UFUNCTION() void OnButtonHovered1();
	UFUNCTION() void OnButtonHovered2();
	UFUNCTION() void OnButtonHovered3();
	UFUNCTION() void OnModalClicked0();
	UFUNCTION() void OnModalClicked1();
	UFUNCTION() void OnModalHovered0();
	UFUNCTION() void OnModalHovered1();
	void ClickMain(int32 Index);
	void ClickModal(int32 Index);

	UButton* MakeButton(FName Style, UTextBlock*& OutLabel, const FText& Text, float FontSize);
	FSlateFontInfo Font(float Size) const;
	FLinearColor ThemeColor(ECozyUiColor Role, const FLinearColor& Fallback) const;

	UPROPERTY(Transient) TObjectPtr<UCozyUiTheme> Theme;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> AccentTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> GradientTexture;

	UPROPERTY(Transient) TObjectPtr<UImage> PaperCover;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ButtonList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VersionText;
	UPROPERTY(Transient) TArray<FCozyTitleButton> Buttons;

	UPROPERTY(Transient) TObjectPtr<UBorder> ModalLayer;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ModalTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ModalBody;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> ModalButtonRow;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ModalButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ModalLabels;

	UPROPERTY(Transient) TObjectPtr<UBorder> SettingsLayer;
	UPROPERTY(Transient) TObjectPtr<USizeBox> SettingsSize;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> SettingsScroll;
	UPROPERTY(Transient) TObjectPtr<UCozyUiScreen> SettingsScreen;

	TArray<FName> ModalIds;
	EModal Modal = EModal::None;
	int32 ModalSelected = 0;
	int32 Selected = 0;
	ECozySaveProbe SaveState = ECozySaveProbe::None;
	FDateTime SavedUtc;
	FString SaveDetail;
	bool bEntering = false;
	double EnterStartTime = 0.0;
	float CoverAlpha = 1.f;
	FText LoadingBase;
};
