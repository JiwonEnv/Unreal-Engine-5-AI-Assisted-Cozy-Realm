#include "Core/CozyRealmEstatePlayerController.h"
#include "Environment/CozySurfaceComponent.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Hero/CozyHeroCharacter.h"
#include "UI/CozyHudWidget.h"
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "InputKeyEventArgs.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "CozyRealm.h"

static TAutoConsoleVariable<int32> CVarCozySurfaceDebug(TEXT("Cozy.SurfaceDebug"), 0,
	TEXT("1: 마우스 아래 표면 ID·경사, 표면별 도색 영역, 주인공 발-지면 높이 차이를 화면에 표시"));

bool ACozyRealmEstatePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const bool Handled = Super::InputKey(Params);
	if (Params.Key == EKeys::LeftControl) bGroundLeftControl = Params.Event != IE_Released;
	if (Params.Key == EKeys::RightControl) bGroundRightControl = Params.Event != IE_Released;
	// 프레임 마지막 상태 대신 입력 순서대로 판단해야 빠른 Ctrl+Z/Y도 놓치지 않는다.
	if (GroundMode == 1 && Params.Event == IE_Pressed && (bGroundLeftControl || bGroundRightControl)
		&& (Params.Key == EKeys::Z || Params.Key == EKeys::Y) && !(Hud && Hud->IsWindowOpen()))
	{
		const bool Changed = Params.Key == EKeys::Z ? UndoGroundPaint() : RedoGroundPaint();
		if (Changed) GetWorld()->GetSubsystem<UCozyEstateSubsystem>()->SaveEstate(TEXT("도색 되돌리기"));
		return true;
	}
	return Handled;
}

void ACozyRealmEstatePlayerController::FlushPressedKeys()
{
	bGroundLeftControl = bGroundRightControl = false;
	FinishGroundStroke();
	Super::FlushPressedKeys();
}

void ACozyRealmEstatePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FinishGroundStroke();
	Super::EndPlay(EndPlayReason);
}

void ACozyRealmEstatePlayerController::CozyGroundMode(int32 Mode)
{
	FinishGroundStroke();
	SetPlacementMode(false);
	GroundMode = FMath::Clamp(Mode, 0, 2);
	bFloorPreview = false; MovingFloorId.Invalidate();
	if (Hero) Hero->SetFrozen(GroundMode != 0);
	if (Hud) Hud->ShowToast(FText::FromString(GroundMode == 1
		? TEXT("지면 브러시 · 1~3 색 · 0 지우개 · [ ] 크기 · Ctrl+Z/Y · G 타일 · Esc 종료")
		: GroundMode == 2 ? TEXT("바닥 타일 · 클릭 미리보기 · Enter 확정 · 우클릭 취소 · H 보관 · U 꺼내기 · V 이동 · Shift+클릭 제거") : TEXT("바닥 꾸미기를 종료했습니다")));
}

void ACozyRealmEstatePlayerController::EndPaintSegment()
{
	if (StrokeSurface.IsValid() && StrokeSurface->EndPaintStroke()) DragSurfaces.Add(StrokeSurface);
	StrokeSurface.Reset();
}

void ACozyRealmEstatePlayerController::FinishGroundStroke()
{
	EndPaintSegment();
	bPaintDragActive = false;
	if (DragSurfaces.IsEmpty()) return;
	// 한 드래그가 평지·경사로·언덕에 걸쳐도 되돌리기는 한 번에 (Claude 후속 2026-10-20)
	PaintUndoGroups.Add(MoveTemp(DragSurfaces));
	DragSurfaces.Reset();
	if (PaintUndoGroups.Num() > 16) PaintUndoGroups.RemoveAt(0);
	PaintRedoGroups.Reset();
	if (auto* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>()) Estate->SaveEstate(TEXT("지면 브러시"));
}

bool ACozyRealmEstatePlayerController::UndoGroundPaint()
{
	FinishGroundStroke();
	while (!PaintUndoGroups.IsEmpty())
	{
		TArray<TWeakObjectPtr<UCozySurfaceComponent>> Group = PaintUndoGroups.Pop();
		bool bAny = false;
		for (int32 I = Group.Num() - 1; I >= 0; --I) if (Group[I].IsValid()) bAny |= Group[I]->UndoPaint();
		if (bAny) { PaintRedoGroups.Add(MoveTemp(Group)); return true; }
	}
	return false;
}

bool ACozyRealmEstatePlayerController::RedoGroundPaint()
{
	FinishGroundStroke();
	while (!PaintRedoGroups.IsEmpty())
	{
		TArray<TWeakObjectPtr<UCozySurfaceComponent>> Group = PaintRedoGroups.Pop();
		bool bAny = false;
		for (const TWeakObjectPtr<UCozySurfaceComponent>& Surface : Group) if (Surface.IsValid()) bAny |= Surface->RedoPaint();
		if (bAny) { PaintUndoGroups.Add(MoveTemp(Group)); return true; }
	}
	return false;
}

void ACozyRealmEstatePlayerController::PaintBrushTo(const FCozySurfaceHit& Hit)
{
	UCozySurfaceComponent* Surface = Hit.Rules.Get();
	// 절벽 벽면·칠할 수 없는 곳을 지날 때는 지금 표면 기록만 닫고 드래그는 이어 간다.
	if (!Surface || !Surface->bAllowPainting || Hit.Normal.Z < .85f) { EndPaintSegment(); return; }
	auto Paintable = [this](UCozySurfaceComponent* Target, const FVector& Position, FCozySurfaceHit& Out)
	{
		return Target && Target->bAllowPainting && CozySurface::GroundAt(GetWorld(), Position, Out, &Target->SurfaceId)
			&& Out.Rules.Get() == Target && Out.Normal.Z >= .85f;
	};
	auto StampOn = [this](UCozySurfaceComponent* Target, const FVector& Position)
	{
		if (StrokeSurface.Get() != Target) { EndPaintSegment(); StrokeSurface = Target; Target->BeginPaintStroke(); }
		return Target->PaintAt(Position, BrushRadius, GroundStyle, .6f);
	};
	if (!bPaintDragActive)
	{
		bPaintDragActive = true;
		LastBrushPosition = Hit.Position;
		const bool Changed = StampOn(Surface, Hit.Position);
		if (FParse::Param(FCommandLine::Get(), TEXT("CozyTerrainTest")))
		{
			FVector2D UV; const bool InBounds = Surface->WorldToPaintUV(Hit.Position, UV);
			UE_LOG(LogTemp, Display, TEXT("[도색 입력] 표면=%s 위치=%s UV=%s 영역=%d 레이어=%d 픽셀=%d 변경=%d"),
				*Surface->SurfaceId.ToString(), *Hit.Position.ToString(), *UV.ToString(), InBounds, GroundStyle, Surface->GetPaintMask().Pixels.Num(), Changed);
		}
		return;
	}
	// 이동 거리에 따라 찍으므로 프레임 수와 상관없이 일정한 밀도로 칠한다.
	// 표면이 바뀌어도 이전 위치부터 이어서 찍어 경계 앞뒤에 빈틈이 생기지 않게 한다 (Claude 후속 2026-10-20).
	const float Step = FMath::Max(10.f, BrushRadius * .2f);
	const FVector Delta = Hit.Position - LastBrushPosition;
	const int32 Count = FMath::Min(512, FMath::FloorToInt(Delta.Size() / Step));
	const FVector StepVector = Delta.GetSafeNormal() * Step;
	for (int32 I = 0; I < Count; ++I)
	{
		LastBrushPosition += StepVector;
		UCozySurfaceComponent* Previous = StrokeSurface.Get();
		FCozySurfaceHit OnTarget, OnPrevious;
		const bool bTarget = Paintable(Surface, LastBrushPosition, OnTarget);
		const bool bPrevious = Previous && Previous != Surface && Paintable(Previous, LastBrushPosition, OnPrevious);
		if (bTarget && bPrevious)
		{
			// 같은 XY에 두 표면이 겹치면 위에서 보이는 쪽(높은 쪽)을 칠한다 (경사로 끝이 언덕 상판 밑으로 들어간 곳).
			// 단, 붓보다 1m 넘게 높은 면(절벽 위)은 지나가는 중이 아니므로 붓 높이에 가까운 쪽.
			const bool bTargetHigher = OnTarget.Position.Z >= OnPrevious.Position.Z;
			const FCozySurfaceHit& Higher = bTargetHigher ? OnTarget : OnPrevious;
			const bool bUseHigher = Higher.Position.Z - LastBrushPosition.Z <= 100.f;
			const bool bUseTarget = bUseHigher ? bTargetHigher : !bTargetHigher;
			StampOn(bUseTarget ? Surface : Previous, bUseTarget ? OnTarget.Position : OnPrevious.Position);
		}
		else if (bTarget) StampOn(Surface, OnTarget.Position);
		else if (bPrevious) StampOn(Previous, OnPrevious.Position);
	}
}

void ACozyRealmEstatePlayerController::DrawSurfaceDebug(const FCozySurfaceHit* InHit) const
{
#if ENABLE_DRAW_DEBUG
	if (CVarCozySurfaceDebug.GetValueOnGameThread() <= 0) return;
	UWorld* World = GetWorld();
	static const FColor Colors[] = { FColor::Cyan, FColor::Orange, FColor::Magenta, FColor::Yellow, FColor::Green };
	int32 Index = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UCozySurfaceComponent* Surface = It->FindComponentByClass<UCozySurfaceComponent>();
		if (!Surface) continue;
		const FColor Color = Colors[Index++ % UE_ARRAY_COUNT(Colors)];
		const FTransform T = It->GetActorTransform();
		// 메시 표면은 윗면 높이(로컬 경계 상자 위)에 테두리를 그린다
		const UStaticMeshComponent* Mesh = It->FindComponentByClass<UStaticMeshComponent>();
		const double Top = Mesh && Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetBoundingBox().Max.Z : 0.0;
		const FVector A = T.TransformPosition(FVector(Surface->PaintMin.X, Surface->PaintMin.Y, Top));
		const FVector B = T.TransformPosition(FVector(Surface->PaintMax.X, Surface->PaintMin.Y, Top));
		const FVector C = T.TransformPosition(FVector(Surface->PaintMax.X, Surface->PaintMax.Y, Top));
		const FVector D = T.TransformPosition(FVector(Surface->PaintMin.X, Surface->PaintMax.Y, Top));
		// 메시 상판은 로컬 Z가 0이 아닐 수 있어 실제 지면 높이에 맞춘 중심에 이름을 단다.
		FCozySurfaceHit Center;
		const FVector Mid = (A + C) * .5;
		const FVector Label = CozySurface::GroundAt(World, Mid, Center, &Surface->SurfaceId) ? Center.Position : Mid;
		DrawDebugLine(World, A, B, Color, false, -1, 0, 3); DrawDebugLine(World, B, C, Color, false, -1, 0, 3);
		DrawDebugLine(World, C, D, Color, false, -1, 0, 3); DrawDebugLine(World, D, A, Color, false, -1, 0, 3);
		const FString Id = Surface->SurfaceId.ToString();
		DrawDebugString(World, Label + FVector(0, 0, 40), FString::Printf(TEXT("Surface %s..%s"), *Id.Left(4), *Id.Right(4)), nullptr, Color, 0.f, true, 1.2f);
	}
	FCozySurfaceHit Hit;
	FVector Origin, Direction;
	if (InHit) Hit = *InHit;
	const bool bHit = InHit || (DeprojectMousePositionToWorld(Origin, Direction) && CozySurface::Trace(World, Origin, Origin + Direction * 100000.f, Hit));
	if (bHit)
	{
		const float Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hit.Normal.Z, -1.f, 1.f)));
		const FString Id = Hit.SurfaceId.ToString();
		DrawDebugString(World, Hit.Position + FVector(0, 0, 60), FString::Printf(TEXT("Cursor: surface %s..%s  slope %.0f deg  z %.0f"),
			*Id.Left(4), *Id.Right(4), Slope, Hit.Position.Z), nullptr, FColor::White, 0.f, true, 1.2f);
	}
	if (Hero)
	{
		const FVector Feet = Hero->GetActorLocation() - FVector(0, 0, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		FCozySurfaceHit Ground;
		if (CozySurface::GroundAt(World, Feet, Ground))
		{
			const float Gap = Feet.Z - Ground.Position.Z;
			const float BodyGap = Gap - Hero->GetVisualDrop();
			DrawDebugString(World, Hero->GetActorLocation() + FVector(0, 0, 140), FString::Printf(TEXT("Capsule-ground %.1f cm / Body-ground %.1f cm"), Gap, BodyGap), nullptr, FMath::Abs(BodyGap) <= 1.f ? FColor::Green : FColor::Red, 0.f, true, 1.2f);
			static double LastLog = 0;
			const double Now = World->GetTimeSeconds();
			if (Now - LastLog >= .5)
			{
				LastLog = Now;
				UE_LOG(LogCozyRealm, Log, TEXT("[표면 디버그] 주인공 캡슐-지면 %.1fcm · 보이는 몸-지면 %.1fcm · 표면 %s · 경사 %.0f도 · 위치 (%.0f, %.0f, %.0f)"), Gap, BodyGap,
					Ground.Rules.IsValid() ? *Ground.SurfaceId.ToString().Left(8) : TEXT("Landscape"),
					FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Ground.Normal.Z, -1.f, 1.f))), Feet.X, Feet.Y, Feet.Z);
			}
		}
	}
#endif
}

void ACozyRealmEstatePlayerController::TickGroundEditing()
{
	if (Hud && Hud->IsWindowOpen()) { FinishGroundStroke(); return; }
	if (WasInputKeyJustPressed(EKeys::Zero)) GroundStyle = 0;
	if (WasInputKeyJustPressed(EKeys::One)) GroundStyle = 1;
	if (WasInputKeyJustPressed(EKeys::Two)) GroundStyle = 2;
	if (WasInputKeyJustPressed(EKeys::Three)) GroundStyle = 3;
	if (WasInputKeyJustPressed(EKeys::R)) GroundRotation = (GroundRotation + 1) % 4;
	if (WasInputKeyJustPressed(EKeys::LeftBracket)) BrushRadius = FMath::Max(40.f, BrushRadius - 40.f);
	if (WasInputKeyJustPressed(EKeys::RightBracket)) BrushRadius = FMath::Min(400.f, BrushRadius + 40.f);
	if (!IsInputKeyDown(EKeys::LeftMouseButton)) FinishGroundStroke();
	FVector Origin, Direction;
	FCozySurfaceHit Hit;
	if (!DeprojectMousePositionToWorld(Origin, Direction) || !CozySurface::Trace(GetWorld(), Origin, Origin + Direction * 100000.f, Hit))
	{
		EndPaintSegment(); return;
	}
	DrawDebugCircle(GetWorld(), Hit.Position + FVector(0, 0, 3), BrushRadius, 32, FColor::Cyan, false, -1, 0, 2, FVector::ForwardVector, FVector::RightVector, false);
	if (GroundMode == 2)
    {
        UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
        FText Reason;
        if (WasInputKeyJustPressed(EKeys::RightMouseButton)) { bFloorPreview = false; MovingFloorId.Invalidate(); }
        const FCozyFloorTile* Hovered = Estate->FindFloorTile(Hit.Position, Hit.SurfaceId);
        if (Hovered && WasInputKeyJustPressed(EKeys::H))
        {
            Estate->StoreFloorTile(Hovered->InstanceId, Reason);
            bFloorPreview = false; MovingFloorId.Invalidate();
        }
        if (Hovered && WasInputKeyJustPressed(EKeys::V))
        {
            MovingFloorId = Hovered->InstanceId; GroundStyle = Hovered->Style + 1;
            GroundRotation = FMath::RoundToInt(Hovered->Transform.Rotator().Yaw / 90.f);
            bFloorPreview = true;
        }
        if (WasInputKeyJustPressed(EKeys::U))
        {
            const FCozyFloorTile* Stored = Estate->GetState().FloorTiles.FindByPredicate([](const FCozyFloorTile& Tile) { return Tile.bStored; });
            if (Stored) { MovingFloorId = Stored->InstanceId; GroundStyle = Stored->Style + 1; bFloorPreview = true; }
            else Reason = FText::FromString(TEXT("보관한 바닥이 없습니다"));
        }
        if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
        {
            if (IsInputKeyDown(EKeys::LeftShift))
            {
                Estate->EditFloorTile(Hit.Position, 0, 0, true, Reason, &Hit.SurfaceId);
                bFloorPreview = false; MovingFloorId.Invalidate();
            }
            else bFloorPreview = true;
        }
        if (bFloorPreview)
        {
            FloorPreviewPosition = Hit.Position; FloorPreviewSurface = Hit.SurfaceId;
            const auto& Config = Estate->GetConfig();
            const FIntPoint Cell = Estate->WorldToCell(Hit.Position);
            const FVector Min = Config.GridOrigin + FVector(Cell.X * Config.CellSize, Cell.Y * Config.CellSize, 0);
            FCozySurfaceHit Support;
            FText PreviewReason;
            const bool Valid = Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Config.GridSize.X && Cell.Y < Config.GridSize.Y
                && CozySurface::CheckFootprint(GetWorld(), Min, FVector2D(Config.CellSize), Config.CellSize * .25f, Support, PreviewReason, &Hit.SurfaceId);
            const FVector Center = Valid ? Support.Position : Min + FVector(Config.CellSize * .5, Config.CellSize * .5, Hit.Position.Z - Min.Z);
            DrawDebugBox(GetWorld(), Center + FVector(0, 0, 4), FVector(Config.CellSize * .49, Config.CellSize * .49, 3), FQuat(FRotator(0, GroundRotation * 90, 0)), Valid ? FColor::Green : FColor::Red, false, -1, 0, 4);
            if (WasInputKeyJustPressed(EKeys::Enter))
            {
                if (Estate->EditFloorTile(FloorPreviewPosition, FMath::Clamp(GroundStyle - 1, 0, 2), GroundRotation, false, Reason, &FloorPreviewSurface, MovingFloorId))
                { bFloorPreview = false; MovingFloorId.Invalidate(); }
            }
        }
        if (Hud && !Reason.IsEmpty()) Hud->ShowToast(Reason);
        return;
    }

	// 한 프레임 안에 누름·뗌이 끝난 짧은 클릭도 첫 도장을 남긴다.
	if (GroundMode != 1 || (!IsInputKeyDown(EKeys::LeftMouseButton) && !WasInputKeyJustPressed(EKeys::LeftMouseButton))) return;
	PaintBrushTo(Hit);
}
