#include "Settings/CozySettingsSubsystem.h"
#include "Settings/CozyGameUserSettings.h"
#include "Estate/CozyEstateSubsystem.h"
#include "CozyRealm.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "UI/CozyHudWidget.h"
#include "Engine/GameViewportClient.h"
#include "UnrealEngine.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#if WITH_EDITOR
#include "Framework/Docking/TabManager.h"
#endif
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "CozySettings"

namespace CozySettings
{
	/** 창 모드 해상도 선택지 (모니터보다 작은 것만 보임) */
	const FIntPoint WindowedChoices[] = { { 1280, 720 }, { 1366, 768 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } };
	/** 화면 탭 기본값 */
	constexpr EWindowMode::Type DefaultMode = EWindowMode::WindowedFullscreen;
	constexpr bool bDefaultVSync = true;
	constexpr int32 DefaultFrameLimit = 60;

	EWindowMode::Type ParseMode(FName Param)
	{
		if (Param == TEXT("Fullscreen")) { return EWindowMode::Fullscreen; }
		if (Param == TEXT("Borderless")) { return EWindowMode::WindowedFullscreen; }
		return EWindowMode::Windowed;
	}

	int32 Percent(float Ratio) { return FMath::RoundToInt(FMath::Clamp(Ratio, 0.f, 1.f) * 100.f); }

	/** 영지 화면의 HUD (타이틀 화면 등 영지 밖이면 nullptr) */
	UCozyHudWidget* FindHud(const APlayerController* Controller)
	{
		const ACozyRealmEstatePlayerController* Cozy = Cast<ACozyRealmEstatePlayerController>(Controller);
		return Cozy ? Cozy->GetHud() : nullptr;
	}
}

UCozySettingsSubsystem* UCozySettingsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCozySettingsSubsystem>() : nullptr;
}

bool UCozySettingsSubsystem::IsSettingsAction(ECozyUiAction Action)
{
	return Action >= ECozyUiAction::OpenSettings && Action <= ECozyUiAction::ToggleCompactTopNow;
}

bool UCozySettingsSubsystem::IsSettingsValue(ECozyUiValue Value)
{
	return Value >= ECozyUiValue::SettingsTab && Value <= ECozyUiValue::SettingsCompactTop;
}

bool UCozySettingsSubsystem::IsSettingsList(ECozyUiListSource Source)
{
	return Source >= ECozyUiListSource::SettingsSoundPage && Source <= ECozyUiListSource::SettingsQuitFailed;
}

// ---------------------------------------------------------------------------
// 창 열기·닫기

void UCozySettingsSubsystem::OpenSettings()
{
	Tab = ETab::Sound;
	QuitState = EQuit::None;
	LoadPendingFromSaved();
	Touch();
}

bool UCozySettingsSubsystem::CloseSettings()
{
	bool bDiscarded = false;
	if (bConfirmActive)
	{
		FinishConfirm(false, false);
		bDiscarded = true;
	}
	if (HasPendingSound())
	{
		// 미리 듣던 음량을 저장된 값으로 되돌림
		ApplySavedAudio(SettingsWorld.Get());
		bDiscarded = true;
	}
	bDiscarded |= HasPendingDisplay();
	QuitState = EQuit::None;
	LoadPendingFromSaved();
	Feedback = FText::GetEmpty();
	Touch();
	return bDiscarded;
}

bool UCozySettingsSubsystem::CancelDialog()
{
	if (bConfirmActive)
	{
		FinishConfirm(false, false);
		return true;
	}
	if (QuitState != EQuit::None)
	{
		QuitState = EQuit::None;
		Touch(LOCTEXT("QuitCanceled", "게임 종료를 취소했습니다"));
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// 값

UCozySettingsSubsystem::FDisplayChoice UCozySettingsSubsystem::ReadAppliedDisplay() const
{
	FDisplayChoice Choice;
	if (const UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get())
	{
		Choice.Mode = Settings->GetFullscreenMode();
		Choice.Resolution = Settings->GetScreenResolution();
		Choice.bVSync = Settings->IsVSyncEnabled();
		Choice.FrameLimit = FMath::RoundToInt(Settings->GetFrameRateLimit());
		Choice.bCompactTop = Settings->bCompactAlwaysOnTop;
		// 작은 창 모드 중에는 엔진이 창 크기를 해상도 설정에 써 넣으므로, 들어가기 전 값을 보여 줌
		if (bCompact)
		{
			Choice.Mode = SavedMode;
			Choice.Resolution = SavedResolution;
		}
	}
	return Choice;
}

void UCozySettingsSubsystem::LoadPendingFromSaved()
{
	LoadPendingSound();
	PendingDisplay = ReadAppliedDisplay();
}

void UCozySettingsSubsystem::LoadPendingSound()
{
	if (const UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get())
	{
		PendingMaster = Settings->MasterVolume;
		PendingMusic = Settings->MusicVolume;
		PendingSfx = Settings->SfxVolume;
	}
}

bool UCozySettingsSubsystem::HasPendingSound() const
{
	const UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
	return Settings && (CozySettings::Percent(PendingMaster) != CozySettings::Percent(Settings->MasterVolume)
		|| CozySettings::Percent(PendingMusic) != CozySettings::Percent(Settings->MusicVolume)
		|| CozySettings::Percent(PendingSfx) != CozySettings::Percent(Settings->SfxVolume));
}

bool UCozySettingsSubsystem::HasPendingDisplay() const
{
	return !(PendingDisplay == ReadAppliedDisplay());
}

TArray<FIntPoint> UCozySettingsSubsystem::GetResolutionOptions(EWindowMode::Type Mode) const
{
	TArray<FIntPoint> Options;
	if (Mode == EWindowMode::Fullscreen)
	{
		// 모니터가 지원하는 해상도 (1280×720 이상 · 같은 값은 한 번만)
		TArray<FIntPoint> Supported;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
		for (const FIntPoint& Each : Supported)
		{
			if (Each.X >= 1280 && Each.Y >= 720)
			{
				Options.AddUnique(Each);
			}
		}
	}
	else if (Mode == EWindowMode::Windowed)
	{
		// 창 모드: 흔한 16:9 크기 중 모니터보다 작은 것 (엔진의 추천 목록은 화면 배율이 곱해진 2133×1333 같은 값이 섞여 씀)
		const UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
		const FIntPoint Desktop = Settings ? Settings->GetDesktopResolution() : FIntPoint(1920, 1080);
		for (const FIntPoint& Each : CozySettings::WindowedChoices)
		{
			if (Each.X < Desktop.X && Each.Y < Desktop.Y)
			{
				Options.Add(Each);
			}
		}
		// 맨 끝: 모니터 크기 = 최대화된 창 (제목 줄·작업 표시줄이 보이고 그림은 남은 공간에 맞춤)
		Options.AddUnique(Desktop);
	}
	// 지금 값이 목록에 없으면 넣어 둠 (창 크기를 손으로 바꾼 경우 등)
	const FIntPoint Current = ReadAppliedDisplay().Resolution;
	if (Mode != EWindowMode::WindowedFullscreen && Current.X > 0 && !Options.Contains(Current))
	{
		Options.Add(Current);
	}
	Options.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
	return Options;
}

FText UCozySettingsSubsystem::ModeName(EWindowMode::Type Mode)
{
	switch (Mode)
	{
	case EWindowMode::Fullscreen: return LOCTEXT("ModeFull", "전체 화면");
	case EWindowMode::WindowedFullscreen: return LOCTEXT("ModeBorderless", "테두리 없는 창");
	default: return LOCTEXT("ModeWindowed", "창 모드");
	}
}

FText UCozySettingsSubsystem::FrameLimitName(int32 Limit)
{
	return Limit <= 0 ? LOCTEXT("FpsNone", "제한 없음") : FText::Format(LOCTEXT("Fps", "{0}"), FText::AsNumber(Limit));
}

FText UCozySettingsSubsystem::ResolutionName(const FIntPoint& Resolution)
{
	return FText::FromString(FString::Printf(TEXT("%d × %d"), Resolution.X, Resolution.Y));
}

FCozyUiValueResult UCozySettingsSubsystem::GetUiValue(ECozyUiValue Value, FName Param) const
{
	FCozyUiValueResult R;
	R.bValid = true;
	R.StateColor = ECozyUiColor::Ink;
	const UCozyGameUserSettings* Saved = UCozyGameUserSettings::Get();
	const FDisplayChoice Applied = ReadAppliedDisplay();
	const bool bBorderless = PendingDisplay.Mode == EWindowMode::WindowedFullscreen;
	const FIntPoint DesktopResolution = Saved ? Saved->GetDesktopResolution() : PendingDisplay.Resolution;
	// 테두리 없는 창은 모니터 해상도를 그대로 쓰므로 그 값을 보여 줌
	auto ShownResolution = [&DesktopResolution](const FDisplayChoice& Choice)
	{
		return ResolutionName(Choice.Mode == EWindowMode::WindowedFullscreen ? DesktopResolution : Choice.Resolution);
	};
	switch (Value)
	{
	case ECozyUiValue::SettingsTab:
	{
		const bool bSound = Param == TEXT("Sound");
		R.Text = bSound ? LOCTEXT("TabSound", "사운드") : LOCTEXT("TabDisplay", "화면");
		R.StateColor = (bSound == (Tab == ETab::Sound)) ? ECozyUiColor::Point : ECozyUiColor::InkMuted;
		break;
	}
	case ECozyUiValue::SettingsVolume:
	{
		const float Ratio = Param == TEXT("Music") ? PendingMusic : (Param == TEXT("Sfx") ? PendingSfx : PendingMaster);
		R.Current = CozySettings::Percent(Ratio);
		R.Max = 100.f;
		break;
	}
	case ECozyUiValue::SettingsAudioNote:
	{
		const UCozyAudioSettings* Audio = UCozyAudioSettings::Get();
		const bool bLinked = Audio && !Audio->VolumeMix.IsNull() && !Audio->MasterClass.IsNull() && !Audio->MusicClass.IsNull() && !Audio->SfxClass.IsNull();
		R.Text = bLinked
			? LOCTEXT("AudioNote", "아직 게임에 배경음·효과음이 없습니다. 음량은 저장해 두었다가, 소리를 넣으면(배경음 → SC_Music · 효과음 → SC_SFX) 바로 이 값을 따릅니다.")
			: LOCTEXT("AudioNoteMissing", "소리 연결(프로젝트 설정 → Cozy Audio)이 비어 있어 음량을 적용할 수 없습니다.");
		R.StateColor = bLinked ? ECozyUiColor::InkMuted : ECozyUiColor::Warning;
		break;
	}
	case ECozyUiValue::SettingsDisplayCurrent:
		R.Text = FText::Format(LOCTEXT("DisplayCurrent", "지금 적용: {0} · {1} · 수직 동기화 {2} · 최대 프레임 {3}"),
			ModeName(Applied.Mode), ShownResolution(Applied),
			Applied.bVSync ? LOCTEXT("On", "켬") : LOCTEXT("Off", "끔"), FrameLimitName(Applied.FrameLimit));
		R.StateColor = ECozyUiColor::InkMuted;
		break;
	case ECozyUiValue::SettingsWindowMode:
	{
		const EWindowMode::Type Mode = CozySettings::ParseMode(Param);
		R.Text = ModeName(Mode);
		R.StateColor = PendingDisplay.Mode == Mode ? ECozyUiColor::Point : ECozyUiColor::Ink;
		break;
	}
	case ECozyUiValue::SettingsResolution:
		R.Text = ShownResolution(PendingDisplay);
		R.StateColor = bBorderless ? ECozyUiColor::Locked : ECozyUiColor::Ink;
		break;
	case ECozyUiValue::SettingsResolutionNote:
		R.Text = bBorderless
			? FText::Format(LOCTEXT("ResLocked", "테두리 없는 창은 모니터 해상도({0})를 그대로 씁니다. 해상도를 고르려면 전체 화면이나 창 모드를 고르세요."), ResolutionName(DesktopResolution))
			: IsFullSizeWindow(PendingDisplay.Mode, PendingDisplay.Resolution, DesktopResolution)
			? LOCTEXT("ResMaximized", "창 모드에서 모니터 크기를 고르면 최대화된 창으로 띄웁니다 (제목 줄·작업 표시줄이 보이고 그림은 남은 공간에 맞춤).")
			: FText::GetEmpty();
		R.StateColor = ECozyUiColor::InkMuted;
		break;
	case ECozyUiValue::SettingsVSync:
		R.Text = PendingDisplay.bVSync ? LOCTEXT("VSyncOn", "켜기") : LOCTEXT("VSyncOff", "끄기");
		R.StateColor = PendingDisplay.bVSync ? ECozyUiColor::Point : ECozyUiColor::Ink;
		break;
	case ECozyUiValue::SettingsFrameLimit:
	{
		const int32 Limit = FCString::Atoi(*Param.ToString());
		R.Text = FrameLimitName(Limit);
		R.StateColor = PendingDisplay.FrameLimit == Limit ? ECozyUiColor::Point : ECozyUiColor::Ink;
		break;
	}
	case ECozyUiValue::SettingsPendingNote:
	{
		const bool bPending = Tab == ETab::Sound ? HasPendingSound() : HasPendingDisplay();
		R.Text = bPending ? LOCTEXT("Pending", "적용하지 않은 변경이 있습니다 · '적용'을 눌러야 저장되고, '변경 취소'나 '닫기'를 누르면 이전 값으로 돌아갑니다") : FText::GetEmpty();
		R.StateColor = ECozyUiColor::Warning;
		break;
	}
	case ECozyUiValue::SettingsResetNote:
		R.Text = Tab == ETab::Sound
			? LOCTEXT("ResetSound", "'이 탭 기본값 복원' → 전체·배경음·효과음 음량을 모두 100으로 (적용을 눌러야 저장 · 화면 설정과 게임 진행은 그대로)")
			: FText::Format(LOCTEXT("ResetDisplay2", "'이 탭 기본값 복원' → {0} · {1} · 수직 동기화 켬 · 최대 프레임 {2} · 작은 창 항상 위 끔 (적용을 눌러야 저장 · 음량과 게임 진행은 그대로)"),
				ModeName(CozySettings::DefaultMode), ResolutionName(DesktopResolution), FrameLimitName(CozySettings::DefaultFrameLimit));
		R.StateColor = ECozyUiColor::InkMuted;
		break;
	case ECozyUiValue::SettingsConfirm:
		R.Current = FMath::CeilToFloat(ConfirmRemaining);
		R.Max = ConfirmSeconds;
		R.RemainingSeconds = ConfirmRemaining;
		R.Text = FText::Format(LOCTEXT("ConfirmText", "이 설정을 유지할까요?\n{0}초 안에 '유지'를 누르지 않으면 이전 설정({1} · {2})으로 돌아갑니다."),
			FText::AsNumber(FMath::CeilToInt(ConfirmRemaining)), ModeName(ConfirmBefore.Mode), ShownResolution(ConfirmBefore));
		R.StateColor = ECozyUiColor::Ink;
		break;
	case ECozyUiValue::SettingsQuit:
		R.Text = QuitState == EQuit::SaveFailed
			? LOCTEXT("QuitFailed", "영지를 저장하지 못해 게임을 종료하지 않았습니다.\n저장 파일이 읽기 전용이거나 디스크에 쓸 수 없을 때 생깁니다. 다시 시도하거나 취소하세요.")
			: LOCTEXT("QuitAsk", "게임을 종료할까요?\n지금까지의 영지를 저장한 뒤 종료합니다.");
		R.StateColor = QuitState == EQuit::SaveFailed ? ECozyUiColor::Warning : ECozyUiColor::Ink;
		break;
	case ECozyUiValue::SettingsFeedback:
		R.Text = Feedback;
		R.StateColor = ECozyUiColor::InkMuted;
		break;
	case ECozyUiValue::SettingsCompactTop:
		R.Text = PendingDisplay.bCompactTop ? LOCTEXT("CompactTopOn", "켜기") : LOCTEXT("CompactTopOff", "끄기");
		R.StateColor = PendingDisplay.bCompactTop ? ECozyUiColor::Point : ECozyUiColor::Ink;
		break;
	default:
		R.bValid = false;
		break;
	}
	return R;
}

void UCozySettingsSubsystem::GetListRows(ECozyUiListSource Source, TArray<FCozyUiListRow>& OutRows) const
{
	bool bRow = false;
	switch (Source)
	{
	case ECozyUiListSource::SettingsSoundPage: bRow = Tab == ETab::Sound; break;
	case ECozyUiListSource::SettingsDisplayPage: bRow = Tab == ETab::Display; break;
	case ECozyUiListSource::SettingsConfirm: bRow = bConfirmActive; break;
	case ECozyUiListSource::SettingsQuitAsk: bRow = QuitState == EQuit::Asking; break;
	case ECozyUiListSource::SettingsQuitFailed: bRow = QuitState == EQuit::SaveFailed; break;
	default: return;
	}
	if (bRow)
	{
		OutRows.Add({ TEXT("Row"), FText::GetEmpty() });
	}
}

bool UCozySettingsSubsystem::CanRun(ECozyUiAction Action, FName Param, FText& OutReason) const
{
	const bool bDialog = HasDialog();
	switch (Action)
	{
	case ECozyUiAction::KeepDisplay:
	case ECozyUiAction::UndoDisplay:
		return bConfirmActive;
	case ECozyUiAction::CancelQuit:
	case ECozyUiAction::ConfirmQuit:
		return QuitState != EQuit::None;
	case ECozyUiAction::OpenSettings:
	case ECozyUiAction::ToggleCompactTopNow:
		return true;
	case ECozyUiAction::ToggleCompactWindow:
		if (bDialog)
		{
			OutReason = LOCTEXT("WhyCompactDialog", "설정 창의 확인을 먼저 닫아 주세요");
			return false;
		}
		return true;
	default:
		break;
	}
	if (bDialog)
	{
		OutReason = bConfirmActive ? LOCTEXT("WhyConfirm", "화면 설정을 유지할지 먼저 골라 주세요") : LOCTEXT("WhyQuit", "게임 종료 확인을 먼저 닫아 주세요");
		return false;
	}
	switch (Action)
	{
	case ECozyUiAction::ResolutionPrev:
	case ECozyUiAction::ResolutionNext:
	{
		if (PendingDisplay.Mode == EWindowMode::WindowedFullscreen)
		{
			OutReason = LOCTEXT("WhyBorderless", "테두리 없는 창은 모니터 해상도를 그대로 씁니다");
			return false;
		}
		const TArray<FIntPoint> Options = GetResolutionOptions(PendingDisplay.Mode);
		const int32 Index = Options.IndexOfByKey(PendingDisplay.Resolution);
		if (Options.Num() <= 1 || (Action == ECozyUiAction::ResolutionPrev && Index <= 0) || (Action == ECozyUiAction::ResolutionNext && Index >= Options.Num() - 1))
		{
			OutReason = Action == ECozyUiAction::ResolutionPrev ? LOCTEXT("WhyLowest", "가장 낮은 해상도입니다") : LOCTEXT("WhyHighest", "가장 높은 해상도입니다");
			return false;
		}
		return true;
	}
	case ECozyUiAction::ApplySettings:
	case ECozyUiAction::RevertSettings:
		if (!(Tab == ETab::Sound ? HasPendingSound() : HasPendingDisplay()))
		{
			OutReason = LOCTEXT("WhyNoChange", "바뀐 설정이 없습니다");
			return false;
		}
		return true;
	default:
		return true;
	}
}

// ---------------------------------------------------------------------------
// 동작

void UCozySettingsSubsystem::Run(ECozyUiAction Action, FName Param, APlayerController* Controller, float Ratio)
{
	if (!IsSettingsAction(Action))
	{
		return;
	}
	if (Controller)
	{
		SettingsWorld = Controller->GetWorld();
	}
	FText Why;
	if (!CanRun(Action, Param, Why))
	{
		return;
	}
	switch (Action)
	{
	case ECozyUiAction::SettingsTab:
		Tab = Param == TEXT("Display") ? ETab::Display : ETab::Sound;
		Touch();
		break;
	case ECozyUiAction::SetVolume:
		if (Ratio >= 0.f)
		{
			// 슬라이더를 움직이는 동안 바로 들리게 (저장은 '적용'에서)
			float& Target = Param == TEXT("Music") ? PendingMusic : (Param == TEXT("Sfx") ? PendingSfx : PendingMaster);
			Target = FMath::RoundToFloat(FMath::Clamp(Ratio, 0.f, 1.f) * 100.f) / 100.f;
			ApplyAudioToWorld(SettingsWorld.Get(), PendingMaster, PendingMusic, PendingSfx);
		}
		break;
	case ECozyUiAction::SetWindowMode:
	{
		PendingDisplay.Mode = CozySettings::ParseMode(Param);
		if (PendingDisplay.Mode != EWindowMode::WindowedFullscreen)
		{
			// 그 모드에서 고를 수 없는 해상도면 가장 가까운 것으로
			const TArray<FIntPoint> Options = GetResolutionOptions(PendingDisplay.Mode);
			if (Options.Num() > 0 && !Options.Contains(PendingDisplay.Resolution))
			{
				const int64 Want = int64(PendingDisplay.Resolution.X) * PendingDisplay.Resolution.Y;
				FIntPoint Best = Options[0];
				for (const FIntPoint& Each : Options)
				{
					if (FMath::Abs(int64(Each.X) * Each.Y - Want) < FMath::Abs(int64(Best.X) * Best.Y - Want))
					{
						Best = Each;
					}
				}
				PendingDisplay.Resolution = Best;
			}
		}
		Touch();
		break;
	}
	case ECozyUiAction::ResolutionPrev: StepResolution(-1); break;
	case ECozyUiAction::ResolutionNext: StepResolution(1); break;
	case ECozyUiAction::ToggleVSync: PendingDisplay.bVSync = !PendingDisplay.bVSync; Touch(); break;
	case ECozyUiAction::ToggleCompactTop: PendingDisplay.bCompactTop = !PendingDisplay.bCompactTop; Touch(); break;
	case ECozyUiAction::ToggleCompactWindow:
	{
		FText Reason;
		if (!ToggleCompact(Controller, Reason) && !Reason.IsEmpty())
		{
			if (UCozyHudWidget* Hud = CozySettings::FindHud(Controller))
			{
				Hud->ShowToast(Reason);
			}
		}
		break;
	}
	case ECozyUiAction::ToggleCompactTopNow:
	{
		// 작은 창의 핀 버튼: 설정 창의 '적용' 없이 바로 바꾸고 저장 (작은 창 크기가 설정 파일에 남지 않게 작은 창이면 원래 값으로 저장)
		if (UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get())
		{
			Settings->bCompactAlwaysOnTop = !Settings->bCompactAlwaysOnTop;
			PendingDisplay.bCompactTop = Settings->bCompactAlwaysOnTop;
			if (bCompact)
			{
				ApplyCompactTopmost(Settings->bCompactAlwaysOnTop);
				SaveRestoredSettings();
			}
			else
			{
				Settings->SaveSettings();
			}
			if (UCozyHudWidget* Hud = CozySettings::FindHud(Controller))
			{
				Hud->ShowToast(Settings->bCompactAlwaysOnTop ? LOCTEXT("CompactTopOn", "작은 창 항상 위: 켬") : LOCTEXT("CompactTopOff", "작은 창 항상 위: 끔"));
			}
			Touch();
		}
		break;
	}
	case ECozyUiAction::SetFrameLimit: PendingDisplay.FrameLimit = FMath::Max(0, FCString::Atoi(*Param.ToString())); Touch(); break;
	case ECozyUiAction::ApplySettings: Tab == ETab::Sound ? ApplySound() : ApplyDisplay(); break;
	case ECozyUiAction::RevertSettings: RevertTab(); break;
	case ECozyUiAction::ResetSettingsTab: ResetTab(); break;
	case ECozyUiAction::KeepDisplay: FinishConfirm(true, false); break;
	case ECozyUiAction::UndoDisplay: FinishConfirm(false, false); break;
	case ECozyUiAction::AskQuit: QuitState = EQuit::Asking; Touch(); break;
	case ECozyUiAction::CancelQuit: QuitState = EQuit::None; Touch(LOCTEXT("QuitCanceled2", "게임 종료를 취소했습니다")); break;
	case ECozyUiAction::ConfirmQuit:
	{
		// 기존 저장 절차(SaveEstate)로 먼저 저장 · 실패하면 종료하지 않고 다시 시도/취소
		UWorld* World = SettingsWorld.Get();
		UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
		// 타이틀처럼 영지를 시작하지 않은 화면에서는 저장할 것이 없으므로 바로 종료 (빈 상태로 저장을 덮어쓰지 않음)
		const bool bEstateRunning = Estate && Estate->IsStarted();
		const bool bSaved = !bEstateRunning || Estate->SaveEstate(TEXT("게임 종료 (설정 창)"));
		if (!bSaved)
		{
			QuitState = EQuit::SaveFailed;
			UE_LOG(LogCozyRealm, Warning, TEXT("[설정] 종료 전 저장 실패 → 종료하지 않음"));
			Touch();
			break;
		}
		UE_LOG(LogCozyRealm, Log, TEXT("[설정] 저장 완료 → 게임 종료"));
		QuitState = EQuit::None;
		// 패키지 게임은 프로그램 종료 · 에디터 플레이(PIE)는 플레이만 끝남 (에디터는 그대로)
		UKismetSystemLibrary::QuitGame(World, Controller, EQuitPreference::Quit, false);
		break;
	}
	default:
		break;
	}
}

void UCozySettingsSubsystem::StepResolution(int32 Direction)
{
	const TArray<FIntPoint> Options = GetResolutionOptions(PendingDisplay.Mode);
	const int32 Index = Options.IndexOfByKey(PendingDisplay.Resolution);
	if (Options.IsValidIndex(Index + Direction))
	{
		PendingDisplay.Resolution = Options[Index + Direction];
	}
	Touch();
}

void UCozySettingsSubsystem::ApplySound()
{
	UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	Settings->MasterVolume = PendingMaster;
	Settings->MusicVolume = PendingMusic;
	Settings->SfxVolume = PendingSfx;
	Settings->SaveSettings();
	ApplyAudioToWorld(SettingsWorld.Get(), PendingMaster, PendingMusic, PendingSfx);
	UE_LOG(LogCozyRealm, Log, TEXT("[설정] 음량 저장: 전체 %d · 배경음 %d · 효과음 %d"), CozySettings::Percent(PendingMaster), CozySettings::Percent(PendingMusic), CozySettings::Percent(PendingSfx));
	Touch(FText::Format(LOCTEXT("SoundSaved", "음량을 저장했습니다 (전체 {0} · 배경음 {1} · 효과음 {2})"),
		FText::AsNumber(CozySettings::Percent(PendingMaster)), FText::AsNumber(CozySettings::Percent(PendingMusic)), FText::AsNumber(CozySettings::Percent(PendingSfx))));
}

void UCozySettingsSubsystem::ApplyDisplay()
{
	UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	const FDisplayChoice Before = ReadAppliedDisplay();
	FDisplayChoice Want = PendingDisplay;
	if (Want.Mode == EWindowMode::WindowedFullscreen)
	{
		Want.Resolution = Settings->GetDesktopResolution();
	}
	const bool bVideoMode = Want.Mode != Before.Mode || (Want.Mode != EWindowMode::WindowedFullscreen && Want.Resolution != Before.Resolution);

	Settings->SetVSyncEnabled(Want.bVSync);
	Settings->SetFrameRateLimit(static_cast<float>(Want.FrameLimit));
	Settings->bCompactAlwaysOnTop = Want.bCompactTop;
	if (bCompact)
	{
		ApplyCompactTopmost(Want.bCompactTop);
	}
	Settings->ApplyNonResolutionSettings();
	if (!bVideoMode)
	{
		Settings->SaveSettings();
		UE_LOG(LogCozyRealm, Log, TEXT("[설정] 화면 적용(모드·해상도 그대로): 수직 동기화 %d · 최대 프레임 %d"), Want.bVSync ? 1 : 0, Want.FrameLimit);
		PendingDisplay = ReadAppliedDisplay();
		Touch(LOCTEXT("DisplaySaved", "화면 설정을 적용하고 저장했습니다"));
		return;
	}
	// 화면 모드·해상도: 적용하고 15초 동안 유지 확인 · 확인 전에는 저장하지 않음 (그 사이 꺼져도 이전 설정이 남음)
	ConfirmBefore = Before;
	RestoreFromMaximize(SettingsWorld.Get());
	Settings->SetFullscreenMode(Want.Mode);
	Settings->SetScreenResolution(Want.Resolution);
	Settings->ApplyResolutionSettings(false);
	ApplyWindowState(SettingsWorld.Get());
	bConfirmActive = true;
	ConfirmRemaining = ConfirmSeconds;
	PendingDisplay = ReadAppliedDisplay();
	UE_LOG(LogCozyRealm, Log, TEXT("[설정] 화면 모드·해상도 적용 → 유지 확인 시작: %s %dx%d (이전 %s %dx%d)"),
		*ModeName(Want.Mode).ToString(), Want.Resolution.X, Want.Resolution.Y, *ModeName(Before.Mode).ToString(), Before.Resolution.X, Before.Resolution.Y);
	Touch();
}

void UCozySettingsSubsystem::FinishConfirm(bool bKeep, bool bTimedOut)
{
	UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
	bConfirmActive = false;
	ConfirmRemaining = 0.f;
	if (!Settings)
	{
		return;
	}
	if (bKeep)
	{
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
		UE_LOG(LogCozyRealm, Log, TEXT("[설정] 화면 설정 유지 → 저장"));
		PendingDisplay = ReadAppliedDisplay();
		Touch(LOCTEXT("DisplayKept", "화면 설정을 유지하고 저장했습니다"));
		return;
	}
	RestoreFromMaximize(SettingsWorld.Get());
	Settings->SetFullscreenMode(ConfirmBefore.Mode);
	Settings->SetScreenResolution(ConfirmBefore.Resolution);
	Settings->ApplyResolutionSettings(false);
	Settings->ConfirmVideoMode();
	ApplyWindowState(SettingsWorld.Get());
	Settings->SaveSettings();
	UE_LOG(LogCozyRealm, Log, TEXT("[설정] 화면 설정 되돌림 (%s) → %s %dx%d"), bTimedOut ? TEXT("15초 초과") : TEXT("거절"),
		*ModeName(ConfirmBefore.Mode).ToString(), ConfirmBefore.Resolution.X, ConfirmBefore.Resolution.Y);
	PendingDisplay = ReadAppliedDisplay();
	Touch(bTimedOut ? LOCTEXT("DisplayTimedOut", "15초 안에 확인하지 않아 이전 화면 설정으로 돌아갔습니다")
		: LOCTEXT("DisplayUndone", "이전 화면 설정으로 돌아갔습니다"));
}

void UCozySettingsSubsystem::RevertTab()
{
	if (Tab == ETab::Sound)
	{
		// 저장된 음량으로 되돌리고 미리 듣던 소리도 그 값으로
		LoadPendingSound();
		ApplySavedAudio(SettingsWorld.Get());
	}
	else
	{
		PendingDisplay = ReadAppliedDisplay();
	}
	Touch(LOCTEXT("Reverted", "적용하지 않은 변경을 취소했습니다"));
}

void UCozySettingsSubsystem::ResetTab()
{
	if (Tab == ETab::Sound)
	{
		PendingMaster = PendingMusic = PendingSfx = 1.f;
		ApplyAudioToWorld(SettingsWorld.Get(), PendingMaster, PendingMusic, PendingSfx);
		Touch(LOCTEXT("ResetSoundDone", "사운드 탭을 기본값(모두 100)으로 바꿨습니다 · '적용'을 눌러야 저장됩니다"));
	}
	else
	{
		const UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
		PendingDisplay.Mode = CozySettings::DefaultMode;
		PendingDisplay.Resolution = Settings ? Settings->GetDesktopResolution() : PendingDisplay.Resolution;
		PendingDisplay.bVSync = CozySettings::bDefaultVSync;
		PendingDisplay.FrameLimit = CozySettings::DefaultFrameLimit;
		PendingDisplay.bCompactTop = false;
		Touch(LOCTEXT("ResetDisplayDone", "화면 탭을 기본값으로 바꿨습니다 · '적용'을 눌러야 바뀌고 저장됩니다"));
	}
}

// ---------------------------------------------------------------------------
// 작은 창 모드

TSharedPtr<SWindow> UCozySettingsSubsystem::GetGameWindow(const UWorld* World) const
{
	const UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	return Viewport ? Viewport->GetWindow() : nullptr;
}

bool UCozySettingsSubsystem::IsFullSizeWindow(EWindowMode::Type Mode, const FIntPoint& Resolution, const FIntPoint& Desktop)
{
	return Mode == EWindowMode::Windowed && Desktop.X > 0 && Resolution.X >= Desktop.X && Resolution.Y >= Desktop.Y - 160;
}

void UCozySettingsSubsystem::RestoreFromMaximize(const UWorld* World)
{
	if (GIsEditor) return;
	TSharedPtr<SWindow> Window = GetGameWindow(World);
	if (Window.IsValid() && Window->IsWindowMaximized())
	{
		Window->Restore();
		UE_LOG(LogCozyRealm, Log, TEXT("[설정] 최대화된 창을 풀고 해상도 변경"));
	}
}

void UCozySettingsSubsystem::ApplyWindowState(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	ApplyWindowStateNow(World);
	// 시작 직후 엔진이 창 크기를 한 번 더 맞추는 경우가 있어 몇 프레임 뒤에도 다시 확인
	MaximizeWorld = World;
	WindowStateFrames = 10;
}

void UCozySettingsSubsystem::ApplyWindowStateNow(UWorld* World)
{
	// 에디터 플레이는 에디터가 창을 관리하므로 건드리지 않음 · 작은 창 중에도 그대로
	UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
	if (GIsEditor || bCompact || !World || !Settings) return;
	TSharedPtr<SWindow> Window = GetGameWindow(World);
	const EWindowMode::Type Mode = Settings->GetFullscreenMode();
	const FIntPoint Resolution = Settings->GetScreenResolution();
	const FIntPoint Desktop = Settings->GetDesktopResolution();
	UE_LOG(LogCozyRealm, Log, TEXT("[설정] 창 상태 확인: 모드 %d · 해상도 %dx%d · 모니터 %dx%d · 창 %s · 최대화 %d"), int32(Mode), Resolution.X, Resolution.Y, Desktop.X, Desktop.Y,
		Window.IsValid() ? TEXT("있음") : TEXT("없음"), Window.IsValid() && Window->IsWindowMaximized() ? 1 : 0);
	if (!Window.IsValid()) return;
	if (IsFullSizeWindow(Mode, Resolution, Desktop))
	{
		// 모니터만 한 창은 제목 줄이 화면 밖으로 밀려나므로 최대화된 창으로 보여 줌 (Claude 2026-10-21)
		if (Window->IsWindowMaximized()) return;
		Window->Maximize();
		MaximizeResolution = Resolution;
		MaximizeWorld = World;
		MaximizeFrames = 6;
		UE_LOG(LogCozyRealm, Log, TEXT("[설정] 창 모드 %dx%d = 모니터 크기 → 최대화된 창 (제목 줄 보임)"), Resolution.X, Resolution.Y);
	}
}

bool UCozySettingsSubsystem::ToggleCompact(APlayerController* Controller, FText& OutReason)
{
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
	TSharedPtr<SWindow> Window = bCompact ? CompactWindow.Pin() : GetGameWindow(World);
	if (!World || !Settings || !Window.IsValid())
	{
		OutReason = LOCTEXT("CompactNoWindow", "게임 창을 찾지 못해 작은 창으로 바꿀 수 없습니다");
		return false;
	}
#if WITH_EDITOR
	// 에디터 뷰포트 안에서 플레이 중이면 게임 창이 곧 에디터 창 → 에디터를 줄이지 않음
	if (GIsEditor && Window == FGlobalTabmanager::Get()->GetRootWindow())
	{
		OutReason = LOCTEXT("CompactInViewport", "에디터 뷰포트 안 플레이에서는 작은 창을 쓸 수 없습니다 (새 창으로 플레이하거나 실행 파일에서)");
		return false;
	}
#endif
	// 확인 창이 떠 있을 때는 유일한 호출 경로인 Run이 CanRun에서 먼저 막는다 (WhyCompactDialog)
	const bool bEditorWindow = GIsEditor;
	if (!bCompact)
	{
		// 들어가기: 지금 창·화면 설정을 기억 (화면 설정 파일은 바꾸지 않음)
		SavedMode = Settings->GetFullscreenMode();
		SavedResolution = Settings->GetScreenResolution();
		SavedClientSize = FVector2D(Window->GetClientSizeInScreen());
		SavedPosition = FVector2D(Window->GetPositionInScreen());
		CompactWindow = Window;
		bCompact = true;
		if (!bEditorWindow)
		{
			// 전체 화면 · 테두리 없는 창이어도 창 모드 작은 창으로 (r.setres와 같은 방법 · 다음 프레임에 적용)
			MaximizeFrames = 0;
			WindowStateFrames = 0;
			if (Window->IsWindowMaximized())
			{
				Window->Restore();
				CompactRequestFrames = 3;
			}
			else
			{
				FSystemResolution::RequestResolutionChange(CompactWidth, CompactHeight, EWindowMode::Windowed);
			}
		}
		else
		{
			Window->Resize(FVector2D(CompactWidth, CompactHeight));
		}
		bMoveToCorner = true;
		CompactMoveFrames = 6;
		UE_LOG(LogCozyRealm, Log, TEXT("[작은 창] 켬: %dx%d (이전 %s %dx%d · 창 %.0fx%.0f)"), CompactWidth, CompactHeight,
			*ModeName(SavedMode).ToString(), SavedResolution.X, SavedResolution.Y, SavedClientSize.X, SavedClientSize.Y);
	}
	else
	{
		// 나오기: 항상 위를 풀고, 화면 설정과 창을 들어가기 전으로
		ApplyCompactTopmost(false);
		bCompact = false;
		RestoreSettingsValues();
		if (!bEditorWindow)
		{
			Settings->ApplyResolutionSettings(false);
			Settings->ConfirmVideoMode();
			ApplyWindowState(World);
		}
		else
		{
			Window->Resize(SavedClientSize);
		}
		bMoveToCorner = false;
		// 최대화된 창으로 돌아가면 위치를 옮기지 않음
		CompactMoveFrames = SavedMode == EWindowMode::Windowed && MaximizeFrames == 0 ? 6 : 0;
		UE_LOG(LogCozyRealm, Log, TEXT("[작은 창] 끔 → %s %dx%d"), *ModeName(SavedMode).ToString(), SavedResolution.X, SavedResolution.Y);
	}

	if (UCozyHudWidget* Hud = CozySettings::FindHud(Controller))
	{
		Hud->SetCompactMode(bCompact);
	}
	Touch();
	return true;
}

void UCozySettingsSubsystem::PlaceCompactWindow()
{
	TSharedPtr<SWindow> Window = CompactWindow.Pin();
	if (!Window.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	if (!bMoveToCorner)
	{
		Window->MoveWindowTo(SavedPosition);
		return;
	}
	// 창이 있는 모니터의 작업 영역(작업 표시줄 제외) 오른쪽 아래 구석
	FDisplayMetrics Metrics;
	FSlateApplication::Get().GetDisplayMetrics(Metrics);
	const FVector2D Position = FVector2D(Window->GetPositionInScreen());
	const FVector2D Size = FVector2D(Window->GetSizeInScreen());
	const FPlatformRect WorkArea = Metrics.GetMonitorWorkAreaFromPoint(Position + Size * 0.5f);
	constexpr float Margin = 16.f;
	Window->MoveWindowTo(FVector2D(WorkArea.Right - Size.X - Margin, WorkArea.Bottom - Size.Y - Margin));
	if (const UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get())
	{
		ApplyCompactTopmost(Settings->bCompactAlwaysOnTop);
	}
}

void UCozySettingsSubsystem::ApplyCompactTopmost(bool bTop)
{
#if PLATFORM_WINDOWS
	TSharedPtr<SWindow> Window = CompactWindow.Pin();
	TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	if (HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr)
	{
		::SetWindowPos(Handle, bTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		UE_LOG(LogCozyRealm, Log, TEXT("[작은 창] 항상 위 %s"), bTop ? TEXT("켬") : TEXT("끔"));
	}
#endif
}

void UCozySettingsSubsystem::RestoreSettingsValues()
{
	// 창 모드에서 창 크기를 바꾸면 엔진이 그 크기를 해상도 설정에 써 넣음 → 들어가기 전 값으로 되돌림
	if (UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get())
	{
		if (SavedResolution.X > 0)
		{
			Settings->SetScreenResolution(SavedResolution);
			Settings->SetFullscreenMode(SavedMode);
		}
	}
}

void UCozySettingsSubsystem::SaveRestoredSettings()
{
	// 엔진은 창 크기가 바뀔 때마다 그 크기를 설정 파일에 바로 저장함(UGameEngine::OnViewportResized)
	// → 작은 창이 된 뒤와 종료 때 들어가기 전 값으로 되돌려 파일에 다시 저장 (갑자기 꺼져도 640×360이 남지 않게)
	if (UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get())
	{
		RestoreSettingsValues();
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
	}
}

void UCozySettingsSubsystem::Deinitialize()
{
	// 작은 창인 채로 끝나도 다음 실행은 원래 화면 설정으로 (작은 창 크기가 설정에 남지 않게)
	if (bCompact)
	{
		ApplyCompactTopmost(false);
		SaveRestoredSettings();
		bCompact = false;
	}
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// 소리

void UCozySettingsSubsystem::ApplySavedAudio(const UObject* WorldContext)
{
	if (const UCozyGameUserSettings* Saved = UCozyGameUserSettings::Get())
	{
		ApplyAudioToWorld(WorldContext, Saved->MasterVolume, Saved->MusicVolume, Saved->SfxVolume);
	}
}

void UCozySettingsSubsystem::ApplyAudioToWorld(const UObject* WorldContext, float Master, float Music, float Sfx)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UCozyAudioSettings* Audio = UCozyAudioSettings::Get();
	if (!World || !Audio)
	{
		return;
	}
	USoundMix* Mix = Audio->VolumeMix.LoadSynchronous();
	USoundClass* MasterClass = Audio->MasterClass.LoadSynchronous();
	USoundClass* MusicClass = Audio->MusicClass.LoadSynchronous();
	USoundClass* SfxClass = Audio->SfxClass.LoadSynchronous();
	if (!Mix || !MasterClass || !MusicClass || !SfxClass)
	{
		UE_LOG(LogCozyRealm, Warning, TEXT("[설정] 소리 연결이 비어 있어 음량을 적용하지 못함 (프로젝트 설정 → Cozy Audio)"));
		return;
	}
	// 클래스마다 실제 음량 = 전체 × 그 클래스 (자식에게 덮어쓰지 않게 클래스별로 따로 넣음)
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, MasterClass, Master, 1.f, 0.f, false);
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, MusicClass, Master * Music, 1.f, 0.f, false);
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, SfxClass, Master * Sfx, 1.f, 0.f, false);
	if (MixWorld.Get() != World)
	{
		UGameplayStatics::PushSoundMixModifier(World, Mix);
		MixWorld = World;
	}
	UE_LOG(LogCozyRealm, Verbose, TEXT("[설정] 음량 적용: 전체 %.2f · 배경음 %.2f · 효과음 %.2f"), Master, Master * Music, Master * Sfx);
}

// ---------------------------------------------------------------------------

void UCozySettingsSubsystem::Tick(float DeltaTime)
{
	if (CompactRequestFrames > 0 && --CompactRequestFrames == 0 && bCompact)
	{
		FSystemResolution::RequestResolutionChange(CompactWidth, CompactHeight, EWindowMode::Windowed);
		CompactMoveFrames = 8;
		UE_LOG(LogCozyRealm, Log, TEXT("[작은 창] 최대화를 푼 뒤 %dx%d 요청"), CompactWidth, CompactHeight);
	}
	if (WindowStateFrames > 0 && --WindowStateFrames == 0)
	{
		ApplyWindowStateNow(MaximizeWorld.Get());
	}
	// 최대화 뒤: 엔진이 줄어든 창 크기를 해상도 설정에 써 넣은 것을 고른 값으로 되돌림 (다음 실행도 모니터 크기 → 최대화)
	if (MaximizeFrames > 0 && --MaximizeFrames == 0)
	{
		UCozyGameUserSettings* Settings = UCozyGameUserSettings::Get();
		if (Settings && !bCompact && Settings->GetFullscreenMode() == EWindowMode::Windowed && Settings->GetScreenResolution() != MaximizeResolution)
		{
			Settings->SetScreenResolution(MaximizeResolution);
			if (!bConfirmActive) { Settings->ConfirmVideoMode(); Settings->SaveSettings(); }
			PendingDisplay = ReadAppliedDisplay();
			UE_LOG(LogCozyRealm, Log, TEXT("[설정] 최대화 뒤 해상도 설정을 %dx%d로 유지"), MaximizeResolution.X, MaximizeResolution.Y);
			Touch();
		}
	}
	// 작은 창 위치: 크기 변경이 적용된 뒤(몇 프레임) 구석으로 · 마지막 프레임에 한 번 더
	if (CompactMoveFrames > 0)
	{
		--CompactMoveFrames;
		if (CompactMoveFrames == 3 || CompactMoveFrames == 0)
		{
			PlaceCompactWindow();
		}
		if (CompactMoveFrames == 0 && bCompact)
		{
			SaveRestoredSettings();
		}
	}
	if (!bConfirmActive)
	{
		return;
	}
	ConfirmRemaining -= DeltaTime;
	if (ConfirmRemaining <= 0.f)
	{
		FinishConfirm(false, true);
	}
}

void UCozySettingsSubsystem::Touch(const FText& Message)
{
	// 안내는 방금 한 일만 (다른 조작을 하면 지워짐)
	Feedback = Message;
	++Revision;
}

#undef LOCTEXT_NAMESPACE
