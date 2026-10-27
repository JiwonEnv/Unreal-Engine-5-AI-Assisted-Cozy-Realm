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
    UPROPERTY(EditAnywhere, Category="01 지형 설계", meta=(DisplayName="설계")) FCozyTerrainRecipe Recipe;
    UPROPERTY(EditAnywhere, Category="02 표시", meta=(DisplayName="3D 미리보기 표시")) bool bShowPreview=true;
    UPROPERTY(EditAnywhere, Category="02 표시", meta=(DisplayName="급경사 기준 (도)",ClampMin="5",ClampMax="60")) float SlopeLimit=30;
    UPROPERTY(EditAnywhere, Category="03 재사용", meta=(DisplayName="저장·불러올 설정 이름")) FString PresetName=TEXT("지형설정_01");
    UPROPERTY(EditAnywhere, Category="04 재질", meta=(DisplayName="적용할 지면 재질 (선택)")) TObjectPtr<class UMaterialInterface> GroundMaterial;
    UPROPERTY(VisibleAnywhere, Category="05 적용 대상", meta=(DisplayName="선택한 Landscape")) TObjectPtr<class ALandscape> Target;
    UPROPERTY(VisibleAnywhere, Category="05 적용 대상", meta=(DisplayName="최근 적용 전 높이 사본")) FString LastBackup;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UProceduralMeshComponent> Preview;
    UPROPERTY() FGuid EditLayerGuid;
    UPROPERTY() TArray<uint16> Baseline;
    UPROPERTY() FIntPoint GridMin=FIntPoint::ZeroValue;
    UPROPERTY() FIntPoint GridMax=FIntPoint::ZeroValue;
    UPROPERTY() FTransform CapturedTransform;
    UPROPERTY() FVector BoundCenter=FVector::ZeroVector;
    UPROPERTY() FVector2D BoundSize=FVector2D::ZeroVector;
    UPROPERTY() bool bHasUnappliedChanges=true;
    float Sample(FVector2D LocalMeters) const;
    void RebuildPreview();
    bool BindLandscape(ALandscape* Landscape, FString& Result);
    bool Apply(FString& Result);
    bool Restore(FString& Result);
    bool SavePreset(FString& Result) const;
    bool LoadPreset(FString& Result);
    void AddDemo();
private:
    bool ReadTarget(TArray<uint16>& Data, FString& Result) const;
    bool WriteLayer(const TArray<uint16>& Data, FString& Result);
    FString PresetPath() const;
};
