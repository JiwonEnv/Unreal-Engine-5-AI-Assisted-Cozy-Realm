#include "Environment/CozyEstateTerrain.h"
#include "Environment/CozySeasonPreset.h"
#include "CozyRealm.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

namespace CozyTerrain
{
	/** 구획 하나의 칸 수 (언리얼 권장 크기 63 · 구획당 1구역) */
	constexpr int32 QuadsPerComponent = 63;
	/** 랜드스케이프 높이 값 0 = 32768 · 높이 배율 100이면 1cm = 1.28 */
	constexpr float HeightUnitsPerCm = 128.f / 100.f;
	constexpr float ZScale = 100.f;

	/** 모서리가 둥근 네모 거리 (p=4) · 영지 칸이 네모라 평평한 땅도 네모에 가깝게 */
	float RoundedSquareDistance(float X, float Y)
	{
		return FMath::Pow(FMath::Pow(FMath::Abs(X), 4.f) + FMath::Pow(FMath::Abs(Y), 4.f), 0.25f);
	}

	float Fractal(float X, float Y, float Size, int32 Seed)
	{
		const FVector2D Offset(Seed * 17.31f, Seed * 9.73f);
		float Sum = 0.f;
		float Amp = 1.f;
		float Freq = 1.f / Size;
		float Norm = 0.f;
		for (int32 Octave = 0; Octave < 3; ++Octave)
		{
			Sum += Amp * FMath::PerlinNoise2D(FVector2D(X, Y) * Freq + Offset * (Octave + 1));
			Norm += Amp;
			Amp *= 0.5f;
			Freq *= 2.1f;
		}
		return Sum / Norm;
	}
}

ACozyEstateTerrain::ACozyEstateTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACozyEstateTerrain::BeginPlay()
{
	Super::BeginPlay();
	// 게임 시작 때 지금 계절 값을 지형에 맞춘다 (에디터에서 고른 계절과 같으면 바뀌는 것 없음)
	ApplySeason();
}

void ACozyEstateTerrain::SetSeason(UCozySeasonPreset* NewSeason)
{
	if (!NewSeason || NewSeason == CurrentSeason)
	{
		return;
	}
	CurrentSeason = NewSeason;
	ApplySeason();
}

void ACozyEstateTerrain::CycleSeason()
{
	if (Seasons.Num() == 0)
	{
		return;
	}
	const int32 Index = Seasons.IndexOfByKey(CurrentSeason);
	SetSeason(Seasons[(Index + 1) % Seasons.Num()]);
}

void ACozyEstateTerrain::ApplySeason()
{
	if (!Landscape || !CurrentSeason || !CurrentSeason->LandscapeMaterial)
	{
		return;
	}
	UMaterialInstance* Season = CurrentSeason->LandscapeMaterial;
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		// 게임 중: 구획마다의 동적 머티리얼에 계절 값을 옮겨 적는다 (랜드스케이프의 Use Dynamic Material Instance가 켜져 있어야 함)
		for (const FScalarParameterValue& Value : Season->ScalarParameterValues)
		{
			Landscape->SetLandscapeMaterialScalarParameterValue(Value.ParameterInfo.Name, Value.ParameterValue);
		}
		for (const FVectorParameterValue& Value : Season->VectorParameterValues)
		{
			Landscape->SetLandscapeMaterialVectorParameterValue(Value.ParameterInfo.Name, Value.ParameterValue);
		}
		for (const FTextureParameterValue& Value : Season->TextureParameterValues)
		{
			Landscape->SetLandscapeMaterialTextureParameterValue(Value.ParameterInfo.Name, Value.ParameterValue);
		}
		UE_LOG(LogCozyRealm, Log, TEXT("[지형] 계절 바꿈 (게임 중): %s"), *CurrentSeason->DisplayName.ToString());
		return;
	}
#if WITH_EDITOR
	// 에디터: 랜드스케이프 머티리얼 자체를 그 계절 것으로 바꿔 저장
	if (Landscape->LandscapeMaterial != Season)
	{
		Landscape->Modify();
		Landscape->LandscapeMaterial = Season;
		FProperty* Property = FindFProperty<FProperty>(Landscape->GetClass(), GET_MEMBER_NAME_CHECKED(ALandscapeProxy, LandscapeMaterial));
		FPropertyChangedEvent Event(Property);
		Landscape->PostEditChangeProperty(Event);
		UE_LOG(LogCozyRealm, Log, TEXT("[지형] 계절 바꿈 (에디터): %s"), *CurrentSeason->DisplayName.ToString());
	}
#endif
}

TArray<FName> ACozyEstateTerrain::ReadLayerNames() const
{
	TArray<FName> Names;
	TArray<FString> Lines;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), LayerTablePath);
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		UE_LOG(LogCozyRealm, Warning, TEXT("[지형] 재질 칸 표를 읽지 못함: %s"), *Path);
		return Names;
	}
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		FString Name;
		Lines[Index].TrimStartAndEnd().Split(TEXT(","), &Name, nullptr);
		Name.TrimStartAndEndInline();
		if (!Name.IsEmpty() && !Name.StartsWith(TEXT("#")))
		{
			Names.AddUnique(FName(*Name));
		}
	}
	return Names;
}

#if WITH_EDITOR

void ACozyEstateTerrain::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(ACozyEstateTerrain, CurrentSeason))
	{
		ApplySeason();
	}
}

ULandscapeLayerInfoObject* ACozyEstateTerrain::FindOrCreateLayerInfo(FName LayerName) const
{
	const FString AssetName = FString::Printf(TEXT("LI_%s"), *LayerName.ToString());
	const FString PackageName = LayerInfoFolder / AssetName;
	if (ULandscapeLayerInfoObject* Existing = LoadObject<ULandscapeLayerInfoObject>(nullptr, *(PackageName + TEXT(".") + AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		return Existing;
	}
	UPackage* Package = CreatePackage(*PackageName);
	ULandscapeLayerInfoObject* Info = NewObject<ULandscapeLayerInfoObject>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Info->SetLayerName(LayerName, false);
	FAssetRegistryModule::AssetCreated(Info);
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, Info, *FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), Args);
	UE_LOG(LogCozyRealm, Log, TEXT("[지형] 레이어 정보 만듦: %s"), *PackageName);
	return Info;
}

float ACozyEstateTerrain::HeightAt(float X, float Y) const
{
	const float Distance = CozyTerrain::RoundedSquareDistance(X, Y);
	if (Distance <= FlatHalfSize)
	{
		return 0.f;
	}
	// 평평한 땅 가장자리 → 산등성이까지 부드럽게 오르고, 오를수록 울퉁불퉁함이 커진다
	const float T = FMath::Clamp((Distance - FlatHalfSize) / FMath::Max(1.f, RidgeDistance - FlatHalfSize), 0.f, 1.f);
	const float Rise = FMath::SmoothStep(0.f, 1.f, T);
	const float Noise = CozyTerrain::Fractal(X, Y, NoiseSize, RandomSeed);
	return FMath::Max(-30.f, Rise * RidgeHeight + Noise * NoiseHeight * Rise);
}

void ACozyEstateTerrain::GenerateTerrain()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const TArray<FName> LayerNames = ReadLayerNames();
	if (LayerNames.Num() == 0)
	{
		UE_LOG(LogCozyRealm, Error, TEXT("[지형] 재질 칸이 없어 지형을 만들지 않음 (%s)"), *LayerTablePath);
		return;
	}

	const int32 Quads = ComponentsPerSide * CozyTerrain::QuadsPerComponent;
	const int32 Size = Quads + 1;
	const float HalfExtent = Quads * QuadSize * 0.5f;

	// 1) 높이 (평평한 땅 = GroundZ)
	TArray<float> Heights;
	Heights.SetNumUninitialized(Size * Size);
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			Heights[Y * Size + X] = HeightAt(X * QuadSize - HalfExtent, Y * QuadSize - HalfExtent);
		}
	}
	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(Size * Size);
	for (int32 Index = 0; Index < Heights.Num(); ++Index)
	{
		HeightData[Index] = static_cast<uint16>(FMath::Clamp(32768.f + Heights[Index] * CozyTerrain::HeightUnitsPerCm, 0.f, 65535.f));
	}

	// 2) 처음 재질 칸 (표의 이름이 기본 칸과 같을 때만 칠함 · 나머지 칸은 비워 두고 붓으로 칠함)
	auto Weight = [&](int32 X, int32 Y, FName Layer) -> float
	{
		const float WX = X * QuadSize - HalfExtent;
		const float WY = Y * QuadSize - HalfExtent;
		const float Distance = CozyTerrain::RoundedSquareDistance(WX, WY);
		const float H = Heights[Y * Size + X];
		const float DX = Heights[Y * Size + FMath::Min(X + 1, Size - 1)] - Heights[Y * Size + FMath::Max(X - 1, 0)];
		const float DY = Heights[FMath::Min(Y + 1, Size - 1) * Size + X] - Heights[FMath::Max(Y - 1, 0) * Size + X];
		const float Slope = FMath::Sqrt(DX * DX + DY * DY) / (2.f * QuadSize);
		const float Rock = FMath::Max(FMath::SmoothStep(0.55f, 0.95f, Slope), FMath::SmoothStep(RidgeHeight * 0.75f, RidgeHeight * 1.05f, H));
		const float Forest = FMath::SmoothStep(FlatHalfSize - 300.f, FlatHalfSize + 700.f, Distance);
		const float Patch = FMath::SmoothStep(0.25f, 0.5f, CozyTerrain::Fractal(WX, WY, 900.f, RandomSeed + 3)) * (1.f - Forest) * FMath::SmoothStep(1300.f, 1800.f, Distance);
		if (Layer == TEXT("Rock")) { return Rock; }
		if (Layer == TEXT("GrassB")) { return Forest * (1.f - Rock); }
		if (Layer == TEXT("DirtA")) { return Patch * (1.f - Rock); }
		if (Layer == TEXT("GrassA")) { return (1.f - Forest) * (1.f - Patch) * (1.f - Rock); }
		return 0.f;
	};

	TArray<FLandscapeImportLayerInfo> ImportLayers;
	for (const FName& Name : LayerNames)
	{
		FLandscapeImportLayerInfo& Layer = ImportLayers.Emplace_GetRef(Name);
		Layer.LayerInfo = FindOrCreateLayerInfo(Name);
		Layer.LayerData.SetNumZeroed(Size * Size);
	}
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			TArray<float, TInlineAllocator<16>> Raw;
			float Sum = 0.f;
			for (const FName& Name : LayerNames)
			{
				Sum += Raw.Add_GetRef(Weight(X, Y, Name));
			}
			const bool bEmpty = Sum <= KINDA_SMALL_NUMBER;
			for (int32 L = 0; L < LayerNames.Num(); ++L)
			{
				// 칸 무게의 합은 255 (언리얼 기본 섞기 방식) · 아무 칸도 안 걸리면 첫 칸
				const float W = bEmpty ? (L == 0 ? 1.f : 0.f) : Raw[L] / Sum;
				ImportLayers[L].LayerData[Y * Size + X] = static_cast<uint8>(FMath::RoundToInt(W * 255.f));
			}
		}
	}

	// 3) 랜드스케이프 새로 만들기 (있던 것은 지움)
	if (Landscape)
	{
		Landscape->Destroy();
		Landscape = nullptr;
	}
	const FVector Location(-HalfExtent, -HalfExtent, GroundZ);
	ALandscape* NewLandscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	NewLandscape->SetActorRelativeScale3D(FVector(QuadSize, QuadSize, CozyTerrain::ZScale));
	NewLandscape->bUseDynamicMaterialInstance = true;
	if (CurrentSeason && CurrentSeason->LandscapeMaterial)
	{
		NewLandscape->LandscapeMaterial = CurrentSeason->LandscapeMaterial;
	}

	TMap<FGuid, TArray<uint16>> HeightPerLayer;
	HeightPerLayer.Add(FGuid(), MoveTemp(HeightData));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialPerLayer;
	MaterialPerLayer.Add(FGuid(), ImportLayers);
	NewLandscape->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, 1, CozyTerrain::QuadsPerComponent, HeightPerLayer, TEXT(""), MaterialPerLayer,
		ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	NewLandscape->SetActorLabel(TEXT("Estate_Landscape"));
	if (ULandscapeInfo* Info = NewLandscape->GetLandscapeInfo())
	{
		Info->UpdateLayerInfoMap(NewLandscape);
	}
	Landscape = NewLandscape;
	SyncLayers();
	Modify();
	MarkPackageDirty();
	UE_LOG(LogCozyRealm, Log, TEXT("[지형] 새로 만듦: %d×%d 꼭짓점 · 칸 %.0fcm · 재질 칸 %d개"), Size, Size, QuadSize, LayerNames.Num());
}

void ACozyEstateTerrain::SyncLayers()
{
	if (!Landscape)
	{
		UE_LOG(LogCozyRealm, Warning, TEXT("[지형] 랜드스케이프가 없어 재질 칸을 붙이지 못함 · 먼저 '지형 새로 만들기'"));
		return;
	}
	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	int32 Added = 0;
	for (const FName& Name : ReadLayerNames())
	{
		ULandscapeLayerInfoObject* LayerInfo = FindOrCreateLayerInfo(Name);
		if (Info && Info->GetLayerInfoIndex(Name) != INDEX_NONE && Info->Layers[Info->GetLayerInfoIndex(Name)].LayerInfoObj == LayerInfo)
		{
			continue;
		}
		Landscape->AddTargetLayer(Name, FLandscapeTargetLayerSettings(LayerInfo));
		if (Info)
		{
			Info->UpdateLayerInfoMap(Landscape);
			const int32 Index = Info->GetLayerInfoIndex(Name);
			if (Info->Layers.IsValidIndex(Index))
			{
				Info->Layers[Index].LayerInfoObj = LayerInfo;
			}
		}
		++Added;
	}
	Landscape->MarkPackageDirty();
	UE_LOG(LogCozyRealm, Log, TEXT("[지형] 재질 칸 갱신: %d개 붙임"), Added);
}

#endif
