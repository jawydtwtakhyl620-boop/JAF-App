// JAF X BAGRAM - shared gameplay types.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "JXTypes.generated.h"

UENUM(BlueprintType)
enum class EJXAmmoType : uint8
{
	None,
	A556     UMETA(DisplayName = "5.56mm"),
	A762     UMETA(DisplayName = "7.62mm"),
	A9mm     UMETA(DisplayName = "9mm"),
	A45      UMETA(DisplayName = ".45 ACP"),
	A12Gauge UMETA(DisplayName = "12 Gauge"),
	A300     UMETA(DisplayName = ".300 Magnum"),
	Rocket   UMETA(DisplayName = "Rocket")
};

UENUM(BlueprintType)
enum class EJXWeaponCategory : uint8
{
	AssaultRifle,
	SMG,
	Sniper,
	DMR,
	Shotgun,
	LMG,
	Pistol,
	Launcher
};

UENUM(BlueprintType)
enum class EJXItemType : uint8
{
	Weapon,
	Ammo,
	Bandage,
	FirstAid,
	MedKit,
	EnergyDrink,
	Vest,
	Helmet,
	FragGrenade,
	SmokeGrenade,
	FlashGrenade
};

UENUM(BlueprintType)
enum class EJXDropState : uint8
{
	OnGround,
	InPlane,
	Freefall,
	Parachute
};

UENUM(BlueprintType)
enum class EJXExplosiveKind : uint8
{
	Explosive,
	Smoke,
	Flash
};

/** Gameplay stats of one weapon. All distances are in Unreal units (1 uu = 1 cm). */
USTRUCT(BlueprintType)
struct JAFXGAME_API FJXWeaponDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EJXWeaponCategory Category = EJXWeaponCategory::AssaultRifle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EJXAmmoType Ammo = EJXAmmoType::A556;
	/** Damage per bullet (per pellet for shotguns). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 40.f;
	/** Rounds per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RPM = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MagSize = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ReloadTime = 2.5f;
	/** Cone half angle in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HipSpread = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AdsSpread = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Range = 40000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAutomatic = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Pellets = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HeadMultiplier = 2.f;
	/** Camera kick per shot in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RecoilPitch = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RecoilYaw = 0.2f;
	/** Camera field of view while aiming down sights / scope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AdsFov = 70.f;
	/** Fires a projectile (rocket) instead of a hitscan bullet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bProjectile = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ExplosionDamage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ExplosionRadius = 0.f;
	/** Relative loot weight. 0 = never found on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LootWeight = 10.f;

	bool IsPistol() const { return Category == EJXWeaponCategory::Pistol; }
};

/** Anything that can lie on the ground or sit in an inventory. */
USTRUCT(BlueprintType)
struct JAFXGAME_API FJXItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EJXItemType Type = EJXItemType::Ammo;
	/** Weapon id when Type == Weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WeaponId;
	/** Ammo type when Type == Ammo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EJXAmmoType Ammo = EJXAmmoType::None;
	/** Stack size, or loaded rounds for a weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Amount = 1;
	/** Level 1-3 for vests and helmets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Level = 1;
	/** Remaining durability for vests and helmets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Durability = 0.f;
};

UCLASS()
class JAFXGAME_API UJXBulletDamageType : public UDamageType
{
	GENERATED_BODY()
};

UCLASS()
class JAFXGAME_API UJXExplosiveDamageType : public UDamageType
{
	GENERATED_BODY()
};

/** Damage from standing outside the safe zone. Ignores armor. */
UCLASS()
class JAFXGAME_API UJXZoneDamageType : public UDamageType
{
	GENERATED_BODY()
};
