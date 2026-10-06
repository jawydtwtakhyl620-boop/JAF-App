#include "JXCombatLibrary.h"
#include "JXSettings.h"
#include "JXTypes.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

bool UJXCombatLibrary::IsHeadBone(FName Bone)
{
	static const FName Head(TEXT("head"));
	static const FName Neck(TEXT("neck_01"));
	static const FName Neck2(TEXT("neck_02"));
	return Bone == Head || Bone == Neck || Bone == Neck2;
}

FJXShotResult UJXCombatLibrary::FireHitscan(UWorld* World, const FJXShotParams& P)
{
	FJXShotResult Result;
	if (!World) return Result;

	const UJXSettings* Settings = UJXSettings::Get();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(JXShot), /*bTraceComplex*/ false);
	Query.bReturnPhysicalMaterial = false;
	if (P.Causer) Query.AddIgnoredActor(P.Causer);
	for (const AActor* A : P.Ignore)
	{
		if (A) Query.AddIgnoredActor(A);
	}

	// Several pellets can hit the same pawn; sum them into one damage event per bone group.
	TMap<AActor*, TPair<float, FHitResult>> Damaged;
	const float HalfCone = FMath::DegreesToRadians(FMath::Max(0.f, P.SpreadDegrees));

	for (int32 i = 0; i < FMath::Max(1, P.Pellets); ++i)
	{
		const FVector Dir = HalfCone > 0.f ? FMath::VRandCone(P.AimDir, HalfCone) : P.AimDir;
		const FVector End = P.AimStart + Dir * P.Range;
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, P.AimStart, End, ECC_Visibility, Query);
		const FVector HitPoint = bHit ? Hit.ImpactPoint : End;

		// Tracer from the gun to what the crosshair hit (only a few per shotgun blast).
		if (i < 3)
		{
			if (UNiagaraSystem* Tracer = Settings->BulletTracer.LoadSynchronous())
			{
				const FRotator Rot = (HitPoint - P.Muzzle).Rotation();
				if (UNiagaraComponent* C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Tracer, P.Muzzle, Rot))
				{
					C->SetVariableVec3(TEXT("BeamEnd"), HitPoint);
				}
			}
#if !UE_BUILD_SHIPPING
			else
			{
				DrawDebugLine(World, P.Muzzle, HitPoint, FColor(255, 220, 120), false, 0.05f, 0, 1.5f);
			}
#endif
		}

		if (!bHit) continue;

		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->IsA<APawn>())
		{
			const bool bHead = IsHeadBone(Hit.BoneName);
			float Dmg = P.Damage * (bHead ? P.HeadMultiplier : 1.f);
			// Bullets lose some power at long range.
			if (Hit.Distance > P.Range * 0.5f) Dmg *= 0.8f;
			TPair<float, FHitResult>& Entry = Damaged.FindOrAdd(HitActor);
			Entry.Key += Dmg;
			if (bHead || Entry.Value.GetActor() == nullptr) Entry.Value = Hit;
			Result.bHeadshot |= bHead;
		}
		else
		{
			SpawnEffect(World, Settings->BulletImpact, Hit.ImpactPoint, Hit.ImpactNormal.Rotation(), 1.f);
		}
	}

	for (TPair<AActor*, TPair<float, FHitResult>>& It : Damaged)
	{
		const FHitResult& Hit = It.Value.Value;
		FPointDamageEvent Event(It.Value.Key, Hit, P.AimDir, UJXBulletDamageType::StaticClass());
		It.Key->TakeDamage(It.Value.Key, Event, P.Instigator, P.Causer);
		++Result.PawnHits;
	}
	return Result;
}

void UJXCombatLibrary::Explode(UObject* WorldContext, FVector Location, float Damage, float Radius, AActor* Causer, AController* Instigator)
{
	const UJXSettings* Settings = UJXSettings::Get();
	TArray<AActor*> Ignore;
	UGameplayStatics::ApplyRadialDamageWithFalloff(WorldContext, Damage, Damage * 0.1f, Location, Radius * 0.3f, Radius, 1.f,
		UJXExplosiveDamageType::StaticClass(), Ignore, Causer, Instigator, ECC_Visibility);
	SpawnEffect(WorldContext, Settings->ExplosionEffect, Location, FRotator::ZeroRotator, Radius / 500.f);
	PlaySoundAt(WorldContext, Settings->ExplosionSound, Location);

#if !UE_BUILD_SHIPPING
	if (Settings->ExplosionEffect.IsNull())
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull))
		{
			DrawDebugSphere(World, Location, Radius, 16, FColor::Orange, false, 0.6f);
		}
	}
#endif
}

void UJXCombatLibrary::PlaySoundAt(UObject* WorldContext, const TSoftObjectPtr<USoundBase>& Sound, FVector Location, float Volume)
{
	if (USoundBase* S = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, S, Location, Volume);
	}
}

void UJXCombatLibrary::SpawnEffect(UObject* WorldContext, const TSoftObjectPtr<UNiagaraSystem>& Effect, FVector Location, FRotator Rotation, float Scale)
{
	if (UNiagaraSystem* Sys = Effect.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, Sys, Location, Rotation, FVector(Scale));
	}
}
