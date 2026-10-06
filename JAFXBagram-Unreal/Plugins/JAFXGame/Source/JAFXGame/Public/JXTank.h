// JAF X BAGRAM - battle tank. Slow and armored: only rockets and grenades really hurt it.
#pragma once

#include "CoreMinimal.h"
#include "JXVehicle.h"
#include "JXTank.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class JAFXGAME_API AJXTank : public AJXVehicle
{
	GENERATED_BODY()

public:
	AJXTank();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<UStaticMeshComponent> Hull;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<UStaticMeshComponent> TrackLeft;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<UStaticMeshComponent> TrackRight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<USceneComponent> TurretPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<UStaticMeshComponent> Turret;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<USceneComponent> GunPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<UStaticMeshComponent> Cannon;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank") TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float MaxSpeed = 1000.f;      // 36 km/h
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float ReverseSpeed = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float Acceleration = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float TurnRate = 40.f;        // degrees/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float TurretTurnRate = 50.f;  // degrees/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float CannonReload = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float CannonDamage = 220.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float CannonRadius = 650.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float ShellSpeed = 18000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float MachineGunDamage = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float MachineGunRPM = 650.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float RunOverDamage = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") TSoftObjectPtr<USoundBase> CannonSound;

	virtual FString GetStatusText() const override;
	virtual FVector GetVelocity() const override { return GetActorForwardVector() * Speed; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void StartFire() override;
	virtual void StopFire() override;
	virtual void SwitchWeapon() override { bUseMachineGun = !bUseMachineGun; StopFire(); }
	virtual void OnDestroyedByDamage(AController* Killer) override;

private:
	void Drive(float DeltaSeconds);
	void AimTurret(float DeltaSeconds);
	void FireCannon();
	void FireMachineGun();
	void SnapToGround();

	float Speed = 0.f;
	float LastCannonTime = -100.f;
	bool bUseMachineGun = false;
	bool bFiringMG = false;
	FTimerHandle MGTimer;
};
