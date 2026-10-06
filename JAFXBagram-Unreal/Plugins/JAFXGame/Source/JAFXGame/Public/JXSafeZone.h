// JAF X BAGRAM - the shrinking safe zone ("blue zone").
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JXSafeZone.generated.h"

class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FJXZonePhase
{
	GENERATED_BODY()

	/** Seconds before this phase starts shrinking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WaitTime = 60.f;
	/** Seconds the shrink takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShrinkTime = 60.f;
	/** Radius after this phase, as a fraction of the starting radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RadiusFraction = 0.5f;
	/** Damage per second to anyone outside while this phase is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamagePerSecond = 1.f;
};

UCLASS()
class JAFXGAME_API AJXSafeZone : public AActor
{
	GENERATED_BODY()

public:
	AJXSafeZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<UStaticMeshComponent> Wall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	TArray<FJXZonePhase> Phases;

	/** Begins the first phase around Center. */
	void StartZone(const FVector2D& Center, float Radius);

	UFUNCTION(BlueprintPure, Category = "Zone") FVector2D GetCenter() const { return CurCenter; }
	UFUNCTION(BlueprintPure, Category = "Zone") float GetRadius() const { return CurRadius; }
	UFUNCTION(BlueprintPure, Category = "Zone") FVector2D GetTargetCenter() const { return TargetCenter; }
	UFUNCTION(BlueprintPure, Category = "Zone") float GetTargetRadius() const { return TargetRadius; }
	UFUNCTION(BlueprintPure, Category = "Zone") bool IsInside(const FVector& Location) const;
	/** "Zone shrinks in 42s" / "Zone shrinking!" for the HUD. */
	UFUNCTION(BlueprintPure, Category = "Zone") FString GetStatusText() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BeginPhase(int32 Index);
	void DamageOutside();
	void UpdateWall();

	int32 PhaseIndex = -1;
	bool bShrinking = false;
	bool bStarted = false;
	float PhaseTimer = 0.f;
	float StartRadius = 0.f;
	FVector2D CurCenter = FVector2D::ZeroVector;
	float CurRadius = 0.f;
	FVector2D FromCenter = FVector2D::ZeroVector;
	float FromRadius = 0.f;
	FVector2D TargetCenter = FVector2D::ZeroVector;
	float TargetRadius = 0.f;
	float DamagePerSecond = 0.4f;
	FTimerHandle DamageTimer;
};
