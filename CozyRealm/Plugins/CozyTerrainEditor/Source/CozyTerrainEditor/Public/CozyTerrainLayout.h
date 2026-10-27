#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CozyTerrainLayout.generated.h"

UENUM(meta=(ScriptName="CozyLandformKind"))
enum class ECozyLandform : uint8
{
    Plateau UMETA(DisplayName="평지"),
    Mountain UMETA(DisplayName="산·능선")
};

USTRUCT()
struct FCozyLandform
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, meta=(DisplayName="이름")) FString Name=TEXT("새 평지");
    UPROPERTY(EditAnywhere, meta=(DisplayName="종류")) ECozyLandform Type=ECozyLandform::Plateau;
    UPROPERTY(EditAnywhere, meta=(DisplayName="중심 위치 (m)")) FVector2D Center=FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, meta=(DisplayName="가로·세로 (m)",ClampMin="2",ClampMax="2000")) FVector2D Size=FVector2D(40,30);
    UPROPERTY(EditAnywhere, meta=(DisplayName="높이 (m)",ClampMin="-200",ClampMax="200")) float Height=0;
    UPROPERTY(EditAnywhere, meta=(DisplayName="주변 연결 폭 (m)",ClampMin="1",ClampMax="200")) float Blend=8;
    UPROPERTY(EditAnywhere, meta=(DisplayName="방향 (도)")) float Rotation=0;
    /** 산만: 꼭대기가 평평하게 넓은 정도 · 0%는 뾰족한 봉우리, 클수록 윗부분이 넓고 옆면이 가파름 */
    UPROPERTY(EditAnywhere, meta=(DisplayName="꼭대기 넓이 (%)",ClampMin="0",ClampMax="90")) float PeakFlat=0;
};

USTRUCT()
struct FCozyTerrainRoute
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, meta=(DisplayName="이름")) FString Name=TEXT("새 연결 길");
    UPROPERTY(EditAnywhere, meta=(DisplayName="경유점 X·Y·높이 (m)")) TArray<FVector> Points;
    UPROPERTY(EditAnywhere, meta=(DisplayName="길 전체 폭 (m)",ClampMin="2",ClampMax="80")) float Width=8;
    UPROPERTY(EditAnywhere, meta=(DisplayName="주변 연결 폭 (m)",ClampMin="1",ClampMax="80")) float Blend=6;
};

USTRUCT()
struct FCozyTerrainRecipe
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, meta=(DisplayName="설정 버전")) int32 Version=1;
    UPROPERTY(EditAnywhere, meta=(DisplayName="편집 영역 가로·세로 (m)",ClampMin="63",ClampMax="1008")) FVector2D MapSize=FVector2D(252,252);
    UPROPERTY(EditAnywhere, meta=(DisplayName="새 지형의 한 변 구획 수",ClampMin="1",ClampMax="8")) int32 Components=4;
    UPROPERTY(EditAnywhere, meta=(DisplayName="기본 높이 (m)",ClampMin="-200",ClampMax="200")) float BaseHeight=0;
    UPROPERTY(EditAnywhere, meta=(DisplayName="평지·산",TitleProperty="Name")) TArray<FCozyLandform> Landforms;
    UPROPERTY(EditAnywhere, meta=(DisplayName="연결 길",TitleProperty="Name")) TArray<FCozyTerrainRoute> Routes;
    bool Validate(FString& Reason) const;
    float Evaluate(FVector2D Position, float Baseline) const;
};

/** 제작 데이터와 가벼운 미리보기만 보유. 패키지에는 실제 Landscape만 남는다. */
UCLASS(NotBlueprintable, meta=(DisplayName="지형 편집 설계"))
class COZYTERRAINEDITOR_API ACozyTerrainLayout : public AActor
{
    GENERATED_BODY()
public:
    ACozyTerrainLayout();
    virtual bool IsEditorOnly() const override { return true; }
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
    virtual void PostEditUndo() override;
    virtual void PostLoad() override;
    UPROPERTY(EditAnywhere, Category="01 지형 설계", meta=(DisplayName="설계")) FCozyTerrainRecipe Recipe;
    UPROPERTY(EditAnywhere, Category="02 표시", meta=(DisplayName="3D 미리보기 표시")) bool bShowPreview=true;
    UPROPERTY(EditAnywhere, Category="02 표시", meta=(DisplayName="급경사 기준 (도)",ClampMin="5",ClampMax="60")) float SlopeLimit=30;
    /** 지도에 같은 높이를 잇는 선을 몇 m마다 그을지 */
    UPROPERTY(EditAnywhere, Category="02 표시", meta=(DisplayName="등고선 간격 (m)",ClampMin="0.5",ClampMax="50")) float ContourInterval=5;
    /** 켜 두면 적용할 때 평지·산·길 원형을 지형에 굳히고 목록을 비운다 (복구하면 원형이 돌아옴) */
    UPROPERTY(EditAnywhere, Category="02 표시", meta=(DisplayName="적용하면 원형 정리")) bool bBakeOnApply=true;
    UPROPERTY(EditAnywhere, Category="03 재사용", meta=(DisplayName="저장·불러올 설정 이름")) FString PresetName=TEXT("지형설정_01");
    UPROPERTY(EditAnywhere, Category="04 재질", meta=(DisplayName="적용할 지면 재질 (선택)")) TObjectPtr<class UMaterialInterface> GroundMaterial;
    UPROPERTY(VisibleAnywhere, Category="05 적용 대상", meta=(DisplayName="선택한 Landscape")) TObjectPtr<class ALandscape> Target;
    UPROPERTY(VisibleAnywhere, Category="05 적용 대상", meta=(DisplayName="최근 적용 전 높이 사본")) FString LastBackup;
    /** 영지(시설 놓는 곳) 안의 평지만 칸·층 높이에 맞춘다. 영지 바깥 산·숲은 자유롭게. */
    UPROPERTY(EditAnywhere, Category="06 영지 칸", meta=(DisplayName="영지 칸 맞춤 사용")) bool bSnapToEstateGrid=true;
    UPROPERTY(EditAnywhere, Category="06 영지 칸", meta=(DisplayName="영지 중심 (m)")) FVector2D EstateCenter=FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, Category="06 영지 칸", meta=(DisplayName="영지 칸 수 (가로·세로)",ClampMin="1",ClampMax="200")) FIntPoint EstateCells=FIntPoint(12,12);
    UPROPERTY(EditAnywhere, Category="06 영지 칸", meta=(DisplayName="칸 한 변 (m)",ClampMin="0.5",ClampMax="20")) float CellSize=2;
    /** 영지 평지가 맞춰질 높이들 (임시값 · 최종값은 아트 방향 확정 뒤) */
    UPROPERTY(EditAnywhere, Category="06 영지 칸", meta=(DisplayName="층 높이 목록 (m)")) TArray<float> TierHeights={0.f,2.4f,4.8f};
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UProceduralMeshComponent> Preview;
    UPROPERTY() FGuid EditLayerGuid;
    UPROPERTY() TArray<uint16> Baseline;
    UPROPERTY() FIntPoint GridMin=FIntPoint::ZeroValue;
    UPROPERTY() FIntPoint GridMax=FIntPoint::ZeroValue;
    UPROPERTY() FTransform CapturedTransform;
    UPROPERTY() FVector BoundCenter=FVector::ZeroVector;
    UPROPERTY() FVector2D BoundSize=FVector2D::ZeroVector;
    UPROPERTY() bool bHasUnappliedChanges=true;
    /** 마지막으로 실제 지형에 적용한 설계 · 그 직전 적용분 (3D 미리보기를 바뀌는 곳만 그리고, 복구 뒤에도 기준을 맞추려고) */
    UPROPERTY() FCozyTerrainRecipe AppliedRecipe;
    UPROPERTY() FCozyTerrainRecipe PrevAppliedRecipe;
    UPROPERTY() bool bHasApplied=false;
    UPROPERTY() bool bHasPrevApplied=false;
    /** 지금까지 굳힌 전용 레이어 높이 (비어 있으면 없음) · 다음 설계는 이 위에서 시작 */
    UPROPERTY() TArray<uint16> Baked;
    /** 마지막 굳히기 직전 상태 (복구 때 원형과 레이어 기준을 되돌림) */
    UPROPERTY() TArray<uint16> BakeUndoLayer;
    UPROPERTY() FCozyTerrainRecipe BakeUndoRecipe;
    UPROPERTY() bool bCanUndoBake=false;
    float Sample(FVector2D LocalMeters) const;
    /** 지금 실제 지형의 높이 추정 (적용한 설계가 있으면 그 설계 · 없으면 연결 당시 원래 높이) */
    float SampleApplied(FVector2D LocalMeters) const;
    void RebuildPreview();
    bool BindLandscape(ALandscape* Landscape, FString& Result);
    bool Apply(FString& Result);
    bool Restore(FString& Result);
    bool SavePreset(FString& Result) const;
    bool LoadPreset(FString& Result);
    void AddDemo();
    /** 영지 범위 (설계 기준 m) */
    FBox2D EstateBounds() const;
    /** 영지 안 평지면 크기·위치를 칸에, 높이를 가장 가까운 층에, 방향을 0도로 맞춘다 · 맞췄으면 true */
    bool SnapLandform(FCozyLandform& Landform) const;
private:
    /** 연결 당시 원래 지형 높이 (m · 설계 기준) · 연결 전이면 0 */
    float BaselineHeight(FVector2D LocalMeters) const;
    float SampleWith(const FCozyTerrainRecipe& R, FVector2D LocalMeters) const;
    bool ReadTarget(TArray<uint16>& Data, FString& Result) const;
    bool WriteLayer(const TArray<uint16>& Data, FString& Result);
    FString PresetPath() const;
};
