// JAF X BAGRAM - DShK heavy machine gun on a tripod. Anyone can use it.
#pragma once

#include "CoreMinimal.h"
#include "JXVehicle.h"
#include "JXMountedGun.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class JAFXGAME_API AJXMountedGun : public AJXVehicle
{
	GENERATED_BODY()

public:
	AJXMountedGun();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DShK") TObjectPtr<UBoxComponent> Base;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DShK") TObjectPtr<UStaticMeshComponent> Tripod;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DShK") TObjectPtr<USceneComponent> YawPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DShK") TObjectPtr<USceneComponent> PitchPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DShK") TObjectPtr<UStaticMeshComponent> Barrel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DShK") TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DShK") float Damage = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DShK") float RPM = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DShK") float SpreadDegrees = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DShK") float Range = 80000.f;
	/** Heat added per shot (1 = overheated). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DShK") float HeatPerShot = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DShK") float CoolPerSecond = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DShK") TSoftObjectPtr<USoundBase> FireSound;

	UFUNCTION(BlueprintPure, Category = "DShK") float GetHeat() const { return Heat; }

	virtual FString GetStatusText() const override;

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void StartFire() override;
	virtual void StopFire() override;
	virtual FVector GetExitLocation() const override;

private:
	void FireOnce();

	float Heat = 0.f;
	bool bOverheated = false;
	bool bFiring = false;
	FTimerHandle FireTimer;
};
