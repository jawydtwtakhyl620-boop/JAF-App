// JAF X BAGRAM - weapon catalogue and loot tables.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JXTypes.h"
#include "JXWeaponLibrary.generated.h"

UCLASS()
class JAFXGAME_API UJXWeaponLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Every weapon in the game. Stats live here so balance changes are one place. */
	static const TArray<FJXWeaponDef>& GetAll();

	/** Returns nullptr when the id is unknown. */
	static const FJXWeaponDef* Find(FName Id);

	UFUNCTION(BlueprintPure, Category = "JAFX|Weapons")
	static bool FindWeapon(FName Id, FJXWeaponDef& OutDef);

	UFUNCTION(BlueprintPure, Category = "JAFX|Weapons")
	static FString AmmoName(EJXAmmoType Ammo);

	/** Rounds found in one ammo pickup. */
	UFUNCTION(BlueprintPure, Category = "JAFX|Weapons")
	static int32 AmmoPackSize(EJXAmmoType Ammo);

	/** Random weapon id using the loot weights. */
	static FName RandomWeaponId(FRandomStream& Rng);

	/** Random ground loot (weapons, ammo, healing, armor, grenades). */
	static FJXItem RandomLoot(FRandomStream& Rng);

	/** Short text shown in pickup prompts and the HUD. */
	UFUNCTION(BlueprintPure, Category = "JAFX|Weapons")
	static FString ItemLabel(const FJXItem& Item);

	static float VestReduction(int32 Level);
	static float HelmetReduction(int32 Level);
	static float VestDurability(int32 Level);
	static float HelmetDurability(int32 Level);
};
