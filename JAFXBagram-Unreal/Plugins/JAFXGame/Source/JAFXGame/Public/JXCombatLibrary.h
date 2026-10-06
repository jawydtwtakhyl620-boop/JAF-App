// JAF X BAGRAM - shared shooting and explosion helpers.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JXCombatLibrary.generated.h"

class AActor;
class AController;
class UNiagaraSystem;
class USoundBase;

struct FJXShotParams
{
	AActor* Causer = nullptr;
	AController* Instigator = nullptr;
	/** Where the aim ray starts (usually the camera). */
	FVector AimStart = FVector::ZeroVector;
	FVector AimDir = FVector::ForwardVector;
	/** Where tracers are drawn from. */
	FVector Muzzle = FVector::ZeroVector;
	float Range = 40000.f;
	float Damage = 40.f;
	float HeadMultiplier = 2.f;
	float SpreadDegrees = 1.f;
	int32 Pellets = 1;
	TArray<const AActor*> Ignore;
};

struct FJXShotResult
{
	int32 PawnHits = 0;
	bool bHeadshot = false;
};

UCLASS()
class JAFXGAME_API UJXCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Instant-hit bullets with spread, headshots, tracers and impact effects. */
	static FJXShotResult FireHitscan(UWorld* World, const FJXShotParams& Params);

	/** Radial explosion damage with effects. */
	UFUNCTION(BlueprintCallable, Category = "JAFX|Combat", meta = (WorldContext = "WorldContext"))
	static void Explode(UObject* WorldContext, FVector Location, float Damage, float Radius, AActor* Causer, AController* Instigator);

	static void PlaySoundAt(UObject* WorldContext, const TSoftObjectPtr<USoundBase>& Sound, FVector Location, float Volume = 1.f);
	static void SpawnEffect(UObject* WorldContext, const TSoftObjectPtr<UNiagaraSystem>& Effect, FVector Location, FRotator Rotation, float Scale = 1.f);

	/** True for bones that count as a headshot. */
	static bool IsHeadBone(FName Bone);
};
