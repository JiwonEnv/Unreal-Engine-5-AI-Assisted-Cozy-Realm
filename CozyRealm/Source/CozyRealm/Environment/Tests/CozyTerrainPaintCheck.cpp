#include "HAL/IConsoleManager.h"
#include "Environment/CozySurfaceComponent.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Crc.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/Tests/CozyTerrainTestUtils.h"

// 브러시 실제 입력 경로(PaintBrushTo)로 빠른 드래그·표면 경계·드래그 단위 되돌리기·재실행 후 일치를 검사한다. (Claude 후속 2026-10-20)
#if !UE_BUILD_SHIPPING
namespace CozyPaintCheck
{
	/** 위에서 내려다봤을 때 보이는 표면 (화면에서 마우스가 닿는 곳과 같음) */
	static bool TopHit(UWorld* World, double X, double Y, FCozySurfaceHit& Out)
	{
		return CozySurface::Trace(World, FVector(X, Y, 5000), FVector(X, Y, -5000), Out);
	}

	/** 이 지점의 덧칠 비율 (0~255) · 표면 영역 밖이면 -1 */
	static int32 LayerAt(const UCozySurfaceComponent* Surface, const FVector& Position, int32 Layer)
	{
		FVector2D UV;
		const FCozyPaintMask& Mask = Surface->GetPaintMask();
		if (!Surface->WorldToPaintUV(Position, UV) || Mask.Pixels.Num() != Mask.Resolution * Mask.Resolution) return -1;
		const int32 X = FMath::Clamp(FMath::FloorToInt(UV.X * Mask.Resolution), 0, Mask.Resolution - 1);
		const int32 Y = FMath::Clamp(FMath::FloorToInt(UV.Y * Mask.Resolution), 0, Mask.Resolution - 1);
		const FColor P = Mask.Pixels[Y * Mask.Resolution + X];
		const uint8 Channels[] = { P.R, P.G, P.B, P.A };
		return Channels[FMath::Clamp(Layer, 0, 3)];
	}

	static TArray<UCozySurfaceComponent*> Surfaces(UWorld* World)
	{
		TArray<UCozySurfaceComponent*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
			if (UCozySurfaceComponent* Surface = It->FindComponentByClass<UCozySurfaceComponent>()) Result.Add(Surface);
		return Result;
	}

	static FString HashLine(const UCozySurfaceComponent* Surface)
	{
		const FCozyPaintMask& Mask = Surface->GetPaintMask();
		return FString::Printf(TEXT("%s %08X"), *Surface->PaintDomainId.ToString(), FCrc::MemCrc32(Mask.Pixels.GetData(), Mask.Pixels.Num() * sizeof(FColor)));
	}

	/** 선을 따라 Frames번에 나눠 끌고 놓는다 (Frames=1이면 한 프레임에 끝까지 = 매우 빠른 드래그) */
	static void Drag(UWorld* World, ACozyRealmEstatePlayerController* Controller, const FVector2D& From, const FVector2D& To, int32 Frames)
	{
		FCozySurfaceHit Hit;
		if (TopHit(World, From.X, From.Y, Hit)) Controller->PaintBrushTo(Hit);
		for (int32 I = 1; I <= Frames; ++I)
		{
			const FVector2D P = FMath::Lerp(From, To, double(I) / Frames);
			if (TopHit(World, P.X, P.Y, Hit)) Controller->PaintBrushTo(Hit);
		}
		Controller->FinishGroundStroke();
	}
}

static FAutoConsoleCommandWithWorld CozyTerrainPaintCheck(
	TEXT("Cozy.TerrainPaintVerify"), TEXT("격리된 검증 맵에서 빠른 드래그·표면 경계 연속성·드래그 단위 되돌리기 검사 후 마스크 값을 저장"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		using namespace CozyPaintCheck;
		if (!CozyTerrainTest::IsAllowed(World)) return;
		auto* Estate = World->GetSubsystem<UCozyEstateSubsystem>();
		auto* Controller = Cast<ACozyRealmEstatePlayerController>(World->GetFirstPlayerController());
		if (!Estate || !Controller) return;
		Estate->Tick(0.f);
		FString Report;
		int32 Failed = 0;
		auto Check = [&](bool bOk, const FString& Name) { Report += FString::Printf(TEXT("%s %s\n"), bOk ? TEXT("통과") : TEXT("실패"), *Name); if (!bOk) ++Failed; };
		const TArray<UCozySurfaceComponent*> All = Surfaces(World);
		TMap<UCozySurfaceComponent*, TArray<FColor>> Before;
		for (UCozySurfaceComponent* Surface : All) Before.Add(Surface, Surface->GetPaintMask().Pixels);
		auto SameAs = [&](const TMap<UCozySurfaceComponent*, TArray<FColor>>& Snapshot)
		{
			for (UCozySurfaceComponent* Surface : All) if (Surface->GetPaintMask().Pixels != Snapshot[Surface]) return false;
			return true;
		};

		// 1) 평지에서 같은 선을 한 프레임(매우 빠름)과 40프레임(느림)으로 끌어 비교
		Controller->SetBrushForTest(80.f, 2);
		const FVector2D A(-1100, -900), B(-300, -900);
		FCozySurfaceHit Start;
		const bool bGround = TopHit(World, A.X, A.Y, Start) && Start.Rules.IsValid();
		Check(bGround, TEXT("평지 시험 지점에 도색 가능한 표면이 있음"));
		if (bGround)
		{
			UCozySurfaceComponent* Ground = Start.Rules.Get();
			Drag(World, Controller, A, B, 1);
			const TArray<FColor> Fast = Ground->GetPaintMask().Pixels;
			int32 Gaps = 0, Samples = 0;
			for (double X = A.X; X <= B.X; X += 5)
			{
				FCozySurfaceHit Hit; ++Samples;
				if (!TopHit(World, X, A.Y, Hit) || Hit.Rules.Get() != Ground || LayerAt(Ground, Hit.Position, 2) < 128) ++Gaps;
			}
			Check(Gaps == 0, FString::Printf(TEXT("한 프레임에 800cm를 끌어도 빈틈 없음 (5cm 간격 %d곳 중 빈틈 %d)"), Samples, Gaps));
			Check(Controller->UndoGroundPaint() && SameAs(Before), TEXT("빠른 드래그 되돌리기 후 원래 마스크"));
			Drag(World, Controller, A, B, 40);
			const TArray<FColor>& Slow = Ground->GetPaintMask().Pixels;
			int32 Different = 0, MaxDiff = 0, FastCovered = 0, SlowCovered = 0;
			for (int32 I = 0; I < Slow.Num(); ++I)
			{
				// 2번 색 = B 채널
				const int32 Diff = FMath::Abs(int32(Fast[I].B) - int32(Slow[I].B));
				if (Diff > 0) ++Different;
				MaxDiff = FMath::Max(MaxDiff, Diff);
				FastCovered += Fast[I].B >= 128; SlowCovered += Slow[I].B >= 128;
			}
			// 지면 높이가 조금씩 달라 찍는 점 위치가 미세하게 다를 수 있다 → 칠해진 넓이가 2% 안이면 같은 세기로 본다
			const double Ratio = FastCovered > 0 ? double(SlowCovered) / FastCovered : 0;
			Check(FMath::Abs(Ratio - 1.0) <= .02, FString::Printf(TEXT("프레임 수와 무관 (1프레임 칠한 칸 %d · 40프레임 %d · 차이 있는 칸 %d · 최대 차 %d)"), FastCovered, SlowCovered, Different, MaxDiff));
			Check(Controller->UndoGroundPaint() && SameAs(Before), TEXT("느린 드래그 되돌리기 후 원래 마스크"));
		}

		// 2) 평지 → 경사로 → 언덕 상판을 한 번에 빠르게 (프레임당 300cm) 가로지르는 획
		Controller->SetBrushForTest(80.f, 1);
		// 고립 상판(X -1000~-600) 옆 평지에서 출발
		const FVector2D From(-560, 0), To(700, 0);
		Drag(World, Controller, From, To, 5);
		TMap<UCozySurfaceComponent*, TArray<FColor>> After;
		for (UCozySurfaceComponent* Surface : All) After.Add(Surface, Surface->GetPaintMask().Pixels);
		TSet<UCozySurfaceComponent*> Touched;
		int32 Gaps = 0, Samples = 0, Skipped = 0;
		FString Boundaries, GapAt;
		const UCozySurfaceComponent* Last = nullptr;
		for (double X = From.X; X <= To.X; X += 10)
		{
			FCozySurfaceHit Hit;
			if (!TopHit(World, X, 0, Hit) || !Hit.Rules.IsValid()) { ++Skipped; continue; }
			UCozySurfaceComponent* Surface = Hit.Rules.Get();
			if (Surface != Last) { Boundaries += FString::Printf(TEXT(" %.0fcm(경사 %.0f도)"), X, CozySurface::SlopeDegrees(Hit.Normal)); Last = Surface; }
			// 칠할 수 없는 면(벽·급경사)은 빈틈 계산에서 뺀다
			if (!Surface->bAllowPainting || Hit.Normal.Z < .85f) { ++Skipped; continue; }
			++Samples; Touched.Add(Surface);
			if (LayerAt(Surface, Hit.Position, 1) < 128) { ++Gaps; GapAt += FString::Printf(TEXT(" %.0fcm"), X); }
		}
		if (!GapAt.IsEmpty()) Report += FString::Printf(TEXT("참고: 빈틈 위치%s\n"), *GapAt);
		Report += FString::Printf(TEXT("참고: 선 위 표면이 바뀌는 곳%s · 칠할 수 없어 뺀 지점 %d\n"), *Boundaries, Skipped);
		Check(Touched.Num() >= 2 && Gaps == 0, FString::Printf(TEXT("표면 %d개를 지나는 빠른 획이 경계 앞뒤로 끊기지 않음 (10cm 간격 %d곳 중 빈틈 %d)"), Touched.Num(), Samples, Gaps));
		Check(Controller->UndoGroundPaint() && SameAs(Before), TEXT("여러 표면에 걸친 한 드래그를 Ctrl+Z 한 번에 모두 되돌림"));
		Check(Controller->RedoGroundPaint() && SameAs(After), TEXT("Ctrl+Y 한 번에 여러 표면을 모두 다시 칠함"));

		// 3) 다시 칠한 상태를 저장하고 마스크 값을 남긴다 → 재실행 후 Cozy.TerrainPaintReloadVerify로 비교
		Estate->SaveEstate(TEXT("도색 검사")); Estate->FlushPendingSave();
		FString Hashes;
		for (const UCozySurfaceComponent* Surface : All) Hashes += HashLine(Surface) + TEXT("\n");
		FFileHelper::SaveStringToFile(Hashes, *(FPaths::ProjectSavedDir() / TEXT("TerrainPaintHashes.txt")), FFileHelper::EEncodingOptions::ForceUTF8);
		Report += FString::Printf(TEXT("저장한 표면 마스크 %d개 값 기록 (TerrainPaintHashes.txt)\n실패 합계: %d\n"), All.Num(), Failed);
		FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TerrainPaintCheck.txt")), FFileHelper::EEncodingOptions::ForceUTF8);
		UE_LOG(LogTemp, Display, TEXT("[도색 검사]\n%s"), *Report);
	}));

static FAutoConsoleCommandWithWorld CozyTerrainPaintReloadCheck(
	TEXT("Cozy.TerrainPaintReloadVerify"), TEXT("재실행 후 불러온 표면 마스크가 Cozy.TerrainPaintVerify가 남긴 값과 같은지 검사"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		using namespace CozyPaintCheck;
		if (!CozyTerrainTest::IsAllowed(World)) return;
		FString Saved;
		const FString Folder = FPaths::ProjectSavedDir();
		int32 Failed = 0, Matched = 0;
		FString Report;
		if (!FFileHelper::LoadFileToString(Saved, *(Folder / TEXT("TerrainPaintHashes.txt"))))
		{
			Report = TEXT("실패 TerrainPaintHashes.txt 없음 (먼저 Cozy.TerrainPaintVerify)\n"); ++Failed;
		}
		else
		{
			TArray<FString> Lines;
			Saved.ParseIntoArrayLines(Lines);
			const TArray<UCozySurfaceComponent*> All = Surfaces(World);
			for (const UCozySurfaceComponent* Surface : All)
			{
				const FString Now = HashLine(Surface);
				if (Lines.Contains(Now)) ++Matched; else { ++Failed; Report += FString::Printf(TEXT("실패 다름: %s\n"), *Now); }
			}
			Report += FString::Printf(TEXT("재실행 후 CPU 마스크 일치 %d / %d (저장 기록 %d줄)\n"), Matched, All.Num(), Lines.Num());
			if (Lines.Num() != All.Num()) { ++Failed; Report += TEXT("실패 표면 수가 저장 기록과 다름\n"); }
		}
		Report += FString::Printf(TEXT("실패 합계: %d\n"), Failed);
		FFileHelper::SaveStringToFile(Report, *(Folder / TEXT("TerrainPaintReloadCheck.txt")), FFileHelper::EEncodingOptions::ForceUTF8);
		UE_LOG(LogTemp, Display, TEXT("[도색 재실행 검사]\n%s"), *Report);
	}));
#endif
