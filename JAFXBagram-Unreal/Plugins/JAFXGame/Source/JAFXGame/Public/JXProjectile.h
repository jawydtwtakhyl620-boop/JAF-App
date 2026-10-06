// JAF X BAGRAM - rockets, tank shells and thrown grenades.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JXTypes.h"
#include "JXProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class JAFXGAME_API AJXProjectile : public AActor
{
	GENERATED_BODY()

public:
	AJXProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	EJXExplosiveKind Kind = EJXExplosiveKind::Explosive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Radius = 700.f;

	/** Seconds until it goes off. 0 = explode on impact (rockets, shells). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float FuseTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float SmokeDuration = 25.f;

	/**
	 * Spawns and launches a projectile.
	 * @param Speed      launch speed in cm/s
	 * @param Gravity    0 = flies straight (rocket), 1 = normal arc (grenade)
	 */
	static AJXProjectile* Launch(UWorld* World, TSubclassOf<AJXProjectile> Class, const FVector& Location, const FRotator& Direction,
		float Speed, float Gravity, bool bBounce, EJXExplosiveKind Kind, float Damage, float Radius, float Fuse, APawn* InstigatorPawn);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnStopped(const FHitResult& ImpactResult);

	void Detonate();

private:
	bool bDetonated = false;
	FTimerHandle FuseTimer;
};

/** Invisible sphere left behind by a smoke grenade that blocks AI sight. */
UCLASS()
class JAFXGAME_API AJXSmokeCloud : public AActor
{
	GENERATED_BODY()

public:
	AJXSmokeCloud();

	UPROPERTY(VisibleAnywhere, Category = "Smoke")
	TObjectPtr<UStaticMeshComponent> Blocker;

	void Init(float Duration, bool bShowFallbackMesh);
};
