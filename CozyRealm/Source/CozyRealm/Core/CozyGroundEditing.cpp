#include "Core/CozyRealmEstatePlayerController.h"
#include "Environment/CozySurfaceComponent.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Hero/CozyHeroCharacter.h"
#include "UI/CozyHudWidget.h"
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "InputKeyEventArgs.h"

bool ACozyRealmEstatePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const bool Handled = Super::InputKey(Params);
	if (Params.Key == EKeys::LeftControl) bGroundLeftControl = Params.Event != IE_Released;
	if (Params.Key == EKeys::RightControl) bGroundRightControl = Params.Event != IE_Released;
	// 프레임 마지막 상태 대신 입력 순서대로 판단해야 빠른 Ctrl+Z/Y도 놓치지 않는다.
	if (GroundMode == 1 && Params.Event == IE_Pressed && (bGroundLeftControl || bGroundRightControl)
		&& (Params.Key == EKeys::Z || Params.Key == EKeys::Y) && !(Hud && Hud->IsWindowOpen()))
	{
		FinishGroundStroke();
		if (LastPaintSurface.IsValid())
		{
			const bool Changed = Params.Key == EKeys::Z ? LastPaintSurface->UndoPaint() : LastPaintSurface->RedoPaint();
			if (Changed) GetWorld()->GetSubsystem<UCozyEstateSubsystem>()->SaveEstate(TEXT("도색 되돌리기"));
		}
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

void ACozyRealmEstatePlayerController::FinishGroundStroke()
{
	if (StrokeSurface.IsValid())
	{
		StrokeSurface->EndPaintStroke();
		LastPaintSurface = StrokeSurface;
		StrokeSurface.Reset();
		if (auto* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>()) Estate->SaveEstate(TEXT("지면 브러시"));
	}
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
		FinishGroundStroke(); return;
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
	UCozySurfaceComponent* Surface = Hit.Rules.Get();
	if (!Surface || !Surface->bAllowPainting || Hit.Normal.Z < .85f) { FinishGroundStroke(); return; }
	if (StrokeSurface.Get() != Surface)
	{
		FinishGroundStroke(); StrokeSurface = Surface;
		Surface->BeginPaintStroke(); LastBrushPosition = Hit.Position;
		const bool Changed = Surface->PaintAt(Hit.Position, BrushRadius, GroundStyle, .6f);
		if (FParse::Param(FCommandLine::Get(), TEXT("CozyTerrainTest")))
		{
			FVector2D UV; const bool InBounds = Surface->WorldToPaintUV(Hit.Position, UV);
			UE_LOG(LogTemp, Display, TEXT("[도색 입력] 표면=%s 위치=%s UV=%s 영역=%d 레이어=%d 픽셀=%d 변경=%d"),
				*Surface->SurfaceId.ToString(), *Hit.Position.ToString(), *UV.ToString(), InBounds, GroundStyle, Surface->GetPaintMask().Pixels.Num(), Changed);
		}
	}
	// 이동 거리에 따라 찍으므로 프레임 수와 상관없이 일정한 밀도로 칠한다.
	const float Step = FMath::Max(10.f, BrushRadius * .2f);
	const FVector Delta = Hit.Position - LastBrushPosition;
	const int32 Count = FMath::Min(128, FMath::FloorToInt(Delta.Size() / Step));
	const FVector StepVector = Delta.GetSafeNormal() * Step;
	for (int32 I = 0; I < Count; ++I)
	{
		LastBrushPosition += StepVector;
		FCozySurfaceHit Sample;
		if (CozySurface::GroundAt(GetWorld(), LastBrushPosition, Sample, &Surface->SurfaceId) && Sample.Rules.Get() == Surface && Sample.Normal.Z >= .85f)
			Surface->PaintAt(Sample.Position, BrushRadius, GroundStyle, .6f);
	}
}
