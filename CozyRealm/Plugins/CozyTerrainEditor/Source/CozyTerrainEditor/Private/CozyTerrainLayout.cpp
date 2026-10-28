#include "CozyTerrainLayout.h"
#include "ProceduralMeshComponent.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

bool FCozyTerrainRecipe::Validate(FString& Reason) const
{
    auto Finite=[](double V){return FMath::IsFinite(V);};
    if(Version!=1||Components<1||Components>8||!Finite(MapSize.X)||!Finite(MapSize.Y)||MapSize.X<63||MapSize.Y<63||MapSize.X>1008||MapSize.Y>1008||!Finite(BaseHeight)||FMath::Abs(BaseHeight)>200||Landforms.Num()>128||Routes.Num()>64)
    {Reason=TEXT("영역은 63~1008m, 구획은 1~8, 기본 높이는 ±200m 범위여야 합니다.");return false;}
    for(const auto& F:Landforms)
        if(!Finite(F.Center.X)||!Finite(F.Center.Y)||!Finite(F.Size.X)||!Finite(F.Size.Y)||F.Size.X<2||F.Size.Y<2||F.Size.X>2000||F.Size.Y>2000||!Finite(F.Height)||FMath::Abs(F.Height)>200||!Finite(F.Blend)||F.Blend<1||F.Blend>200||!Finite(F.Rotation)||!Finite(F.PeakFlat)||F.PeakFlat<0||F.PeakFlat>90||uint8(F.Type)>1)
        {Reason=TEXT("평지·산의 크기·높이·주변 연결 수치를 확인하세요.");return false;}
    for(const auto& R:Routes)
    {
        if(!Finite(R.Width)||R.Width<2||R.Width>80||!Finite(R.Blend)||R.Blend<1||R.Blend>80||R.Points.Num()>256)
        {Reason=TEXT("길 폭은 2~80m, 연결 폭은 1~80m 범위여야 합니다.");return false;}
        for(const auto& P:R.Points) if(P.ContainsNaN()||!Finite(P.X)||!Finite(P.Y)||!Finite(P.Z)||FMath::Abs(P.Z)>400)
        {Reason=TEXT("길 경유점의 위치와 높이를 확인하세요.");return false;}
    }
    return true;
}

float FCozyTerrainRecipe::Evaluate(FVector2D P,float Base) const
{
    // 산끼리 겹치면 높이를 더하지 않고 부드럽게 높은 쪽을 따른다 (더하면 겹친 곳이 두 배로 솟았음)
    // 이음매 폭 K는 높이에 비례 (산에서 먼 평지에서는 0이라 바닥이 들뜨지 않음)
    auto SmoothMax=[](float A,float B){const float K=FMath::Min(4.f,.25f*FMath::Max(A,B));if(K<=KINDA_SMALL_NUMBER)return FMath::Max(A,B);const float W=FMath::Max(K-FMath::Abs(A-B),0.f)/K;return FMath::Max(A,B)+W*W*K*.25f;};
    float Up=0,Down=0;
    for(const auto& F:Landforms)
    {
        if(F.Type!=ECozyLandform::Mountain) continue;
        const FVector2D Q=(P-F.Center).GetRotated(-F.Rotation);
        const double D=FMath::Sqrt(FMath::Square(Q.X/FMath::Max(1.,F.Size.X*.5))+FMath::Square(Q.Y/FMath::Max(1.,F.Size.Y*.5)));
        // 꼭대기 넓이 T 안은 꼭대기 높이 그대로, 그 바깥부터 줄어듦 (T가 클수록 옆면이 가파름)
        const double T=FMath::Clamp(F.PeakFlat,0.f,90.f)/100.,R=FMath::Max(0.,D-T)/(1-T);
        const float Bump=F.Height*FMath::Exp(-2*R*R);
        if(Bump>=0)Up=SmoothMax(Up,Bump);else Down=-SmoothMax(-Down,-Bump);
    }
    float H=Base+Up+Down;
    for(const auto& F:Landforms)
    {
        if(F.Type==ECozyLandform::Mountain) continue;
        const FVector2D Q=(P-F.Center).GetRotated(-F.Rotation);
        const double D=FMath::Sqrt(FMath::Square(Q.X/FMath::Max(1.,F.Size.X*.5))+FMath::Square(Q.Y/FMath::Max(1.,F.Size.Y*.5)));
        H=FMath::Lerp(H,F.Height,float(1-FMath::SmoothStep(1.,1.+F.Blend/FMath::Max(1.,FMath::Min(F.Size.X,F.Size.Y)*.5),D)));
    }
    float BestWeight=0,BestHeight=H; double BestDistance=DBL_MAX;
    for(const auto& R:Routes) for(int I=1;I<R.Points.Num();++I)
    {
        const FVector2D A(R.Points[I-1]),B(R.Points[I]),D=B-A;
        const double T=FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(.00001,D.SizeSquared()),0.,1.);
        const double Distance=FVector2D::Distance(P,A+D*T);
        const float W=1-FMath::SmoothStep(R.Width*.5f,R.Width*.5f+R.Blend,float(Distance));
        if(W>BestWeight||(W==BestWeight&&Distance<BestDistance))
        {BestWeight=W;BestDistance=Distance;BestHeight=FMath::Lerp(R.Points[I-1].Z,R.Points[I].Z,T);}
    }
    return FMath::Lerp(H,BestHeight,BestWeight);
}

ACozyTerrainLayout::ACozyTerrainLayout()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("편집 영역 중심"));
    Preview=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("적용 전 미리보기"));
    Preview->SetupAttachment(RootComponent); Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);Preview->SetHiddenInGame(true);
    Preview->CastShadow=false; Preview->bIsEditorOnly=true;
    Preview->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial")));
}
void ACozyTerrainLayout::PostEditChangeProperty(FPropertyChangedEvent& E)
{
    Super::PostEditChangeProperty(E);
    if(E.MemberProperty&&E.MemberProperty->GetFName()==GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,Recipe)){bHasUnappliedChanges=true;bShowPreview=true;}
    RebuildPreview();
}
void ACozyTerrainLayout::PostEditUndo(){Super::PostEditUndo();RebuildPreview();}
void ACozyTerrainLayout::PostLoad()
{
    Super::PostLoad();
    // 적용 기록이 생기기 전에 저장한 설계: 적용된 상태로 저장됐으면 지금 설계를 적용분으로 본다 (미리보기 기준)
    if(!bHasApplied&&EditLayerGuid.IsValid()&&!bHasUnappliedChanges){AppliedRecipe=Recipe;bHasApplied=true;}
}

float ACozyTerrainLayout::SampleApplied(FVector2D P) const
{
    if(bHasApplied) return SampleWith(AppliedRecipe,P);
    // 아직 적용 전: 연결 당시 원래 높이 그대로 (새로 만들 지형이면 0)
    return BaselineHeight(P);
}

float ACozyTerrainLayout::Sample(FVector2D P) const{return SampleWith(Recipe,P);}
float ACozyTerrainLayout::SampleWith(const FCozyTerrainRecipe& R,FVector2D P) const{return R.Evaluate(P,BaselineHeight(P)+R.BaseHeight);}

float ACozyTerrainLayout::BaselineHeight(FVector2D P) const
{
    float Base=0;
    const int W=GridMax.X-GridMin.X+1,H=GridMax.Y-GridMin.Y+1;
    if(Baseline.Num()==W*H&&W>1&&H>1&&BoundSize.X>0&&BoundSize.Y>0)
    {
        const double GX=FMath::Clamp((P.X/BoundSize.X+.5)*(W-1),0.,double(W-1));
        const double GY=FMath::Clamp((P.Y/BoundSize.Y+.5)*(H-1),0.,double(H-1));
        const int X=FMath::FloorToInt(GX),Y=FMath::FloorToInt(GY),X2=FMath::Min(X+1,W-1),Y2=FMath::Min(Y+1,H-1);
        // 원래 높이 + 지금까지 굳힌 높이 (굳힌 뒤 새 설계는 바뀐 지형 위에서 시작)
        const bool bBaked=Baked.Num()==Baseline.Num();
        auto At=[&](int I){return double(Baseline[I])+(bBaked?double(Baked[I])-32768:0.);};
        const double Raw=FMath::Lerp(FMath::Lerp(At(Y*W+X),At(Y*W+X2),GX-X),FMath::Lerp(At(Y2*W+X),At(Y2*W+X2),GX-X),GY-Y);
        Base=((Raw-32768)/128*CapturedTransform.GetScale3D().Z+CapturedTransform.GetLocation().Z-GetActorLocation().Z)/100;
    }
    return Base;
}

void ACozyTerrainLayout::RebuildPreview()
{
    FString Reason;
    if(!Recipe.Validate(Reason)){Preview->ClearAllMeshSections();return;}
    // 실제 지형과 높이가 달라지는 곳만 그린다 (전체를 덮으면 실제 지형 위에 하얗게 겹쳐 알아보기 어려웠음)
    // 색: 올라가는 곳 주황 · 내려가는 곳 파랑 · 급경사 빨강
    constexpr int N=65; constexpr float ChangedMeters=.05f;
    TArray<FVector> V,Normals; TArray<int32> Indices; TArray<FVector2D> UV; TArray<FLinearColor> Colors; TArray<FProcMeshTangent> Tangents;
    TArray<bool> Changed; V.Reserve(N*N); Colors.Reserve(N*N); Changed.Reserve(N*N);
    for(int Y=0;Y<N;++Y) for(int X=0;X<N;++X)
    {
        FVector2D P((double(X)/(N-1)-.5)*Recipe.MapSize.X,(double(Y)/(N-1)-.5)*Recipe.MapSize.Y);
        const float Z=Sample(P),Delta=Z-SampleApplied(P);
        V.Add(FVector(P.X*100,P.Y*100,Z*100+15));
        const float DX=Sample(P+FVector2D(.5,0))-Sample(P-FVector2D(.5,0));
        const float DY=Sample(P+FVector2D(0,.5))-Sample(P-FVector2D(0,.5));
        Normals.Add(FVector(-DX,-DY,1).GetSafeNormal());
        UV.Add(FVector2D(double(X)/(N-1),double(Y)/(N-1)));
        FLinearColor C=Delta>=0?FLinearColor(.95,.55,.15):FLinearColor(.2,.45,.95);
        if(FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX+DY*DY)))>SlopeLimit) C=FLinearColor(.9,.16,.08);
        Colors.Add(C); Changed.Add(FMath::Abs(Delta)>ChangedMeters);
    }
    for(int Y=0;Y<N-1;++Y) for(int X=0;X<N-1;++X)
    {
        const int A=Y*N+X;
        if(Changed[A]||Changed[A+1]||Changed[A+N]||Changed[A+N+1]) Indices.Append({A,A+N,A+1,A+1,A+N,A+N+1});
    }
    if(Indices.IsEmpty()){Preview->ClearAllMeshSections();if(GEditor)GEditor->RedrawLevelEditingViewports();return;}
    Preview->CreateMeshSection_LinearColor(0,V,Indices,Normals,UV,Colors,Tangents,false);
    Preview->SetVisibility(bShowPreview); if(GEditor) GEditor->RedrawLevelEditingViewports();
}

bool ACozyTerrainLayout::BindLandscape(ALandscape* L,FString& Result)
{
    if(!L||L->GetWorld()!=GetWorld()||EditLayerGuid.IsValid()) {Result=TEXT("현재 레벨의 Landscape를 선택하세요. 이미 연결된 설계는 새 설계에서 다시 연결하세요.");return false;}
    ULandscapeInfo* Info=L->GetLandscapeInfo();
    if(!Info||!L->GetActorRotation().IsNearlyZero()||L->GetActorScale3D().GetMin()<=0||!Info->GetLandscapeExtent(GridMin.X,GridMin.Y,GridMax.X,GridMax.Y))
    {Result=TEXT("양수 배율·회전 0인 로드된 Landscape가 필요합니다.");return false;}
    const int64 Count=int64(GridMax.X-GridMin.X+1)*(GridMax.Y-GridMin.Y+1);
    if(Count<=0||Count>1100000){Result=TEXT("한 번에 최대 약 100만 높이점까지 연결합니다. 분할 월드는 별도 검증이 필요합니다.");return false;}
    Target=L; CapturedTransform=L->LandscapeActorToWorld();
    const FVector Min=CapturedTransform.TransformPosition(FVector(GridMin.X,GridMin.Y,0)),Max=CapturedTransform.TransformPosition(FVector(GridMax.X,GridMax.Y,0));
    BoundCenter=(Min+Max)*.5; BoundSize=FVector2D(Max-Min)/100;
    Recipe.MapSize=BoundSize; SetActorLocation(BoundCenter);
    if(!ReadTarget(Baseline,Result)){Target=nullptr;return false;}
    Baked.Reset();BakeUndoLayer.Reset();bCanUndoBake=false;
    GroundMaterial=L->GetLandscapeMaterial(); RebuildPreview();
    Result=TEXT("현재 지형을 기준으로 연결했습니다. 원본 편집 레이어는 유지하고 새 전용 레이어로 적용합니다."); return true;
}

bool ACozyTerrainLayout::ReadTarget(TArray<uint16>& Data,FString& Result) const
{
    if(!IsValid(Target)||Target->GetWorld()!=GetWorld()||!Target->GetLandscapeInfo()) {Result=TEXT("대상 지형이 현재 레벨에 없습니다.");return false;}
    FIntPoint Min,Max;
    if(!Target->GetLandscapeInfo()->GetLandscapeExtent(Min.X,Min.Y,Max.X,Max.Y)||Min!=GridMin||Max!=GridMax)
    {Result=TEXT("지형의 구획 범위가 변경됐습니다. 새 설계에서 다시 연결하세요.");return false;}
    Target->ForceUpdateLayersContent();
    Data.Init(32768,(GridMax.X-GridMin.X+1)*(GridMax.Y-GridMin.Y+1));
    FHeightmapAccessor<false> Access(Target->GetLandscapeInfo()); Access.SetEditLayer(FGuid());
    Access.GetDataFast(GridMin.X,GridMin.Y,GridMax.X,GridMax.Y,Data.GetData());return true;
}

bool ACozyTerrainLayout::WriteLayer(const TArray<uint16>& Data,FString& Result)
{
    if(!Target||!Target->GetEditLayer(EditLayerGuid)){Result=TEXT("전용 지형 편집 레이어를 찾지 못했습니다.");return false;}
    FScopedSetLandscapeEditingLayer Scope(Target,EditLayerGuid);
    FHeightmapAccessor<false> Access(Target->GetLandscapeInfo()); Access.SetEditLayer(EditLayerGuid);
    Access.SetData(GridMin.X,GridMin.Y,GridMax.X,GridMax.Y,Data.GetData());Access.Flush();
    Target->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All,true);
    Target->MarkPackageDirty(); return true;
}

bool ACozyTerrainLayout::Apply(FString& Result)
{
    if(!GEditor||GEditor->PlayWorld||!Recipe.Validate(Result)) return false;
    if(!GetActorRotation().IsNearlyZero()||!GetActorScale3D().Equals(FVector::OneVector,.001))
    {Result=TEXT("설계 액터의 회전은 0, 배율은 1을 유지하세요. 크기는 설계의 미터 수치로 바꾸세요.");return false;}
    const FScopedTransaction Transaction(NSLOCTEXT("CozyTerrain","Apply","지형 설계 적용")); Modify();
    if(!Target)
    {
        // 기존 지형을 자동 선택하거나 삭제하지 않는다. 겹치는 새 지형 생성도 거부한다.
        const FVector P=GetActorLocation();
        const FBox Area(P-FVector(Recipe.MapSize.X*50,Recipe.MapSize.Y*50,100000),P+FVector(Recipe.MapSize.X*50,Recipe.MapSize.Y*50,100000));
        for(TActorIterator<ALandscapeProxy> It(GetWorld());It;++It) if(Area.Intersect(It->GetComponentsBoundingBox(true)))
        {Result=TEXT("기존 Landscape와 영역이 겹칩니다. 기존 지형 연결을 사용하거나 편집 영역을 옮기세요.");return false;}
        Target=GetWorld()->SpawnActor<ALandscape>();Target->SetFlags(RF_Transactional);Target->Modify();
        const int Q=Recipe.Components*63;
        Target->SetActorLocation(P-FVector(Recipe.MapSize.X*50,Recipe.MapSize.Y*50,0));
        Target->SetActorScale3D(FVector(Recipe.MapSize.X*100/Q,Recipe.MapSize.Y*100/Q,200));
        TMap<FGuid,TArray<uint16>> Heights;Heights.Add(FGuid(),TArray<uint16>());Heights[FGuid()].Init(32768,(Q+1)*(Q+1));
        TMap<FGuid,TArray<FLandscapeImportLayerInfo>> ImportLayers;ImportLayers.Add(FGuid(),{});
        Target->Import(FGuid::NewGuid(),0,0,Q,Q,1,63,Heights,TEXT(""),ImportLayers,ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());
        Target->SetActorLabel(TEXT("설계에서 만든 Landscape"));Target->SetFolderPath(TEXT("지형 편집기/적용 지형"));
        GridMin=FIntPoint(0,0);GridMax=FIntPoint(Q,Q);CapturedTransform=Target->LandscapeActorToWorld();
        BoundCenter=GetActorLocation();BoundSize=Recipe.MapSize;Baseline.Init(32768,(Q+1)*(Q+1));Baked.Reset();BakeUndoLayer.Reset();bCanUndoBake=false;
        Target->LandscapeMaterial=GroundMaterial?GroundMaterial.Get():LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        Target->PostEditChange();
    }
    if(!Target->LandscapeActorToWorld().Equals(CapturedTransform,.01)||!GetActorLocation().Equals(BoundCenter,.01)||!Recipe.MapSize.Equals(BoundSize,.01))
    {Result=TEXT("연결 후 영역 위치·크기 또는 대상 지형의 변환이 바뀌었습니다. 새 설계에서 다시 연결하세요.");return false;}
    TArray<uint16> Current; if(!ReadTarget(Current,Result)) return false;
    const bool Existed=EditLayerGuid.IsValid()&&Target->GetEditLayer(EditLayerGuid);
    TArray<uint16> Old;
    if(Existed){Old.Init(32768,Baseline.Num());FHeightmapAccessor<false> A(Target->GetLandscapeInfo());A.SetEditLayer(EditLayerGuid);A.GetDataFast(GridMin.X,GridMin.Y,GridMax.X,GridMax.Y,Old.GetData());}
    for(int I=0;I<Baseline.Num();++I)
    {
        const int Expected=FMath::Clamp(int(Baseline[I])+(Existed?int(Old[I])-32768:0),0,65535);
        if(FMath::Abs(int(Current[I])-Expected)>3){Result=TEXT("다른 편집 레이어의 높이가 연결 당시와 달라졌습니다. 원본을 보존하기 위해 적용을 중단했습니다. 새 설계에서 다시 연결하세요.");return false;}
    }
    // 실제 원본 레이어를 덮어쓰지 않고 전용 가산 레이어에 높이 차이만 기록한다.
    TArray<uint16> Delta;Delta.SetNum(Baseline.Num());const int W=GridMax.X-GridMin.X+1,H=GridMax.Y-GridMin.Y+1;
    for(int Y=0;Y<H;++Y) for(int X=0;X<W;++X)
    {
        FVector2D P((double(X)/(W-1)-.5)*BoundSize.X,(double(Y)/(H-1)-.5)*BoundSize.Y);
        const float Base=((int(Baseline[Y*W+X])-32768)/128.f*CapturedTransform.GetScale3D().Z+CapturedTransform.GetLocation().Z-GetActorLocation().Z)/100;
        const double Value=32768+(Sample(P)-Base)*100*128/CapturedTransform.GetScale3D().Z;
        if(!FMath::IsFinite(Value)||Value<0||Value>65535){Result=TEXT("높이가 대상 지형의 표현 범위를 넘습니다. 봉우리나 기본 높이를 낮추세요.");return false;}
        Delta[Y*W+X]=FMath::RoundToInt(Value);
    }
    FBufferArchive Backup;int32 Version=1;FGuid Guid=Target->GetLandscapeGuid();FIntPoint Min=GridMin,Max=GridMax;bool Had=Existed;
    Backup<<Version<<Guid<<Min<<Max<<Had<<Old;
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("CozyTerrainEditor/Backups");IFileManager::Get().MakeDirectory(*Folder,true);
    const FString BackupPath=Folder/(FGuid::NewGuid().ToString()+TEXT(".bin"));
    if(!FFileHelper::SaveArrayToFile(Backup,*BackupPath)){Result=TEXT("사본 저장에 실패해 적용하지 않았습니다.");return false;}
    LastBackup=BackupPath;Target->Modify();
    if(!Existed)
    {
        const int Index=Target->CreateLayer(FName(*FString::Printf(TEXT("지형 편집기_%s"),*GetName())));
        if(Index==INDEX_NONE){Result=TEXT("편집 레이어를 추가하지 못했습니다.");return false;}
        EditLayerGuid=Target->GetEditLayer(Index)->GetGuid();
    }
    if(!WriteLayer(Delta,Result)) return false;
    PrevAppliedRecipe=AppliedRecipe;bHasPrevApplied=bHasApplied;
    if(bBakeOnApply)
    {
        // 원형을 지형에 굳힌다: 쓴 레이어를 새 기준으로 삼고 평지·산·길 목록을 비움 (기본 높이도 레이어에 들어감)
        BakeUndoLayer=Baked;BakeUndoRecipe=Recipe;bCanUndoBake=true;
        Baked=Delta;Recipe.Landforms.Reset();Recipe.Routes.Reset();Recipe.BaseHeight=0;
    }
    else bCanUndoBake=false;
    AppliedRecipe=Recipe;bHasApplied=true;
    bHasUnappliedChanges=false;bShowPreview=false;Preview->SetVisibility(false);MarkPackageDirty();
    Result=bBakeOnApply?TEXT("지형에 굳혔습니다 · 원형 목록을 비웠습니다. 되돌리려면 '적용 전 높이 복구'. 맵 저장(Ctrl+S)은 직접 하세요.")
        :TEXT("적용 완료 · 원본 레이어 유지 · 적용 전 사본 저장됨. 맵 저장은 직접 실행하세요.");return true;
}

bool ACozyTerrainLayout::Restore(FString& Result)
{
    if(!GEditor||GEditor->PlayWorld||!Target||!EditLayerGuid.IsValid()||!Target->GetEditLayer(EditLayerGuid)){Result=TEXT("복구할 전용 레이어가 없습니다.");return false;}
    TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*LastBackup)){Result=TEXT("적용 전 사본을 읽지 못했습니다.");return false;}
    FMemoryReader Reader(Bytes);int32 Version;FGuid Guid;FIntPoint Min,Max;bool Existed;TArray<uint16> Old;
    Reader<<Version<<Guid<<Min<<Max<<Existed;
    int32 Count=0;Reader<<Count;
    if(Reader.IsError()||Count!=(Existed?Baseline.Num():0)||Reader.TotalSize()-Reader.Tell()!=int64(Count)*sizeof(uint16))
    {Result=TEXT("사본 크기가 올바르지 않아 복구하지 않았습니다.");return false;}
    Old.SetNum(Count);if(Count)Reader.Serialize(Old.GetData(),Count*sizeof(uint16));
    if(Reader.IsError()||Version!=1||Guid!=Target->GetLandscapeGuid()||Min!=GridMin||Max!=GridMax||(Existed&&Old.Num()!=Baseline.Num())||!Target->LandscapeActorToWorld().Equals(CapturedTransform,.01))
    {Result=TEXT("사본의 대상·범위가 현재 지형과 달라 복구하지 않았습니다.");return false;}
    const FScopedTransaction Transaction(NSLOCTEXT("CozyTerrain","Restore","적용 전 지형으로 복구"));Modify();Target->Modify();
    if(Existed){if(!WriteLayer(Old,Result))return false;}
    else {Target->DeleteLayer(Target->GetLayerIndex(EditLayerGuid));EditLayerGuid.Invalidate();Target->RequestLayersContentUpdateForceAll();}
    // 미리보기 기준도 직전 적용분으로 (복구는 한 단계만)
    AppliedRecipe=PrevAppliedRecipe;bHasApplied=bHasPrevApplied;bHasPrevApplied=false;
    // 굳혔던 적용이면 기준 레이어와 원형 목록도 굳히기 전으로
    const bool bWasBaked=bCanUndoBake;
    if(bCanUndoBake){Baked=BakeUndoLayer;Recipe=BakeUndoRecipe;bCanUndoBake=false;bShowPreview=true;}
    bHasUnappliedChanges=true;Target->MarkPackageDirty();RebuildPreview();
    Result=bWasBaked?TEXT("마지막 적용 전 높이로 복구하고 굳혔던 원형을 다시 불러왔습니다."):TEXT("마지막 적용 전 높이로 복구했습니다. 설계 수치는 유지됩니다.");return true;
}

FString ACozyTerrainLayout::PresetPath() const{return FPaths::ProjectSavedDir()/TEXT("CozyTerrainEditor/Presets")/(FPaths::MakeValidFileName(PresetName)+TEXT(".json"));}
bool ACozyTerrainLayout::SavePreset(FString& Result) const
{
    if(!Recipe.Validate(Result)||PresetName.TrimStartAndEnd().IsEmpty())return false;
    FString Json;FJsonObjectConverter::UStructToJsonObjectString(Recipe,Json);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(PresetPath()),true);
    if(!FFileHelper::SaveStringToFile(Json,*PresetPath(),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)){Result=TEXT("설정 저장 실패");return false;}
    Result=TEXT("설정 저장: ")+PresetPath();return true;
}
bool ACozyTerrainLayout::LoadPreset(FString& Result)
{
    FString Json;FCozyTerrainRecipe Candidate;
    if(!FFileHelper::LoadFileToString(Json,*PresetPath())||!FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Candidate,0,0)||!Candidate.Validate(Result))
    {Result=TEXT("설정 파일이 없거나 지원하는 형식이 아닙니다.");return false;}
    const FScopedTransaction Transaction(NSLOCTEXT("CozyTerrain","Load","지형 설정 불러오기"));Modify();Recipe=MoveTemp(Candidate);bHasUnappliedChanges=true;bShowPreview=true;RebuildPreview();MarkPackageDirty();
    Result=TEXT("설정을 불러왔습니다. 실제 지형은 적용 버튼을 누르기 전까지 바뀌지 않습니다.");return true;
}
FBox2D ACozyTerrainLayout::EstateBounds() const
{
    const FVector2D Half=FVector2D(EstateCells)*CellSize*.5;
    return FBox2D(EstateCenter-Half,EstateCenter+Half);
}
bool ACozyTerrainLayout::SnapLandform(FCozyLandform& F) const
{
    const FBox2D Estate=EstateBounds();
    if(!bSnapToEstateGrid||F.Type!=ECozyLandform::Plateau||CellSize<=0||!Estate.IsInside(F.Center))return false;
    // 90·270도는 가로·세로를 바꿔 0도로 (타원은 180도 돌려도 같음)
    const int Quarter=(FMath::RoundToInt(F.Rotation/90.f)%4+4)%4;
    if(Quarter%2)F.Size=FVector2D(F.Size.Y,F.Size.X);
    F.Rotation=0;
    F.Size.X=FMath::Max(1.,FMath::RoundToDouble(F.Size.X/CellSize))*CellSize;
    F.Size.Y=FMath::Max(1.,FMath::RoundToDouble(F.Size.Y/CellSize))*CellSize;
    // 왼쪽 아래 모서리를 칸 선에 맞춘 뒤 중심을 다시 계산
    const FVector2D Corner=F.Center-F.Size*.5-Estate.Min;
    F.Center=Estate.Min+FVector2D(FMath::RoundToDouble(Corner.X/CellSize),FMath::RoundToDouble(Corner.Y/CellSize))*CellSize+F.Size*.5;
    if(TierHeights.Num())
    {
        float Best=TierHeights[0];
        for(float Tier:TierHeights)if(FMath::Abs(Tier-F.Height)<FMath::Abs(Best-F.Height))Best=Tier;
        F.Height=Best;
    }
    return true;
}
void ACozyTerrainLayout::AddDemo()
{
    Recipe.Landforms.Reset();Recipe.Routes.Reset();
    FCozyLandform Hill;Hill.Type=ECozyLandform::Mountain;Hill.Name=TEXT("뒤쪽 능선");Hill.Center=FVector2D(0,45);Hill.Size=FVector2D(150,100);Hill.Height=45;Recipe.Landforms.Add(Hill);
    FCozyLandform A;A.Name=TEXT("시작 평지");A.Center=FVector2D(-35,-30);A.Size=FVector2D(54,42);A.Height=0;Recipe.Landforms.Add(A);
    FCozyLandform B;B.Name=TEXT("확장 평지");B.Center=FVector2D(40,30);B.Size=FVector2D(50,40);B.Height=12;Recipe.Landforms.Add(B);
    FCozyTerrainRoute R;R.Name=TEXT("시작에서 확장");R.Points={FVector(-20,-25,0),FVector(0,-20,0),FVector(25,0,12),FVector(35,20,12)};Recipe.Routes.Add(R);RebuildPreview();
}
