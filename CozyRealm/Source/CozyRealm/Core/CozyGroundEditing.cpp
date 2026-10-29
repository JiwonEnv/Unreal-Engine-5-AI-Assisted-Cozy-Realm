#include "Core/CozyRealmEstatePlayerController.h"
#include "Environment/CozySurfaceComponent.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Hero/CozyHeroCharacter.h"
#include "UI/CozyHudWidget.h"
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "InputKeyEventArgs.h"
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
	CancelFloorPreview();
	// 타일 모드의 종류 번호는 1~3 (GroundStyle - 1 = 타일 0~2) · 붓 지우개(0)에서 넘어오면 첫 타일로
	if (GroundMode == 2 && GroundStyle < 1) GroundStyle = 1;
	if (Hero) Hero->SetFrozen(GroundMode != 0);
	// 바닥 이름·칸당 비용을 함께 안내 (꾸미기 상점 '바닥'과 같은 비용 · 출시 수정 2차)
	if (Hud) Hud->ShowToast(GroundMode == 0 ? FText::FromString(TEXT("바닥 꾸미기를 종료했습니다")) : DescribeGroundTool());
}

void ACozyRealmEstatePlayerController::BeginFloorPainting(FName FloorId)
{
	UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	const FCozyFloorRow* Floor = Estate ? Estate->GetFloorDef(FloorId) : nullptr;
	if (!Floor)
	{
		if (Hud) Hud->ShowToast(FText::FromString(TEXT("바닥 목록(Floors.csv)에 없는 바닥입니다")));
		return;
	}
	// 붓 1~3 = 흙·돌·모래 · 타일 0~2는 키 1~3 (GroundStyle - 1)
	GroundStyle = Floor->Tool == ECozyFloorTool::Brush ? FMath::Clamp(Floor->Style, 1, 3) : FMath::Clamp(Floor->Style + 1, 1, 3);
	CozyGroundMode(Floor->Tool == ECozyFloorTool::Brush ? 1 : 2);
}

FText ACozyRealmEstatePlayerController::DescribeGroundTool() const
{
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	if (GroundMode == 1 && GroundStyle == 0)
	{
		return FText::FromString(TEXT("지면 붓 · 지우개(원래 풀로 · 무료 · 환불 없음) · 1~3 바닥 고르기 · [ ] 크기 · Ctrl+Z/Y · G 타일 · Esc 종료"));
	}
	const ECozyFloorTool Tool = GroundMode == 2 ? ECozyFloorTool::Tile : ECozyFloorTool::Brush;
	const int32 Style = GroundMode == 2 ? GroundStyle - 1 : GroundStyle;
	const FName FloorId = Estate ? Estate->FindFloorId(Tool, Style) : NAME_None;
	const FCozyFloorQuote Quote = Estate && !FloorId.IsNone() ? Estate->GetFloorQuote(FloorId, 1) : FCozyFloorQuote();
	FString Cost = TEXT("무료");
	if (Quote.bValid && Quote.Costs.Num() > 0)
	{
		Cost.Reset();
		for (const FCozyFloorQuote::FAmount& Each : Quote.Costs)
		{
			const FCozyItemRow* Item = Estate->GetItemDef(Each.ItemId);
			Cost += FString::Printf(TEXT("%s%s %d"), Cost.IsEmpty() ? TEXT("") : TEXT(" + "), Item ? *Item->DisplayName.ToString() : *Each.ItemId.ToString(), Each.Amount);
		}
		Cost = TEXT("칸당 ") + Cost;
	}
	const FString Name = Quote.bValid ? Quote.Name.ToString() : FString::Printf(TEXT("%d번"), FMath::Max(1, GroundStyle));
	return FText::FromString(GroundMode == 2
		? FString::Printf(TEXT("바닥 타일 %s · %s · 1~3 종류 · 클릭 미리보기 · Enter 확정 · 우클릭 취소 · H 보관 · U 꺼내기 · V 이동 · Shift+클릭 제거(무료·환불 없음) · Esc 종료"), *Name, *Cost)
		: FString::Printf(TEXT("지면 붓 %s · %s · 1~3 바닥 · 0 지우개(무료·환불 없음) · [ ] 크기 · Ctrl+Z/Y · G 타일 · Esc 종료"), *Name, *Cost));
}

bool ACozyRealmEstatePlayerController::PayBrushCells(const FVector& Position)
{
	// 🤖 지우개(0 = 원래 풀)는 무료 · 환불 없음
	if (GroundStyle == 0)
	{
		return true;
	}
	if (bStrokeBlocked)
	{
		return false;
	}
	UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	if (!Estate)
	{
		return true;
	}
	// 붓 원이 새로 걸치는 칸만 (한 번 끄는 동안 같은 칸은 한 번만 냄)
	TArray<FIntPoint> NewCells;
	for (const FIntPoint& Cell : Estate->GetCellsInCircle(Position, BrushRadius))
	{
		if (!StrokePaidCells.Contains(Cell)) NewCells.Add(Cell);
	}
	if (NewCells.Num() == 0)
	{
		return true;
	}
	FText Message;
	if (!Estate->PayFloorCost(Estate->FindFloorId(ECozyFloorTool::Brush, GroundStyle), NewCells.Num(), Message))
	{
		// 부족하면 이번 드래그는 여기서 멈춤 (이미 칠한 칸은 그대로 · 마우스를 떼면 다시 시도)
		bStrokeBlocked = true;
		if (Hud) Hud->ShowToast(Message);
		return false;
	}
	StrokePaidCells.Append(NewCells);
	return true;
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
	StrokePaidCells.Reset();
	bStrokeBlocked = false;
	LastStampSurface.Reset();
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
		// 칸당 비용 (꾸미기 상점 '바닥'과 같은 Floors.csv · 부족하면 찍지 않음)
		if (!PayBrushCells(Position)) return false;
		if (StrokeSurface.Get() != Target) { EndPaintSegment(); StrokeSurface = Target; Target->BeginPaintStroke(); }
		LastStampSurface = Target;
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
		// 한 프레임이 경사로 끝면 같은 벽에 떨어져 기록이 닫혔어도 마지막으로 칠한 표면을 이어 칠할 후보로 둔다
		UCozySurfaceComponent* Previous = StrokeSurface.IsValid() ? StrokeSurface.Get() : LastStampSurface.Get();
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

bool ACozyRealmEstatePlayerController::TraceSurfaceUnderCursor(FCozySurfaceHit& Out, bool bSkipFlatRay) const
{
	FVector Origin, Direction;
	return DeprojectMousePositionToWorld(Origin, Direction) && !(bSkipFlatRay && FMath::IsNearlyZero(Direction.Z))
		&& CozySurface::Trace(GetWorld(), Origin, Origin + Direction * 100000.f, Out);
}

void ACozyRealmEstatePlayerController::DrawSurfaceDebug() const
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
	if (TraceSurfaceUnderCursor(Hit))
	{
		const float Slope = CozySurface::SlopeDegrees(Hit.Normal);
		const FString Id = Hit.SurfaceId.ToString();
		DrawDebugString(World, Hit.Position + FVector(0, 0, 60), FString::Printf(TEXT("Cursor: surface %s..%s  slope %.0f deg  z %.0f"),
			*Id.Left(4), *Id.Right(4), Slope, Hit.Position.Z), nullptr, FColor::White, 0.f, true, 1.2f);
	}
	if (Hero)
	{
		const FVector Feet = Hero->GetFeetLocation();
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
					CozySurface::SlopeDegrees(Ground.Normal), Feet.X, Feet.Y, Feet.Z);
			}
		}
	}
#endif
}

void ACozyRealmEstatePlayerController::TickGroundEditing()
{
	if (Hud && Hud->IsWindowOpen()) { FinishGroundStroke(); return; }
	const int32 StyleBefore = GroundStyle;
	if (WasInputKeyJustPressed(EKeys::Zero) && GroundMode == 1) GroundStyle = 0;
	if (WasInputKeyJustPressed(EKeys::One)) GroundStyle = 1;
	if (WasInputKeyJustPressed(EKeys::Two)) GroundStyle = 2;
	if (WasInputKeyJustPressed(EKeys::Three)) GroundStyle = 3;
	// 바닥을 바꾸면 이름·칸당 비용을 다시 안내
	if (GroundStyle != StyleBefore && Hud) Hud->ShowToast(DescribeGroundTool());
	if (WasInputKeyJustPressed(EKeys::R)) GroundRotation = (GroundRotation + 1) % 4;
	if (WasInputKeyJustPressed(EKeys::LeftBracket)) BrushRadius = FMath::Max(40.f, BrushRadius - 40.f);
	if (WasInputKeyJustPressed(EKeys::RightBracket)) BrushRadius = FMath::Min(400.f, BrushRadius + 40.f);
	if (!IsInputKeyDown(EKeys::LeftMouseButton)) FinishGroundStroke();
	FCozySurfaceHit Hit;
	if (!TraceSurfaceUnderCursor(Hit))
	{
		EndPaintSegment(); return;
	}
	DrawDebugCircle(GetWorld(), Hit.Position + FVector(0, 0, 3), BrushRadius, 32, FColor::Cyan, false, -1, 0, 2, FVector::ForwardVector, FVector::RightVector, false);
	if (GroundMode == 2)
    {
        UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
        FText Reason;
        if (WasInputKeyJustPressed(EKeys::RightMouseButton)) CancelFloorPreview();
        const FCozyFloorTile* Hovered = Estate->FindFloorTile(Hit.Position, Hit.SurfaceId);
        if (Hovered && WasInputKeyJustPressed(EKeys::H))
        {
            Estate->StoreFloorTile(Hovered->InstanceId, Reason);
            CancelFloorPreview();
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
                CancelFloorPreview();
            }
            else bFloorPreview = true;
        }
        if (bFloorPreview)
        {
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
                if (Estate->EditFloorTile(Hit.Position, FMath::Clamp(GroundStyle - 1, 0, 2), GroundRotation, false, Reason, &Hit.SurfaceId, MovingFloorId))
                    CancelFloorPreview();
            }
        }
        if (Hud && !Reason.IsEmpty()) Hud->ShowToast(Reason);
        return;
    }

	// 한 프레임 안에 누름·뗌이 끝난 짧은 클릭도 첫 도장을 남긴다.
	// GroundMode는 여기서 항상 1 (0이면 이 함수를 부르지 않고 2는 위에서 끝남)
	if (!IsInputKeyDown(EKeys::LeftMouseButton) && !WasInputKeyJustPressed(EKeys::LeftMouseButton)) return;
	PaintBrushTo(Hit);
}
