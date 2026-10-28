#include "HAL/IConsoleManager.h"
#include "Environment/CozyMountainWorld.h"
#include "Landscape.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld CozyMountainVerify(
	TEXT("Cozy.MountainVerify"), TEXT("산 월드의 실제 Landscape 충돌·평지·연결 길을 검사하고 결과를 저장"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for(TActorIterator<ACozyMountainWorld> It(World);It;++It)
		{
			ACozyMountainWorld* Layout=*It;
			if(!Layout->Landscape) return;
			FString Report; int32 Failed=0, Samples=0;
			auto Check=[&](bool Good,const FString& Message)
			{
				if(!Good) ++Failed;
				Report+=FString(Good?TEXT("통과: "):TEXT("실패: "))+Message+TEXT("\n");
			};
			FCollisionQueryParams Params;
			for(TActorIterator<AActor> A(World);A;++A) if(*A!=Layout->Landscape) Params.AddIgnoredActor(*A);
			auto Ground=[&](FVector P,FHitResult& Hit)
			{
				++Samples;
				return World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,24000),FVector(P.X,P.Y,-24000),ECC_Visibility,Params)
					&& Hit.GetActor()==Layout->Landscape;
			};
			for(const FCozyWorldClearing& C:Layout->Clearings)
			{
				bool Good=true;
				for(int32 Y=-2;Y<=2;++Y) for(int32 X=-2;X<=2;++X)
				{
					const FVector P=C.Center+FVector(X*C.Radius.X*.22,Y*C.Radius.Y*.22,Layout->GroundZ);
					FHitResult Hit;
					Good=Ground(P,Hit)&&FMath::Abs(Hit.ImpactPoint.Z-P.Z)<=3&&Hit.ImpactNormal.Z>.99&&Good;
				}
				Check(Good,C.Name+TEXT(" 평지 25개 지점의 충돌·높이·기울기"));
			}
			for(const FCozyWorldRoute& R:Layout->Routes)
			{
				bool Good=true; double Worst=0;
				for(int32 I=1;I<R.Points.Num();++I)
				{
					const int32 Steps=FMath::CeilToInt(FVector::Dist2D(R.Points[I-1],R.Points[I])/75.);
					for(int32 S=0;S<=Steps;++S)
					{
						const FVector P=FMath::Lerp(R.Points[I-1],R.Points[I],double(S)/Steps)+FVector(0,0,Layout->GroundZ);
						FHitResult Hit;
						if(!Ground(P,Hit)) { Good=false; continue; }
						Worst=FMath::Max(Worst,FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hit.ImpactNormal.Z,-1.,1.))));
						const double Expected=Layout->GetLayoutHeight(P.X,P.Y)+Layout->GroundZ;
						if(FMath::Abs(Hit.ImpactPoint.Z-Expected)>15||Hit.ImpactNormal.Z<.72) Good=false;
					}
				}
				Check(Good,FString::Printf(TEXT("%s 75cm 간격 실제 길 충돌·단차·경사 (최대 %.1f도)"),*R.Name,Worst));
			}
			Check(Layout->ComponentsPerSide==8&&Layout->QuadSize==100,TEXT("504m Landscape 규격"));
			Report+=FString::Printf(TEXT("실제 충돌 조회 %d회 / 실패 %d건\n"),Samples,Failed);
			const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/MountainWorld");
			IFileManager::Get().MakeDirectory(*Dir,true);
			FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("verify.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			UE_LOG(LogTemp,Display,TEXT("MOUNTAIN_VERIFY_FAILED=%d\n%s"),Failed,*Report);
			return;
		}
		UE_LOG(LogTemp,Error,TEXT("산 월드 맵이 아니어서 검사를 실행하지 않음"));
	}));
#endif
