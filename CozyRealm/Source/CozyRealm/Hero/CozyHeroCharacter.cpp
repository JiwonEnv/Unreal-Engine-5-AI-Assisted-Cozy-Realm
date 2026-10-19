#include "Hero/CozyHeroCharacter.h"
#include "Environment/CozySurfaceNavigation.h"
#include "Estate/CozyEstateSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "CozyRealm.h"

namespace CozyHero
{
	/** 엔진 기본 도형은 한 변(지름)이 100cm */
	constexpr float BasicShapeSize = 100.f;
	const FName ColorParam(TEXT("Color"));
	/** 경유점에 이만큼 가까우면 다음 점으로 */
	constexpr float WaypointTolerance = 20.f;

	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetMaterial(0, Material);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		return Part;
	}
}

void FCozyHeroBody::Create(AActor* Owner, USceneComponent* Parent)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Body = CozyHero::MakePart(Owner, Parent, TEXT("BodyShape"), Cylinder.Object, ShapeMaterial.Object);
	Head = CozyHero::MakePart(Owner, Parent, TEXT("HeadShape"), Sphere.Object, ShapeMaterial.Object);
	// 앞쪽 표시 (어느 쪽을 보는지 알 수 있게 · 머리 앞의 작은 상자)
	Nose = CozyHero::MakePart(Owner, Parent, TEXT("FacingShape"), Cube.Object, ShapeMaterial.Object);
}

void FCozyHeroBody::Apply(const FCozyHeroConfig& Config, float Height, float BaseZ)
{
	if (!Body || !Head || !Nose)
	{
		return;
	}
	const float HeadSize = Height * Config.HeadRatio;
	const float BodyHeight = FMath::Max(1.f, Height - HeadSize);
	const float BodyWidth = HeadSize * Config.BodyWidthRatio;
	const float S = CozyHero::BasicShapeSize;

	Body->SetRelativeScale3D(FVector(BodyWidth / S, BodyWidth / S, BodyHeight / S));
	Body->SetRelativeLocation(FVector(0.f, 0.f, BaseZ + BodyHeight * 0.5f));
	Head->SetRelativeScale3D(FVector(HeadSize / S));
	Head->SetRelativeLocation(FVector(0.f, 0.f, BaseZ + BodyHeight + HeadSize * 0.5f));
	const float NoseSize = HeadSize * 0.18f;
	Nose->SetRelativeScale3D(FVector(NoseSize / S));
	Nose->SetRelativeLocation(FVector(HeadSize * 0.5f, 0.f, BaseZ + BodyHeight + HeadSize * 0.45f));

	if (!BodyMaterial)
	{
		BodyMaterial = Body->CreateDynamicMaterialInstance(0);
		HeadMaterial = Head->CreateDynamicMaterialInstance(0);
		Nose->SetMaterial(0, BodyMaterial);
	}
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(CozyHero::ColorParam, Config.BodyColor);
	}
	if (HeadMaterial)
	{
		HeadMaterial->SetVectorParameterValue(CozyHero::ColorParam, Config.HeadColor);
	}
}

// ---------------------------------------------------------------------------

ACozyHeroCharacter::ACozyHeroCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	bUseControllerRotationYaw = false;

	// 커서 클릭(시설 선택 · 땅 이동)이 주인공에 막히지 않게
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	if (USkeletalMeshComponent* Skeletal = GetMesh())
	{
		Skeletal->SetVisibility(false);
		Skeletal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Shape.Create(this, Capsule);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->bRunPhysicsWithNoController = true;
	Move->BrakingDecelerationWalking = 4000.f;
	Move->MaxAcceleration = 4000.f;
	Move->bUseSeparateBrakingFriction = true;
}

void ACozyHeroCharacter::SetSettings(UCozyHeroSettings* InSettings)
{
	if (Settings)
	{
		Settings->OnSettingsChanged.Remove(SettingsChangedHandle);
	}
	Settings = InSettings;
	if (Settings)
	{
		SettingsChangedHandle = Settings->OnSettingsChanged.AddUObject(this, &ACozyHeroCharacter::HandleSettingsChanged);
	}
	ApplyConfig();
	// 시작 칸이 시설에 덮여 있으면 먼저 빈 칸으로 옮긴 뒤 비교용 인형을 그 옆에 세움
	if (const UCozyEstateSubsystem* Estate = GetWorld() ? GetWorld()->GetSubsystem<UCozyEstateSubsystem>() : nullptr)
	{
		FVector Free;
		UCozySurfaceNavigation* Navigation = GetWorld()->GetSubsystem<UCozySurfaceNavigation>();
		const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (!Navigation->IsFree(Feet) && Navigation->FindNearestFree(Feet, Free))
		{
			SetActorLocation(Free + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	SpawnCompareDolls();
}

void ACozyHeroCharacter::HandleSettingsChanged(const UCozyHeroSettings* Changed)
{
	const float OldHalf = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	ApplyConfig();
	// 키가 바뀌면 발이 바닥에 닿도록 높이를 맞춤
	const float NewHalf = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	AddActorWorldOffset(FVector(0.f, 0.f, NewHalf - OldHalf));
	SpawnCompareDolls();
}

void ACozyHeroCharacter::ApplyConfig()
{
	Config = Settings ? Settings->Config : FCozyHeroConfig();
	const float Half = FMath::Max(Config.Height * 0.5f, 10.f);
	const float Radius = FMath::Clamp(Config.Height * Config.HeadRatio * 0.5f, 8.f, Half);
	GetCapsuleComponent()->SetCapsuleSize(Radius, Half);
	Shape.Apply(Config, Config.Height, -Half);

	const UCozyEstateSubsystem* Estate = GetWorld() ? GetWorld()->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	const float Cell = Estate ? Estate->GetCellSize() : 200.f;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = Config.WalkSpeedCells * Cell;
	Move->RotationRate = FRotator(0.f, Config.TurnSpeed, 0.f);
	UE_LOG(LogCozyRealm, Log, TEXT("[주인공] 키 %.0fcm · 머리 %.0f%% · 걷기 %.1f칸/초 (%.0fcm/초)"), Config.Height, Config.HeadRatio * 100.f, Config.WalkSpeedCells, Move->MaxWalkSpeed);
}

void ACozyHeroCharacter::BeginPlay()
{
	Super::BeginPlay();
	LastInputTime = GetWorld()->GetTimeSeconds();
	ApplyConfig();
}

void ACozyHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Settings)
	{
		Settings->OnSettingsChanged.Remove(SettingsChangedHandle);
	}
	DestroyCompareDolls();
	Super::EndPlay(EndPlayReason);
}

void ACozyHeroCharacter::SpawnCompareDolls()
{
	DestroyCompareDolls();
	const UCozyEstateSubsystem* Estate = GetWorld() ? GetWorld()->GetSubsystem<UCozyEstateSubsystem>() : nullptr;
	if (!Estate || !HasActorBegunPlay())
	{
		return;
	}
	// 주인공 옆 빈 칸에 한 줄로 (칸마다 하나)
	const FIntPoint HeroCell = Estate->WorldToCell(GetActorLocation());
	int32 Placed = 0;
	for (int32 Step = 1; Step <= Estate->GetGridSize().X && Placed < Config.CompareHeights.Num(); ++Step)
	{
		for (const int32 Dir : { 1, -1 })
		{
			const FIntPoint Cell(HeroCell.X, HeroCell.Y + Step * Dir);
			if (Placed >= Config.CompareHeights.Num() || !Estate->IsCellWalkable(Cell))
			{
				continue;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ACozyScaleDoll* Doll = GetWorld()->SpawnActor<ACozyScaleDoll>(Estate->GetCellCenter(Cell), GetActorRotation(), Params))
			{
				Doll->Setup(Config, Config.CompareHeights[Placed]);
				CompareDolls.Add(Doll);
				++Placed;
			}
		}
	}
}

void ACozyHeroCharacter::DestroyCompareDolls()
{
	for (AActor* Doll : CompareDolls)
	{
		if (IsValid(Doll))
		{
			Doll->Destroy();
		}
	}
	CompareDolls.Reset();
}

void ACozyHeroCharacter::SetKeyboardInput(const FVector& WorldDirection, bool bRun)
{
	KeyboardDirection = WorldDirection.GetClampedToMaxSize(1.f);
	bKeyboardRun = bRun;
	if (!KeyboardDirection.IsNearlyZero())
	{
		PathPoints.Reset();
		bWandering = false;
		NotifyPlayerInput();
	}
}

void ACozyHeroCharacter::NotifyPlayerInput()
{
	LastInputTime = GetWorld()->GetTimeSeconds();
	if (bWandering)
	{
		bWandering = false;
		PathPoints.Reset();
	}
}

void ACozyHeroCharacter::SetFrozen(bool bFreeze)
{
	bFrozen = bFreeze;
	PathPoints.Reset();
	bWandering = false;
	KeyboardDirection = FVector::ZeroVector;
	NotifyPlayerInput();
}

bool ACozyHeroCharacter::MoveToLocation(const FVector& Target)
{
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	if (!Estate)
	{
		return false;
	}
	NotifyPlayerInput();
	TArray<FVector> Points;
    UCozySurfaceNavigation* Navigation = GetWorld()->GetSubsystem<UCozySurfaceNavigation>();
    const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if (!Navigation || !Navigation->IsFree(Target) || !Navigation->FindPath(Feet, Target, Points)) return false;

	PathPoints = MoveTemp(Points);
	bWandering = false;
	return true;
}

FGuid ACozyHeroCharacter::FindInteractFacility() const
{
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	return Estate ? Estate->FindNearestFacility(GetActorLocation(), Config.InteractRangeCells * Estate->GetCellSize()) : FGuid();
}

bool ACozyHeroCharacter::FindNearestWalkable(FIntPoint Cell, FIntPoint& OutCell) const
{
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	const FIntPoint Size = Estate->GetGridSize();
	Cell = FIntPoint(FMath::Clamp(Cell.X, 0, Size.X - 1), FMath::Clamp(Cell.Y, 0, Size.Y - 1));
	const int32 MaxRing = FMath::Max(Size.X, Size.Y);
	for (int32 Ring = 0; Ring <= MaxRing; ++Ring)
	{
		float BestDistance = TNumericLimits<float>::Max();
		bool bFound = false;
		for (int32 Y = Cell.Y - Ring; Y <= Cell.Y + Ring; ++Y)
		{
			for (int32 X = Cell.X - Ring; X <= Cell.X + Ring; ++X)
			{
				if (FMath::Max(FMath::Abs(X - Cell.X), FMath::Abs(Y - Cell.Y)) != Ring || !Estate->IsCellWalkable(FIntPoint(X, Y)))
				{
					continue;
				}
				const float Distance = FVector2D(X - Cell.X, Y - Cell.Y).SizeSquared();
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					OutCell = FIntPoint(X, Y);
					bFound = true;
				}
			}
		}
		if (bFound)
		{
			return true;
		}
	}
	return false;
}

bool ACozyHeroCharacter::FindPath(FIntPoint Start, FIntPoint Goal, TArray<FVector>& OutPoints) const
{
    const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
    UCozySurfaceNavigation* Navigation = GetWorld()->GetSubsystem<UCozySurfaceNavigation>();
    const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    return Navigation && Navigation->FindPath(Feet, Estate->GetCellCenter(Goal), OutPoints);
}

void ACozyHeroCharacter::StartWander()
{
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	const FIntPoint Here = Estate->WorldToCell(GetActorLocation());
	const int32 Radius = FMath::Max(1, FMath::RoundToInt(Config.WanderRadiusCells));
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const FIntPoint Target(Here.X + FMath::RandRange(-Radius, Radius), Here.Y + FMath::RandRange(-Radius, Radius));
		TArray<FVector> Points;
		FIntPoint Start;
		if (Target != Here && Estate->IsCellWalkable(Target) && FindNearestWalkable(Here, Start) && FindPath(Start, Target, Points))
		{
			PathPoints = MoveTemp(Points);
			bWandering = true;
			return;
		}
	}
	WanderResumeTime = GetWorld()->GetTimeSeconds() + 1.0;
}

void ACozyHeroCharacter::FollowPath(float SpeedScale)
{
	// 이동 중 새로 놓인 시설이 다음 지점을 막으면 기존 경로를 폐기한다.
	if (!PathPoints.IsEmpty() && !GetWorld()->GetSubsystem<UCozySurfaceNavigation>()->IsFree(PathPoints[0]))
	{
		PathPoints.Reset(); bWandering = false; return;
	}
	while (PathPoints.Num() > 0)
	{
		const FVector ToTarget = FVector(PathPoints[0].X - GetActorLocation().X, PathPoints[0].Y - GetActorLocation().Y, 0.f);
		if (ToTarget.Size() > CozyHero::WaypointTolerance)
		{
			AddMovementInput(ToTarget.GetSafeNormal(), SpeedScale);
			return;
		}
		PathPoints.RemoveAt(0);
	}
	if (bWandering)
	{
		// 산책: 도착하면 잠깐 쉬고 다시
		bWandering = false;
		WanderResumeTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(Config.WanderPauseSeconds.X, FMath::Max(Config.WanderPauseSeconds.X, Config.WanderPauseSeconds.Y));
	}
}

void ACozyHeroCharacter::KeepInsideEstate()
{
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	const float Cell = Estate->GetCellSize();
	const FVector Min = Estate->GetCellCenter(FIntPoint(0, 0)) - FVector(Cell * 0.5f, Cell * 0.5f, 0.f);
	const FVector Max = Min + FVector(Estate->GetGridSize().X * Cell, Estate->GetGridSize().Y * Cell, 0.f);
	const float R = GetCapsuleComponent()->GetScaledCapsuleRadius();
	FVector Location = GetActorLocation();
	const FVector Clamped(FMath::Clamp(Location.X, Min.X + R, Max.X - R), FMath::Clamp(Location.Y, Min.Y + R, Max.Y - R), Location.Z);
	if (!Clamped.Equals(Location, 0.1f))
	{
		SetActorLocation(Clamped);
	}
}

void ACozyHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	if (!Estate)
	{
		return;
	}
	const float Cell = Estate->GetCellSize();
	UCharacterMovementComponent* Move = GetCharacterMovement();

	// 시설이 옮겨져 주인공 자리를 덮으면 가장 가까운 빈 칸으로
	FVector Free;
	UCozySurfaceNavigation* Navigation = GetWorld()->GetSubsystem<UCozySurfaceNavigation>();
	const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	if (!bFrozen && !Navigation->IsFree(Feet) && Navigation->FindNearestFree(Feet, Free))
	{
		SetActorLocation(Free + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2), false, nullptr, ETeleportType::TeleportPhysics);
		PathPoints.Reset();
	}
	if (bFrozen)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (!KeyboardDirection.IsNearlyZero())
	{
		const bool bRun = bKeyboardRun && Config.RunSpeedCells > 0.f;
		Move->MaxWalkSpeed = (bRun ? Config.RunSpeedCells : Config.WalkSpeedCells) * Cell;
		AddMovementInput(KeyboardDirection, 1.f);
	}
	else if (PathPoints.Num() > 0)
	{
		Move->MaxWalkSpeed = Config.WalkSpeedCells * Cell * (bWandering ? Config.WanderSpeedRatio : 1.f);
		FollowPath(1.f);
	}
	else if (Config.IdleWanderSeconds > 0.f && Now - LastInputTime >= Config.IdleWanderSeconds && Now >= WanderResumeTime)
	{
		StartWander();
	}
	KeepInsideEstate();
}

// ---------------------------------------------------------------------------

ACozyScaleDoll::ACozyScaleDoll()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Shape.Create(this, RootComponent);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeightLabel"));
	Label->SetupAttachment(RootComponent);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetTextRenderColor(FColor::White);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACozyScaleDoll::Setup(const FCozyHeroConfig& Config, float Height)
{
	FCozyHeroConfig Gray = Config;
	Gray.BodyColor = FLinearColor(0.55f, 0.57f, 0.62f);
	Gray.HeadColor = FLinearColor(0.78f, 0.79f, 0.82f);
	Shape.Apply(Gray, Height, 0.f);
	Label->SetText(FText::FromString(FString::Printf(TEXT("%.0fcm"), Height)));
	Label->SetWorldSize(FMath::Clamp(Height * 0.25f, 24.f, 60.f));
	Label->SetRelativeLocation(FVector(0.f, 0.f, Height + 15.f));
	// 글자가 항상 카메라 쪽을 보게 (쿼터뷰 기본 방향 · 회전하면 비스듬히 보일 수 있음)
	Label->SetWorldRotation(FRotator(45.f, 225.f, 0.f));
}
