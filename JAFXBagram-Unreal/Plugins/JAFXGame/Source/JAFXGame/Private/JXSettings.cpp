#include "JXSettings.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"

UJXSettings::UJXSettings()
{
	// Defaults match the UE5 Third Person template so the game runs before any art is assigned.
	CharacterMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny")));
	CharacterAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
}
