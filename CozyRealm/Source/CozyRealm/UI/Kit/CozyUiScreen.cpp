#include "UI/Kit/CozyUiScreen.h"
#include "UI/Kit/CozyUiTheme.h"
#include "UI/Kit/CozyUiScreenConfig.h"
#include "UI/Kit/CozyUiWidgets.h"
#include "UI/CozyHudWidget.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/ComboBoxString.h"
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
	if (bRefreshPending)
	{
		bRefreshPending = false;
		BindAssetEvents();
		RefreshTheme();
		RefreshValues();
	}
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
	// 플레이 중에는 다음 프레임에 적용한다.
	// 에셋 편집은 되돌리기 기록(트랜잭션) 안에서 알려 오는데, 그 안에서 플레이용 위젯을 만들면
	// 기록이 플레이 위젯을 붙잡아 플레이 종료 때 에디터가 멈춘다 (2026-10-08 확인).
	if (!IsDesignTime())
	{
		if (!bRefreshPending)
		{
			UE_LOG(LogCozyRealm, Log, TEXT("[UI] 설정 변경 → 다음 프레임에 반영: %s → %s"), Asset ? *Asset->GetName() : TEXT("?"), *GetClass()->GetName());
		}
		bRefreshPending = true;
		return;
	}
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

const FCozyUiElementEntry* UCozyUiScreen::FindElement(FName ElementId) const
{
	return Config ? Config->FindElement(ElementId) : nullptr;
}

const FCozyUiAreaLayout* UCozyUiScreen::FindArea(FName AreaId) const
{
	return Config ? Config->FindArea(AreaId) : nullptr;
}

TArray<FCozyUiElementEntry> UCozyUiScreen::GetElementsForArea(FName AreaId) const
{
	TArray<FCozyUiElementEntry> Result;
	if (Config && !AreaId.IsNone())
	{
		for (const FCozyUiElementEntry& Each : Config->Elements)
		{
			if (Each.Area == AreaId)
			{
				Result.Add(Each);
			}
		}
		Result.StableSort([](const FCozyUiElementEntry& A, const FCozyUiElementEntry& B) { return A.Order < B.Order; });
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
	// 영역이 요소를 새로 만들 수 있으므로 영역을 먼저, 그다음 전체
	ForEachElement([this](ICozyUiElement& Element)
	{
		if (Cast<UCozyUiArea>(&Element))
		{
			Element.ApplyCozyTheme(*this);
		}
	});
	ForEachElement([this](ICozyUiElement& Element)
	{
		if (!Cast<UCozyUiArea>(&Element))
		{
			Element.ApplyCozyTheme(*this);
		}
	});
	EnforcePassThrough();
}

void UCozyUiScreen::EnforcePassThrough() const
{
	// 디자이너에서 실수로 '보임(Visible)'으로 둔 배경·글자도 플레이에서는 통과시킨다.
	// 입력이 필요한 위젯(버튼·체크·슬라이더·글 입력·스크롤·목록 상자)만 그대로 둔다.
	TFunction<void(const UUserWidget*)> Walk;
	Walk = [&Walk](const UUserWidget* Owner)
	{
		if (!Owner || !Owner->WidgetTree)
		{
			return;
		}
		Owner->WidgetTree->ForEachWidget([&Walk](UWidget* Widget)
		{
			const bool bInteractive = Widget->IsA<UButton>() || Widget->IsA<UCheckBox>() || Widget->IsA<USlider>()
				|| Widget->IsA<UEditableText>() || Widget->IsA<UEditableTextBox>() || Widget->IsA<UScrollBox>() || Widget->IsA<UComboBoxString>();
			if (!bInteractive && Widget->GetVisibility() == ESlateVisibility::Visible)
			{
				Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			if (const UUserWidget* Nested = Cast<UUserWidget>(Widget))
			{
				Walk(Nested);
			}
		});
	};
	Walk(this);
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
	case ECozyUiValue::StorageSpace:
		R.Text = PreviewState == ECozyUiPreviewState::Full ? LOCTEXT("PvSpaceFull", "가득 참 — 받을 수 있는 공간이 없습니다") : LOCTEXT("PvSpace", "더 받을 수 있음 38개");
		R.StateColor = PreviewState == ECozyUiPreviewState::Full ? ECozyUiColor::Warning : ECozyUiColor::InkMuted;
		break;
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
	// 매개변수: @Window = 이 창의 시설 · 시설 고유 ID 글자 · 시설 정의 ID(같은 시설이 여럿이면 첫 번째)
	auto FindFirst = [this, Estate](FName Param) -> const FCozyFacilityState*
	{
		if (Param == TEXT("@Window") || Param.IsNone())
		{
			return ContextFacility.IsValid() ? Estate->FindFacility(ContextFacility) : nullptr;
		}
		FGuid Id;
		if (FGuid::Parse(Param.ToString(), Id))
		{
			return Estate->FindFacility(Id);
		}
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			if (Facility.DefinitionId == Param && !Facility.bStored)
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
	case ECozyUiValue::StorageSpace:
	{
		const FCozyItemRow* Item = Estate->GetItemDef(Param);
		if (Item && Item->Category == ECozyItemCategory::Material)
		{
			const int32 Space = Estate->GetStorageSpace(Param);
			R.Current = Space;
			R.Max = Estate->GetConfig().StorageCapPerItem;
			R.Text = Space > 0 ? FText::Format(LOCTEXT("SpaceLeft", "더 받을 수 있음 {0}개"), FText::AsNumber(Space)) : LOCTEXT("SpaceFull", "가득 참 — 받을 수 있는 공간이 없습니다");
			R.StateColor = Space > 0 ? ECozyUiColor::InkMuted : ECozyUiColor::Warning;
		}
		else
		{
			R.Text = LOCTEXT("SpaceCurrency", "재화 · 한도 없음");
			R.StateColor = ECozyUiColor::InkMuted;
		}
		break;
	}
	case ECozyUiValue::FacilityName:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility->DefinitionId);
			R.Current = Facility->Level;
			// 관리 시설의 효과를 받는 시설(밭)은 개별 레벨이 없음 (D39)
			R.Text = (Def && Def->ManagerFacilityId.IsNone())
				? FText::Format(LOCTEXT("FacNameLv", "{0}  Lv.{1}"), Estate->GetFacilityDisplayName(Facility->InstanceId), FText::AsNumber(Facility->Level))
				: Estate->GetFacilityDisplayName(Facility->InstanceId);
		}
		break;
	case ECozyUiValue::Residents:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility->DefinitionId);
			FString Names;
			for (const FGuid& ResidentId : Facility->AssignedResidents)
			{
				Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Estate->GetResidentDisplayName(ResidentId).ToString();
			}
			R.Current = Facility->AssignedResidents.Num();
			R.Max = Def ? Def->MaxResidents : 0;
			R.Text = Names.IsEmpty() ? LOCTEXT("NoResidents", "없음") : FText::FromString(Names);
			R.StateColor = (Def && Facility->AssignedResidents.Num() < FMath::Max(1, Def->MinResidents)) ? ECozyUiColor::Warning : ECozyUiColor::Ink;
		}
		break;
	case ECozyUiValue::UnclaimedStorage:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const FCozyUnclaimedView View = Estate->GetUnclaimedView(Facility->InstanceId);
			R.Current = View.StoredAmount;
			R.Max = View.StorageCap;
			R.Text = View.Amount <= 0 ? LOCTEXT("NothingToCollect", "지금 수령할 것이 없습니다")
				: View.CollectableNow > 0 ? FText::Format(LOCTEXT("CollectableNow", "창고 {0} {1}/{2} · 지금 수령 가능 {3}개"), View.ItemName, FText::AsNumber(View.StoredAmount), FText::AsNumber(View.StorageCap), FText::AsNumber(View.CollectableNow))
				: FText::Format(LOCTEXT("StorageNoRoom", "창고에 {0}|hpp(을,를) 받을 공간이 없습니다 ({1}/{2})"), View.ItemName, FText::AsNumber(View.StoredAmount), FText::AsNumber(View.StorageCap));
			R.StateColor = (View.Amount > 0 && View.CollectableNow <= 0) ? ECozyUiColor::Warning : ECozyUiColor::InkMuted;
		}
		break;
	case ECozyUiValue::GrowthEffect:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const FCozyProductionView View = Estate->GetProductionView(Facility->InstanceId);
			FNumberFormattingOptions Fmt;
			Fmt.MinimumFractionalDigits = 1;
			Fmt.MaximumFractionalDigits = 2;
			R.Current = View.SpeedMultiplier;
			R.Text = !View.bHasGrowthSource ? FText::GetEmpty()
				: View.GrowthSourceLevel > 0
					? FText::Format(LOCTEXT("GrowthLine", "공통 관리 효과: 생산 속도 ×{0} ({1} Lv{2}) · 단계가 오르면 다음 주기부터 적용"), FText::AsNumber(View.SpeedMultiplier, &Fmt), View.GrowthSourceName, FText::AsNumber(View.GrowthSourceLevel))
					: FText::Format(LOCTEXT("GrowthLineNone", "공통 관리 효과: 기본 속도 ×{0} ({1}|hpp(이,가) 아직 없습니다)"), FText::AsNumber(View.SpeedMultiplier, &Fmt), View.GrowthSourceName);
		}
		break;
	case ECozyUiValue::CurrentCrop:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const FCozyCropRow* Crop = Estate->GetCropDef(Facility->SelectedCropId);
			R.Text = Crop ? Crop->DisplayName : LOCTEXT("NoCrop", "없음");
		}
		break;
	case ECozyUiValue::CropOption:
	{
		const FCozyFacilityState* Facility = ContextFacility.IsValid() ? Estate->FindFacility(ContextFacility) : nullptr;
		const FCozyCropRow* Crop = Estate->GetCropDef(Param);
		const FText Name = Crop ? Crop->DisplayName : FText::FromName(Param);
		const bool bCurrent = Facility && Facility->SelectedCropId == Param;
		const bool bUnlocked = Estate->IsCropUnlocked(Param);
		R.Text = bCurrent ? FText::Format(LOCTEXT("CropCurrent", "{0} (키우는 중)"), Name) : bUnlocked ? Name : FText::Format(LOCTEXT("CropLocked", "{0} (잠김)"), Name);
		R.StateColor = bCurrent ? ECozyUiColor::Ok : bUnlocked ? ECozyUiColor::Ink : ECozyUiColor::Locked;
		break;
	}
	case ECozyUiValue::FieldManagement:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			const FCozyFieldManagementView View = Estate->GetFieldManagementView(Facility->InstanceId);
			auto Join = [](const TArray<FText>& Items)
			{
				FString Out;
				for (const FText& Item : Items)
				{
					Out += (Out.IsEmpty() ? TEXT("") : TEXT(", ")) + Item.ToString();
				}
				return Out.IsEmpty() ? FString(TEXT("없음")) : Out;
			};
			FNumberFormattingOptions Fmt;
			Fmt.MinimumFractionalDigits = 1;
			Fmt.MaximumFractionalDigits = 2;
			FString Text = FString::Printf(TEXT("관리 단계 Lv%d%s\n모든 밭 생산 속도 ×%s (관리하는 밭 %d개 · 새로 지은 밭도 같은 효과)\n고를 수 있는 작물: %s"),
				View.Level, View.bUpgrading ? TEXT(" (업그레이드 중 · 밭은 지금 효과로 계속 생산)") : TEXT(""),
				*FText::AsNumber(View.CurrentMultiplier, &Fmt).ToString(), View.ManagedFacilities, *Join(View.UnlockedCrops));
			Text += View.NextMultiplier > 0.f
				? FString::Printf(TEXT("\n다음 단계: 속도 ×%s · 해금 작물 %s"), *FText::AsNumber(View.NextMultiplier, &Fmt).ToString(), *Join(View.NextUnlockCrops))
				: FString(TEXT("\n다음 단계: 아직 없음"));
			R.Current = View.Level;
			R.Text = FText::FromString(Text);
		}
		break;
	case ECozyUiValue::OfflineSummary:
		if (const FCozyOfflineReport* Report = Estate->GetPendingOfflineReport())
		{
			const int32 Minutes = FMath::FloorToInt(Report->AwaySeconds / 60.0);
			R.Current = Report->AwaySeconds;
			R.Text = FText::Format(Report->bClamped ? LOCTEXT("OfflineAwayClamped", "자리를 비운 동안: {0}분 · 최대 {1}분까지만 정산") : LOCTEXT("OfflineAway2", "자리를 비운 동안: {0}분"),
				FText::AsNumber(Minutes), FText::AsNumber(FMath::FloorToInt(Report->AppliedSeconds / 60.0)));
			if (Report->Lines.Num() == 0)
			{
				R.Text = FText::Format(LOCTEXT("OfflineNothing2", "{0}\n그동안 쌓인 것이 없습니다 (주민 배치·미수령 공간을 확인해 주세요)"), R.Text);
			}
		}
		else
		{
			R.Text = LOCTEXT("OfflineNone2", "새로 받은 방치 보상이 없습니다");
		}
		break;
	case ECozyUiValue::ResidentPlacement:
	{
		FGuid ResidentId;
		FGuid::Parse(Param.ToString(), ResidentId);
		const FCozyResidentState* Resident = Estate->GetState().Residents.FindByPredicate([&ResidentId](const FCozyResidentState& Each) { return Each.InstanceId == ResidentId; });
		R.Text = (Resident && Resident->AssignedFacility.IsValid())
			? FText::Format(LOCTEXT("AssignedAt2", "배치: {0}"), Estate->GetFacilityDisplayName(Resident->AssignedFacility))
			: LOCTEXT("Unassigned2", "미배치 (나가야)");
		R.StateColor = (Resident && Resident->AssignedFacility.IsValid()) ? ECozyUiColor::InkMuted : ECozyUiColor::Point;
		break;
	}
	case ECozyUiValue::WindowTarget:
		R.Text = ContextTarget.IsValid() ? FText::Format(LOCTEXT("TargetLine2", "배치할 시설: {0}"), Estate->GetFacilityDisplayName(ContextTarget)) : FText::GetEmpty();
		break;
	case ECozyUiValue::Feedback:
	{
		const ACozyRealmEstatePlayerController* Controller = Cast<ACozyRealmEstatePlayerController>(GetOwningPlayer());
		const FText Last = (Controller && Controller->GetHud()) ? Controller->GetHud()->GetLastFeedback() : FText::GetEmpty();
		R.Text = Last.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("JustDid2", "방금 한 일 — {0}"), Last);
		break;
	}
	default:
		R.bValid = false;
		break;
	}
	return R;
}

TArray<FCozyUiListRow> UCozyUiScreen::GetListRows(ECozyUiListSource Source, FName ParentRowId) const
{
	TArray<FCozyUiListRow> Rows;
	if (Source == ECozyUiListSource::None)
	{
		return Rows;
	}
	const UWorld* World = GetWorld();
	const UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	if (!Estate || IsDesignTime())
	{
		// 디자이너 미리보기용 예시 줄
		Rows.Add({ TEXT("Wheat"), LOCTEXT("SampleWheat", "밀 (예시)") });
		Rows.Add({ TEXT("Flour"), LOCTEXT("SampleFlour", "밀가루 (예시)") });
		Rows.Add({ TEXT("Gold"), LOCTEXT("SampleGold", "골드 (예시)") });
		return Rows;
	}
	auto AddItems = [&Rows, Estate](const TArray<FName>& Ids, TOptional<ECozyItemCategory> Only)
	{
		for (const FName& Id : Ids)
		{
			const FCozyItemRow* Item = Estate->GetItemDef(Id);
			if (Item && (!Only.IsSet() || Item->Category == Only.GetValue()))
			{
				Rows.Add({ Id, Item->DisplayName });
			}
		}
	};
	switch (Source)
	{
	case ECozyUiListSource::StorageItems: AddItems(Estate->GetStorageItems(), {}); break;
	case ECozyUiListSource::StorageMaterials: AddItems(Estate->GetStorageItems(), ECozyItemCategory::Material); break;
	case ECozyUiListSource::StorageCurrencies: AddItems(Estate->GetStorageItems(), ECozyItemCategory::Currency); break;
	case ECozyUiListSource::SaleItems: AddItems(Estate->GetSaleListItems(), {}); break;
	case ECozyUiListSource::OfflineReportLines:
		if (const FCozyOfflineReport* Report = Estate->GetPendingOfflineReport())
		{
			for (int32 Index = 0; Index < Report->Lines.Num(); ++Index)
			{
				Rows.Add({ FName(*FString::Printf(TEXT("Line%d"), Index)), Report->Lines[Index] });
			}
		}
		break;
	case ECozyUiListSource::Residents:
		for (const FCozyResidentState& Resident : Estate->GetState().Residents)
		{
			Rows.Add({ FName(*Resident.InstanceId.ToString()), Estate->GetResidentDisplayName(Resident.InstanceId) });
		}
		break;
	case ECozyUiListSource::WindowCrops:
		if (const FCozyFacilityState* Facility = ContextFacility.IsValid() ? Estate->FindFacility(ContextFacility) : nullptr)
		{
			const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility->DefinitionId);
			if (Def && Def->bProductionItemSelectable)
			{
				for (const FName& CropId : Def->ProductionItems)
				{
					const FCozyCropRow* Crop = Estate->GetCropDef(CropId);
					Rows.Add({ CropId, Crop ? Crop->DisplayName : FText::FromName(CropId) });
				}
			}
		}
		break;
	case ECozyUiListSource::AssignTargets:
	{
		// 부모 줄 = 주민 · 바로가기로 연 나가야면 그 시설만, 아니면 주민을 받을 수 있는 모든 시설 (지금 있는 곳 제외)
		FGuid ResidentId;
		FGuid::Parse(ParentRowId.ToString(), ResidentId);
		const FCozyResidentState* Resident = Estate->GetState().Residents.FindByPredicate([&ResidentId](const FCozyResidentState& Each) { return Each.InstanceId == ResidentId; });
		if (!Resident)
		{
			break;
		}
		const bool bAssigned = Resident->AssignedFacility.IsValid();
		for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
		{
			const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility.DefinitionId);
			if (!Def || Def->MaxResidents <= 0 || Facility.bStored || Facility.InstanceId == Resident->AssignedFacility
				|| (ContextTarget.IsValid() && Facility.InstanceId != ContextTarget))
			{
				continue;
			}
			const FText FacilityName = Estate->GetFacilityDisplayName(Facility.InstanceId);
			const FText Label = bAssigned
				? (ContextTarget.IsValid() ? LOCTEXT("MoveHere2", "이 시설로 옮기기") : FText::Format(LOCTEXT("MoveTo2", "{0}|hpp(으로,로) 옮기기"), FacilityName))
				: (ContextTarget.IsValid() ? LOCTEXT("AssignHere2", "이 시설에 배치") : FText::Format(LOCTEXT("AssignTo2", "{0}에 배치"), FacilityName));
			Rows.Add({ FName(*FString::Printf(TEXT("%s|%s"), *ResidentId.ToString(), *Facility.InstanceId.ToString())), Label });
		}
		break;
	}
	case ECozyUiListSource::StoredFacilities:
		for (const FGuid& Id : Estate->GetStoredFacilities())
		{
			const FCozyFacilityState* Facility = Estate->FindFacility(Id);
			const FCozyFacilityRow* Def = Facility ? Estate->GetFacilityDef(Facility->DefinitionId) : nullptr;
			Rows.Add({ FName(*Id.ToString()), Def ? Def->DisplayName : FText::FromString(Id.ToString()) });
		}
		break;
	default: break;
	}
	return Rows;
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
	case ECozyUiAction::CollectWindow: return LOCTEXT("ActCollectWindow", "이 시설의 미수령분 수령");
	case ECozyUiAction::OpenNagayaForWindow: return LOCTEXT("ActNagayaWindow", "이 시설에 주민 배치");
	case ECozyUiAction::ConfirmOfflineReport: return LOCTEXT("ActOfflineOk", "방치 보상 확인");
	case ECozyUiAction::SelectCrop: return LOCTEXT("ActSelectCrop", "이 작물로 바꾸기");
	case ECozyUiAction::AssignResident: return LOCTEXT("ActAssign", "주민 배치");
	case ECozyUiAction::UnassignResident: return LOCTEXT("ActUnassign", "주민 배치 해제");
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

bool UCozyUiScreen::CanRunAction(const FCozyUiElementEntry& Entry, FText& OutReason) const
{
	const UWorld* World = GetWorld();
	const UCozyEstateSubsystem* Estate = World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	if (!Estate || bPreview || IsDesignTime())
	{
		return true;
	}
	switch (Entry.Action)
	{
	case ECozyUiAction::CollectWindow:
	{
		const FCozyUnclaimedView View = Estate->GetUnclaimedView(ContextFacility);
		if (View.Amount <= 0)
		{
			OutReason = LOCTEXT("CanNotCollectNone", "수령할 것이 없습니다");
			return false;
		}
		return true;
	}
	case ECozyUiAction::SelectCrop:
		return Estate->CanSelectCrop(ContextFacility, Entry.ActionParam, OutReason);
	case ECozyUiAction::AssignResident:
	{
		FString ResidentText, FacilityText;
		FGuid FacilityId;
		if (!Entry.ActionParam.ToString().Split(TEXT("|"), &ResidentText, &FacilityText) || !FGuid::Parse(FacilityText, FacilityId))
		{
			return false;
		}
		return Estate->CanAcceptResident(FacilityId, OutReason);
	}
	case ECozyUiAction::UnassignResident:
	{
		FGuid ResidentId;
		FGuid::Parse(Entry.ActionParam.ToString(), ResidentId);
		const FCozyResidentState* Resident = Estate->GetState().Residents.FindByPredicate([&ResidentId](const FCozyResidentState& Each) { return Each.InstanceId == ResidentId; });
		if (!Resident || !Resident->AssignedFacility.IsValid())
		{
			OutReason = LOCTEXT("NotAssigned", "배치되어 있지 않습니다");
			return false;
		}
		return true;
	}
	default:
		return true;
	}
}

void UCozyUiScreen::RunAction(const FCozyUiElementEntry& Entry)
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
	case ECozyUiAction::ConfirmOfflineReport: Estate->DismissOfflineReport(); Hud->CloseWindow(); break;
	case ECozyUiAction::CollectWindow: Hud->CollectFromWindow(ContextFacility); break;
	case ECozyUiAction::OpenNagayaForWindow: Hud->OpenWindow(ECozyWindowKind::Nagaya, FGuid(), ContextFacility); break;
	case ECozyUiAction::SelectCrop:
	{
		FText Message;
		Estate->SelectCrop(ContextFacility, Entry.ActionParam, Message);
		Hud->SetFeedbackText(Message);
		break;
	}
	case ECozyUiAction::AssignResident:
	{
		FString ResidentText, FacilityText;
		FGuid ResidentId, FacilityId;
		if (Entry.ActionParam.ToString().Split(TEXT("|"), &ResidentText, &FacilityText) && FGuid::Parse(ResidentText, ResidentId) && FGuid::Parse(FacilityText, FacilityId))
		{
			FText Reason;
			Hud->SetFeedbackText(Estate->AssignResident(ResidentId, FacilityId, Reason) ? FText::GetEmpty() : Reason);
		}
		break;
	}
	case ECozyUiAction::UnassignResident:
	{
		FGuid ResidentId;
		if (FGuid::Parse(Entry.ActionParam.ToString(), ResidentId))
		{
			FText Reason;
			Hud->SetFeedbackText(Estate->UnassignResident(ResidentId, Reason) ? FText::GetEmpty() : Reason);
		}
		break;
	}
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
