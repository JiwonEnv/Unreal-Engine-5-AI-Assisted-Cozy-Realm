#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Environment/CozyPaintMask.h"
#include "CozySurfaceComponent.generated.h"

/** 고정 지형의 규칙. 메시 언덕 액터에 붙이며 GUID는 저장 후 변경하지 않는다. */
UCLASS(ClassGroup=(CozyRealm), meta=(BlueprintSpawnableComponent))
class COZYREALM_API UCozySurfaceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCozySurfaceComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="표면") FGuid SurfaceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="표면") FGuid PaintDomainId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="표면") bool bAllowBuilding = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="표면") bool bAllowPainting = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="표면") bool bAllowWalking = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="표면", meta=(ClampMin="0", ClampMax="45")) float MaxBuildSlope = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="표면", meta=(ClampMin="0")) float MaxSupportHeightDifference = 12.f;
	/** 액터 로컬 XY 영역. 겹친 층은 서로 다른 도메인을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="페인팅") FVector2D PaintMin = FVector2D(-1000.f, -1000.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="페인팅") FVector2D PaintMax = FVector2D(1000.f, 1000.f);
	bool WorldToPaintUV(const FVector& WorldPosition, FVector2D& OutUV) const;
	UFUNCTION(BlueprintCallable, Category="바닥 꾸미기") void BeginPaintStroke();
	UFUNCTION(BlueprintCallable, Category="바닥 꾸미기") bool PaintAt(FVector WorldPosition, float Radius, int32 Layer, float Strength);
	UFUNCTION(BlueprintCallable, Category="바닥 꾸미기") void EndPaintStroke();
	UFUNCTION(BlueprintCallable, Category="바닥 꾸미기") bool UndoPaint();
	UFUNCTION(BlueprintCallable, Category="바닥 꾸미기") bool RedoPaint();
	const FCozyPaintMask& GetPaintMask() const { return PaintMask; }
	bool CanExportPaint() const { return !bPreserveInvalidSavedMask; }
	bool RestorePaint(const FCozyPaintMask& Saved);
	virtual void OnComponentCreated() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	bool bPaintDirty = false;
	bool bPreserveInvalidSavedMask = false;
	FIntRect DirtyPaintRect = FIntRect(0, 0, 0, 0);
	void RefreshPaintTexture(bool bWholeMask = true);
	UPROPERTY(Transient) FCozyPaintMask PaintMask;
	UPROPERTY(Transient) TObjectPtr<class UTexture2D> PaintTexture;
	FCozyPaintHistory History;
};

struct FCozySurfaceHit
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	FGuid SurfaceId;
	TWeakObjectPtr<UCozySurfaceComponent> Rules;
};

/** 장식·시설·주인공을 지면으로 오인하지 않도록 지형만 조회한다. */
namespace CozySurface
{
	COZYREALM_API bool Trace(const UWorld* World, const FVector& Start, const FVector& End, FCozySurfaceHit& Out, const FGuid* RequiredSurface = nullptr);
	COZYREALM_API bool GroundAt(const UWorld* World, const FVector& Position, FCozySurfaceHit& Out, const FGuid* RequiredSurface = nullptr);
	COZYREALM_API bool CheckFootprint(const UWorld* World, const FVector& Min, const FVector2D& Size, float Spacing, FCozySurfaceHit& Out, FText& Reason, const FGuid* RequiredSurface = nullptr);
}
