#include "UI/Kit/CozyUiScreen.h"
#include "UI/Kit/CozyUiTheme.h"
#include "UI/Kit/CozyUiScreenConfig.h"
#include "UI/Kit/CozyUiWidgets.h"
#include "UI/CozyHudWidget.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Settings/CozySettingsSubsystem.h"
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

namespace CozyUiScreenUtil
{
	UCozyHudWidget* FindHud(const UUserWidget* Widget)
	{
		const ACozyRealmEstatePlayerController* Controller = Widget ? Cast<ACozyRealmEstatePlayerController>(Widget->GetOwningPlayer()) : nullptr;
		return Controller ? Controller->GetHud() : nullptr;
	}

	/** 줄 ID·동작 매개변수(주민 고유 ID 글자)로 주민 찾기 */
	const FCozyResidentState* FindResident(const UCozyEstateSubsystem* Estate, FName Param)
	{
		FGuid ResidentId;
		FGuid::Parse(Param.ToString(), ResidentId);
		return Estate->GetState().Residents.FindByPredicate([&ResidentId](const FCozyResidentState& Each) { return Each.InstanceId == ResidentId; });
	}

	/** 재료 표시 이름 (정의가 없으면 ID 그대로) */
	FText ItemName(const UCozyEstateSubsystem* Estate, FName ItemId)
	{
		const FCozyItemRow* Item = Estate->GetItemDef(ItemId);
		return Item ? Item->DisplayName : FText::FromName(ItemId);
	}

	/** 글자 목록을 구분자로 잇기 (앞에 이은 글자가 아직 없으면 구분자 없이) */
	FString JoinTexts(const TArray<FText>& Items, const TCHAR* Separator)
	{
		FString Out;
		for (const FText& Item : Items)
		{
			Out += (Out.IsEmpty() ? TEXT("") : Separator) + Item.ToString();
		}
		return Out;
	}

	/** 가공 칸에 놓인 작업 (진행·일시 정지 · 대기 작업 제외) */
	TArray<FCozyProcessingJobView> ActiveJobs(const UCozyEstateSubsystem* Estate, const FGuid& FacilityId)
	{
		TArray<FCozyProcessingJobView> Jobs = Estate->GetProcessingJobs(FacilityId);
		Jobs.RemoveAll([](const FCozyProcessingJobView& Each) { return Each.bQueued; });
		return Jobs;
	}

	/** 배율 글자 (1.4) */
	FText Multiplier(float Value)
	{
		FNumberFormattingOptions Fmt;
		Fmt.MinimumFractionalDigits = 1;
		Fmt.MaximumFractionalDigits = 2;
		return FText::AsNumber(Value, &Fmt);
	}

	/** 'Slot0' → 0 · 형식이 다르면 -1 */
	int32 SlotIndex(FName Param)
	{
		const FString Text = Param.ToString();
		return Text.StartsWith(TEXT("Slot")) ? FCString::Atoi(*Text.Mid(4)) : -1;
	}
}

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
	RefreshAll();
}

void UCozyUiScreen::NativeDestruct()
{
	UnbindAssetEvents();
	Super::NativeDestruct();
}

void UCozyUiScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 설정 창의 탭 · 확인 창이 바뀌면 줄 구성을 다시 만든다 (버튼 밖에서 바뀐 경우 · 예: 15초가 지나 이전 화면 설정으로)
	if (const UCozySettingsSubsystem* Settings = IsDesignTime() ? nullptr : UCozySettingsSubsystem::Get(this))
	{
		if (Settings->GetRevision() != SeenSettingsRevision)
		{
			SeenSettingsRevision = Settings->GetRevision();
			bRefreshPending = true;
		}
	}
	if (bRefreshPending)
	{
		bRefreshPending = false;
		RefreshAll();
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

void UCozyUiScreen::RefreshAll()
{
	BindAssetEvents();
	RefreshTheme();
	RefreshValues();
}

UCozyEstateSubsystem* UCozyUiScreen::GetEstate() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
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
	RefreshAll();
	UE_LOG(LogCozyRealm, Log, TEXT("[UI] 설정 변경 반영: %s → %s"), Asset ? *Asset->GetName() : TEXT("?"), *GetClass()->GetName());
}

void UCozyUiScreen::SetPreview(bool bEnable)
{
	bPreview = bEnable;
	RefreshTheme();
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

const FSlateBrush* UCozyUiScreen::FindImage(FName ImageKey) const
{
	const UCozyUiTheme* Theme = GetTheme();
	if (!Theme) { return nullptr; }
	const FString Key = ImageKey.ToString();
	if (Key.StartsWith(TEXT("Facility.")))
	{
		FGuid FacilityId;
		const UCozyEstateSubsystem* Estate = GetEstate();
		if (Estate && FGuid::Parse(Key.Mid(9), FacilityId))
		{
			if (const FCozyFacilityState* Facility = Estate->FindFacility(FacilityId))
			{
				ImageKey = FName(*(TEXT("Facility.") + Facility->DefinitionId.ToString()));
			}
		}
	}
	return Theme->FindImage(ImageKey);
}

const FCozyUiElementEntry* UCozyUiScreen::FindElement(FName ElementId) const
{
	return Config ? Config->FindElement(ElementId) : nullptr;
}

const FCozyUiAreaLayout* UCozyUiScreen::FindArea(FName AreaId) const
{
	return Config ? Config->FindArea(AreaId) : nullptr;
}

void UCozyUiScreen::SetCompact(bool bEnable)
{
	if (bCompact != bEnable)
	{
		bCompact = bEnable;
		bRefreshPending = true;
	}
}

bool UCozyUiScreen::IsElementShown(FName ElementId) const
{
	if (!Config || (Config->CompactElements.Num() == 0 && Config->CompactOnlyElements.Num() == 0))
	{
		return true;
	}
	const bool bCompactOnly = Config->CompactOnlyElements.Contains(ElementId);
	if (!bCompact)
	{
		return !bCompactOnly;
	}
	// 작은 창: 정한 요소와 그 안쪽 요소만 (목록에 없는 요소는 숨김)
	if (bCompactOnly || Config->CompactElements.Contains(ElementId))
	{
		return true;
	}
	const FCozyUiElementEntry* Entry = Config->FindElement(ElementId);
	if (!Entry || Entry->Area.IsNone())
	{
		return false;
	}
	// 안쪽 영역의 요소: 그 영역을 담은 요소가 보이면 보임
	for (const FCozyUiElementEntry& Parent : Config->Elements)
	{
		if (Parent.ChildArea == Entry->Area && Parent.Id != ElementId && (Config->CompactElements.Contains(Parent.Id) || Config->CompactOnlyElements.Contains(Parent.Id)))
		{
			return true;
		}
	}
	return false;
}

TArray<FCozyUiElementEntry> UCozyUiScreen::GetElementsForArea(FName AreaId) const
{
	TArray<FCozyUiElementEntry> Result;
	if (Config && !AreaId.IsNone())
	{
		for (const FCozyUiElementEntry& Each : Config->Elements)
		{
			if (Each.Area == AreaId && IsElementShown(Each.Id))
			{
				// 목록과 상세 영역은 동시에 보여 주지 않는다. 편집 에셋의 배치는 그대로 유지한다.
				const bool bDetail = SelectedUpgradeFacility.IsValid();
				if ((Each.ListSource == ECozyUiListSource::UpgradeFacilities && Each.ValueParam == TEXT("Overview") && bDetail)
					|| (Each.ListSource == ECozyUiListSource::UpgradeFacilities && Each.ValueParam == TEXT("Detail") && !bDetail)
					|| (Each.Action == ECozyUiAction::BackUpgradeList && !bDetail))
				{
					continue;
				}
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
	// 버튼·제목에 쓰는 값은 짧은 예시 글자 (긴 '(미리보기) …' 글자가 버튼 밖으로 넘치지 않게)
	case ECozyUiValue::ProcStartLabel: R.Text = LOCTEXT("PvStartLabel", "제작 시작"); break;
	case ECozyUiValue::RecipeOption: R.Text = LOCTEXT("PvRecipe", "밀가루 ×1"); break;
	case ECozyUiValue::CropOption: R.Text = LOCTEXT("PvCrop", "밀"); break;
	case ECozyUiValue::SpeedCount: R.Text = LOCTEXT("PvSpeedCount", "1장 (보유 3장)"); break;
	case ECozyUiValue::StoredLabel: R.Text = LOCTEXT("PvStored", "제분소 Lv2"); break;
	case ECozyUiValue::DecorScore: R.Current = 62.f; R.Max = 100.f; R.Text = LOCTEXT("PvDecorScore", "꾸미기 점수 62점 · 생산 +5% · 다음 단계까지 38점"); R.StateColor = ECozyUiColor::Point; break;
	case ECozyUiValue::DecorLine: R.Current = 3.f; R.Text = LOCTEXT("PvDecorLine", "1×1 · 3점 · 골드 30 · 보유 1 (놓음 1)"); R.StateColor = ECozyUiColor::InkMuted; break;
	case ECozyUiValue::FacilityName: R.Text = LOCTEXT("PvFacName", "제분소  Lv.2 (예시)"); break;
	case ECozyUiValue::UpgradeSlots: R.Current = PreviewState == ECozyUiPreviewState::Empty ? 0.f : 1.f; R.Max = 1.f; break;
	case ECozyUiValue::StoredFacilities: R.Current = 1.f; R.Max = 0.f; break;
	case ECozyUiValue::StorageSpace:
		R.Text = PreviewState == ECozyUiPreviewState::Full ? LOCTEXT("PvSpaceFull", "가득 참 — 받을 수 있는 공간이 없습니다") : LOCTEXT("PvSpace", "더 받을 수 있음 38개");
		R.StateColor = PreviewState == ECozyUiPreviewState::Full ? ECozyUiColor::Warning : ECozyUiColor::InkMuted;
		break;
	default: break;
	}
	if (PreviewState == ECozyUiPreviewState::Empty)
	{
		return R;
	}
	if (R.Text.IsEmpty())
	{
		// 표시 이름 'Sale Line (판매 재료 한 줄)'의 괄호 안 한글을 꺼내 '(미리보기) 판매 재료 한 줄'로 · 빈 줄이 접히지 않아 배치를 확인할 수 있음
		FString Name = UEnum::GetDisplayValueAsText(Value).ToString();
		int32 Open = INDEX_NONE;
		if (Name.FindChar(TEXT('('), Open) && Name.EndsWith(TEXT(")")))
		{
			Name = Name.Mid(Open + 1, Name.Len() - Open - 2);
		}
		R.Text = FText::Format(LOCTEXT("PvValueName", "(미리보기) {0}"), FText::FromString(Name));
	}
	return R;
}

FCozyUiValueResult UCozyUiScreen::GetValue(ECozyUiValue Value, FName Param) const
{
	if (Value == ECozyUiValue::None)
	{
		return FCozyUiValueResult();
	}
	// 설정 값은 영지 상태와 상관없이 설정 서브시스템이 답한다 (미리보기 중에도 실제 설정 · 디자이너에서는 예시)
	if (UCozySettingsSubsystem::IsSettingsValue(Value))
	{
		if (const UCozySettingsSubsystem* Settings = IsDesignTime() ? nullptr : UCozySettingsSubsystem::Get(this))
		{
			return Settings->GetUiValue(Value, Param);
		}
		FCozyUiValueResult Sample;
		Sample.bValid = true;
		Sample.Current = 80.f;
		Sample.Max = 100.f;
		Sample.Text = LOCTEXT("PvSettings", "설정 (미리보기)");
		Sample.StateColor = ECozyUiColor::Ink;
		return Sample;
	}
	const UCozyEstateSubsystem* Estate = GetEstate();
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
		// 게임 시계 (하루 30분 · 현실 시간에서 계산)
		const int32 DayMinutes = FMath::FloorToInt32(Estate->GetDayHour() * 60.0);
		R.Text = FText::Format(LOCTEXT("Clock", "{0} · {1}:{2}"), Estate->IsNight() ? LOCTEXT("Night", "밤") : LOCTEXT("Day", "낮"),
			FText::FromString(FString::Printf(TEXT("%02d"), DayMinutes / 60)), FText::FromString(FString::Printf(TEXT("%02d"), DayMinutes % 60)));
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
			TArray<FText> ResidentNames;
			for (const FGuid& ResidentId : Facility->AssignedResidents)
			{
				ResidentNames.Add(Estate->GetResidentDisplayName(ResidentId));
			}
			const FString Names = CozyUiScreenUtil::JoinTexts(ResidentNames, TEXT(", "));
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
			R.Current = View.SpeedMultiplier;
			R.Text = !View.bHasGrowthSource ? FText::GetEmpty()
				: View.GrowthSourceLevel > 0
					? FText::Format(LOCTEXT("GrowthLine", "공통 관리 효과: 생산 속도 ×{0} ({1} Lv{2}) · 단계가 오르면 다음 주기부터 적용"), CozyUiScreenUtil::Multiplier(View.SpeedMultiplier), View.GrowthSourceName, FText::AsNumber(View.GrowthSourceLevel))
					: FText::Format(LOCTEXT("GrowthLineNone", "공통 관리 효과: 기본 속도 ×{0} ({1}|hpp(이,가) 아직 없습니다)"), CozyUiScreenUtil::Multiplier(View.SpeedMultiplier), View.GrowthSourceName);
			// 꾸미기 보너스가 있으면 위 배율에 포함되어 있음을 밝힘
			const float DecorBonus = Estate->GetDecorationScoreView().Bonus;
			if (!R.Text.IsEmpty() && DecorBonus > 0.f)
			{
				R.Text = FText::Format(LOCTEXT("GrowthLineDecor", "{0} · 꾸미기 +{1}% 포함"), R.Text, FText::AsNumber(FMath::RoundToInt(DecorBonus * 100.f)));
			}
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
				const FString Out = CozyUiScreenUtil::JoinTexts(Items, TEXT(", "));
				return Out.IsEmpty() ? FString(TEXT("없음")) : Out;
			};
			FString Text = FString::Printf(TEXT("관리 단계 Lv%d%s\n모든 밭 생산 속도 ×%s (관리하는 밭 %d개 · 새로 지은 밭도 같은 효과)\n고를 수 있는 작물: %s"),
				View.Level, View.bUpgrading ? TEXT(" (업그레이드 중 · 밭은 지금 효과로 계속 생산)") : TEXT(""),
				*CozyUiScreenUtil::Multiplier(View.CurrentMultiplier).ToString(), View.ManagedFacilities, *Join(View.UnlockedCrops));
			Text += View.NextMultiplier > 0.f
				? FString::Printf(TEXT("\n다음 단계: 속도 ×%s · 해금 작물 %s"), *CozyUiScreenUtil::Multiplier(View.NextMultiplier).ToString(), *Join(View.NextUnlockCrops))
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
		const FCozyResidentState* Resident = CozyUiScreenUtil::FindResident(Estate, Param);
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
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		const FText Last = Hud ? Hud->GetLastFeedback() : FText::GetEmpty();
		R.Text = Last.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("JustDid2", "방금 한 일 — {0}"), Last);
		break;
	}
	case ECozyUiValue::SaleLine:
	{
		const FCozyItemRow* Item = Estate->GetItemDef(Param);
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		const bool bSelected = Hud && Hud->GetSellItem() == Param;
		R.Current = Estate->GetAmount(Param);
		if (Item && Item->SellPrice > 0)
		{
			R.Text = FText::Format(LOCTEXT("SaleLine", "창고 {0}개 · 1개 {1}골드{2}"), FText::AsNumber(R.Current), FText::AsNumber(Item->SellPrice),
				bSelected ? LOCTEXT("SaleLineSel", "  ◀ 선택") : FText::GetEmpty());
			R.StateColor = bSelected ? ECozyUiColor::Point : ECozyUiColor::InkMuted;
		}
		else
		{
			R.Text = FText::Format(LOCTEXT("SaleLineNo", "창고 {0}개 · 판매 불가"), FText::AsNumber(R.Current));
			R.StateColor = ECozyUiColor::Locked;
		}
		break;
	}
	case ECozyUiValue::SaleSelection:
	case ECozyUiValue::SaleAmount:
	case ECozyUiValue::SaleSummary:
	case ECozyUiValue::SaleBlock:
	{
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		if (!Hud)
		{
			break;
		}
		const FName ItemId = Hud->GetSellItem();
		const int32 Amount = Hud->GetSellAmount();
		const FCozySellQuote Quote = Estate->GetSellQuote(ContextFacility, ItemId, Amount);
		if (Value == ECozyUiValue::SaleSelection)
		{
			R.Text = ItemId.IsNone()
				? LOCTEXT("SaleNothing", "팔 수 있는 재료가 없습니다")
				: FText::Format(LOCTEXT("SaleSel", "선택: {0} · 1개 {1}{2}"), Quote.ItemName, FText::AsNumber(Quote.UnitPrice), Quote.CurrencyName);
		}
		else if (Value == ECozyUiValue::SaleAmount)
		{
			R.Current = Amount;
			R.Max = Quote.MaxAmount;
		}
		else if (Value == ECozyUiValue::SaleSummary)
		{
			R.Current = Quote.TotalPrice;
			R.Text = ItemId.IsNone() ? FText::GetEmpty() : FText::Format(LOCTEXT("SaleSum", "{0} {1}개 × {2} = {3} {4}  (창고 보유 {5}개 · 판매 후 {6}개)"),
				Quote.ItemName, FText::AsNumber(Amount), FText::AsNumber(Quote.UnitPrice), FText::AsNumber(Quote.TotalPrice), Quote.CurrencyName,
				FText::AsNumber(Quote.MaxAmount), FText::AsNumber(FMath::Max(0, Quote.MaxAmount - Amount)));
		}
		else
		{
			R.Text = Quote.BlockReason;
			R.StateColor = ECozyUiColor::Warning;
		}
		break;
	}
	case ECozyUiValue::RecipeOption:
		if (const FCozyRecipeRow* Recipe = Estate->GetRecipeDef(Param))
		{
			const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
			R.Text = FText::Format(Recipe->bTestOnly ? LOCTEXT("RecipeOptTest", "{0} ×{1} (테스트)") : LOCTEXT("RecipeOpt", "{0} ×{1}"),
				CozyUiScreenUtil::ItemName(Estate, Recipe->OutputItem), FText::AsNumber(Recipe->OutputAmount));
			R.StateColor = (Hud && Hud->GetProcRecipe() == Param) ? ECozyUiColor::Point : ECozyUiColor::Ink;
		}
		break;
	case ECozyUiValue::ProcSelection:
	case ECozyUiValue::ProcRuns:
	case ECozyUiValue::ProcMaxInfo:
	case ECozyUiValue::ProcSummary:
	case ECozyUiValue::ProcBlock:
	case ECozyUiValue::ProcStorageNote:
	case ECozyUiValue::ProcStartLabel:
	{
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		if (!Hud)
		{
			break;
		}
		const FName RecipeId = Hud->GetProcRecipe();
		const int32 Runs = Hud->GetProcRuns();
		const FCozyRecipeQuote Quote = Estate->GetRecipeQuote(ContextFacility, RecipeId, Runs);
		const FCozyRecipeRow* Recipe = Estate->GetRecipeDef(RecipeId);
		switch (Value)
		{
		case ECozyUiValue::ProcSelection:
			if (Recipe)
			{
				FString InputsText;
				for (const TPair<FName, int32>& Input : Recipe->Inputs)
				{
					InputsText += FString::Printf(TEXT("%s%s %d"), InputsText.IsEmpty() ? TEXT("") : TEXT(" + "), *CozyUiScreenUtil::ItemName(Estate, Input.Key).ToString(), Input.Value);
				}
				R.Text = FText::Format(LOCTEXT("ProcSel", "선택: {0} → {1} {2}개 · 1회 {3}"),
					FText::FromString(InputsText), Quote.OutputName, FText::AsNumber(Quote.OutputPerRun), CozyUiFormat::Duration(Quote.SecondsPerRun));
			}
			else
			{
				R.Text = LOCTEXT("ProcNoRecipe2", "이 시설에서 만들 수 있는 레시피가 없습니다");
			}
			break;
		case ECozyUiValue::ProcRuns:
			R.Current = Runs;
			R.Max = Quote.MaxRuns;
			R.Text = Quote.bAppend
				? FText::Format(LOCTEXT("ProcRunsAppend2", "추가 {0}회  →  {1} {2}개   (현재 남은 {3}회 뒤에 이어서)"), FText::AsNumber(Runs), Quote.OutputName, FText::AsNumber(Quote.TotalOutput), FText::AsNumber(Quote.CurrentRemainingRuns))
				: FText::Format(LOCTEXT("ProcRuns2", "{0}회  →  {1} {2}개"), FText::AsNumber(Runs), Quote.OutputName, FText::AsNumber(Quote.TotalOutput));
			break;
		case ECozyUiValue::ProcMaxInfo:
			R.Current = Param == TEXT("Materials") ? Quote.MaxByMaterials : Param == TEXT("Space") ? Quote.MaxBySpace : Quote.MaxRuns;
			R.Text = FText::Format(Quote.bAppend
					? LOCTEXT("ProcMaxAppend2", "추가 가능 {0}회 · 재료 {1}회 / 공간 {2}회")
					: LOCTEXT("ProcMax2", "최대 {0}회 · 재료 {1}회 / 공간 {2}회"),
				FText::AsNumber(Quote.MaxRuns), FText::AsNumber(Quote.MaxByMaterials), FText::AsNumber(Quote.MaxBySpace));
			R.StateColor = ECozyUiColor::InkMuted;
			break;
		case ECozyUiValue::ProcSummary:
		{
			FString InputsNeed;
			for (const FCozyRecipeQuote::FInput& Input : Quote.Inputs)
			{
				InputsNeed += FString::Printf(TEXT("%s%s %d개 (보유 %d개)"), InputsNeed.IsEmpty() ? TEXT("") : TEXT(", "), *CozyUiScreenUtil::ItemName(Estate, Input.ItemId).ToString(), Input.Need, Input.Have);
			}
			R.Text = Recipe ? FText::Format(LOCTEXT("ProcSummary2", "필요 재료  {0}\n완료품  {1} {2}개\n예상 시간  {4}"),
				FText::FromString(InputsNeed), Quote.OutputName, FText::AsNumber(Quote.TotalOutput), FText::AsNumber(Quote.OutputPerRun), CozyUiFormat::Duration(Quote.TotalSeconds)) : FText::GetEmpty();
			break;
		}
		case ECozyUiValue::ProcBlock:
			R.Text = Quote.BlockReason;
			R.StateColor = ECozyUiColor::Warning;
			break;
		case ECozyUiValue::ProcStorageNote:
			// 🙋 완료품의 공용 창고가 가득해도 시작은 허용하고 안내만 (D35)
			R.Text = (Recipe && Estate->GetStorageSpace(Recipe->OutputItem) <= 0)
				? FText::Format(LOCTEXT("ProcStorageFull2", "창고에 {0} 공간이 없습니다. 완성품은 시설에 보관되며, 수령하려면 창고 공간이 필요합니다"), Quote.OutputName)
				: FText::GetEmpty();
			R.StateColor = ECozyUiColor::Point;
			break;
		default:
			R.Current = Runs;
			R.Text = Quote.bAppend ? LOCTEXT("ProcAppendLabel", "제작 추가") : LOCTEXT("ProcStartLabel", "제작 시작");
			break;
		}
		break;
	}
	case ECozyUiValue::ProcSlotStatus:
	case ECozyUiValue::ProcSlotTime:
	{
		const int32 Index = CozyUiScreenUtil::SlotIndex(Param);
		const TArray<FCozyProcessingJobView> Jobs = CozyUiScreenUtil::ActiveJobs(Estate, ContextFacility);
		R.Max = 100.f;
		if (Jobs.IsValidIndex(Index))
		{
			const FCozyProcessingJobView& Job = Jobs[Index];
			R.Current = Job.RunProgress01 * 100.f;
			R.RemainingSeconds = Job.RunRemainingSeconds;
			R.StateColor = Job.bPaused ? ECozyUiColor::Paused : ECozyUiColor::Progress;
			if (Value == ECozyUiValue::ProcSlotStatus)
			{
				R.Text = Job.bPaused
					? FText::Format(LOCTEXT("SlotPaused2", "{0} — {1} · {2}/{3}회 완성 · 현재 회차 진행도 유지"), Job.Status, Job.OutputName, FText::AsNumber(Job.CompletedRuns), FText::AsNumber(Job.TotalRuns))
					: Job.Status;
			}
			else
			{
				R.Text = FText::Format(LOCTEXT("SlotTime2", "이번 회 남은 {0} · 전체 남은 {1} · 완성 {2}/{3}회 ({4}개)"),
					CozyUiFormat::Duration(Job.RunRemainingSeconds), CozyUiFormat::Duration(Job.TotalRemainingSeconds),
					FText::AsNumber(Job.CompletedRuns), FText::AsNumber(Job.TotalRuns), FText::AsNumber(Job.CompletedRuns * Job.OutputPerRun));
				R.StateColor = ECozyUiColor::InkMuted;
			}
		}
		else
		{
			R.StateColor = ECozyUiColor::InkMuted;
			R.Text = Value == ECozyUiValue::ProcSlotStatus
				? FText::Format(LOCTEXT("SlotEmpty2", "가공 칸 {0}: 비어 있음"), FText::AsNumber(Index + 1))
				: FText::GetEmpty();
		}
		break;
	}
	case ECozyUiValue::ProcQueueLine:
	{
		FString QueueLine;
		int32 QueueRuns = 0;
		for (const FCozyProcessingJobView& Each : Estate->GetProcessingJobs(ContextFacility))
		{
			if (Each.bQueued)
			{
				QueueLine += (QueueLine.IsEmpty() ? TEXT("") : TEXT(" · ")) + FString::Printf(TEXT("%s %d회(%d개)"), *Each.OutputName.ToString(), Each.TotalRuns, Each.TotalRuns * Each.OutputPerRun);
				QueueRuns += Each.TotalRuns;
			}
		}
		R.Current = QueueRuns;
		R.Text = QueueRuns > 0 ? FText::Format(LOCTEXT("ProcQueueLine2", "제작 추가 대기 {0}회: {1} — 지금 작업이 끝나면 이어서 시작 (가공 칸을 차지하지 않음)"), FText::AsNumber(QueueRuns), FText::FromString(QueueLine)) : FText::GetEmpty();
		R.StateColor = ECozyUiColor::InkMuted;
		break;
	}
	case ECozyUiValue::ProcCancelConfirm:
	{
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		const FGuid Pending = Hud ? Hud->GetProcPendingCancel() : FGuid();
		const TArray<FCozyProcessingJobView> Jobs = Estate->GetProcessingJobs(ContextFacility);
		const FCozyProcessingJobView* Job = Pending.IsValid() ? Jobs.FindByPredicate([&Pending](const FCozyProcessingJobView& Each) { return Each.JobId == Pending; }) : nullptr;
		R.Text = Job ? FText::Format(LOCTEXT("ProcConfirm2", "취소하면 투입한 재료를 돌려받을 수 없습니다.\n완성된 {0}회분({1}개)은 시설에 남고, 남은 {2}회는 완료품 없이 종료됩니다. 취소할까요?"),
			FText::AsNumber(Job->CompletedRuns), FText::AsNumber(Job->CompletedRuns * Job->OutputPerRun), FText::AsNumber(Job->TotalRuns - Job->CompletedRuns)) : FText::GetEmpty();
		R.StateColor = ECozyUiColor::Warning;
		break;
	}
	case ECozyUiValue::UpgradeJob:
	case ECozyUiValue::SpeedCount:
	case ECozyUiValue::SpeedPreview:
	{
		const int32 Index = CozyUiScreenUtil::SlotIndex(Param);
		const TArray<FCozyUpgradeJobView> Jobs = Estate->GetUpgradeJobs();
		const FCozyUpgradeJobView* Job = Jobs.IsValidIndex(Index) ? &Jobs[Index] : nullptr;
		if (Value == ECozyUiValue::UpgradeJob)
		{
			R.Max = 100.f;
			if (Job)
			{
				R.Current = Job->Progress01 * 100.f;
				R.RemainingSeconds = Job->RemainingSeconds;
				R.StateColor = ECozyUiColor::Progress;
				R.Text = FText::Format(LOCTEXT("UpJob2", "{0} → Lv{1} · 남은 {2}"), Job->FacilityName, FText::AsNumber(Job->ToLevel), CozyUiFormat::Duration(Job->RemainingSeconds));
			}
			else
			{
				R.StateColor = ECozyUiColor::InkMuted;
				R.Text = FText::Format(LOCTEXT("UpJobEmpty2", "작업 칸 {0}: 비어 있음"), FText::AsNumber(Index + 1));
			}
			break;
		}
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		if (!Job || !Hud)
		{
			break;
		}
		const FCozySpeedupQuote Speed = Estate->GetSpeedupQuote(Job->JobId, Hud->GetUpgradeSpeedCount(Index));
		if (Value == ECozyUiValue::SpeedCount)
		{
			R.Current = Speed.Count;
			R.Max = Speed.Owned;
			R.Text = FText::Format(LOCTEXT("SpeedCount2", "{0}장 (보유 {1}장)"), FText::AsNumber(Speed.Count), FText::AsNumber(Speed.Owned));
		}
		else
		{
			FString Preview = FString::Printf(TEXT("미리보기: 1장 = %s 단축 · %d장 → %s 단축 · 남은 시간 %s → %s"),
				*CozyUiFormat::Duration(Speed.SecondsPerItem).ToString(), Speed.Count, *CozyUiFormat::Duration(Speed.Reduce).ToString(),
				*CozyUiFormat::Duration(Speed.RemainingBefore).ToString(), *CozyUiFormat::Duration(Speed.RemainingAfter).ToString());
			if (Speed.Wasted > 0.f)
			{
				Preview += FString::Printf(TEXT(" · 남은 시간보다 많아 %s은 버려집니다"), *CozyUiFormat::Duration(Speed.Wasted).ToString());
			}
			if (!Speed.BlockReason.IsEmpty())
			{
				Preview += TEXT(" · ") + Speed.BlockReason.ToString();
			}
			R.Text = FText::FromString(Preview);
			R.StateColor = ECozyUiColor::InkMuted;
		}
		break;
	}
	case ECozyUiValue::UpgradeTitle:
	case ECozyUiValue::UpgradeDetail:
	case ECozyUiValue::UpgradeBlock:
	{
		const FCozyFacilityState* Facility = FindFirst(Param);
		if (!Facility)
		{
			break;
		}
		const FCozyUpgradeQuote Quote = Estate->GetUpgradeQuote(Facility->InstanceId);
		if (Value == ECozyUiValue::UpgradeTitle)
		{
			const FText Name = Estate->GetFacilityDisplayName(Facility->InstanceId);
			R.Current = Quote.FromLevel;
			R.Max = Quote.ToLevel;
			R.Text = Quote.bValid
				? FText::Format(LOCTEXT("UpRowTitle2", "{0}   Lv{1} → Lv{2}"), Name, FText::AsNumber(Quote.FromLevel), FText::AsNumber(Quote.ToLevel))
				: FText::Format(LOCTEXT("UpRowTitleMax2", "{0}   Lv{1}"), Name, FText::AsNumber(Quote.FromLevel));
			break;
		}
		if (Value == ECozyUiValue::UpgradeBlock)
		{
			R.Text = Quote.BlockReason;
			R.StateColor = ECozyUiColor::Warning;
			break;
		}
		FString Detail;
		if (Quote.bValid)
		{
			FString Costs;
			for (const FCozyUpgradeQuote::FAmount& Cost : Quote.Costs)
			{
				Costs += FString::Printf(TEXT("%s%s %d개 (보유 %d)"), Costs.IsEmpty() ? TEXT("") : TEXT(" · "), *CozyUiScreenUtil::ItemName(Estate, Cost.ItemId).ToString(), Cost.Amount, Cost.Have);
			}
			Detail = FString::Printf(TEXT("비용: %s · 시간: %s"), Costs.IsEmpty() ? TEXT("없음") : *Costs, *CozyUiFormat::Duration(Quote.Seconds).ToString());
			// 조건마다 충족 여부 (신사 상한 · 선행 시설 D41)
			if (Quote.Conditions.Num() > 0)
			{
				FString Conditions;
				for (const FCozyUpgradeQuote::FCondition& Condition : Quote.Conditions)
				{
					Conditions += FString::Printf(TEXT("%s%s %s"), Conditions.IsEmpty() ? TEXT("") : TEXT(" · "), *Condition.Label.ToString(),
						Condition.bMet ? TEXT("(충족)") : *FString::Printf(TEXT("(부족 · 지금 Lv%d)"), Condition.Current));
				}
				Detail += FString::Printf(TEXT("\n조건: %s"), *Conditions);
			}
			if (Quote.UnlockFacilityNames.Num() > 0)
			{
				Detail += FString::Printf(TEXT("\n해금 시설: %s"), *CozyUiScreenUtil::JoinTexts(Quote.UnlockFacilityNames, TEXT(", ")));
			}
			if (Quote.NextFieldSpeedMultiplier > 0.f)
			{
				Detail += FString::Printf(TEXT("\n효과: 모든 밭 생산 속도 ×%s (진행 중인 주기는 그대로, 다음 주기부터)"), *CozyUiScreenUtil::Multiplier(Quote.NextFieldSpeedMultiplier).ToString());
			}
			if (Quote.UnlockCropNames.Num() > 0)
			{
				Detail += FString::Printf(TEXT("\n해금 작물: %s (각 밭의 작물 선택에 표시 · 자동으로 바뀌지 않음)"), *CozyUiScreenUtil::JoinTexts(Quote.UnlockCropNames, TEXT(", ")));
			}
			// D36: 시작하면 종료될 가공과 반환될 재료
			if (Quote.EndingJobs.Num() > 0)
			{
				const FString Ending = CozyUiScreenUtil::JoinTexts(Quote.EndingJobs, TEXT(" / "));
				FString Refunds;
				for (const FCozyUpgradeQuote::FAmount& Refund : Quote.Refunds)
				{
					Refunds += FString::Printf(TEXT("%s%s %d개"), Refunds.IsEmpty() ? TEXT("") : TEXT(", "), *CozyUiScreenUtil::ItemName(Estate, Refund.ItemId).ToString(), Refund.Amount);
				}
				Detail += FString::Printf(TEXT("\n시작하면 진행 중인 가공이 종료됩니다: %s\n미완료 회차 재료 반환: %s · 완성된 가공품은 시설에 남습니다"), *Ending, Refunds.IsEmpty() ? TEXT("없음") : *Refunds);
			}
		}
		R.Text = FText::FromString(Detail);
		R.StateColor = ECozyUiColor::InkMuted;
		break;
	}
	case ECozyUiValue::PlacementStatus:
		if (Estate->IsPlacing())
		{
			FText PlaceReason;
			const bool bCanPlace = Estate->CanPlaceFacility(Estate->GetPlacementId(), Estate->GetPlacementCoord(), Estate->GetPlacementRotation(), PlaceReason);
			FText StoreReason;
			const bool bCanStore = !Estate->IsPlacementFromStorage() && Estate->CanStoreFacility(Estate->GetPlacementId(), StoreReason);
			FString Status = bCanPlace ? TEXT("놓을 수 있습니다") : PlaceReason.ToString();
			if (!Estate->IsPlacementFromStorage() && !bCanStore)
			{
				Status += TEXT("\n보관 불가: ") + StoreReason.ToString();
			}
			R.Text = FText::FromString(Status);
			R.Current = bCanPlace ? 1.f : 0.f;
			R.StateColor = bCanPlace ? ECozyUiColor::Ok : ECozyUiColor::Warning;
		}
		break;
	case ECozyUiValue::StoredLabel:
		if (const FCozyFacilityState* Facility = FindFirst(Param))
		{
			// 꾸미기 장식은 레벨 대신 점수 (놓으면 더해짐)
			if (const FCozyDecorationRow* Decoration = Estate->GetDecorationDef(Facility->DefinitionId))
			{
				R.Current = Decoration->Points;
				R.Text = FText::Format(LOCTEXT("StoredDecor", "{0} · 꾸미기 {1}점"), Decoration->DisplayName, FText::AsNumber(Decoration->Points));
				break;
			}
			R.Current = Facility->Level;
			FString Label = FString::Printf(TEXT("%s Lv%d"), *Estate->GetFacilityDisplayName(Facility->InstanceId).ToString(), Facility->Level);
			if (const FCozyCropRow* Crop = Facility->SelectedCropId.IsNone() ? nullptr : Estate->GetCropDef(Facility->SelectedCropId))
			{
				Label += FString::Printf(TEXT(" · %s"), *Crop->DisplayName.ToString());
			}
			R.Text = FText::FromString(Label);
		}
		break;
	case ECozyUiValue::DecorScore:
	{
		const FCozyDecorationScoreView View = Estate->GetDecorationScoreView();
		R.Current = View.Score;
		R.Max = View.bMaxed ? 0.f : View.Score + View.PointsToNext;
		R.Text = View.bMaxed
			? FText::Format(LOCTEXT("DecorScoreMax", "꾸미기 점수 {0}점 · 모든 생산 +{1}% (최대)"), FText::AsNumber(View.Score), FText::AsNumber(FMath::RoundToInt(View.Bonus * 100.f)))
			: FText::Format(LOCTEXT("DecorScore", "꾸미기 점수 {0}점 · 모든 생산 +{1}% · 다음 단계(+{2}%)까지 {3}점"), FText::AsNumber(View.Score), FText::AsNumber(FMath::RoundToInt(View.Bonus * 100.f)),
				FText::AsNumber(FMath::RoundToInt((View.Bonus + Estate->GetConfig().DecorBonusPerStep) * 100.f)), FText::AsNumber(View.PointsToNext));
		R.StateColor = View.Bonus > 0.f ? ECozyUiColor::Ok : ECozyUiColor::InkMuted;
		break;
	}
	case ECozyUiValue::DecorLine:
	{
		const FCozyDecorationQuote Quote = Estate->GetDecorationQuote(Param);
		if (!Quote.bValid)
		{
			R.Text = Quote.BlockReason;
			R.StateColor = ECozyUiColor::Locked;
			break;
		}
		TArray<FText> Prices;
		for (const FCozyDecorationQuote::FAmount& Cost : Quote.Costs)
		{
			Prices.Add(FText::Format(LOCTEXT("DecorPrice", "{0} {1}"), CozyUiScreenUtil::ItemName(Estate, Cost.ItemId), FText::AsNumber(Cost.Amount)));
		}
		R.Current = Quote.Points;
		const FText Line = FText::Format(LOCTEXT("DecorLine", "{0}×{1} · {2}점 · {3} · 보유 {4} (놓음 {5})"), FText::AsNumber(Quote.Size.X), FText::AsNumber(Quote.Size.Y),
			FText::AsNumber(Quote.Points), FText::FromString(CozyUiScreenUtil::JoinTexts(Prices, TEXT(" + "))), FText::AsNumber(Quote.Placed + Quote.Stored), FText::AsNumber(Quote.Placed));
		R.Text = Quote.bCanBuy ? Line : FText::Format(LOCTEXT("DecorLineBlocked", "{0} · 살 수 없음: {1}"), Line, Quote.BlockReason);
		R.StateColor = Quote.bCanBuy ? ECozyUiColor::InkMuted : ECozyUiColor::Warning;
		break;
	}
	case ECozyUiValue::ProcReserved:
	{
		const FCozyFacilityState* Facility = FindFirst(Param);
		const FCozyUnclaimedView View = Estate->GetUnclaimedView(Facility ? Facility->InstanceId : ContextFacility);
		R.Current = View.Reserved;
		R.Text = View.Reserved > 0 ? FText::Format(LOCTEXT("ProcReserved", "제작 중 확보 {0}"), FText::AsNumber(View.Reserved)) : FText::GetEmpty();
		R.StateColor = ECozyUiColor::InkMuted;
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
	// 설정 창의 탭 · 확인 창 줄 (디자이너에서는 모두 한 줄씩 보여 배치를 고칠 수 있게)
	if (UCozySettingsSubsystem::IsSettingsList(Source))
	{
		TArray<FCozyUiListRow> Rows;
		if (const UCozySettingsSubsystem* Settings = IsDesignTime() ? nullptr : UCozySettingsSubsystem::Get(this))
		{
			Settings->GetListRows(Source, Rows);
		}
		else
		{
			Rows.Add({ TEXT("Row"), LOCTEXT("PvSettingsRow", "설정 (미리보기)") });
		}
		return Rows;
	}
	if (Source == ECozyUiListSource::UpgradeFacilities && SelectedUpgradeFacility.IsValid())
	{
		const UCozyEstateSubsystem* Estate = GetEstate();
		if (Estate && Estate->FindFacility(SelectedUpgradeFacility))
		{
			return {{ FName(*SelectedUpgradeFacility.ToString()), Estate->GetFacilityDisplayName(SelectedUpgradeFacility) }};
		}
		return {};
	}
	if (!bPreview || Source == ECozyUiListSource::None || IsDesignTime())
	{
		return GetRealListRows(Source, ParentRowId);
	}
	// 미리보기: '비어 있음' 상태는 0줄 · 조건부 줄은 늘 한 줄 · 메뉴는 모든 버튼 · 나머지는 실제 줄(없으면 예시 2줄)
	TArray<FCozyUiListRow> Rows;
	if (PreviewState == ECozyUiPreviewState::Empty)
	{
		return Rows;
	}
	switch (Source)
	{
	case ECozyUiListSource::ProcQueue: Rows.Add({ TEXT("Queue"), LOCTEXT("PvQueueRow", "제작 추가 대기 (미리보기)") }); return Rows;
	case ECozyUiListSource::ProcPendingCancel: Rows.Add({ TEXT("Confirm"), LOCTEXT("PvConfirmRow", "취소 확인 (미리보기)") }); return Rows;
	case ECozyUiListSource::UpgradeSpeed: Rows.Add({ ParentRowId.IsNone() ? FName(TEXT("Slot0")) : ParentRowId, LOCTEXT("PvSpeedRow", "시간 단축 (미리보기)") }); return Rows;
	case ECozyUiListSource::FacilityMenuItems:
		Rows.Add({ TEXT("Info"), LOCTEXT("PvMenuInfo", "정보") });
		Rows.Add({ TEXT("Processing"), LOCTEXT("PvMenuProc", "가공") });
		Rows.Add({ TEXT("Sales"), LOCTEXT("PvMenuSales", "판매") });
		Rows.Add({ TEXT("Upgrade"), LOCTEXT("PvMenuUp", "업그레이드") });
		Rows.Add({ TEXT("FieldManagement"), LOCTEXT("PvMenuField", "밭 관리") });
		Rows.Add({ TEXT("Nagaya"), LOCTEXT("PvMenuNagaya", "주민 관리") });
		Rows.Add({ TEXT("Resident"), LOCTEXT("PvMenuResident", "주민") });
		return Rows;
	default:
		break;
	}
	Rows = GetRealListRows(Source, ParentRowId);
	if (Rows.Num() == 0)
	{
		const bool bSlots = Source == ECozyUiListSource::ProcSlots || Source == ECozyUiListSource::UpgradeSlots;
		Rows.Add({ bSlots ? FName(TEXT("Slot0")) : FName(TEXT("Sample1")), LOCTEXT("PvSample1", "예시 1 (미리보기)") });
		Rows.Add({ bSlots ? FName(TEXT("Slot1")) : FName(TEXT("Sample2")), LOCTEXT("PvSample2", "예시 2 (미리보기)") });
	}
	return Rows;
}

TArray<FCozyUiListRow> UCozyUiScreen::GetRealListRows(ECozyUiListSource Source, FName ParentRowId) const
{
	TArray<FCozyUiListRow> Rows;
	if (Source == ECozyUiListSource::None)
	{
		return Rows;
	}
	const UCozyEstateSubsystem* Estate = GetEstate();
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
	case ECozyUiListSource::ProcRecipes:
		for (const FName& RecipeId : Estate->GetFacilityRecipes(ContextFacility))
		{
			const FCozyRecipeRow* Recipe = Estate->GetRecipeDef(RecipeId);
			const FCozyItemRow* Output = Recipe ? Estate->GetItemDef(Recipe->OutputItem) : nullptr;
			Rows.Add({ RecipeId, Output ? Output->DisplayName : FText::FromName(RecipeId) });
		}
		break;
	case ECozyUiListSource::ProcSlots:
		if (const FCozyFacilityState* Facility = Estate->FindFacility(ContextFacility))
		{
			const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility->DefinitionId);
			for (int32 Index = 0; Index < FMath::Max(1, Def ? Def->ProcessingSlots : 1); ++Index)
			{
				Rows.Add({ FName(*FString::Printf(TEXT("Slot%d"), Index)), FText::Format(LOCTEXT("SlotName", "가공 칸 {0}"), FText::AsNumber(Index + 1)) });
			}
		}
		break;
	case ECozyUiListSource::FacilityMenuItems:
		if (const FCozyFacilityState* Facility = Estate->FindFacility(ContextFacility))
		{
			if (const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility->DefinitionId))
			{
				for (const TPair<FName, FText>& Item : UCozyHudWidget::GetFacilityMenu(*Def))
				{
					Rows.Add({ Item.Key, Item.Value });
				}
			}
		}
		break;
	case ECozyUiListSource::UpgradeSlots:
		for (int32 Index = 0; Index < FMath::Max(1, Estate->GetUpgradeSlotCount()); ++Index)
		{
			Rows.Add({ FName(*FString::Printf(TEXT("Slot%d"), Index)), FText::Format(LOCTEXT("UpSlotName", "작업 칸 {0}"), FText::AsNumber(Index + 1)) });
		}
		break;
	case ECozyUiListSource::UpgradeSpeed:
		if (Estate->GetUpgradeJobs().IsValidIndex(CozyUiScreenUtil::SlotIndex(ParentRowId)))
		{
			Rows.Add({ ParentRowId, LOCTEXT("SpeedRow", "시간 단축") });
		}
		break;
	case ECozyUiListSource::UpgradeFacilities:
		for (const FGuid& FacilityId : Estate->GetUpgradableFacilities())
		{
			Rows.Add({ FName(*FacilityId.ToString()), Estate->GetFacilityDisplayName(FacilityId) });
		}
		break;
	case ECozyUiListSource::UpgradeConditionTargets:
	{
		// 시설 줄 안이면 그 줄의 시설 · 창에 바로 놓였으면 창 시설 (예: 신사 창)
		FGuid Parsed;
		const FGuid FacilityId = FGuid::Parse(ParentRowId.ToString(), Parsed) ? Parsed : ContextFacility;
		if (FacilityId.IsValid())
		{
			for (const FCozyUpgradeQuote::FCondition& Condition : Estate->GetUpgradeQuote(FacilityId).Conditions)
			{
				const FCozyFacilityRow* TargetDef = Condition.FacilityId.IsNone() ? nullptr : Estate->GetFacilityDef(Condition.FacilityId);
				if (TargetDef)
				{
					Rows.Add({ Condition.FacilityId, TargetDef->DisplayName });
				}
			}
		}
		break;
	}
	case ECozyUiListSource::ProcQueue:
		if (Estate->GetProcessingJobs(ContextFacility).ContainsByPredicate([](const FCozyProcessingJobView& Each) { return Each.bQueued; }))
		{
			Rows.Add({ TEXT("Queue"), LOCTEXT("QueueRow", "제작 추가 대기") });
		}
		break;
	case ECozyUiListSource::ProcPendingCancel:
	{
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		const FGuid Pending = Hud ? Hud->GetProcPendingCancel() : FGuid();
		if (Pending.IsValid() && Estate->GetProcessingJobs(ContextFacility).ContainsByPredicate([&Pending](const FCozyProcessingJobView& Each) { return Each.JobId == Pending; }))
		{
			Rows.Add({ TEXT("Confirm"), LOCTEXT("ConfirmRow", "취소 확인") });
		}
		break;
	}
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
		const FCozyResidentState* Resident = CozyUiScreenUtil::FindResident(Estate, ParentRowId);
		if (!Resident)
		{
			break;
		}
		const FGuid ResidentId = Resident->InstanceId;
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
	case ECozyUiListSource::DecorCatalog:
		for (const FName& DecorationId : Estate->GetDecorationCatalog())
		{
			const FCozyDecorationRow* Decoration = Estate->GetDecorationDef(DecorationId);
			Rows.Add({ DecorationId, Decoration ? Decoration->DisplayName : FText::FromName(DecorationId) });
		}
		break;
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
	case ECozyUiAction::SelectSaleItem: return LOCTEXT("ActSaleItem", "판매 재료 고르기");
	case ECozyUiAction::SaleLess: return LOCTEXT("ActSaleLess", "판매 수량 줄이기");
	case ECozyUiAction::SaleMore: return LOCTEXT("ActSaleMore", "판매 수량 늘리기");
	case ECozyUiAction::SaleAll: return LOCTEXT("ActSaleAll", "전부 팔기 수량");
	case ECozyUiAction::SellSelected: return LOCTEXT("ActSell", "판매");
	case ECozyUiAction::SelectRecipe: return LOCTEXT("ActRecipe", "레시피 고르기");
	case ECozyUiAction::ProcLess: return LOCTEXT("ActProcLess", "제작 횟수 줄이기");
	case ECozyUiAction::ProcMore: return LOCTEXT("ActProcMore", "제작 횟수 늘리기");
	case ECozyUiAction::ProcMax: return LOCTEXT("ActProcMax", "최대 횟수");
	case ECozyUiAction::StartProcessing: return LOCTEXT("ActProcStart", "제작 시작·추가");
	case ECozyUiAction::CancelSlot: return LOCTEXT("ActCancelSlot", "이 칸 취소 묻기");
	case ECozyUiAction::CancelQueued: return LOCTEXT("ActCancelQueued", "추가분 취소 묻기");
	case ECozyUiAction::ConfirmCancel: return LOCTEXT("ActConfirmCancel", "취소 확정");
	case ECozyUiAction::KeepProcessing: return LOCTEXT("ActKeep", "계속 제작");
	case ECozyUiAction::SpeedLess: return LOCTEXT("ActSpeedLess", "부적 줄이기");
	case ECozyUiAction::SpeedMore: return LOCTEXT("ActSpeedMore", "부적 늘리기");
	case ECozyUiAction::SpeedFit: return LOCTEXT("ActSpeedFit", "딱 맞게");
	case ECozyUiAction::ApplySpeedup: return LOCTEXT("ActSpeedApply", "단축 확정");
	case ECozyUiAction::SelectUpgradeFacility: return LOCTEXT("UpgradeSelect", "시설의 업그레이드 상세 정보 보기");
	case ECozyUiAction::BackUpgradeList: return LOCTEXT("UpgradeBack", "시설 목록으로 돌아가기");
	case ECozyUiAction::OpenUpgradeForWindow: return LOCTEXT("UpgradeForWindow", "후신소에서 이 시설 업그레이드 보기");
	case ECozyUiAction::OpenSettings: return LOCTEXT("ActSettings", "설정 (사운드 · 화면 · 게임 종료)");
	case ECozyUiAction::SettingsTab: return LOCTEXT("ActSettingsTab", "이 탭 보기");
	case ECozyUiAction::SetVolume: return LOCTEXT("ActVolume", "음량 (움직이면 바로 들림 · 적용을 눌러야 저장)");
	case ECozyUiAction::SetWindowMode: return LOCTEXT("ActWindowMode", "화면 모드 고르기 (적용을 눌러야 바뀜)");
	case ECozyUiAction::ResolutionPrev: return LOCTEXT("ActResPrev", "더 낮은 해상도");
	case ECozyUiAction::ResolutionNext: return LOCTEXT("ActResNext", "더 높은 해상도");
	case ECozyUiAction::ToggleVSync: return LOCTEXT("ActVSync", "수직 동기화 켜기·끄기");
	case ECozyUiAction::SetFrameLimit: return LOCTEXT("ActFps", "최대 프레임 고르기");
	case ECozyUiAction::ApplySettings: return LOCTEXT("ActApply", "이 탭의 변경 적용 · 저장");
	case ECozyUiAction::RevertSettings: return LOCTEXT("ActRevert", "적용하지 않은 변경 취소");
	case ECozyUiAction::ResetSettingsTab: return LOCTEXT("ActResetTab", "이 탭만 기본값으로 (적용을 눌러야 저장 · 게임 진행은 그대로)");
	case ECozyUiAction::KeepDisplay: return LOCTEXT("ActKeep2", "이 화면 설정 유지");
	case ECozyUiAction::UndoDisplay: return LOCTEXT("ActUndo", "이전 화면 설정으로");
	case ECozyUiAction::AskQuit: return LOCTEXT("ActAskQuit", "게임 종료 (확인 후 저장하고 종료)");
	case ECozyUiAction::CancelQuit: return LOCTEXT("ActCancelQuit", "종료하지 않음");
	case ECozyUiAction::ConfirmQuit: return LOCTEXT("ActConfirmQuit", "영지를 저장하고 종료");
	case ECozyUiAction::ToggleCompactWindow: return LOCTEXT("ActCompact", "작은 창으로 띄워 두기 / 크게 보기 (M)");
	case ECozyUiAction::ToggleCompactTop: return LOCTEXT("ActCompactTop", "작은 창을 항상 위에 켜기·끄기 (적용을 눌러야 저장)");
	case ECozyUiAction::ToggleCompactTopNow: return LOCTEXT("ActCompactTopNow", "작은 창 항상 위 켜기·끄기 (핀만 = 켬 · 핀에 X = 끔)");
	case ECozyUiAction::RestartTutorial: return LOCTEXT("ActTutorial", "첫 실행 안내를 처음부터 다시 봅니다");
	case ECozyUiAction::OpenDecorShop: return LOCTEXT("ActDecorShop", "꾸미기 상점 창");
	case ECozyUiAction::BuyDecoration: return LOCTEXT("ActBuyDecor", "장식 사기 (보관함으로)");
	case ECozyUiAction::StartUpgrade: return LOCTEXT("ActUpStart", "업그레이드 시작");
	case ECozyUiAction::GoToFacility: return LOCTEXT("ActGoTo", "그 시설로 이동");
	case ECozyUiAction::TakeOutStored: return LOCTEXT("ActTakeOut", "보관함에서 꺼내기");
	case ECozyUiAction::OpenFacilityFunction: return LOCTEXT("ActFacFunc", "시설 기능 창 열기");
	case ECozyUiAction::RotatePlacement: return LOCTEXT("ActRotate", "배치 회전");
	case ECozyUiAction::StorePlacing: return LOCTEXT("ActStorePlacing", "보관함에 넣기");
	case ECozyUiAction::ConfirmPlacing: return LOCTEXT("ActConfirmPlacing", "배치 확정");
	case ECozyUiAction::CancelPlacing: return LOCTEXT("ActCancelPlacing", "배치 취소");
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
	if (UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this))
	{
		Hud->ShowToast(Message);
	}
}

bool UCozyUiScreen::CanRunAction(const FCozyUiElementEntry& Entry, FText& OutReason) const
{
	if (UCozySettingsSubsystem::IsSettingsAction(Entry.Action))
	{
		const UCozySettingsSubsystem* Settings = IsDesignTime() ? nullptr : UCozySettingsSubsystem::Get(this);
		return !Settings || Settings->CanRun(Entry.Action, Entry.ActionParam, OutReason);
	}
	const UCozyEstateSubsystem* Estate = GetEstate();
	if (bPreview)
	{
		const bool bAlwaysOn = Entry.Action == ECozyUiAction::CloseWindow || Entry.Action == ECozyUiAction::TogglePreview
			|| Entry.Action == ECozyUiAction::ToggleDebug || Entry.Action == ECozyUiAction::None
			|| Entry.Action == ECozyUiAction::SelectUpgradeFacility || Entry.Action == ECozyUiAction::BackUpgradeList;
		if (!bAlwaysOn && PreviewState == ECozyUiPreviewState::Locked)
		{
			OutReason = LOCTEXT("PvLockedButton", "미리보기: '잠김' 상태에서 버튼이 꺼진 모습입니다");
			return false;
		}
		return true;
	}
	if (!Estate || IsDesignTime())
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
	case ECozyUiAction::BuyDecoration:
	{
		const FCozyDecorationQuote Quote = Estate->GetDecorationQuote(Entry.ActionParam);
		OutReason = Quote.BlockReason;
		return Quote.bCanBuy;
	}
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
	case ECozyUiAction::SelectSaleItem:
	{
		const FCozyItemRow* Item = Estate->GetItemDef(Entry.ActionParam);
		if (!Item || Item->SellPrice <= 0)
		{
			OutReason = LOCTEXT("CanNotSellItem", "판매할 수 없는 재료입니다");
			return false;
		}
		return true;
	}
	case ECozyUiAction::SaleLess:
	case ECozyUiAction::SaleMore:
	case ECozyUiAction::SaleAll:
	case ECozyUiAction::SellSelected:
	{
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		if (!Hud)
		{
			return false;
		}
		const int32 Amount = Hud->GetSellAmount();
		const FCozySellQuote Quote = Estate->GetSellQuote(ContextFacility, Hud->GetSellItem(), Amount);
		switch (Entry.Action)
		{
		case ECozyUiAction::SaleLess:
			OutReason = LOCTEXT("SaleMin", "1개보다 적게 팔 수 없습니다");
			return Amount > 1;
		case ECozyUiAction::SaleMore:
			OutReason = LOCTEXT("SaleMaxReached", "창고 보유량까지 골랐습니다");
			return Amount < Quote.MaxAmount;
		case ECozyUiAction::SaleAll:
			OutReason = Quote.MaxAmount > 0 ? LOCTEXT("SaleAlreadyAll", "이미 전부 골랐습니다") : LOCTEXT("SaleNone", "창고에 없습니다");
			return Quote.MaxAmount > 0 && Amount != Quote.MaxAmount;
		default:
			OutReason = Quote.BlockReason;
			return Quote.bCanSell;
		}
	}
	case ECozyUiAction::ProcLess:
	case ECozyUiAction::ProcMore:
	case ECozyUiAction::ProcMax:
	case ECozyUiAction::StartProcessing:
	{
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		if (!Hud)
		{
			return false;
		}
		const int32 Runs = Hud->GetProcRuns();
		const FCozyRecipeQuote Quote = Estate->GetRecipeQuote(ContextFacility, Hud->GetProcRecipe(), Runs);
		switch (Entry.Action)
		{
		case ECozyUiAction::ProcLess:
			OutReason = LOCTEXT("ProcMin", "1회보다 적게 만들 수 없습니다");
			return Runs > 1;
		case ECozyUiAction::ProcMore:
			OutReason = Quote.BlockReason.IsEmpty() ? LOCTEXT("ProcMaxReached", "지금 최대 횟수까지 골랐습니다") : Quote.BlockReason;
			return Runs < Quote.MaxRuns;
		case ECozyUiAction::ProcMax:
			OutReason = Quote.MaxRuns > 0 ? LOCTEXT("ProcAlreadyMax", "이미 최대 횟수입니다") : Quote.BlockReason;
			return Quote.MaxRuns > 0 && Runs != Quote.MaxRuns;
		default:
			OutReason = Quote.BlockReason;
			return Quote.bCanStart;
		}
	}
	case ECozyUiAction::SpeedLess:
	case ECozyUiAction::SpeedMore:
	case ECozyUiAction::SpeedFit:
	case ECozyUiAction::ApplySpeedup:
	{
		const UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this);
		const int32 Index = CozyUiScreenUtil::SlotIndex(Entry.ActionParam);
		const TArray<FCozyUpgradeJobView> Jobs = Estate->GetUpgradeJobs();
		if (!Hud || !Jobs.IsValidIndex(Index))
		{
			OutReason = LOCTEXT("NoUpgradeJob", "진행 중인 업그레이드가 없습니다");
			return false;
		}
		const int32 Count = Hud->GetUpgradeSpeedCount(Index);
		const FCozySpeedupQuote Speed = Estate->GetSpeedupQuote(Jobs[Index].JobId, Count);
		switch (Entry.Action)
		{
		case ECozyUiAction::SpeedLess:
			OutReason = LOCTEXT("SpeedMin", "1장보다 적게 쓸 수 없습니다");
			return Count > 1;
		case ECozyUiAction::SpeedMore:
			OutReason = LOCTEXT("SpeedMaxReached", "보유량·필요량까지 골랐습니다");
			return Count < FMath::Max(1, FMath::Max(Speed.Owned, Speed.MaxUseful));
		case ECozyUiAction::SpeedFit:
			OutReason = LOCTEXT("SpeedAlreadyFit", "이미 딱 맞는 장수입니다");
			return Count != FMath::Max(1, FMath::Min(Speed.Owned, Speed.MaxUseful));
		default:
			OutReason = Speed.BlockReason;
			return Speed.bCanApply;
		}
	}
	case ECozyUiAction::RotatePlacement:
	case ECozyUiAction::CancelPlacing:
		OutReason = LOCTEXT("NotPlacing", "배치 중인 시설이 없습니다");
		return Estate->IsPlacing();
	case ECozyUiAction::StorePlacing:
		if (!Estate->IsPlacing() || Estate->IsPlacementFromStorage())
		{
			OutReason = LOCTEXT("StoreFromStorage", "보관함에서 꺼낸 시설은 놓거나 취소합니다");
			return false;
		}
		return Estate->CanStoreFacility(Estate->GetPlacementId(), OutReason);
	case ECozyUiAction::ConfirmPlacing:
		if (!Estate->IsPlacing())
		{
			OutReason = LOCTEXT("NotPlacing2", "배치 중인 시설이 없습니다");
			return false;
		}
		return Estate->CanPlaceFacility(Estate->GetPlacementId(), Estate->GetPlacementCoord(), Estate->GetPlacementRotation(), OutReason);
	case ECozyUiAction::StartUpgrade:
	{
		FGuid FacilityId;
		if (!FGuid::Parse(Entry.ActionParam.ToString(), FacilityId))
		{
			return false;
		}
		const FCozyUpgradeQuote Quote = Estate->GetUpgradeQuote(FacilityId);
		OutReason = Quote.BlockReason;
		return Quote.bCanStart;
	}
	case ECozyUiAction::CancelSlot:
		OutReason = LOCTEXT("SlotEmptyReason", "비어 있는 칸입니다");
		return CozyUiScreenUtil::ActiveJobs(Estate, ContextFacility).IsValidIndex(CozyUiScreenUtil::SlotIndex(Entry.ActionParam));
	case ECozyUiAction::CancelQueued:
		OutReason = LOCTEXT("NoQueued", "기다리는 추가분이 없습니다");
		return Estate->GetProcessingJobs(ContextFacility).ContainsByPredicate([](const FCozyProcessingJobView& Each) { return Each.bQueued; });
	case ECozyUiAction::UnassignResident:
	{
		const FCozyResidentState* Resident = CozyUiScreenUtil::FindResident(Estate, Entry.ActionParam);
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

void UCozyUiScreen::RunSliderAction(const FCozyUiElementEntry& Entry, float Ratio)
{
	// 슬라이더는 설정 동작만 (게임 상태를 바꾸지 않음) · 끄는 중에는 줄을 다시 만들지 않고 값만 갱신
	if (UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::IsSettingsAction(Entry.Action) ? UCozySettingsSubsystem::Get(this) : nullptr)
	{
		Settings->Run(Entry.Action, Entry.ActionParam, GetOwningPlayer(), Ratio);
		RefreshValues();
	}
}

void UCozyUiScreen::RunAction(const FCozyUiElementEntry& Entry)
{
	// 설정 동작: 영지 상태에 손대지 않으므로 미리보기 중에도 그대로 동작
	if (UCozySettingsSubsystem::IsSettingsAction(Entry.Action))
	{
		UE_LOG(LogCozyRealm, Log, TEXT("[UI] 설정 버튼: %s → %s"), *Entry.Id.ToString(), *GetActionName(Entry.Action).ToString());
		if (Entry.Action == ECozyUiAction::OpenSettings)
		{
			if (UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this))
			{
				Hud->ToggleSettingsWindow();
			}
			return;
		}
		if (UCozySettingsSubsystem* Settings = UCozySettingsSubsystem::Get(this))
		{
			Settings->Run(Entry.Action, Entry.ActionParam, GetOwningPlayer());
		}
		bRefreshPending = true;
		return;
	}
	// 페이지 전환은 실제 데이터에 손대지 않으며 미리보기에서도 사용할 수 있다.
	if (Entry.Action == ECozyUiAction::SelectUpgradeFacility || Entry.Action == ECozyUiAction::BackUpgradeList)
	{
		if (Entry.Action == ECozyUiAction::BackUpgradeList)
		{
			SelectedUpgradeFacility.Invalidate();
		}
		else if (!FGuid::Parse(Entry.ActionParam.ToString(), SelectedUpgradeFacility))
		{
			return;
		}
		// 클릭 중인 요소를 그 자리에서 지우지 않고 다음 프레임에 페이지를 바꾼다.
		bRefreshPending = true;
		return;
	}
	if (Entry.Action == ECozyUiAction::TogglePreview)
	{
		SetPreview(!bPreview);
		ShowMessage(bPreview ? LOCTEXT("PvOn", "UI 미리보기 켜짐 · 실제 재료를 쓰지 않습니다") : LOCTEXT("PvOff", "UI 미리보기 꺼짐"));
		return;
	}
	// 미리보기 중에도 창 닫기 · 디버그 창 열고 닫기는 동작 (게임 상태를 바꾸지 않는 화면 조작)
	if (bPreview && (Entry.Action == ECozyUiAction::CloseWindow || Entry.Action == ECozyUiAction::ToggleDebug))
	{
		if (UCozyHudWidget* Hud = CozyUiScreenUtil::FindHud(this))
		{
			Entry.Action == ECozyUiAction::CloseWindow ? Hud->CloseWindow() : Hud->ToggleDebugPanel();
		}
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
	UCozyEstateSubsystem* Estate = GetEstate();
	// 영지 HUD가 없는 곳(타이틀 화면)에 띄운 창: 닫기만 띄운 쪽에 맡긴다 (Claude 2026-10-21)
	if (!Hud && Entry.Action == ECozyUiAction::CloseWindow)
	{
		UE_LOG(LogCozyRealm, Log, TEXT("[UI] 버튼: %s → 창 닫기 (HUD 밖)"), *Entry.Id.ToString());
		OnCloseRequested.Broadcast(this);
		return;
	}
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
	case ECozyUiAction::RestartTutorial: Hud->CloseWindow(); Hud->StartTutorial(); break;
	case ECozyUiAction::OpenDecorShop: Hud->ToggleDecorShopWindow(); break;
	case ECozyUiAction::BuyDecoration:
	{
		FText Message;
		Estate->BuyDecoration(Entry.ActionParam, Message);
		Hud->SetFeedbackText(Message);
		Hud->ShowToast(Message);
		Hud->RefreshWindowScreen();
		break;
	}
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
	case ECozyUiAction::SelectSaleItem: Hud->SetSellSelection(Entry.ActionParam, 1); break;
	case ECozyUiAction::SaleLess: Hud->SetSellSelection(Hud->GetSellItem(), Hud->GetSellAmount() - 1); break;
	case ECozyUiAction::SaleMore: Hud->SetSellSelection(Hud->GetSellItem(), Hud->GetSellAmount() + 1); break;
	case ECozyUiAction::SaleAll: Hud->SetSellSelection(Hud->GetSellItem(), Estate->GetSellQuote(ContextFacility, Hud->GetSellItem(), 1).MaxAmount); break;
	case ECozyUiAction::SellSelected:
	{
		// 판매 직전에 서비스가 조건을 다시 확인 · 실패하면 아무것도 바뀌지 않음
		FText Message;
		Estate->SellItem(ContextFacility, Hud->GetSellItem(), Hud->GetSellAmount(), Message);
		Hud->SetFeedbackText(Message);
		Hud->SetSellSelection(Hud->GetSellItem(), Hud->GetSellAmount());
		break;
	}
	case ECozyUiAction::SelectRecipe: Hud->SetProcSelection(Entry.ActionParam, 1); break;
	case ECozyUiAction::ProcLess: Hud->SetProcSelection(Hud->GetProcRecipe(), Hud->GetProcRuns() - 1); break;
	case ECozyUiAction::ProcMore: Hud->SetProcSelection(Hud->GetProcRecipe(), Hud->GetProcRuns() + 1); break;
	case ECozyUiAction::ProcMax: Hud->SetProcSelection(Hud->GetProcRecipe(), Estate->GetRecipeQuote(ContextFacility, Hud->GetProcRecipe(), 1).MaxRuns); break;
	case ECozyUiAction::StartProcessing:
	{
		// 시작 직전에 서비스가 조건을 다시 확인 · 실패하면 아무것도 바뀌지 않음
		FText Message;
		Estate->StartProcessing(ContextFacility, Hud->GetProcRecipe(), Hud->GetProcRuns(), Message);
		Hud->SetFeedbackText(Message);
		Hud->SetProcSelection(Hud->GetProcRecipe(), Hud->GetProcRuns());
		break;
	}
	case ECozyUiAction::CancelSlot:
	{
		// 🙋 취소 전 안내와 확인 (D29) · 확인 줄의 숫자는 매번 최신 값
		const TArray<FCozyProcessingJobView> Jobs = CozyUiScreenUtil::ActiveJobs(Estate, ContextFacility);
		const int32 Index = CozyUiScreenUtil::SlotIndex(Entry.ActionParam);
		if (Jobs.IsValidIndex(Index))
		{
			Hud->SetProcPendingCancel(Jobs[Index].JobId);
		}
		break;
	}
	case ECozyUiAction::CancelQueued:
	{
		// 가장 나중에 추가한 대기 작업부터 취소 (확인 안내는 기존 취소와 같음)
		const TArray<FCozyProcessingJobView> Jobs = Estate->GetProcessingJobs(ContextFacility);
		for (int32 Index = Jobs.Num() - 1; Index >= 0; --Index)
		{
			if (Jobs[Index].bQueued)
			{
				Hud->SetProcPendingCancel(Jobs[Index].JobId);
				break;
			}
		}
		break;
	}
	case ECozyUiAction::ConfirmCancel:
	{
		FText Message;
		Estate->CancelProcessing(Hud->GetProcPendingCancel(), Message);
		Hud->SetFeedbackText(Message);
		Hud->SetProcPendingCancel(FGuid());
		break;
	}
	case ECozyUiAction::KeepProcessing: Hud->SetProcPendingCancel(FGuid()); break;
	case ECozyUiAction::SpeedLess:
	case ECozyUiAction::SpeedMore:
	case ECozyUiAction::SpeedFit:
	case ECozyUiAction::ApplySpeedup:
	{
		const int32 Index = CozyUiScreenUtil::SlotIndex(Entry.ActionParam);
		const TArray<FCozyUpgradeJobView> Jobs = Estate->GetUpgradeJobs();
		if (!Jobs.IsValidIndex(Index))
		{
			break;
		}
		const int32 Count = Hud->GetUpgradeSpeedCount(Index);
		const FCozySpeedupQuote Speed = Estate->GetSpeedupQuote(Jobs[Index].JobId, 1);
		if (Entry.Action == ECozyUiAction::ApplySpeedup)
		{
			// 확정할 때 서비스가 다시 계산 · 실패하면 부적·시간 그대로
			FText Message;
			Estate->ApplySpeedup(Jobs[Index].JobId, Count, Message);
			Hud->SetFeedbackText(Message);
			Hud->SetUpgradeSpeedCount(Index, 1);
		}
		else if (Entry.Action == ECozyUiAction::SpeedFit)
		{
			Hud->SetUpgradeSpeedCount(Index, FMath::Min(Speed.Owned, Speed.MaxUseful));
		}
		else
		{
			const int32 Limit = FMath::Max(1, FMath::Max(Speed.Owned, Speed.MaxUseful));
			Hud->SetUpgradeSpeedCount(Index, FMath::Clamp(Count + (Entry.Action == ECozyUiAction::SpeedMore ? 1 : -1), 1, Limit));
		}
		break;
	}
	case ECozyUiAction::StartUpgrade:
	{
		FGuid FacilityId;
		if (FGuid::Parse(Entry.ActionParam.ToString(), FacilityId))
		{
			// 시작 직전에 서비스가 최신 상태로 다시 검사 · 실패하면 아무것도 바뀌지 않음 (D38)
			FText Message;
			Estate->StartUpgrade(FacilityId, Message);
			Hud->SetFeedbackText(Message);
			Hud->RefreshWindowScreen();
		}
		break;
	}
	case ECozyUiAction::OpenFacilityFunction: Hud->OpenFacilityFunction(Entry.ActionParam, ContextFacility); break;
	case ECozyUiAction::RotatePlacement: Estate->RotatePlacement(); break;
	case ECozyUiAction::StorePlacing:
	{
		FText Message;
		Estate->StoreFacility(Estate->GetPlacementId(), Message);
		Hud->ShowToast(Message);
		break;
	}
	case ECozyUiAction::ConfirmPlacing:
	{
		FText Message;
		Estate->ConfirmPlacement(Message);
		Hud->ShowToast(Message);
		break;
	}
	case ECozyUiAction::CancelPlacing: Estate->CancelPlacement(); break;
	case ECozyUiAction::TakeOutStored:
	{
		FGuid FacilityId;
		if (FGuid::Parse(Entry.ActionParam.ToString(), FacilityId))
		{
			Estate->BeginPlacement(FacilityId);
		}
		break;
	}
	case ECozyUiAction::GoToFacility:
		Hud->CloseWindow();
		Controller->SelectFacilityByDefinition(Entry.ActionParam);
		break;
	case ECozyUiAction::OpenUpgradeForWindow: Hud->OpenUpgradeFor(ContextFacility); break;
	case ECozyUiAction::OpenUpgrade: Hud->OpenUpgradeFor(FGuid()); break;
	default: break;
	}
}

#undef LOCTEXT_NAMESPACE
