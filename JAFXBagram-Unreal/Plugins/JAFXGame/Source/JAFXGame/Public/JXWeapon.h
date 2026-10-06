// JAF X BAGRAM - a gun held by a character.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JXTypes.h"
#include "JXWeapon.generated.h"

class AJXCharacter;
class USceneComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USoundBase;
class UNiagaraSystem;

UCLASS()
class JAFXGAME_API AJXWeapon : public AActor
{
	GENERATED_BODY()

public:
	AJXWeapon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> StaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh;

	/** Sets stats and art for the given weapon. */
	void Init(const FJXWeaponDef& InDef, int32 InMag);

	const FJXWeaponDef& GetDef() const { return Def; }
	int32 GetMag() const { return Mag; }

	void StartFire();
	void StopFire();
	/** Returns false when the magazine is full or there is no spare ammo. */
	bool StartReload();
	void CancelReload();
	bool IsReloading() const { return bReloading; }
	/** 0..1 while reloading. */
	float GetReloadProgress() const;
	/** Current cone half-angle in degrees (for the crosshair). */
	float GetCurrentSpread() const;
	FTransform GetHandOffset() const { return HandOffset; }
	FVector GetMuzzleLocation() const;

private:
	void FireOnce();
	void FinishReload();
	AJXCharacter* GetOwnerCharacter() const;

	FJXWeaponDef Def;
	int32 Mag = 0;
	bool bWantsFire = false;
	bool bReloading = false;
	float LastFireTime = -100.f;
	float ReloadStartTime = 0.f;
	FTimerHandle FireTimer;
	FTimerHandle ReloadTimer;
	FTransform HandOffset;
	FName MuzzleSocket;
	TSoftObjectPtr<USoundBase> FireSound;
	TSoftObjectPtr<UNiagaraSystem> MuzzleFlash;
};
