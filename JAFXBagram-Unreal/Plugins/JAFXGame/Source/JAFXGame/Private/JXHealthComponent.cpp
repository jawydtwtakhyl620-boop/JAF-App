#include "JXHealthComponent.h"
#include "JXTypes.h"
#include "GameFramework/Actor.h"

UJXHealthComponent::UJXHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJXHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UJXHealthComponent::HandleAnyDamage);
	}
}

void UJXHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bDead || Damage <= 0.f)
	{
		return;
	}
	if (DamageType && DamageType->IsA<UJXExplosiveDamageType>())
	{
		Damage *= ExplosiveMultiplier;
	}
	else if (DamageType && DamageType->IsA<UJXZoneDamageType>())
	{
		if (!bTakesZoneDamage) return;
	}
	else
	{
		Damage *= BulletMultiplier;
	}

	Health = FMath::Max(0.f, Health - Damage);
	OnDamaged.Broadcast(Damage, InstigatedBy);
	if (Health <= 0.f)
	{
		bDead = true;
		OnDeath.Broadcast(DamagedActor, InstigatedBy, DamageCauser);
	}
}

void UJXHealthComponent::Heal(float Amount, float Cap)
{
	if (bDead) return;
	const float Limit = FMath::Min(Cap, MaxHealth);
	if (Health < Limit)
	{
		Health = FMath::Min(Limit, Health + Amount);
	}
}
