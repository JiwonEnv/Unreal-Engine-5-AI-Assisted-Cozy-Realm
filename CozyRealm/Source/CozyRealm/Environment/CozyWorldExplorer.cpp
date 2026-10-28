#include "Environment/CozyWorldExplorer.h"
#include "Environment/CozyMountainWorld.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

ACozyWorldExplorer::ACozyWorldExplorer()
{
	PrimaryActorTick.bCanEverTick=true;
	GetCapsuleComponent()->InitCapsuleSize(30,70);
	GetCharacterMovement()->MaxWalkSpeed=650;
	GetCharacterMovement()->MaxStepHeight=35;
	GetCharacterMovement()->SetWalkableFloorAngle(44);
	bUseControllerRotationYaw=false;
	GetCharacterMovement()->bOrientRotationToMovement=true;
	Arm=CreateDefaultSubobject<USpringArmComponent>(TEXT("검토카메라붐"));
	Arm->SetupAttachment(RootComponent); Arm->TargetArmLength=1400;
	Arm->SetRelativeRotation(FRotator(-48,0,0)); Arm->bDoCollisionTest=false;
	Arm->SetUsingAbsoluteRotation(true);
	Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("검토카메라")); Camera->SetupAttachment(Arm);
	UStaticMeshComponent* Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("140cm비율기준"));
	Body->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
	Body->SetStaticMesh(Sphere.Object); Body->SetRelativeScale3D(FVector(.55,.55,1.35));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACozyWorldExplorer::BeginPlay() { Super::BeginPlay(); Visit(0); }
void ACozyWorldExplorer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if(WalkCheckRemaining>0)
	{
		Forward(1);
		WalkCheckRemaining-=DeltaSeconds;
		if(WalkCheckRemaining<=0)
		{
			const float Distance=FVector::Dist2D(WalkCheckStart,GetActorLocation());
			const bool bPassed=Distance>500&&GetCharacterMovement()->IsMovingOnGround();
			UE_LOG(LogTemp,Display,TEXT("[월드검토] 보행 시연 %s / 실제 이동 %.1fcm / 지면 보행 %d"),bPassed?TEXT("통과"):TEXT("실패"),Distance,GetCharacterMovement()->IsMovingOnGround());
		}
	}
	if(APlayerController* PC=Cast<APlayerController>(Controller))
	{
		Forward(float(PC->IsInputKeyDown(EKeys::W))-float(PC->IsInputKeyDown(EKeys::S)));
		Right(float(PC->IsInputKeyDown(EKeys::D))-float(PC->IsInputKeyDown(EKeys::A)));
	}
}
void ACozyWorldExplorer::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxisKey(EKeys::MouseX,this,&ACozyWorldExplorer::Yaw);
	Input->BindAxisKey(EKeys::MouseY,this,&ACozyWorldExplorer::Pitch);
	Input->BindAxisKey(EKeys::MouseWheelAxis,this,&ACozyWorldExplorer::Zoom);
	Input->BindKey(EKeys::M,IE_Pressed,this,&ACozyWorldExplorer::ToggleOverview);
	Input->BindKey(EKeys::F6,IE_Pressed,this,&ACozyWorldExplorer::WorldWalkCheck);
	Input->BindKey(EKeys::One,IE_Pressed,this,&ACozyWorldExplorer::Zone1);
	Input->BindKey(EKeys::Two,IE_Pressed,this,&ACozyWorldExplorer::Zone2);
	Input->BindKey(EKeys::Three,IE_Pressed,this,&ACozyWorldExplorer::Zone3);
	Input->BindKey(EKeys::Four,IE_Pressed,this,&ACozyWorldExplorer::Zone4);
	Input->BindKey(EKeys::Five,IE_Pressed,this,&ACozyWorldExplorer::Zone5);
}
void ACozyWorldExplorer::Forward(float V) { if(!bOverview) AddMovementInput(FRotationMatrix(FRotator(0,Arm->GetComponentRotation().Yaw,0)).GetUnitAxis(EAxis::X),V); }
void ACozyWorldExplorer::Back(float V) { Forward(-V); }
void ACozyWorldExplorer::Right(float V) { if(!bOverview) AddMovementInput(FRotationMatrix(FRotator(0,Arm->GetComponentRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V); }
void ACozyWorldExplorer::Left(float V) { Right(-V); }
void ACozyWorldExplorer::Yaw(float V) { if(!bOverview) Arm->AddWorldRotation(FRotator(0,V*1.2f,0)); }
void ACozyWorldExplorer::Pitch(float V) { if(!bOverview) { FRotator R=Arm->GetComponentRotation(); R.Pitch=FMath::Clamp(R.Pitch-V,-75.,-15.); Arm->SetWorldRotation(R); } }
void ACozyWorldExplorer::Zoom(float V) { if(!bOverview) Arm->TargetArmLength=FMath::Clamp(Arm->TargetArmLength-V*150,400.f,5000.f); }
void ACozyWorldExplorer::Visit(int32 Index)
{
	WalkCheckRemaining=0;
	for(TActorIterator<ACozyMountainWorld> It(GetWorld());It;++It)
	if(It->Clearings.IsValidIndex(Index))
	{
		const FVector P=It->Clearings[Index].Center+FVector(0,0,It->GroundZ+85);
		GetCharacterMovement()->StopMovementImmediately();
		if(bOverview) { bOverview=false; GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
		SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
		Arm->TargetArmLength=4200; Arm->SetWorldRotation(FRotator(-48,65,0));
		return;
	}
}
void ACozyWorldExplorer::ToggleOverview()
{
	WalkCheckRemaining=0;
	bOverview=!bOverview;
	GetCharacterMovement()->StopMovementImmediately();
	if(bOverview)
	{
		ReturnPosition=GetActorLocation(); GetCharacterMovement()->DisableMovement();
		SetActorLocation(FVector(0,0,2500),false,nullptr,ETeleportType::TeleportPhysics);
		Arm->TargetArmLength=65000; Arm->SetWorldRotation(FRotator(-65,80,0));
	}
	else
	{
		SetActorLocation(ReturnPosition,false,nullptr,ETeleportType::TeleportPhysics);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Arm->TargetArmLength=4200; Arm->SetWorldRotation(FRotator(-48,65,0));
	}
}
void ACozyWorldExplorer::WorldWalkCheck()
{
	if(bOverview) return;
	WalkCheckStart=GetActorLocation(); WalkCheckRemaining=3;
	UE_LOG(LogTemp,Display,TEXT("[월드검토] 3초 실제 보행 시연 시작"));
}
void ACozyWorldExplorer::Zone1(){Visit(0);} void ACozyWorldExplorer::Zone2(){Visit(1);}
void ACozyWorldExplorer::Zone3(){Visit(2);} void ACozyWorldExplorer::Zone4(){Visit(3);}
void ACozyWorldExplorer::Zone5(){Visit(4);}
ACozyWorldExplorerMode::ACozyWorldExplorerMode()
{
	DefaultPawnClass=ACozyWorldExplorer::StaticClass(); PlayerControllerClass=ACozyWorldExplorerController::StaticClass();
}
void ACozyWorldExplorerController::BeginPlay()
{
	Super::BeginPlay(); SetInputMode(FInputModeGameOnly()); bShowMouseCursor=false;
	Guide=CreateWidget<UCozyWorldGuide>(this,UCozyWorldGuide::StaticClass());
	Guide->WidgetTree=NewObject<UWidgetTree>(Guide);
	UBorder* Box=Guide->WidgetTree->ConstructWidget<UBorder>();
	Box->SetBrushColor(FLinearColor(.025f,.05f,.035f,.8f)); Box->SetPadding(FMargin(18,10));
	Box->SetHorizontalAlignment(HAlign_Left); Box->SetVerticalAlignment(VAlign_Top);
	UTextBlock* Text=Guide->WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetText(FText::FromString(TEXT("산 월드 · 레벨디자인 검토\nWASD 이동  |  마우스 시점  |  휠 확대\nM 전체 지도 / 돌아오기  |  F6 보행 시연 3초\n1 시작  2 서쪽 확장  3 동쪽 확장  4 능선  5 산기슭\n구역·시설은 검토 배치이며 실제 저장을 사용하지 않습니다")));
	FSlateFontInfo Font=Text->GetFont(); Font.Size=18;
	if(UObject* Korean=LoadObject<UObject>(nullptr,TEXT("/Game/CozyRealm/UI/Fonts/F_Cozy_JuaFamily.F_Cozy_JuaFamily"))) Font.FontObject=Korean;
	Text->SetFont(Font);
	Box->AddChild(Text); Guide->WidgetTree->RootWidget=Box;
	Guide->AddToViewport(); Guide->SetDesiredSizeInViewport(FVector2D(720,180)); Guide->SetPositionInViewport(FVector2D(20,20));
}
