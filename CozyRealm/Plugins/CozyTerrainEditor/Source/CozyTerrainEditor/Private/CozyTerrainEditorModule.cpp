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
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
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
    virtual bool SupportsKeyboardFocus() const override{return true;}
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
    TUniquePtr<FScopedTransaction> Drag;
    bool bResize=false;
    // 평지·산 추가: 누른 곳에서 끈 범위로 만든다
    bool bCreating=false;FVector2D CreateStart=FVector2D::ZeroVector,CreateStartPixel=FVector2D::ZeroVector;
};

class SPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPanel){} SLATE_END_ARGS()
    TWeakObjectPtr<ACozyTerrainLayout> Plan;
    TWeakObjectPtr<UWorld> LastWorld;
    TSharedPtr<IDetailsView> Details;
    TSharedPtr<SMap> Map;
    // 오른쪽 요소 목록 (0 이상 = 평지·산 번호 · 음수 = -1-길 번호)
    TArray<TSharedPtr<int32>> Items;
    TSharedPtr<SListView<TSharedPtr<int32>>> List;
    int Mode=0,Selected=INDEX_NONE,Route=INDEX_NONE,Node=INDEX_NONE;
    bool bContours=true,bSlope=true;
    FString Status=TEXT("현재 레벨에서 새 설계를 만들거나, Landscape를 선택한 뒤 연결하세요. 실제 지형은 적용 전까지 바뀌지 않습니다.");
    ACozyTerrainLayout* GetPlan() const
    {
        auto* P=Plan.Get();return P&&GEditor&&P->GetWorld()==GEditor->GetEditorWorldContext().World()?P:nullptr;
    }
    void Refresh()
    {
        // 막대를 끄는 중에는 세부 정보 칸을 다시 만들지 않음 (느려지고 스크롤이 튐)
        if(auto* P=GetPlan()){P->RebuildPreview();if(Details&&!SliderTx)Details->SetObject(P,true);}
        if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);
        RebuildList();
    }
    void RebuildList()
    {
        Items.Reset();TSharedPtr<int32> Current;
        if(auto* P=GetPlan())
        {
            for(int I=0;I<P->Recipe.Landforms.Num();++I){Items.Add(MakeShared<int32>(I));if(I==Selected)Current=Items.Last();}
            for(int R=0;R<P->Recipe.Routes.Num();++R){Items.Add(MakeShared<int32>(-1-R));if(R==Route&&Selected==INDEX_NONE)Current=Items.Last();}
        }
        if(!List)return;
        List->RequestListRefresh();
        if(Current){List->SetSelection(Current,ESelectInfo::Direct);List->RequestScrollIntoView(Current);}else List->ClearSelection();
    }
    FText ItemText(int32 Id) const
    {
        auto* P=GetPlan();if(!P)return FText::GetEmpty();
        if(P->Recipe.Landforms.IsValidIndex(Id)){const auto& F=P->Recipe.Landforms[Id];return FText::FromString(FString::Printf(TEXT("%s · %s · %.1fm"),F.Type==ECozyLandform::Plateau?TEXT("평지"):TEXT("산"),*F.Name,F.Height));}
        const int R=-1-Id;if(P->Recipe.Routes.IsValidIndex(R)){const auto& W=P->Recipe.Routes[R];return FText::FromString(FString::Printf(TEXT("길 · %s · 점 %d개"),*W.Name,W.Points.Num()));}
        return FText::GetEmpty();
    }
    /** 지도 좌표에 있는 평지·산 (위에 그려진 것부터) */
    int HitLandform(const ACozyTerrainLayout& P,FVector2D Pos) const
    {
        for(int I=P.Recipe.Landforms.Num()-1;I>=0;--I){const auto& F=P.Recipe.Landforms[I];FVector2D D=(Pos-F.Center).GetRotated(-F.Rotation);if(FMath::Square(D.X/F.Size.X*2)+FMath::Square(D.Y/F.Size.Y*2)<=1)return I;}
        return INDEX_NONE;
    }
    void DeleteSelected()
    {
        Mutate([this](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms.RemoveAt(Selected);else if(P.Recipe.Routes.IsValidIndex(Route)){if(P.Recipe.Routes[Route].Points.IsValidIndex(Node))P.Recipe.Routes[Route].Points.RemoveAt(Node);else P.Recipe.Routes.RemoveAt(Route);}SelectNone();});
    }
    void Message(FString S){Status=MoveTemp(S);Refresh();}
    void Mutate(TFunction<void(ACozyTerrainLayout&)> Fn)
    {
        if(auto* P=GetPlan()) {const FScopedTransaction Tx(T(TEXT("지형 설계 편집")));P->Modify();Fn(*P);SnapSelected(*P);P->bHasUnappliedChanges=true;P->bShowPreview=true;P->MarkPackageDirty();Refresh();}
    }
    // 영지 안 평지는 고칠 때마다 칸·층 높이에 맞춤
    void SnapSelected(ACozyTerrainLayout& P)
    {
        if(P.Recipe.Landforms.IsValidIndex(Selected)&&P.SnapLandform(P.Recipe.Landforms[Selected]))
            Status=FString::Printf(TEXT("영지 안 평지라 %.0fm 칸과 층 높이에 맞췄습니다. 끄려면 왼쪽 '영지 칸 맞춤' 체크를 해제하세요."),P.CellSize);
    }
    TSharedRef<SWidget> Button(const TCHAR* Label,TFunction<void()> Action)
    {return SNew(SButton).Text(T(Label)).OnClicked_Lambda([Action](){Action();return FReply::Handled();});}
    // 숫자 막대를 끄는 동안 한 번의 되돌리기로 묶음
    TUniquePtr<FScopedTransaction> SliderTx;
    /** 세부 정보 칸처럼 끌어서 바꾸는 막대 + 직접 입력 · 소수 한 자리 */
    TSharedRef<SWidget> Number(const TCHAR* Label,float SliderMin,float SliderMax,TFunction<TOptional<float>()> Getter,TFunction<void(float)> Setter)
    {
        auto Set=[Setter](float V){if(FMath::IsFinite(V))Setter(FMath::RoundToFloat(V*10)/10);};
        return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(STextBlock).Text(T(Label))]
        +SHorizontalBox::Slot().FillWidth(1.4f).Padding(2)[SNew(SNumericEntryBox<float>).AllowSpin(true).MinSliderValue(SliderMin).MaxSliderValue(SliderMax).Delta(.1f).MaxFractionalDigits(1)
            .Value_Lambda([Getter](){return Getter();})
            .OnBeginSliderMovement_Lambda([this](){SliderTx=MakeUnique<FScopedTransaction>(T(TEXT("숫자 막대로 조절")));})
            .OnValueChanged_Lambda([this,Set](float V){if(SliderTx)Set(V);})
            .OnEndSliderMovement_Lambda([this](float){SliderTx.Reset();Refresh();})
            .OnValueCommitted_Lambda([Set](float V,ETextCommit::Type){Set(V);})];
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
                GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Landforms),GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Routes),GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Version),
                GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,ContourInterval),GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,bBakeOnApply),GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,bSnapToEstateGrid)};
            if(Hidden.Contains(Root->GetFName())||Hidden.Contains(P.Property.GetFName())) return false;
            for(const FProperty* Parent:P.ParentProperties) if(Hidden.Contains(Parent->GetFName())) return false;
            return true;
        }));
        Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&){if(auto* P=GetPlan())P->RebuildPreview();if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);});
        auto Tool=[this](const TCHAR* Label,int M)->TSharedRef<SWidget>
        {
            static const TCHAR* Help[]={TEXT("요소를 클릭해 고르고 끌어 옮기세요. 사각 손잡이로 크기 · Delete 키로 삭제."),TEXT("지도에서 끌어 평지 범위를 그리세요 (그냥 클릭하면 기본 크기). 다 그리면 선택 도구로 돌아갑니다."),TEXT("지도에서 끌어 산·능선 범위를 그리세요 (그냥 클릭하면 기본 크기). 다 그리면 선택 도구로 돌아갑니다."),TEXT("지도에 경유점을 차례로 클릭하세요. 끝내려면 Esc 또는 선택 도구.")};
            // 지금 켜진 도구는 파랗게
            return SNew(SButton).Text(T(Label)).ButtonColorAndOpacity_Lambda([this,M](){return Mode==M?FLinearColor(.25,.65,1):FLinearColor::White;})
                .OnClicked_Lambda([this,M](){Mode=M;if(M==3){Route=Node=INDEX_NONE;Selected=INDEX_NONE;}Message(Help[M]);return FReply::Handled();});
        };
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
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Button(TEXT("선택 요소 삭제"),[this](){DeleteSelected();})]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Button(TEXT("모두 지우기"),[this](){Mutate([this](auto& P){P.Recipe.Landforms.Reset();P.Recipe.Routes.Reset();SelectNone();});Message(TEXT("평지·산·길을 모두 지웠습니다. 실제 지형은 적용 전까지 그대로입니다. 되돌리기로 복구할 수 있습니다."));})]
                // 등고선: 같은 높이를 잇는 선 · 간격은 옆 숫자로
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bContours=S==ECheckBoxState::Checked;Refresh();})[SNew(STextBlock).Text(T(TEXT("등고선"))).ToolTipText(T(TEXT("지도에 같은 높이를 잇는 흰 선을 정해진 간격(m)마다 긋습니다. 선이 촘촘하면 가파르고 넓으면 완만합니다. 실제 지형에는 영향이 없습니다.")))]]
                    +SHorizontalBox::Slot().FillWidth(1).Padding(6,0,0,0)[SNew(SNumericEntryBox<float>).AllowSpin(true).MinSliderValue(1.f).MaxSliderValue(20.f).MinValue(.5f).MaxValue(50.f).Delta(.5f).MaxFractionalDigits(1)
                        .Value_Lambda([this]()->TOptional<float>{auto* P=GetPlan();return P?TOptional<float>(P->ContourInterval):TOptional<float>();})
                        .OnValueChanged_Lambda([this](float V){if(auto* P=GetPlan()){P->ContourInterval=FMath::Clamp(V,.5f,50.f);if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);}})
                        .OnValueCommitted_Lambda([this](float V,ETextCommit::Type){if(auto* P=GetPlan()){P->Modify();P->ContourInterval=FMath::Clamp(V,.5f,50.f);P->MarkPackageDirty();if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);}})]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3,0)[SNew(STextBlock).Text(T(TEXT("m")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSlope=S==ECheckBoxState::Checked;Refresh();})[SNew(STextBlock).Text(T(TEXT("급경사 표시")))]]
                // 3D 미리보기 켜기·끄기 (아래 따로 있던 '미리보기' 버튼과 세부 정보 칸의 같은 체크를 여기 하나로)
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked_Lambda([this](){auto* P=GetPlan();return P&&P->bShowPreview?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* P=GetPlan()){P->bShowPreview=S==ECheckBoxState::Checked;P->RebuildPreview();}})[SNew(STextBlock).Text(T(TEXT("3D 미리보기"))).ToolTipText(T(TEXT("설계를 바꾼 곳만 3D 화면에 색으로 표시합니다 (주황 올라감 · 파랑 내려감 · 빨강 급경사)")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked_Lambda([this](){auto* P=GetPlan();return P&&P->bSnapToEstateGrid?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* P=GetPlan()){const FScopedTransaction Tx(T(TEXT("영지 칸 맞춤 전환")));P->Modify();P->bSnapToEstateGrid=S==ECheckBoxState::Checked;P->MarkPackageDirty();Refresh();}})[SNew(STextBlock).Text(T(TEXT("영지 칸 맞춤"))).ToolTipText(T(TEXT("초록 격자(영지) 안의 평지를 칸 크기와 층 높이에 맞춥니다. 영지 위치·칸 수·층 높이는 아래 '06 영지 칸'에서 바꿉니다.")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked_Lambda([this](){auto* P=GetPlan();return P&&P->bBakeOnApply?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* P=GetPlan()){P->Modify();P->bBakeOnApply=S==ECheckBoxState::Checked;P->MarkPackageDirty();}})[SNew(STextBlock).Text(T(TEXT("적용하면 원형 정리"))).ToolTipText(T(TEXT("'지형에 적용'을 누르면 평지·산·길 원형을 지형에 굳히고 목록을 비웁니다. 다음 설계는 바뀐 지형 위에서 시작합니다. '적용 전 높이 복구'를 누르면 지형과 원형이 함께 돌아옵니다.")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(STextBlock).AutoWrapText(true).Text(T(TEXT("청록 낮음\n노랑 높음\n빨강 급경사\n사각 손잡이 = 크기")))]]
            +SSplitter::Slot().Value(.53f)[SAssignNew(Map,SMap,SharedThis(this))]
            +SSplitter::Slot().Value(.32f)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text_Lambda([this](){auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return FText::FromString(P->Recipe.Landforms[Selected].Name);if(P&&P->Recipe.Routes.IsValidIndex(Route))return FText::FromString(P->Recipe.Routes[Route].Name);return T(TEXT("선택 요소 / 전체 설계"));})]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("선택 높이 (m)"),-20,60,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Height;if(P&&P->Recipe.Routes.IsValidIndex(Route)&&P->Recipe.Routes[Route].Points.IsValidIndex(Node))return P->Recipe.Routes[Route].Points[Node].Z;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Height=FMath::Clamp(V,-200.f,200.f);else if(P.Recipe.Routes.IsValidIndex(Route)&&P.Recipe.Routes[Route].Points.IsValidIndex(Node))P.Recipe.Routes[Route].Points[Node].Z=FMath::Clamp(V,-200.f,200.f);});})]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("가로 / 길 폭 (m)"),2,200,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Size.X;if(P&&P->Recipe.Routes.IsValidIndex(Route))return P->Recipe.Routes[Route].Width;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Size.X=FMath::Clamp(V,2.f,2000.f);else if(P.Recipe.Routes.IsValidIndex(Route))P.Recipe.Routes[Route].Width=FMath::Clamp(V,2.f,80.f);});})]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("세로 (m)"),2,200,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Size.Y;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Size.Y=FMath::Clamp(V,2.f,2000.f);});})]
                // 세부 정보 칸에서 원본 목록을 숨긴 대신 거기서만 고칠 수 있던 값을 여기로
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("방향 (도)"),0,360,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Rotation;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Rotation=FMath::Fmod(V,360.f);});})]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("주변 연결 폭 (m)"),1,60,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Blend;if(P&&P->Recipe.Routes.IsValidIndex(Route))return P->Recipe.Routes[Route].Blend;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Blend=FMath::Clamp(V,1.f,200.f);else if(P.Recipe.Routes.IsValidIndex(Route))P.Recipe.Routes[Route].Blend=FMath::Clamp(V,1.f,80.f);});})]
                // 산만: 0%는 뾰족한 봉우리, 클수록 꼭대기가 넓고 옆면이 가파름
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("꼭대기 넓이 (%)"),0,90,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected)&&P->Recipe.Landforms[Selected].Type==ECozyLandform::Mountain)return P->Recipe.Landforms[Selected].PeakFlat;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected)&&P.Recipe.Landforms[Selected].Type==ECozyLandform::Mountain)P.Recipe.Landforms[Selected].PeakFlat=FMath::Clamp(V,0.f,90.f);});})]
                +SVerticalBox::Slot().AutoHeight().Padding(4,8,4,2)[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("요소 목록 (%d개)"),Items.Num()));})]
                +SVerticalBox::Slot().AutoHeight().Padding(2)[SNew(SBox).HeightOverride(150)[SAssignNew(List,SListView<TSharedPtr<int32>>).ListItemsSource(&Items).SelectionMode(ESelectionMode::Single)
                    .OnGenerateRow_Lambda([this](TSharedPtr<int32> Item,const TSharedRef<STableViewBase>& Owner){const int32 Id=*Item;return SNew(STableRow<TSharedPtr<int32>>,Owner)[SNew(STextBlock).Text_Lambda([this,Id](){return ItemText(Id);})];})
                    .OnSelectionChanged_Lambda([this](TSharedPtr<int32> Item,ESelectInfo::Type How){if(How==ESelectInfo::Direct||!Item)return;SelectNone();if(*Item>=0)Selected=*Item;else Route=-1-*Item;Mode=0;if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);})]]
                +SVerticalBox::Slot().AutoHeight().Padding(4,8,4,2)[SNew(STextBlock).Text(T(TEXT("전체 설계 · 재질 · 설정 이름")))]
                +SVerticalBox::Slot().FillHeight(1)[Details.ToSharedRef()]]]
        +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("되돌리기"),[this](){GEditor->UndoTransaction();SelectNone();Refresh();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("다시 실행"),[this](){GEditor->RedoTransaction();SelectNone();Refresh();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("설정 저장"),[this](){if(auto* P=GetPlan()){FString R;P->SavePreset(R);Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("설정 불러오기"),[this](){if(auto* P=GetPlan()){FString R;P->LoadPreset(R);SelectNone();Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("지형에 적용"),[this](){if(auto* P=GetPlan()){FString R;if(P->Apply(R))SelectNone();Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("적용 전 높이 복구"),[this](){if(auto* P=GetPlan()){FString R;if(P->Restore(R))SelectNone();Message(R);}})]]
        +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(Status);})]];
        LastWorld=GEditor->GetEditorWorldContext().World();LoadExisting();
    }
    virtual void Tick(const FGeometry& G,double Tm,float Dt) override
    {
        SCompoundWidget::Tick(G,Tm,Dt);
        auto* W=GEditor->GetEditorWorldContext().World();if(W!=LastWorld.Get()){LastWorld=W;Plan=nullptr;SelectNone();Details->SetObject(nullptr);Status=TEXT("레벨이 변경됐습니다. 이 레벨의 설계를 열거나 새로 만드세요.");}
    }
};

// 지도는 실제 지형 비율 그대로 창 가운데에 그린다 (창을 늘리고 줄여도 찌그러지지 않게 · 남는 곳은 빈 여백)
static void FitMap(const FVector2D& Size,const FVector2D& Span,FVector2D& Origin,FVector2D& Drawn){const double S=FMath::Min(Size.X/FMath::Max(1.,Span.X),Size.Y/FMath::Max(1.,Span.Y));Drawn=Span*S;Origin=(Size-Drawn)*.5;}
static FVector2D ToMap(FVector2D P,const FVector2D& Size,const FVector2D& Span){FVector2D O,D;FitMap(Size,Span,O,D);return O+FVector2D((P.X/Span.X+.5)*D.X,(.5-P.Y/Span.Y)*D.Y);}
static FVector2D FromMap(FVector2D P,const FVector2D& Size,const FVector2D& Span){FVector2D O,D;FitMap(Size,Span,O,D);P-=O;return FVector2D((P.X/D.X-.5)*Span.X,(.5-P.Y/D.Y)*Span.Y);}
int32 SMap::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const
{
    const auto Owner=Panel.Pin();const auto* P=Owner?Owner->GetPlan():nullptr;const FVector2D Size=G.GetLocalSize();
    const FSlateBrush* Brush=FAppStyle::GetBrush("WhiteBrush");
    FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor(.025,.04,.05));
    if(!P){FSlateDrawElement::MakeText(Out,L+1,G.ToPaintGeometry(),TEXT("새 설계 또는 선택 Landscape 연결로 시작"),FAppStyle::GetFontStyle("NormalFont"),ESlateDrawEffect::None,FLinearColor::White);return L+1;}
    FVector2D MO,MD;FitMap(Size,P->Recipe.MapSize,MO,MD);
    constexpr int N=48;float Heights[N+1][N+1];float Low=FLT_MAX,High=-FLT_MAX;
    for(int Y=0;Y<=N;++Y)for(int X=0;X<=N;++X){float Z=P->Sample(FromMap(MO+FVector2D(double(X)/N*MD.X,double(Y)/N*MD.Y),Size,P->Recipe.MapSize));Heights[Y][X]=Z;Low=FMath::Min(Low,Z);High=FMath::Max(High,Z);}
    auto Line=[&](FVector2D A,FVector2D B,FLinearColor C,float Width=1.f){TArray<FVector2D> V={A,B};FSlateDrawElement::MakeLines(Out,L+2,G.ToPaintGeometry(),V,ESlateDrawEffect::None,C,true,Width);};
    for(int Y=0;Y<N;++Y)for(int X=0;X<N;++X)
    {
        const float Z=Heights[Y][X],T=(Z-Low)/FMath::Max(1.f,High-Low);
        FLinearColor C=FMath::Lerp(FLinearColor(.04,.25,.25),FLinearColor(.88,.65,.25),T);
        const float DX=(Heights[Y][X+1]-Z)/(P->Recipe.MapSize.X/N),DY=(Heights[Y+1][X]-Z)/(P->Recipe.MapSize.Y/N);
        if(Owner->bSlope&&FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX+DY*DY)))>P->SlopeLimit)C=FLinearColor(.75,.12,.07);
        const FVector2D A=MO+FVector2D(double(X)/N*MD.X,double(Y)/N*MD.Y),Cell(MD.X/N+1,MD.Y/N+1);
        FSlateDrawElement::MakeBox(Out,L+1,G.ToPaintGeometry(FVector2f(Cell),FSlateLayoutTransform(FVector2f(A))),Brush,ESlateDrawEffect::None,C);
        if(Owner->bContours)
        {
            // 정사각 셀 변의 등고선 교점을 선형 보간한다.
            const FVector2D Corners[]={A,A+FVector2D(MD.X/N,0),A+FVector2D(MD.X/N,MD.Y/N),A+FVector2D(0,MD.Y/N)};
            const float Values[]={Z,Heights[Y][X+1],Heights[Y+1][X+1],Heights[Y+1][X]};
            float Min=Z,Max=Z;for(float V:Values){Min=FMath::Min(Min,V);Max=FMath::Max(Max,V);}
            const float Gap=FMath::Max(.5f,P->ContourInterval);
            for(int Band=FMath::CeilToInt(Min/Gap);Band<=FMath::FloorToInt(Max/Gap)&&Band<FMath::CeilToInt(Min/Gap)+50;++Band)
            {
                TArray<FVector2D> Hits;float Level=Band*Gap;
                for(int E=0;E<4;++E){int B=(E+1)%4;if((Values[E]<Level)!=(Values[B]<Level))Hits.Add(FMath::Lerp(Corners[E],Corners[B],double((Level-Values[E])/(Values[B]-Values[E]))));}
                for(int I=1;I<Hits.Num();I+=2)Line(Hits[I-1],Hits[I],FLinearColor(1,1,1,.25));
            }
        }
    }
    if(P->bSnapToEstateGrid&&P->CellSize>0)
    {
        // 영지 칸 격자: 칸 선은 옅게, 테두리는 진하게
        const FBox2D Estate=P->EstateBounds();const FLinearColor Grid(.55,1,.55,.22),Edge(.55,1,.55,.9);
        for(int X=0;X<=P->EstateCells.X;++X){const double W=Estate.Min.X+X*P->CellSize;Line(ToMap(FVector2D(W,Estate.Min.Y),Size,P->Recipe.MapSize),ToMap(FVector2D(W,Estate.Max.Y),Size,P->Recipe.MapSize),X%P->EstateCells.X?Grid:Edge,X%P->EstateCells.X?1.f:2.f);}
        for(int Y=0;Y<=P->EstateCells.Y;++Y){const double H=Estate.Min.Y+Y*P->CellSize;Line(ToMap(FVector2D(Estate.Min.X,H),Size,P->Recipe.MapSize),ToMap(FVector2D(Estate.Max.X,H),Size,P->Recipe.MapSize),Y%P->EstateCells.Y?Grid:Edge,Y%P->EstateCells.Y?1.f:2.f);}
        FSlateDrawElement::MakeText(Out,L+3,G.ToPaintGeometry(FVector2f(Size),FSlateLayoutTransform(FVector2f(ToMap(FVector2D(Estate.Min.X,Estate.Max.Y),Size,P->Recipe.MapSize)+FVector2D(2,-16)))),FString::Printf(TEXT("영지 %d×%d칸"),P->EstateCells.X,P->EstateCells.Y),FAppStyle::GetFontStyle("SmallFont"),ESlateDrawEffect::None,Edge);
    }
    for(int I=0;I<P->Recipe.Landforms.Num();++I)
    {
        const auto& F=P->Recipe.Landforms[I];const FLinearColor C=Owner->Selected==I?FLinearColor(.1,.9,1):FLinearColor(.95,.95,.85);
        for(int K=0;K<48;++K){auto Pt=[&](int A){double R=A*2*PI/48;return ToMap(F.Center+FVector2D(FMath::Cos(R)*F.Size.X*.5,FMath::Sin(R)*F.Size.Y*.5).GetRotated(F.Rotation),Size,P->Recipe.MapSize);};Line(Pt(K),Pt(K+1),C,Owner->Selected==I?3:1);}
        const FVector2D Center=ToMap(F.Center,Size,P->Recipe.MapSize);
        // 이름·높이 글자는 요소가 적을 때만 전부, 많으면 선택한 것만 (지도가 글자로 덮이지 않게)
        if(Owner->Selected==I||P->Recipe.Landforms.Num()<=12)FSlateDrawElement::MakeText(Out,L+3,G.ToPaintGeometry(FVector2f(Size),FSlateLayoutTransform(FVector2f(Center+FVector2D(4,4)))),FString::Printf(TEXT("%s\n%.1fm"),*F.Name,F.Height),FAppStyle::GetFontStyle("NormalFont"),ESlateDrawEffect::None,FLinearColor::White);
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
    // 지도 밖 여백을 누르면 아무것도 하지 않음
    if(FMath::Abs(Pos.X)>P->Recipe.MapSize.X*.5+1||FMath::Abs(Pos.Y)>P->Recipe.MapSize.Y*.5+1)return FReply::Handled();
    if(O->Mode==3)
    {
        O->Mutate([&](auto& A){if(!A.Recipe.Routes.IsValidIndex(O->Route)){O->Route=A.Recipe.Routes.AddDefaulted();O->Selected=INDEX_NONE;}auto& R=A.Recipe.Routes[O->Route];if(R.Points.Num()<256){R.Points.Add(FVector(Pos.X,Pos.Y,A.Sample(Pos)));O->Node=R.Points.Num()-1;}});return FReply::Handled();
    }
    if(O->Mode==1||O->Mode==2)
    {
        // 기존 요소 위를 누르면 새로 만들지 않고 그 요소를 고른다 (아래 선택 처리)
        if(O->HitLandform(*P,Pos)==INDEX_NONE)
        {
            const bool bPlateau=O->Mode==1;
            Drag=MakeUnique<FScopedTransaction>(T(bPlateau?TEXT("평지 그리기"):TEXT("산·능선 그리기")));P->Modify();
            FCozyLandform F;F.Type=bPlateau?ECozyLandform::Plateau:ECozyLandform::Mountain;F.Center=Pos;F.Size=FVector2D(2,2);
            // 누른 곳 땅 높이를 0.1m 단위로 (영지 안이면 아래 맞춤에서 층 높이로)
            F.Height=bPlateau?FMath::RoundToFloat(P->Sample(Pos)*10)/10:25;
            F.Name=FString::Printf(TEXT("%s %d"),bPlateau?TEXT("평지"):TEXT("산·능선"),P->Recipe.Landforms.Num()+1);
            O->Selected=P->Recipe.Landforms.Add(F);O->Route=O->Node=INDEX_NONE;
            bCreating=true;bResize=false;CreateStart=FVector2D(FMath::RoundToDouble(Pos.X*10)/10,FMath::RoundToDouble(Pos.Y*10)/10);CreateStartPixel=Mouse;
            P->bHasUnappliedChanges=true;P->bShowPreview=true;O->Refresh();
            return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
        }
        O->Mode=0;
    }
    bResize=false;
    if(P->Recipe.Landforms.IsValidIndex(O->Selected)){const auto& F=P->Recipe.Landforms[O->Selected];bResize=FVector2D::Distance(Mouse,ToMap(F.Center+F.Size*.5,G.GetLocalSize(),P->Recipe.MapSize))<14;}
    if(!bResize)
    {
        O->SelectNone();
        for(int R=0;R<P->Recipe.Routes.Num();++R)for(int I=0;I<P->Recipe.Routes[R].Points.Num();++I)
            if(FVector2D::Distance(Mouse,ToMap(FVector2D(P->Recipe.Routes[R].Points[I]),G.GetLocalSize(),P->Recipe.MapSize))<13){O->Route=R;O->Node=I;}
        if(O->Route==INDEX_NONE)O->Selected=O->HitLandform(*P,Pos);
    }
    if(O->Selected!=INDEX_NONE||O->Route!=INDEX_NONE){Drag=MakeUnique<FScopedTransaction>(T(TEXT("지도에서 지형 조절")));P->Modify();O->Refresh();return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));}
    O->Refresh();return FReply::Handled().SetUserFocus(SharedThis(this));
}
FReply SMap::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    const auto O=Panel.Pin();auto* P=O?O->GetPlan():nullptr;if(!Drag||!P||!HasMouseCapture())return FReply::Unhandled();
    FVector2D Pos=FromMap(G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize(),P->Recipe.MapSize);
    // 끌어서 바꾸는 위치·크기는 0.1m 단위 (31.103222 같은 값이 나오지 않게)
    Pos=FVector2D(FMath::RoundToDouble(Pos.X*10)/10,FMath::RoundToDouble(Pos.Y*10)/10);
    Pos.X=FMath::Clamp(Pos.X,-P->Recipe.MapSize.X*.5,P->Recipe.MapSize.X*.5);Pos.Y=FMath::Clamp(Pos.Y,-P->Recipe.MapSize.Y*.5,P->Recipe.MapSize.Y*.5);
    if(bCreating&&P->Recipe.Landforms.IsValidIndex(O->Selected)){auto& F=P->Recipe.Landforms[O->Selected];F.Center=(CreateStart+Pos)*.5;F.Size=FVector2D(FMath::Max(2.,FMath::Abs(Pos.X-CreateStart.X)),FMath::Max(2.,FMath::Abs(Pos.Y-CreateStart.Y)));O->SnapSelected(*P);}
    else if(P->Recipe.Landforms.IsValidIndex(O->Selected)){auto& F=P->Recipe.Landforms[O->Selected];if(bResize)F.Size=FVector2D(FMath::Clamp(FMath::Abs(Pos.X-F.Center.X)*2,2.,2000.),FMath::Clamp(FMath::Abs(Pos.Y-F.Center.Y)*2,2.,2000.));else F.Center=Pos;O->SnapSelected(*P);}
    else if(P->Recipe.Routes.IsValidIndex(O->Route)&&P->Recipe.Routes[O->Route].Points.IsValidIndex(O->Node)){auto& V=P->Recipe.Routes[O->Route].Points[O->Node];V.X=Pos.X;V.Y=Pos.Y;}
    P->bHasUnappliedChanges=true;P->bShowPreview=true;P->RebuildPreview();Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();
}
FReply SMap::OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
    if(auto O=Panel.Pin())if(auto* P=O->GetPlan())
    {
        if(bCreating&&P->Recipe.Landforms.IsValidIndex(O->Selected))
        {
            // 거의 끌지 않았으면 기본 크기 (평지는 영지 안에 들어가는 12×10m)
            auto& F=P->Recipe.Landforms[O->Selected];
            if(FVector2D::Distance(G.AbsoluteToLocal(E.GetScreenSpacePosition()),CreateStartPixel)<6){F.Center=CreateStart;F.Size=F.Type==ECozyLandform::Plateau?FVector2D(12,10):FVector2D(90,70);}
            O->SnapSelected(*P);O->Mode=0;
            O->Status=FString::Printf(TEXT("'%s'을 만들었습니다 (%.0f×%.0fm · %.1fm). 선택 도구로 돌아갔습니다."),*F.Name,F.Size.X,F.Size.Y,F.Height);
        }
        P->MarkPackageDirty();
    }
    bCreating=false;Drag.Reset();
    if(auto O=Panel.Pin())O->Refresh();
    return FReply::Handled().ReleaseMouseCapture();
}
void SMap::OnMouseCaptureLost(const FCaptureLostEvent&){bCreating=false;Drag.Reset();}
FReply SMap::OnKeyDown(const FGeometry&,const FKeyEvent& E)
{
    const auto O=Panel.Pin();if(!O||!O->GetPlan())return FReply::Unhandled();
    if(E.GetKey()==EKeys::Delete||E.GetKey()==EKeys::BackSpace){O->DeleteSelected();return FReply::Handled();}
    if(E.GetKey()==EKeys::Escape){O->Mode=0;O->SelectNone();O->Message(TEXT("선택 도구 · 선택 해제"));return FReply::Handled();}
    return FReply::Unhandled();
}
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
