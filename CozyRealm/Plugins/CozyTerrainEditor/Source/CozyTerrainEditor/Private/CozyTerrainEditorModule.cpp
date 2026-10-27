#include "Modules/ModuleManager.h"
#include "CozyTerrainLayout.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "ProceduralMeshComponent.h"
#include "ToolMenus.h"
#include "ScopedTransaction.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Rendering/DrawElements.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Docking/TabManager.h"
#include "UObject/UnrealType.h"
#include "LevelEditorViewport.h"

namespace CozyTerrainUI
{
static const FName TabName(TEXT("CozyTerrainEditor"));
static FText T(const TCHAR* S){return FText::FromString(S);}
class SPanel;
class SMap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMap){} SLATE_END_ARGS()
    TWeakPtr<SPanel> Panel;
    void Construct(const FArguments&,TSharedRef<SPanel> InPanel){Panel=InPanel;SetClipping(EWidgetClipping::ClipToBounds);}
    // 작게 시작해 창이 화면 밖으로 나가지 않게 (창을 키우면 지도도 같이 커짐)
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(460,380);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
    TUniquePtr<FScopedTransaction> Drag;
    bool bResize=false;
};

class SPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPanel){} SLATE_END_ARGS()
    TWeakObjectPtr<ACozyTerrainLayout> Plan;
    TWeakObjectPtr<UWorld> LastWorld;
    TSharedPtr<IDetailsView> Details;
    TSharedPtr<SMap> Map;
    int Mode=0,Selected=INDEX_NONE,Route=INDEX_NONE,Node=INDEX_NONE;
    bool bContours=true,bSlope=true;
    FString Status=TEXT("현재 레벨에서 새 설계를 만들거나, Landscape를 선택한 뒤 연결하세요. 실제 지형은 적용 전까지 바뀌지 않습니다.");
    ACozyTerrainLayout* GetPlan() const
    {
        auto* P=Plan.Get();return P&&GEditor&&P->GetWorld()==GEditor->GetEditorWorldContext().World()?P:nullptr;
    }
    void Refresh()
    {
        if(auto* P=GetPlan()){P->RebuildPreview();if(Details)Details->SetObject(P,true);}
        if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);
    }
    void Message(FString S){Status=MoveTemp(S);Refresh();}
    void Mutate(TFunction<void(ACozyTerrainLayout&)> Fn)
    {
        if(auto* P=GetPlan()) {const FScopedTransaction Tx(T(TEXT("지형 설계 편집")));P->Modify();Fn(*P);P->bHasUnappliedChanges=true;P->bShowPreview=true;P->MarkPackageDirty();Refresh();}
    }
    TSharedRef<SWidget> Button(const TCHAR* Label,TFunction<void()> Action)
    {return SNew(SButton).Text(T(Label)).OnClicked_Lambda([Action](){Action();return FReply::Handled();});}
    TSharedRef<SWidget> Number(const TCHAR* Label,TFunction<TOptional<float>()> Getter,TFunction<void(float)> Setter)
    {
        return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(STextBlock).Text(T(Label))]
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(SNumericEntryBox<float>).AllowSpin(false).Value_Lambda([Getter](){return Getter();}).OnValueCommitted_Lambda([Setter](float V,ETextCommit::Type){if(FMath::IsFinite(V)) Setter(V);})];
    }
    void SelectNone(){Selected=Route=Node=INDEX_NONE;}
    void FitView()
    {
        auto* P=GetPlan();
        if(!P){Message(TEXT("먼저 설계를 열거나 만드세요."));return;}
        // 연결된 Landscape가 있으면 그 범위, 없으면 설계 영역 중심 (미리보기를 꺼 두면 설계 액터의 범위가 비어 원점을 보던 문제)
        const FBox Box=P->Target?P->Target->GetComponentsBoundingBox(true):FBox();
        const FVector Center=Box.IsValid?Box.GetCenter():P->GetActorLocation();
        const double Span=FMath::Max(P->Recipe.MapSize.X,P->Recipe.MapSize.Y)*100;
        const FVector Eye=Center+FVector(-.55*Span,-.7*Span,.6*Span);
        // 지금 쓰는 레벨 뷰포트 → 없으면 첫 원근 뷰포트 · 자동 밝기 보정은 그대로 둠 (끄면 화면이 검게 됐음)
        FLevelEditorViewportClient* Client=GCurrentLevelEditingViewportClient&&GCurrentLevelEditingViewportClient->IsPerspective()?GCurrentLevelEditingViewportClient:nullptr;
        if(!Client)for(auto* Each:GEditor->GetLevelViewportClients())if(Each&&Each->IsPerspective()){Client=Each;break;}
        if(!Client){Message(TEXT("원근 레벨 뷰포트를 찾지 못했습니다."));return;}
        Client->SetViewLocation(Eye);Client->SetViewRotation((Center-Eye).Rotation());Client->Invalidate();
        Message(TEXT("레벨 뷰포트를 설계 영역에 맞췄습니다."));
    }
    void LoadExisting()
    {
        auto* W=GEditor->GetEditorWorldContext().World();
        for(TActorIterator<ACozyTerrainLayout> It(W);It;++It){Plan=*It;SelectNone();Message(TEXT("현재 레벨의 저장된 설계를 열었습니다."));return;}
        Message(TEXT("저장된 설계가 없습니다. 새 설계를 눌러 시작하세요."));
    }
    void NewPlan()
    {
        if(GEditor->PlayWorld){Message(TEXT("플레이를 종료한 뒤 편집하세요."));return;}
        const FScopedTransaction Tx(T(TEXT("새 지형 설계")));
        auto* P=GEditor->GetEditorWorldContext().World()->SpawnActor<ACozyTerrainLayout>();P->SetFlags(RF_Transactional);P->SetActorLabel(TEXT("지형 설계"));P->SetFolderPath(TEXT("지형 편집기/설계"));Plan=P;SelectNone();Message(TEXT("새 설계 · 왼쪽 도구를 고른 뒤 지도에 클릭하세요."));
    }
    void BindSelected()
    {
        ALandscape* L=nullptr;
        for(FSelectionIterator I(*GEditor->GetSelectedActors());I;++I)if(auto* Proxy=Cast<ALandscapeProxy>(*I))if(Proxy->GetLandscapeInfo())L=Proxy->GetLandscapeInfo()->LandscapeActor.Get();
        if(!L){Message(TEXT("먼저 아웃라이너에서 적용할 Landscape를 선택하세요."));return;}
        if(!GetPlan())NewPlan();
        const FScopedTransaction Tx(T(TEXT("현재 Landscape 연결")));GetPlan()->Modify();FString R;GetPlan()->BindLandscape(L,R);Message(R);
    }
    void Construct(const FArguments&)
    {
        FDetailsViewArgs Args;Args.bHideSelectionTip=true;Args.bLockable=false;Args.bAllowSearch=true;
        Details=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(Args);
        Details->SetDisableCustomDetailLayouts(true);
        Details->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P)
        {
            const FProperty* Root=P.ParentProperties.IsEmpty()?&P.Property:P.ParentProperties.Last();
            if(Root->GetOwnerStruct()!=ACozyTerrainLayout::StaticClass()||!Root->HasMetaData(TEXT("Category"))) return false;
            // 지도·위 숫자 칸과 겹치는 값은 숨김: 미리보기 체크(왼쪽 체크로) · 평지·산·길 원본 목록 · 설정 버전
            static const TSet<FName> Hidden={GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,Preview),GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,bShowPreview),
                GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Landforms),GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Routes),GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Version)};
            if(Hidden.Contains(Root->GetFName())||Hidden.Contains(P.Property.GetFName())) return false;
            for(const FProperty* Parent:P.ParentProperties) if(Hidden.Contains(Parent->GetFName())) return false;
            return true;
        }));
        Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&){if(auto* P=GetPlan())P->RebuildPreview();if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);});
        auto Tool=[this](const TCHAR* Label,int M){return Button(Label,[this,M](){Mode=M;if(M==3){Route=Node=INDEX_NONE;Selected=INDEX_NONE;}Message(M==3?TEXT("지도에 경유점을 차례로 클릭하세요. 선택 도구로 바꾸면 길이 끝납니다."):TEXT("도구 선택됨 · 지도에서 클릭하거나 선택한 요소를 드래그하세요."));});};
        ChildSlot[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).Text(T(TEXT("지형 편집기  ·  현재 레벨의 평지·산·길 설계"))).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))]
        +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("새 설계"),[this](){NewPlan();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("저장된 설계 열기"),[this](){LoadExisting();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("선택 Landscape 연결"),[this](){BindSelected();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("3D 화면 맞추기"),[this](){FitView();})]
            +SHorizontalBox::Slot().FillWidth(1).Padding(6)[SNew(STextBlock).Text_Lambda([](){auto* W=GEditor->GetEditorWorldContext().World();return FText::FromString(W?W->GetName():TEXT("레벨 없음"));})]]
        +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(STextBlock).Text_Lambda([this](){auto* P=GetPlan();if(!P)return T(TEXT("① 새 설계 → ② 평지·산·길 → ③ 3D 확인 → ④ 적용 → ⑤ 맵 저장"));return FText::FromString(FString::Printf(TEXT("%.0f × %.0fm  |  %d개 지형 요소 · %d개 길  |  %s"),P->Recipe.MapSize.X,P->Recipe.MapSize.Y,P->Recipe.Landforms.Num(),P->Recipe.Routes.Num(),P->bHasUnappliedChanges?TEXT("미적용 변경 있음 · 적용 버튼 필요"):TEXT("Landscape 적용됨")));})]
        +SVerticalBox::Slot().FillHeight(1).Padding(4)[SNew(SSplitter)
            +SSplitter::Slot().Value(.15f)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("선택·이동"),0)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("평지 추가"),1)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("산·능선 추가"),2)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("길 그리기"),3)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Button(TEXT("선택 요소 삭제"),[this](){Mutate([this](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms.RemoveAt(Selected);else if(P.Recipe.Routes.IsValidIndex(Route)){if(P.Recipe.Routes[Route].Points.IsValidIndex(Node))P.Recipe.Routes[Route].Points.RemoveAt(Node);else P.Recipe.Routes.RemoveAt(Route);}SelectNone();});})]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bContours=S==ECheckBoxState::Checked;Refresh();})[SNew(STextBlock).Text(T(TEXT("등고선 5m")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSlope=S==ECheckBoxState::Checked;Refresh();})[SNew(STextBlock).Text(T(TEXT("급경사 표시")))]]
                // 3D 미리보기 켜기·끄기 (아래 따로 있던 '미리보기' 버튼과 세부 정보 칸의 같은 체크를 여기 하나로)
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked_Lambda([this](){auto* P=GetPlan();return P&&P->bShowPreview?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* P=GetPlan()){P->bShowPreview=S==ECheckBoxState::Checked;P->RebuildPreview();}})[SNew(STextBlock).Text(T(TEXT("3D 미리보기 (바뀌는 곳만)")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(STextBlock).AutoWrapText(true).Text(T(TEXT("청록 낮음 · 노랑 높음 · 빨강 급경사\n사각 손잡이 = 크기")))]]
            +SSplitter::Slot().Value(.53f)[SAssignNew(Map,SMap,SharedThis(this))]
            +SSplitter::Slot().Value(.32f)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text_Lambda([this](){auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return FText::FromString(P->Recipe.Landforms[Selected].Name);if(P&&P->Recipe.Routes.IsValidIndex(Route))return FText::FromString(P->Recipe.Routes[Route].Name);return T(TEXT("선택 요소 / 전체 설계"));})]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("선택 높이 (m)"),[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Height;if(P&&P->Recipe.Routes.IsValidIndex(Route)&&P->Recipe.Routes[Route].Points.IsValidIndex(Node))return P->Recipe.Routes[Route].Points[Node].Z;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Height=FMath::Clamp(V,-200.f,200.f);else if(P.Recipe.Routes.IsValidIndex(Route)&&P.Recipe.Routes[Route].Points.IsValidIndex(Node))P.Recipe.Routes[Route].Points[Node].Z=FMath::Clamp(V,-200.f,200.f);});})]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("가로 / 길 폭 (m)"),[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Size.X;if(P&&P->Recipe.Routes.IsValidIndex(Route))return P->Recipe.Routes[Route].Width;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Size.X=FMath::Clamp(V,2.f,2000.f);else if(P.Recipe.Routes.IsValidIndex(Route))P.Recipe.Routes[Route].Width=FMath::Clamp(V,2.f,80.f);});})]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("세로 (m)"),[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Size.Y;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Size.Y=FMath::Clamp(V,2.f,2000.f);});})]
                // 세부 정보 칸에서 원본 목록을 숨긴 대신 거기서만 고칠 수 있던 값을 여기로
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("방향 (도)"),[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Rotation;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Rotation=FMath::Fmod(V,360.f);});})]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("주변 연결 폭 (m)"),[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Blend;if(P&&P->Recipe.Routes.IsValidIndex(Route))return P->Recipe.Routes[Route].Blend;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Blend=FMath::Clamp(V,1.f,200.f);else if(P.Recipe.Routes.IsValidIndex(Route))P.Recipe.Routes[Route].Blend=FMath::Clamp(V,1.f,80.f);});})]
                +SVerticalBox::Slot().AutoHeight().Padding(4,8,4,2)[SNew(STextBlock).Text(T(TEXT("전체 설계 · 재질 · 설정 이름")))]
                +SVerticalBox::Slot().FillHeight(1)[Details.ToSharedRef()]]]
        +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("되돌리기"),[this](){GEditor->UndoTransaction();SelectNone();Refresh();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("다시 실행"),[this](){GEditor->RedoTransaction();SelectNone();Refresh();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("설정 저장"),[this](){if(auto* P=GetPlan()){FString R;P->SavePreset(R);Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("설정 불러오기"),[this](){if(auto* P=GetPlan()){FString R;P->LoadPreset(R);SelectNone();Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("사본 보존 후 적용"),[this](){if(auto* P=GetPlan()){FString R;P->Apply(R);Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("적용 전 높이 복구"),[this](){if(auto* P=GetPlan()){FString R;P->Restore(R);Message(R);}})]]
        +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(Status);})]];
        LastWorld=GEditor->GetEditorWorldContext().World();LoadExisting();
    }
    virtual void Tick(const FGeometry& G,double Tm,float Dt) override
    {
        SCompoundWidget::Tick(G,Tm,Dt);
        auto* W=GEditor->GetEditorWorldContext().World();if(W!=LastWorld.Get()){LastWorld=W;Plan=nullptr;SelectNone();Details->SetObject(nullptr);Status=TEXT("레벨이 변경됐습니다. 이 레벨의 설계를 열거나 새로 만드세요.");}
    }
};

static FVector2D ToMap(FVector2D P,const FVector2D& Size,const FVector2D& Span){return FVector2D((P.X/Span.X+.5)*Size.X,(.5-P.Y/Span.Y)*Size.Y);}
static FVector2D FromMap(FVector2D P,const FVector2D& Size,const FVector2D& Span){return FVector2D((P.X/Size.X-.5)*Span.X,(.5-P.Y/Size.Y)*Span.Y);}
int32 SMap::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const
{
    const auto Owner=Panel.Pin();const auto* P=Owner?Owner->GetPlan():nullptr;const FVector2D Size=G.GetLocalSize();
    const FSlateBrush* Brush=FAppStyle::GetBrush("WhiteBrush");
    FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor(.025,.04,.05));
    if(!P){FSlateDrawElement::MakeText(Out,L+1,G.ToPaintGeometry(),TEXT("새 설계 또는 선택 Landscape 연결로 시작"),FAppStyle::GetFontStyle("NormalFont"),ESlateDrawEffect::None,FLinearColor::White);return L+1;}
    constexpr int N=48;float Heights[N+1][N+1];float Low=FLT_MAX,High=-FLT_MAX;
    for(int Y=0;Y<=N;++Y)for(int X=0;X<=N;++X){float Z=P->Sample(FromMap(FVector2D(double(X)/N*Size.X,double(Y)/N*Size.Y),Size,P->Recipe.MapSize));Heights[Y][X]=Z;Low=FMath::Min(Low,Z);High=FMath::Max(High,Z);}
    auto Line=[&](FVector2D A,FVector2D B,FLinearColor C,float Width=1.f){TArray<FVector2D> V={A,B};FSlateDrawElement::MakeLines(Out,L+2,G.ToPaintGeometry(),V,ESlateDrawEffect::None,C,true,Width);};
    for(int Y=0;Y<N;++Y)for(int X=0;X<N;++X)
    {
        const float Z=Heights[Y][X],T=(Z-Low)/FMath::Max(1.f,High-Low);
        FLinearColor C=FMath::Lerp(FLinearColor(.04,.25,.25),FLinearColor(.88,.65,.25),T);
        const float DX=(Heights[Y][X+1]-Z)/(P->Recipe.MapSize.X/N),DY=(Heights[Y+1][X]-Z)/(P->Recipe.MapSize.Y/N);
        if(Owner->bSlope&&FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX+DY*DY)))>P->SlopeLimit)C=FLinearColor(.75,.12,.07);
        const FVector2D A(double(X)/N*Size.X,double(Y)/N*Size.Y),Cell(Size.X/N+1,Size.Y/N+1);
        FSlateDrawElement::MakeBox(Out,L+1,G.ToPaintGeometry(FVector2f(Cell),FSlateLayoutTransform(FVector2f(A))),Brush,ESlateDrawEffect::None,C);
        if(Owner->bContours)
        {
            // 정사각 셀 변의 등고선 교점을 선형 보간한다.
            const FVector2D Corners[]={A,A+FVector2D(Size.X/N,0),A+FVector2D(Size.X/N,Size.Y/N),A+FVector2D(0,Size.Y/N)};
            const float Values[]={Z,Heights[Y][X+1],Heights[Y+1][X+1],Heights[Y+1][X]};
            float Min=Z,Max=Z;for(float V:Values){Min=FMath::Min(Min,V);Max=FMath::Max(Max,V);}
            for(int Band=FMath::CeilToInt(Min/5);Band<=FMath::FloorToInt(Max/5)&&Band<FMath::CeilToInt(Min/5)+50;++Band)
            {
                TArray<FVector2D> Hits;float Level=Band*5;
                for(int E=0;E<4;++E){int B=(E+1)%4;if((Values[E]<Level)!=(Values[B]<Level))Hits.Add(FMath::Lerp(Corners[E],Corners[B],double((Level-Values[E])/(Values[B]-Values[E]))));}
                for(int I=1;I<Hits.Num();I+=2)Line(Hits[I-1],Hits[I],FLinearColor(1,1,1,.25));
            }
        }
    }
    for(int I=0;I<P->Recipe.Landforms.Num();++I)
    {
        const auto& F=P->Recipe.Landforms[I];const FLinearColor C=Owner->Selected==I?FLinearColor(.1,.9,1):FLinearColor(.95,.95,.85);
        for(int K=0;K<48;++K){auto Pt=[&](int A){double R=A*2*PI/48;return ToMap(F.Center+FVector2D(FMath::Cos(R)*F.Size.X*.5,FMath::Sin(R)*F.Size.Y*.5).GetRotated(F.Rotation),Size,P->Recipe.MapSize);};Line(Pt(K),Pt(K+1),C,Owner->Selected==I?3:1);}
        const FVector2D Center=ToMap(F.Center,Size,P->Recipe.MapSize);
        FSlateDrawElement::MakeText(Out,L+3,G.ToPaintGeometry(FVector2f(Size),FSlateLayoutTransform(FVector2f(Center+FVector2D(4,4)))),FString::Printf(TEXT("%s\n%.1fm"),*F.Name,F.Height),FAppStyle::GetFontStyle("NormalFont"),ESlateDrawEffect::None,FLinearColor::White);
        if(Owner->Selected==I){FVector2D H=ToMap(F.Center+F.Size*.5,Size,P->Recipe.MapSize);FSlateDrawElement::MakeBox(Out,L+4,G.ToPaintGeometry(FVector2f(12,12),FSlateLayoutTransform(FVector2f(H-FVector2D(6,6)))),Brush,ESlateDrawEffect::None,C);}
    }
    for(int R=0;R<P->Recipe.Routes.Num();++R)
    {
        const auto& Path=P->Recipe.Routes[R];for(int I=0;I<Path.Points.Num();++I)
        {
            FVector2D B=ToMap(FVector2D(Path.Points[I]),Size,P->Recipe.MapSize);
            if(I){FVector2D A=ToMap(FVector2D(Path.Points[I-1]),Size,P->Recipe.MapSize);double Slope=FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Path.Points[I].Z-Path.Points[I-1].Z),FMath::Max(.001,FVector::Dist2D(Path.Points[I],Path.Points[I-1]))));Line(A,B,Slope>P->SlopeLimit?FLinearColor(1,.1,.1):FLinearColor(.1,.65,1),4);}
            FSlateDrawElement::MakeBox(Out,L+4,G.ToPaintGeometry(FVector2f(9,9),FSlateLayoutTransform(FVector2f(B-FVector2D(4.5,4.5)))),Brush,ESlateDrawEffect::None,FLinearColor(.1,.8,1));
        }
    }
    return L+4;
}

FReply SMap::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    const auto O=Panel.Pin();auto* P=O?O->GetPlan():nullptr;if(!P||E.GetEffectingButton()!=EKeys::LeftMouseButton||GEditor->PlayWorld)return FReply::Unhandled();
    const FVector2D Mouse=G.AbsoluteToLocal(E.GetScreenSpacePosition()),Pos=FromMap(Mouse,G.GetLocalSize(),P->Recipe.MapSize);
    if(O->Mode==3)
    {
        O->Mutate([&](auto& A){if(!A.Recipe.Routes.IsValidIndex(O->Route)){O->Route=A.Recipe.Routes.AddDefaulted();O->Selected=INDEX_NONE;}auto& R=A.Recipe.Routes[O->Route];if(R.Points.Num()<256){R.Points.Add(FVector(Pos.X,Pos.Y,A.Sample(Pos)));O->Node=R.Points.Num()-1;}});return FReply::Handled();
    }
    if(O->Mode==1||O->Mode==2)
    {
        O->Mutate([&](auto& A){FCozyLandform F;F.Type=O->Mode==1?ECozyLandform::Plateau:ECozyLandform::Mountain;F.Center=Pos;F.Height=O->Mode==1?A.Sample(Pos):25;F.Name=O->Mode==1?TEXT("새 평지"):TEXT("새 산·능선");if(O->Mode==2)F.Size=FVector2D(90,70);O->Selected=A.Recipe.Landforms.Add(F);O->Route=O->Node=INDEX_NONE;});return FReply::Handled();
    }
    bResize=false;
    if(P->Recipe.Landforms.IsValidIndex(O->Selected)){const auto& F=P->Recipe.Landforms[O->Selected];bResize=FVector2D::Distance(Mouse,ToMap(F.Center+F.Size*.5,G.GetLocalSize(),P->Recipe.MapSize))<14;}
    if(!bResize)
    {
        O->SelectNone();
        for(int R=0;R<P->Recipe.Routes.Num();++R)for(int I=0;I<P->Recipe.Routes[R].Points.Num();++I)
            if(FVector2D::Distance(Mouse,ToMap(FVector2D(P->Recipe.Routes[R].Points[I]),G.GetLocalSize(),P->Recipe.MapSize))<13){O->Route=R;O->Node=I;}
        if(O->Route==INDEX_NONE)for(int I=P->Recipe.Landforms.Num()-1;I>=0;--I){const auto& F=P->Recipe.Landforms[I];FVector2D D=(Pos-F.Center).GetRotated(-F.Rotation);if(FMath::Square(D.X/F.Size.X*2)+FMath::Square(D.Y/F.Size.Y*2)<=1){O->Selected=I;break;}}
    }
    if(O->Selected!=INDEX_NONE||O->Route!=INDEX_NONE){Drag=MakeUnique<FScopedTransaction>(T(TEXT("지도에서 지형 조절")));P->Modify();O->Refresh();return FReply::Handled().CaptureMouse(SharedThis(this));}
    O->Refresh();return FReply::Handled();
}
FReply SMap::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    const auto O=Panel.Pin();auto* P=O?O->GetPlan():nullptr;if(!Drag||!P||!HasMouseCapture())return FReply::Unhandled();
    FVector2D Pos=FromMap(G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize(),P->Recipe.MapSize);
    Pos.X=FMath::Clamp(Pos.X,-P->Recipe.MapSize.X*.5,P->Recipe.MapSize.X*.5);Pos.Y=FMath::Clamp(Pos.Y,-P->Recipe.MapSize.Y*.5,P->Recipe.MapSize.Y*.5);
    if(P->Recipe.Landforms.IsValidIndex(O->Selected)){auto& F=P->Recipe.Landforms[O->Selected];if(bResize)F.Size=FVector2D(FMath::Clamp(FMath::Abs(Pos.X-F.Center.X)*2,2.,2000.),FMath::Clamp(FMath::Abs(Pos.Y-F.Center.Y)*2,2.,2000.));else F.Center=Pos;}
    else if(P->Recipe.Routes.IsValidIndex(O->Route)&&P->Recipe.Routes[O->Route].Points.IsValidIndex(O->Node)){auto& V=P->Recipe.Routes[O->Route].Points[O->Node];V.X=Pos.X;V.Y=Pos.Y;}
    P->bHasUnappliedChanges=true;P->bShowPreview=true;P->RebuildPreview();Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();
}
FReply SMap::OnMouseButtonUp(const FGeometry&,const FPointerEvent& E){if(E.GetEffectingButton()==EKeys::LeftMouseButton){Drag.Reset();if(auto O=Panel.Pin()){if(auto* P=O->GetPlan())P->MarkPackageDirty();O->Refresh();}return FReply::Handled().ReleaseMouseCapture();}return FReply::Unhandled();}
void SMap::OnMouseCaptureLost(const FCaptureLostEvent&){Drag.Reset();}
}

class FCozyTerrainEditorModule : public IModuleInterface
{
    TUniquePtr<FAutoConsoleCommand> OpenCommand;
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(CozyTerrainUI::TabName,FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&){return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(CozyTerrainUI::SPanel)];})).SetDisplayName(CozyTerrainUI::T(TEXT("지형 편집기"))).SetTooltipText(CozyTerrainUI::T(TEXT("현재 레벨에서 지형을 설계·미리보기·적용합니다")));
        // 창 메뉴 항목은 위 탭 등록이 자동으로 만든다 (따로 메뉴를 더하면 '지형 편집기'가 두 번 나왔음)
        OpenCommand=MakeUnique<FAutoConsoleCommand>(TEXT("Cozy.TerrainEditor"),TEXT("현재 레벨의 지형 편집기 열기"),FConsoleCommandDelegate::CreateLambda([](){FGlobalTabmanager::Get()->TryInvokeTab(CozyTerrainUI::TabName);}));
    }
    virtual void ShutdownModule() override
    {
        OpenCommand.Reset();FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CozyTerrainUI::TabName);
    }
};
IMPLEMENT_MODULE(FCozyTerrainEditorModule,CozyTerrainEditor)
