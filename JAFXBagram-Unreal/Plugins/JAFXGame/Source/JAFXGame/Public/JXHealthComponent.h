// JAF X BAGRAM - health for characters and vehicles.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JXHealthComponent.generated.h"

class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FJXOnDeath, AActor*, Victim, AController*, Killer, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJXOnDamaged, float, Damage, AController*, InstigatedBy);

UCLASS(ClassGroup = (JAFX), meta = (BlueprintSpawnableComponent))
class JAFXGAME_API UJXHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJXHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 100.f;

	/** Multiplier for bullets (tanks use a tiny value so only explosives hurt them). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float BulletMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float ExplosiveMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	bool bTakesZoneDamage = true;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FJXOnDeath OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FJXOnDamaged OnDamaged;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	/** Heals but never above Cap (e.g. bandages stop at 75). */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount, float Cap = 100.f);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

private:
	float Health = 100.f;
	bool bDead = false;
};
