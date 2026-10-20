#include "HAL/IConsoleManager.h"
#include "Environment/CozySurfaceComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TextureResource.h"

// 지면 그림 크기 비교: 평지(Landscape)와 메시 언덕·경사로가 같은 크기·같은 시작점으로 그림을 반복하는지
// 계산값으로 비교하고, 화면에서 이음매를 볼 수 있게 눈금 무늬를 임시로 씌운다 (에셋은 바꾸지 않음 · Claude 후속 2026-10-20).
// Cozy.TerrainTextureCheck 1 = 눈금 무늬 + 비교 기록 · 0 = 원래 흰 그림으로 되돌림
#if !UE_BUILD_SHIPPING
namespace CozyTextureCheck
{
	/** 평지와 메시 재질이 같이 쓰는 그림 칸 이름 (LandscapeLayers.csv 칸) */
	static const TCHAR* TextureNames[] = { TEXT("GrassA_Texture"), TEXT("GrassB_Texture"), TEXT("DirtA_Texture"), TEXT("DirtB_Texture"), TEXT("Rock_Texture"), TEXT("Path_Texture"), TEXT("TestSand_Texture") };

	/** 64×64 눈금: 8칸 바둑판 + 그림 한 장의 왼쪽·아래 가장자리를 빨강·파랑 선으로 표시 (반복 경계가 보이게) */
	static UTexture2D* MakeChecker()
	{
		constexpr int32 N = 64;
		UTexture2D* Texture = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
		Texture->SRGB = true;
		Texture->Filter = TF_Nearest;
		Texture->AddressX = TA_Wrap; Texture->AddressY = TA_Wrap;
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < N; ++Y)
		for (int32 X = 0; X < N; ++X)
		{
			const bool bDark = ((X / 8) + (Y / 8)) % 2 == 0;
			FColor C = bDark ? FColor(70, 70, 70) : FColor(235, 235, 235);
			if (X < 2) C = FColor(230, 40, 40);
			if (Y < 2) C = FColor(40, 80, 230);
			Pixels[Y * N + X] = C;
		}
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

static FAutoConsoleCommandWithWorldAndArgs CozyTerrainTextureCheck(
	TEXT("Cozy.TerrainTextureCheck"), TEXT("1: 평지·언덕·경사로에 눈금 무늬를 임시로 씌우고 그림 반복 크기·시작점 비교 기록 · 0: 원래 그림으로"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace CozyTextureCheck;
		if (!World || !World->IsGameWorld()) return;
		const bool bOn = Args.IsEmpty() || Args[0] != TEXT("0");
		static TWeakObjectPtr<UTexture2D> Checker;
		if (bOn && !Checker.IsValid()) { Checker = MakeChecker(); Checker->AddToRoot(); }
		UTexture* Shown = bOn ? static_cast<UTexture*>(Checker.Get()) : LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		FString Report;
		int32 Failed = 0;
		TArray<double> Repeats;
		TArray<FVector2D> Origins;
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			// LandscapeLayerCoords(크기 배율 1) × TextureTiling → 그림 한 장 = 랜드스케이프 한 칸 크기 ÷ Tiling, 시작점 = 액터 위치
			float Tiling = .25f;
			if (UMaterialInterface* Material = It->GetLandscapeMaterial()) Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("TextureTiling")), Tiling);
			const FVector Scale = It->GetActorScale3D();
			const FVector Origin = It->GetActorLocation();
			const double Repeat = Scale.X / FMath::Max(.0001f, Tiling);
			Repeats.Add(Repeat); Origins.Add(FVector2D(Origin.X, Origin.Y));
			Report += FString::Printf(TEXT("평지 %s: 한 칸 %.1fcm × Tiling %.3f → 그림 한 장 %.1fcm · 시작점 (%.0f, %.0f)\n"), *It->GetName(), Scale.X, Tiling, Repeat, Origin.X, Origin.Y);
			if (Shown) for (const TCHAR* Name : TextureNames) It->SetLandscapeMaterialTextureParameterValue(Name, Shown);
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<ALandscapeProxy>() || !It->FindComponentByClass<UCozySurfaceComponent>()) continue;
			TArray<UMeshComponent*> Meshes;
			It->GetComponents(Meshes);
			for (UMeshComponent* Mesh : Meshes)
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));
				if (!Material) Material = Mesh->CreateDynamicMaterialInstance(Slot);
				if (!Material) continue;
				// 메시 재질: (월드 XY - CozyTextureOrigin) × TextureTiling / 64 → 그림 한 장 = 64 ÷ Tiling
				float Tiling = .25f; FLinearColor Origin(-8064, -8064, 20, 0);
				Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("TextureTiling")), Tiling);
				Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("CozyTextureOrigin")), Origin);
				const double Repeat = 64.0 / FMath::Max(.0001f, Tiling);
				Repeats.Add(Repeat); Origins.Add(FVector2D(Origin.R, Origin.G));
				const FString Id = It->FindComponentByClass<UCozySurfaceComponent>()->SurfaceId.ToString();
				Report += FString::Printf(TEXT("메시 표면 %s..%s: 64 ÷ Tiling %.3f → 그림 한 장 %.1fcm · 시작점 (%.0f, %.0f)\n"), *Id.Left(4), *Id.Right(4), Tiling, Repeat, Origin.R, Origin.G);
				if (Shown) Material->SetTextureParameterValue(TEXT("GrassA_Texture"), Shown);
			}
		}
		bool bSame = Repeats.Num() >= 2;
		for (int32 I = 1; I < Repeats.Num(); ++I)
		{
			// 시작점 차이가 그림 한 장 크기의 정수배면 무늬가 그대로 이어진다
			const double Shift = FMath::Fmod(FMath::Abs(Origins[I].X - Origins[0].X), Repeats[0]) + FMath::Fmod(FMath::Abs(Origins[I].Y - Origins[0].Y), Repeats[0]);
			const double Phase = FMath::Min(Shift, FMath::Abs(Repeats[0] - Shift));
			if (FMath::Abs(Repeats[I] - Repeats[0]) > .5 || Phase > .5) bSame = false;
		}
		if (!bSame) ++Failed;
		Report += FString::Printf(TEXT("%s 평지·메시 그림 한 장 크기와 시작점이 같음 (표면 %d개 비교)\n눈금 무늬: %s\n실패 합계: %d\n"),
			bSame ? TEXT("통과") : TEXT("실패"), Repeats.Num(), bOn ? TEXT("씌움 (빨간 선 = 그림 한 장의 왼쪽 끝 · 파란 선 = 아래 끝)") : TEXT("원래 흰 그림으로 되돌림"), Failed);
		FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TerrainTextureCheck.txt")), FFileHelper::EEncodingOptions::ForceUTF8);
		UE_LOG(LogTemp, Display, TEXT("[지면 그림 크기 검사]\n%s"), *Report);
	}));
#endif
