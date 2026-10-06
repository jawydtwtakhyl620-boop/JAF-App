// JAF X BAGRAM - project settings (Project Settings > Game > JAF X BAGRAM).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "JXSettings.generated.h"

class UStaticMesh;
class USkeletalMesh;
class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;
class UAnimInstance;
class AJXCharacter;
class AJXTank;
class AJXMountedGun;
class AJXDropPlane;

/** Art for one weapon. Leave empty to use a simple placeholder box. */
USTRUCT(BlueprintType)
struct FJXWeaponVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> StaticMesh;
	/** Offset from the hand socket so the gun sits correctly in the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform HandOffset;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<USoundBase> FireSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UNiagaraSystem> MuzzleFlash;
	/** Socket on the weapon mesh where bullets leave. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MuzzleSocket = TEXT("Muzzle");
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "JAF X BAGRAM"))
class JAFXGAME_API UJXSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UJXSettings();

	static const UJXSettings* Get() { return GetDefault<UJXSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ---- Match ----
	UPROPERTY(Config, EditAnywhere, Category = "Match", meta = (ClampMin = 0, ClampMax = 99))
	int32 BotCount = 24;

	/** Half size of the playable square in cm (72000 = 1.44 km map). */
	UPROPERTY(Config, EditAnywhere, Category = "Match")
	float MapHalfSize = 72000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Match")
	bool bUseDropPlane = true;

	UPROPERTY(Config, EditAnywhere, Category = "Match")
	float PlaneAltitude = 60000.f;

	/** Show the on-screen touch controls in the editor / desktop for testing. */
	UPROPERTY(Config, EditAnywhere, Category = "Match")
	bool bForceTouchControls = false;

	// ---- Character ----
	/** Used when the character blueprint has no mesh (default: UE5 Third Person template). */
	UPROPERTY(Config, EditAnywhere, Category = "Character")
	TSoftObjectPtr<USkeletalMesh> CharacterMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Character")
	TSoftClassPtr<UAnimInstance> CharacterAnimClass;

	/** Bone or socket the weapon is attached to. */
	UPROPERTY(Config, EditAnywhere, Category = "Character")
	FName HandSocket = TEXT("hand_r");

	// ---- Classes (set these to your Blueprint children) ----
	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<AJXCharacter> BotClass;

	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<AJXTank> TankClass;

	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<AJXMountedGun> MountedGunClass;

	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<AJXDropPlane> PlaneClass;

	// ---- Art ----
	/** Key = weapon id (K47, M4A, SCRL, G36, UZ9, VK45, UM45, K98, AWM, SKSD, S12, S686, PKM, P92, R45, RPG7J). */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TMap<FName, FJXWeaponVisual> WeaponVisuals;

	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TSoftObjectPtr<UNiagaraSystem> BulletTracer;

	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TSoftObjectPtr<UNiagaraSystem> BulletImpact;

	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TSoftObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TSoftObjectPtr<UNiagaraSystem> SmokeEffect;

	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TSoftObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TSoftObjectPtr<USoundBase> DefaultFireSound;

	/** Translucent material for the safe zone wall. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TSoftObjectPtr<UMaterialInterface> ZoneMaterial;

	const FJXWeaponVisual* FindWeaponVisual(FName WeaponId) const { return WeaponVisuals.Find(WeaponId); }
};
