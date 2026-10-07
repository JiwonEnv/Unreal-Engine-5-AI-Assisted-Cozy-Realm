#include "UI/Kit/CozyUiScreen.h"
#include "UI/Kit/CozyUiTheme.h"
#include "UI/Kit/CozyUiScreenConfig.h"
#include "UI/Kit/CozyUiWidgets.h"
#include "UI/CozyHudWidget.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "CozyRealm.h"

#define LOCTEXT_NAMESPACE "CozyUi"

void UCozyUiScreen::NativePreConstruct()
{
	Super::NativePreConstruct();
	// 디자이너에서도 테마·버튼 영역·미리보기 값이 보이게
	RefreshTheme();
	RefreshValues();
}

void UCozyUiScreen::NativeConstruct()
{
	Super::NativeConstruct();
	BindAssetEvents();
	RefreshTheme();
	RefreshValues();
}

void UCozyUiScreen::NativeDestruct()
{
	UnbindAssetEvents();
	Super::NativeDestruct();
}

void UCozyUiScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ValueTimer += InDeltaTime;
	if (ValueTimer >= 0.25f)
	{
		ValueTimer = 0.f;
		RefreshValues();
	}
}

void UCozyUiScreen::BindAssetEvents()
{
	UnbindAssetEvents();
	if (UCozyUiTheme* Theme = GetTheme())
	{
		ThemeHandle = Theme->OnChanged.AddUObject(this, &UCozyUiScreen::HandleAssetChanged);
		BoundTheme = Theme;
	}
	if (Config)
	{
		ConfigHandle = Config->OnChanged.AddUObject(this, &UCozyUiScreen::HandleAssetChanged);
		BoundConfig = Config;
	}
}

void UCozyUiScreen::UnbindAssetEvents()
{
	if (UCozyUiTheme* Theme = BoundTheme.Get())
	{
		Theme->OnChanged.Remove(ThemeHandle);
	}
	if (UCozyUiScreenConfig* Old = BoundConfig.Get())
	{
		Old->OnChanged.Remove(ConfigHandle);
	}
	BoundTheme.Reset();
	BoundConfig.Reset();
}

void UCozyUiScreen::HandleAssetChanged(const UObject* Asset)
{
	// 테마를 다른 에셋으로 바꿨을 수도 있으니 다시 연결
	BindAssetEvents();
	RefreshTheme();
	RefreshValues();
	UE_LOG(LogCozyRealm, Log, TEXT("[UI] 설정 변경 반영: %s → %s"), Asset ? *Asset->GetName() : TEXT("?"), *GetClass()->GetName());
}

void UCozyUiScreen::SetPreview(bool bEnable)
{
	bPreview = bEnable;
	RefreshTheme();
	RefreshValues();
}

void UCozyUiScreen::SetPreviewState(ECozyUiPreviewState NewState)
{
	PreviewState = NewState;
	RefreshValues();
}

UCozyUiTheme* UCozyUiScreen::GetTheme() const
{
	if (Config && Config->ThemeOverride)
	{
		return Config->ThemeOverride;
	}
	return UCozyUiSettings::LoadDefaultTheme();
}

FLinearColor UCozyUiScreen::GetColor(ECozyUiColor Role) const
{
	if (Config)
	{
		if (const FLinearColor* Override = Config->ColorOverrides.Find(Role))
		{
			return *Override;
		}
	}
	const UCozyUiTheme* Theme = GetTheme();
	return Theme ? Theme->Colors.Get(Role) : FLinearColor::White;
}

FSlateFontInfo UCozyUiScreen::GetFont(ECozyUiTextRole Role) const
{
	if (Config)
	{
		if (const FSlateFontInfo* Override = Config->FontOverrides.Find(Role))
		{
			return *Override;
		}
	}
	const UCozyUiTheme* Theme = GetTheme();
	return Theme ? Theme->Fonts.Get(Role) : FSlateFontInfo();
}

const FCozyUiButtonEntry* UCozyUiScreen::FindButtonEntry(FName ButtonId) const
{
	if (!Config || ButtonId.IsNone())
	{
		return nullptr;
	}
	return Config->Buttons.FindByPredicate([ButtonId](const FCozyUiButtonEntry& Each) { return Each.Id == ButtonId; });
}

TArray<FCozyUiButtonEntry> UCozyUiScreen::GetButtonsForArea(FName AreaId) const
{
	TArray<FCozyUiButtonEntry> Result;
	if (Config && !AreaId.IsNone())
	{
		for (const FCozyUiButtonEntry& Each : Config->Buttons)
		{
			if (Each.Area == AreaId)
			{
				Result.Add(Each);
			}
		}
		Result.StableSort([](const FCozyUiButtonEntry& A, const FCozyUiButtonEntry& B) { return A.Order < B.Order; });
	}
	return Result;
}

void UCozyUiScreen::ForEachElement(TFunctionRef<void(ICozyUiElement&)> Visit) const
{
	// 안쪽 사용자 위젯(버튼 · 게이지 · 칩 블루프린트)까지 따라 들어감
	TFunction<void(const UUserWidget*)> Walk;
	Walk = [&Walk, &Visit](const UUserWidget* Owner)
	{
		if (!Owner || !Owner->WidgetTree)
		{
			return;
		}
		Owner->WidgetTree->ForEachWidget([&Walk, &Visit](UWidget* Widget)
		{
			if (ICozyUiElement* Element = Cast<ICozyUiElement>(Widget))
			{
				Visit(*Element);
			}
			if (const UUserWidget* Nested = Cast<UUserWidget>(Widget))
			{
				if (!Nested->IsA<UCozyUiScreen>())
				{
					Walk(Nested);
				}
			}
		});
	};
	Walk(this);
}

void UCozyUiScreen::RefreshTheme()
{
	if (!WidgetTree)
	{
		return;
	}
	// 버튼 영역이 버튼을 새로 만들 수 있으므로 영역을 먼저, 그다음 전체
	ForEachElement([this](ICozyUiElement& Element)
	{
		if (Cast<UCozyUiButtonArea>(&Element))
		{
			Element.ApplyCozyTheme(*this);
		}
	});
	ForEachElement([this](ICozyUiElement& Element)
	{
		if (!Cast<UCozyUiButtonArea>(&Element))
		{
			Element.ApplyCozyTheme(*this);
		}
	});
}

void UCozyUiScreen::RefreshValues()
{
	if (!WidgetTree)
	{
		return;
	}
	ForEachElement([this](ICozyUiElement& Element) { Element.UpdateCozyValue(*this); });
}

FCozyUiValueResult UCozyUiScreen::GetPreviewValue(ECozyUiValue Value) const
{
	FCozyUiValueResult R;
	R.bValid = true;
	R.Max = 100.f;
	switch (PreviewState)
	{
	case ECozyUiPreviewState::Progress: R.Current = 62.f; R.RemainingSeconds = 8.f; R.StateColor = ECozyUiColor::Progress; break;
	case ECozyUiPreviewState::Paused: R.Current = 40.f; R.RemainingSeconds = 12.f; R.StateColor = ECozyUiColor::Paused; R.Text = LOCTEXT("PvPaused", "일시 정지"); break;
	case ECozyUiPreviewState::Full: R.Current = 100.f; R.StateColor = ECozyUiColor::Warning; R.Text = LOCTEXT("PvFull", "가득 참"); break;
	case ECozyUiPreviewState::Locked: R.Current = 0.f; R.StateColor = ECozyUiColor::Locked; R.Text = LOCTEXT("PvLocked", "잠김"); break;
	case ECozyUiPreviewState::Claimable: R.Current = 100.f; R.StateColor = ECozyUiColor::Claimable; R.Text = LOCTEXT("PvClaim", "수령 가능"); break;
	default: R.Current = 0.f; R.StateColor = ECozyUiColor::InkMuted; break;
	}
	switch (Value)
	{
	case ECozyUiValue::ShrineLevel:
	case ECozyUiValue::FacilityLevelCap: R.Current = 2.f; R.Max = 0.f; break;
	case ECozyUiValue::GameClock: R.Text = LOCTEXT("PvClock", "미리보기 · 14:20"); break;
	case ECozyUiValue::UpgradeSlots: R.Current = PreviewState == ECozyUiPreviewState::Empty ? 0.f : 1.f; R.Max = 1.f; break;
	case ECozyUiValue::StoredFacilities: R.Current = 1.f; R.Max = 0.f; break;
	default: break;
	}
	return R;
}

FCozyUiValueResult UCozyUiScreen::GetValue(ECozyUiValue Value, FName Param) const
{
	if (Value == ECozyUiValue::None)
	{
		return FCozyUiValueResult();
	}
	const UWorld* World = GetWorld();
	const UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	if (bPreview || !Estate || IsDesignTime())
	{
		return GetPreviewValue(Value);
	}

	FCozyUiValueResult R;
	R.bValid = true;
	auto FindFirst = [Estate](FName DefinitionId) -> const FCozyFacilityState*
	{
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			if (Facility.DefinitionId == DefinitionId && !Facility.bStored)
			{
				return &Facility;
			}
		}
		return nullptr;
	};

	switch (Value)
	{
	case ECozyUiValue::ResourceAmount:
	{
		R.Current = Estate->GetAmount(Param);
		const FCozyItemRow* Item = Estate->GetItemDef(Param);
		R.Max = (Item && Item->Category == ECozyItemCategory::Material) ? Estate->GetConfig().StorageCapPerItem : 0.f;
		R.StateColor = (R.Max > 0.f && R.Current >= R.Max) ? ECozyUiColor::Warning : ECozyUiColor::Ok;
		break;
	}
	case ECozyUiValue::ShrineLevel:
	case ECozyUiValue::FacilityLevelCap:
		R.Current = Estate->GetShrineLevel();
		break;
	case ECozyUiValue::GameClock:
	{
		const FDateTime Now = FDateTime::Now();
		R.Text = FText::Format(LOCTEXT("Clock", "{0} · {1}:{2}"), Estate->IsNight() ? LOCTEXT("Night", "밤") : LOCTEXT("Day", "낮"),
			FText::FromString(FString::Printf(TEXT("%02d"), Now.GetHour())), FText::FromString(FString::Printf(TEXT("%02d"), Now.GetMinute())));
		break;
	}
	case ECozyUiValue::ProductionProgress:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const FCozyProductionView View = Estate->GetProductionView(Facility->InstanceId);
			R.Current = View.Progress01 * 100.f;
			R.Max = 100.f;
			R.RemainingSeconds = View.RemainingSeconds;
			R.Text = View.Status;
			R.StateColor = !View.bWorking ? ECozyUiColor::Paused : (View.UnclaimedCapacity > 0 && View.UnclaimedAmount >= View.UnclaimedCapacity ? ECozyUiColor::Warning : ECozyUiColor::Progress);
		}
		break;
	case ECozyUiValue::UnclaimedAmount:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const FCozyUnclaimedView View = Estate->GetUnclaimedView(Facility->InstanceId);
			R.Current = View.Amount;
			R.Max = View.Capacity;
			R.Text = View.ItemName;
			R.StateColor = (R.Max > 0.f && R.Current >= R.Max) ? ECozyUiColor::Warning : (R.Current > 0.f ? ECozyUiColor::Claimable : ECozyUiColor::InkMuted);
		}
		break;
	case ECozyUiValue::ProcessingProgress:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const TArray<FCozyProcessingJobView> Jobs = Estate->GetProcessingJobs(Facility->InstanceId);
			R.Max = 100.f;
			if (Jobs.Num() > 0)
			{
				R.Current = Jobs[0].RunProgress01 * 100.f;
				R.RemainingSeconds = Jobs[0].TotalRemainingSeconds;
				R.Text = Jobs[0].Status;
				R.StateColor = Jobs[0].bPaused ? ECozyUiColor::Paused : ECozyUiColor::Progress;
			}
			else
			{
				R.StateColor = ECozyUiColor::InkMuted;
			}
		}
		break;
	case ECozyUiValue::UpgradeProgress:
	{
		const TArray<FCozyUpgradeJobView> Jobs = Estate->GetUpgradeJobs();
		R.Max = 100.f;
		if (Jobs.Num() > 0)
		{
			R.Current = Jobs[0].Progress01 * 100.f;
			R.RemainingSeconds = Jobs[0].RemainingSeconds;
			R.Text = Jobs[0].FacilityName;
			R.StateColor = ECozyUiColor::Progress;
		}
		else
		{
			R.StateColor = ECozyUiColor::InkMuted;
			R.Text = LOCTEXT("NoUpgrade", "업그레이드 없음");
		}
		break;
	}
	case ECozyUiValue::UpgradeSlots:
		R.Current = Estate->GetUpgradeJobs().Num();
		R.Max = Estate->GetUpgradeSlotCount();
		break;
	case ECozyUiValue::StoredFacilities:
		R.Current = Estate->GetStoredFacilities().Num();
		break;
	default:
		R.bValid = false;
		break;
	}
	return R;
}

FText UCozyUiScreen::GetActionName(ECozyUiAction Action)
{
	switch (Action)
	{
	case ECozyUiAction::OpenStorage: return LOCTEXT("ActStorage", "창고 열기");
	case ECozyUiAction::OpenResidents: return LOCTEXT("ActResidents", "주민 창");
	case ECozyUiAction::TogglePlacement: return LOCTEXT("ActPlace", "배치 모드");
	case ECozyUiAction::CollectAll: return LOCTEXT("ActCollect", "전부 수확");
	case ECozyUiAction::OpenUpgrade: return LOCTEXT("ActUpgrade", "후신소 창");
	case ECozyUiAction::ToggleDebug: return LOCTEXT("ActDebug", "디버그 창");
	case ECozyUiAction::CloseWindow: return LOCTEXT("ActClose", "창 닫기");
	case ECozyUiAction::TogglePreview: return LOCTEXT("ActPreview", "UI 미리보기");
	default: return LOCTEXT("ActNone", "동작 없음");
	}
}

FText UCozyUiScreen::GetPreviewStateName(ECozyUiPreviewState State)
{
	switch (State)
	{
	case ECozyUiPreviewState::Progress: return LOCTEXT("StProgress", "진행 중");
	case ECozyUiPreviewState::Paused: return LOCTEXT("StPaused", "일시 정지");
	case ECozyUiPreviewState::Full: return LOCTEXT("StFull", "가득 참");
	case ECozyUiPreviewState::Locked: return LOCTEXT("StLocked", "잠김");
	case ECozyUiPreviewState::Claimable: return LOCTEXT("StClaim", "수령 가능");
	default: return LOCTEXT("StEmpty", "비어 있음");
	}
}

void UCozyUiScreen::ShowMessage(const FText& Message) const
{
	if (const ACozyRealmEstatePlayerController* Controller = Cast<ACozyRealmEstatePlayerController>(GetOwningPlayer()))
	{
		if (UCozyHudWidget* Hud = Controller->GetHud())
		{
			Hud->ShowToast(Message);
		}
	}
}

void UCozyUiScreen::RunAction(const FCozyUiButtonEntry& Entry)
{
	if (Entry.Action == ECozyUiAction::TogglePreview)
	{
		SetPreview(!bPreview);
		ShowMessage(bPreview ? LOCTEXT("PvOn", "UI 미리보기 켜짐 · 실제 재료를 쓰지 않습니다") : LOCTEXT("PvOff", "UI 미리보기 꺼짐"));
		return;
	}
	if (bPreview)
	{
		ShowMessage(FText::Format(LOCTEXT("PvAction", "미리보기: '{0}' → {1} (실제 동작 안 함)"), Entry.Label, GetActionName(Entry.Action)));
		UE_LOG(LogCozyRealm, Log, TEXT("[UI] 미리보기 버튼: %s → %s"), *Entry.Id.ToString(), *GetActionName(Entry.Action).ToString());
		return;
	}
	ACozyRealmEstatePlayerController* Controller = Cast<ACozyRealmEstatePlayerController>(GetOwningPlayer());
	UCozyHudWidget* Hud = Controller ? Controller->GetHud() : nullptr;
	UCozyEstateSubsystem* Estate = GetWorld() ? GetWorld()->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	if (!Controller || !Hud || !Estate)
	{
		return;
	}
	UE_LOG(LogCozyRealm, Log, TEXT("[UI] 버튼: %s → %s"), *Entry.Id.ToString(), *GetActionName(Entry.Action).ToString());
	switch (Entry.Action)
	{
	case ECozyUiAction::OpenStorage: Hud->ToggleStorageWindow(); break;
	case ECozyUiAction::OpenResidents: Hud->ToggleNagayaWindow(); break;
	case ECozyUiAction::TogglePlacement: Controller->SetPlacementMode(!Controller->IsPlacementMode()); break;
	case ECozyUiAction::CollectAll: Hud->ShowToast(Estate->CollectAllProduction()); break;
	case ECozyUiAction::ToggleDebug: Hud->ToggleDebugPanel(); break;
	case ECozyUiAction::CloseWindow: Hud->CloseWindow(); break;
	case ECozyUiAction::OpenUpgrade:
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility.DefinitionId);
			if (Def && Def->Functions.Contains(ECozyFacilityFunction::UpgradeQueue) && !Facility.bStored)
			{
				Hud->OpenWindow(ECozyWindowKind::Upgrade, Facility.InstanceId);
				break;
			}
		}
		break;
	default: break;
	}
}

#undef LOCTEXT_NAMESPACE
