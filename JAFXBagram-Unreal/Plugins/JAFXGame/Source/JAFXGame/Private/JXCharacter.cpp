#include "JXCharacter.h"
#include "JXCombatLibrary.h"
#include "JXDropPlane.h"
#include "JXGameMode.h"
#include "JXHealthComponent.h"
#include "JXInteractable.h"
#include "JXPickup.h"
#include "JXPlayerController.h"
#include "JXProjectile.h"
#include "JXSettings.h"
#include "JXVehicle.h"
#include "JXWeapon.h"
#include "JXWeaponLibrary.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr int32 SlotCount = 3; // 0 and 1 = primary weapons, 2 = pistol
	constexpr float BandageTime = 4.f;
	constexpr float FirstAidTime = 6.f;
	constexpr float MedKitTime = 8.f;
	constexpr float EnergyDrinkTime = 4.f;
}

AJXCharacter::AJXCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	// Bullets pass the capsule and hit the skeletal mesh, so we know which bone (head) was hit.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 600.f, 0.f);
	Move->JumpZVelocity = 450.f;
	Move->AirControl = 0.35f;
	Move->MaxWalkSpeed = RunSpeed;
	Move->MaxWalkSpeedCrouched = 220.f;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.f;
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 65.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 90.f;

	Health = CreateDefaultSubobject<UJXHealthComponent>(TEXT("Health"));

	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr; // set by the game mode for bots
}

void AJXCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyDefaultMesh();
	Weapons.SetNum(SlotCount);
	DefaultArmLength = CameraBoom->TargetArmLength;
	DefaultSocketOffset = CameraBoom->SocketOffset;
	DefaultFov = FollowCamera->FieldOfView;
	Health->OnDeath.AddDynamic(this, &AJXCharacter::HandleDeath);
	Health->OnDamaged.AddDynamic(this, &AJXCharacter::HandleDamaged);
	UpdateMovementSpeed();
}

void AJXCharacter::ApplyDefaultMesh()
{
	USkeletalMeshComponent* M = GetMesh();
	if (M->GetSkeletalMeshAsset()) return; // Blueprint already set a mesh

	const UJXSettings* S = UJXSettings::Get();
	if (USkeletalMesh* Mesh = S->CharacterMesh.LoadSynchronous())
	{
		M->SetSkeletalMesh(Mesh);
	}
	UClass* Anim = S->CharacterAnimClass.LoadSynchronous();
	if (!Anim)
	{
		// Older Third Person templates (UE 5.0 - 5.3) keep the animation blueprint here.
		Anim = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Anim)
	{
		M->SetAnimInstanceClass(Anim);
	}
}

// ------------------------------------------------------------------ queries

bool AJXCharacter::IsAlive() const
{
	return Health && !Health->IsDead();
}

AJXWeapon* AJXCharacter::GetCurrentWeapon() const
{
	return GetWeaponInSlot(CurrentSlot);
}

AJXWeapon* AJXCharacter::GetWeaponInSlot(int32 Slot) const
{
	return Weapons.IsValidIndex(Slot) ? Weapons[Slot].Get() : nullptr;
}

int32 AJXCharacter::GetAmmo(EJXAmmoType Type) const
{
	const int32* N = Ammo.Find(Type);
	return N ? *N : 0;
}

int32 AJXCharacter::GetThrowableCount(EJXItemType Type) const
{
	switch (Type)
	{
	case EJXItemType::FragGrenade: return FragGrenades;
	case EJXItemType::SmokeGrenade: return SmokeGrenades;
	case EJXItemType::FlashGrenade: return FlashGrenades;
	default: return 0;
	}
}

float AJXCharacter::GetHealProgress() const
{
	if (!bHealing || HealDuration <= 0.f) return 0.f;
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - HealStartTime) / HealDuration, 0.f, 1.f);
}

FString AJXCharacter::GetHealLabel() const
{
	switch (HealingItem)
	{
	case EJXItemType::Bandage: return TEXT("Using Bandage");
	case EJXItemType::FirstAid: return TEXT("Using First Aid");
	case EJXItemType::MedKit: return TEXT("Using Med Kit");
	case EJXItemType::EnergyDrink: return TEXT("Drinking Energy Drink");
	default: return TEXT("Healing");
	}
}

float AJXCharacter::GetSpreadMultiplier() const
{
	float M = BotSpreadMultiplier;
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Move->IsFalling()) M *= 3.f;
	else if (GetVelocity().Size2D() > 50.f) M *= 1.6f;
	if (bIsCrouched) M *= 0.75f;
	return M;
}

FString AJXCharacter::GetInteractPrompt() const
{
	if (const IJXInteractable* I = Cast<IJXInteractable>(FocusedInteractable.Get()))
	{
		return I->GetInteractText(this);
	}
	return FString();
}

int32 AJXCharacter::TakeAmmo(EJXAmmoType Type, int32 Wanted)
{
	int32& Have = Ammo.FindOrAdd(Type);
	const int32 Taken = FMath::Clamp(Wanted, 0, Have);
	Have -= Taken;
	return Taken;
}

void AJXCharacter::AddAmmo(EJXAmmoType Type, int32 Amount)
{
	if (Type == EJXAmmoType::None) return;
	Ammo.FindOrAdd(Type) += Amount;
}

bool AJXCharacter::IsLocalHuman() const
{
	return IsPlayerControlled() && IsLocallyControlled();
}

AJXPlayerController* AJXCharacter::GetJXController() const
{
	return Cast<AJXPlayerController>(GetController());
}

// ------------------------------------------------------------------ input

void AJXCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	AJXPlayerController* PC = GetJXController();
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!PC || !EIC) return;

	const FJXInputActions& A = PC->GetInputActions();
	EIC->BindAction(A.Move, ETriggerEvent::Triggered, this, &AJXCharacter::InputMove);
	EIC->BindAction(A.Look, ETriggerEvent::Triggered, this, &AJXCharacter::InputLook);
	EIC->BindAction(A.Jump, ETriggerEvent::Started, this, &AJXCharacter::JumpOrDrop);
	EIC->BindAction(A.Jump, ETriggerEvent::Completed, this, &AJXCharacter::InputJumpReleased);
	EIC->BindAction(A.Sprint, ETriggerEvent::Started, this, &AJXCharacter::InputSprintStart);
	EIC->BindAction(A.Sprint, ETriggerEvent::Completed, this, &AJXCharacter::InputSprintStop);
	EIC->BindAction(A.Crouch, ETriggerEvent::Started, this, &AJXCharacter::ToggleCrouch);
	EIC->BindAction(A.Fire, ETriggerEvent::Started, this, &AJXCharacter::StartFire);
	EIC->BindAction(A.Fire, ETriggerEvent::Completed, this, &AJXCharacter::StopFire);
	EIC->BindAction(A.Fire, ETriggerEvent::Canceled, this, &AJXCharacter::StopFire);
	EIC->BindAction(A.Aim, ETriggerEvent::Started, this, &AJXCharacter::InputAimStart);
	EIC->BindAction(A.Aim, ETriggerEvent::Completed, this, &AJXCharacter::InputAimStop);
	EIC->BindAction(A.Reload, ETriggerEvent::Started, this, &AJXCharacter::Reload);
	EIC->BindAction(A.Interact, ETriggerEvent::Started, this, &AJXCharacter::Interact);
	EIC->BindAction(A.Slot1, ETriggerEvent::Started, this, &AJXCharacter::InputSlot1);
	EIC->BindAction(A.Slot2, ETriggerEvent::Started, this, &AJXCharacter::InputSlot2);
	EIC->BindAction(A.Slot3, ETriggerEvent::Started, this, &AJXCharacter::InputSlot3);
	EIC->BindAction(A.NextWeapon, ETriggerEvent::Started, this, &AJXCharacter::NextWeapon);
	EIC->BindAction(A.Heal, ETriggerEvent::Started, this, &AJXCharacter::UseBestHeal);
	EIC->BindAction(A.Throw, ETriggerEvent::Started, this, &AJXCharacter::ThrowGrenade);
	EIC->BindAction(A.CycleThrowable, ETriggerEvent::Started, this, &AJXCharacter::CycleThrowable);
}

void AJXCharacter::InputMove(const FInputActionValue& Value)
{
	if (!Controller || !IsAlive() || DropState == EJXDropState::InPlane) return;
	const FVector2D V = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
}

void AJXCharacter::InputLook(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	float Scale = bAiming ? 0.6f : 1.f;
	if (const AJXPlayerController* PC = GetJXController()) Scale *= PC->LookSensitivity;
	AddControllerYawInput(V.X * Scale);
	AddControllerPitchInput(V.Y * Scale);
}

// ------------------------------------------------------------------ actions

void AJXCharacter::StartFire()
{
	if (!IsAlive() || DropState != EJXDropState::OnGround || Vehicle.IsValid()) return;
	CancelHealing();
	if (bSprinting) SetSprinting(false);
	bFiring = true;
	LastShotTime = GetWorld()->GetTimeSeconds();
	UpdateCombatStance();
	if (AJXWeapon* W = GetCurrentWeapon())
	{
		W->StartFire();
	}
}

void AJXCharacter::StopFire()
{
	bFiring = false;
	if (AJXWeapon* W = GetCurrentWeapon())
	{
		W->StopFire();
	}
}

void AJXCharacter::SetAiming(bool bNewAiming)
{
	bAiming = bNewAiming && IsAlive() && DropState == EJXDropState::OnGround && GetCurrentWeapon() != nullptr;
	if (bAiming && bSprinting) bSprinting = false;
	UpdateMovementSpeed();
	UpdateCombatStance();
}

void AJXCharacter::Reload()
{
	if (AJXWeapon* W = GetCurrentWeapon())
	{
		if (W->StartReload()) CancelHealing();
	}
}

void AJXCharacter::Interact()
{
	if (!IsAlive()) return;
	if (IJXInteractable* I = Cast<IJXInteractable>(FocusedInteractable.Get()))
	{
		I->Interact(this);
	}
}

void AJXCharacter::EquipSlot(int32 Slot)
{
	if (!GetWeaponInSlot(Slot) || Slot == CurrentSlot) return;
	if (AJXWeapon* Old = GetCurrentWeapon())
	{
		Old->StopFire();
		Old->CancelReload();
	}
	CurrentSlot = Slot;
	RefreshWeaponVisibility();
	if (bAiming) SetAiming(true);
}

void AJXCharacter::NextWeapon()
{
	for (int32 i = 1; i <= SlotCount; ++i)
	{
		const int32 Slot = (FMath::Max(CurrentSlot, 0) + i) % SlotCount;
		if (GetWeaponInSlot(Slot))
		{
			EquipSlot(Slot);
			return;
		}
	}
}

void AJXCharacter::UseBestHeal()
{
	if (!IsAlive() || bHealing || DropState != EJXDropState::OnGround) return;
	const float H = Health->GetHealth();
	EJXItemType Pick = EJXItemType::Weapon; // "none"
	if (H < 40.f && MedKits > 0) Pick = EJXItemType::MedKit;
	else if (H < 60.f && FirstAids > 0) Pick = EJXItemType::FirstAid;
	else if (H < 75.f && Bandages > 0) Pick = EJXItemType::Bandage;
	else if (H < 75.f && FirstAids > 0) Pick = EJXItemType::FirstAid;
	else if (H < 100.f && EnergyDrinks > 0) Pick = EJXItemType::EnergyDrink;
	else if (H < 100.f && MedKits > 0) Pick = EJXItemType::MedKit;
	if (Pick == EJXItemType::Weapon) return;

	StopFire();
	if (AJXWeapon* W = GetCurrentWeapon()) W->CancelReload();
	bHealing = true;
	HealingItem = Pick;
	HealStartTime = GetWorld()->GetTimeSeconds();
	HealDuration = Pick == EJXItemType::MedKit ? MedKitTime : Pick == EJXItemType::FirstAid ? FirstAidTime
		: Pick == EJXItemType::EnergyDrink ? EnergyDrinkTime : BandageTime;
	UpdateMovementSpeed();
}

void AJXCharacter::CancelHealing()
{
	if (!bHealing) return;
	bHealing = false;
	UpdateMovementSpeed();
}

void AJXCharacter::UpdateHealing()
{
	if (!bHealing || GetHealProgress() < 1.f) return;
	bHealing = false;
	switch (HealingItem)
	{
	case EJXItemType::Bandage: if (Bandages > 0) { --Bandages; Health->Heal(10.f, 75.f); } break;
	case EJXItemType::FirstAid: if (FirstAids > 0) { --FirstAids; Health->Heal(100.f, 75.f); } break;
	case EJXItemType::MedKit: if (MedKits > 0) { --MedKits; Health->Heal(100.f, 100.f); } break;
	case EJXItemType::EnergyDrink: if (EnergyDrinks > 0) { --EnergyDrinks; Health->Heal(20.f, 100.f); } break;
	default: break;
	}
	UpdateMovementSpeed();
}

void AJXCharacter::ThrowGrenade()
{
	if (!IsAlive() || DropState != EJXDropState::OnGround || Vehicle.IsValid()) return;
	if (GetThrowableCount(SelectedThrowable) <= 0) CycleThrowable();
	if (GetThrowableCount(SelectedThrowable) <= 0) return;

	EJXExplosiveKind Kind = EJXExplosiveKind::Explosive;
	float Damage = 150.f, Radius = 700.f, Fuse = 4.f;
	switch (SelectedThrowable)
	{
	case EJXItemType::SmokeGrenade: --SmokeGrenades; Kind = EJXExplosiveKind::Smoke; Damage = 0.f; Fuse = 2.f; break;
	case EJXItemType::FlashGrenade: --FlashGrenades; Kind = EJXExplosiveKind::Flash; Damage = 0.f; Radius = 2000.f; Fuse = 2.5f; break;
	default: --FragGrenades; break;
	}
	CancelHealing();
	FRotator Aim = GetControlRotation();
	Aim.Pitch += 8.f;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 60.f) + Aim.Vector() * 60.f;
	AJXProjectile::Launch(GetWorld(), AJXProjectile::StaticClass(), Start, Aim, 1800.f, 1.f, true, Kind, Damage, Radius, Fuse, this);
}

void AJXCharacter::CycleThrowable()
{
	static const EJXItemType Order[] = { EJXItemType::FragGrenade, EJXItemType::SmokeGrenade, EJXItemType::FlashGrenade };
	int32 Index = 0;
	for (int32 i = 0; i < 3; ++i) if (Order[i] == SelectedThrowable) Index = i;
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		const EJXItemType Next = Order[(Index + Step) % 3];
		if (GetThrowableCount(Next) > 0)
		{
			SelectedThrowable = Next;
			return;
		}
	}
}

void AJXCharacter::ToggleCrouch()
{
	if (DropState != EJXDropState::OnGround) return;
	if (bIsCrouched) UnCrouch(); else Crouch();
}

void AJXCharacter::SetSprinting(bool bNewSprinting)
{
	bSprinting = bNewSprinting && !bAiming && !bHealing;
	if (bSprinting && bIsCrouched) UnCrouch();
	UpdateMovementSpeed();
}

void AJXCharacter::JumpOrDrop()
{
	if (!IsAlive()) return;
	switch (DropState)
	{
	case EJXDropState::InPlane: EjectFromPlane(); break;
	case EJXDropState::Freefall: OpenParachute(); break;
	case EJXDropState::Parachute: break;
	default:
		if (bIsCrouched) UnCrouch(); else Jump();
		break;
	}
}

void AJXCharacter::SetForceCombatStance(bool bForce)
{
	bForceCombatStance = bForce;
	UpdateCombatStance();
}

void AJXCharacter::UpdateCombatStance()
{
	// Face the camera direction while aiming or shooting (like other battle royale games).
	const bool bCombat = DropState == EJXDropState::OnGround && IsAlive() &&
		(bAiming || bFiring || bForceCombatStance || GetWorld()->GetTimeSeconds() - LastShotTime < 1.5f);
	bUseControllerRotationYaw = bCombat;
	GetCharacterMovement()->bOrientRotationToMovement = !bCombat;
}

void AJXCharacter::UpdateMovementSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (DropState != EJXDropState::OnGround) return;
	Move->MaxWalkSpeed = bHealing ? HealingSpeed : bAiming ? AimSpeed : bSprinting ? SprintSpeed : RunSpeed;
}

// ------------------------------------------------------------------ items

bool AJXCharacter::PickUp(const FJXItem& Item)
{
	if (!IsAlive()) return false;
	if (Weapons.Num() < SlotCount) Weapons.SetNum(SlotCount);
	const FVector DropAt = GetActorLocation() - FVector(0.f, 0.f, 80.f);
	switch (Item.Type)
	{
	case EJXItemType::Weapon:
	{
		const FJXWeaponDef* Def = UJXWeaponLibrary::Find(Item.WeaponId);
		if (!Def) return false;
		int32 Slot = 2;
		if (!Def->IsPistol())
		{
			Slot = !Weapons[0] ? 0 : !Weapons[1] ? 1 : (CurrentSlot == 1 ? 1 : 0);
		}
		if (AJXWeapon* Old = Weapons[Slot])
		{
			FJXItem OldItem;
			OldItem.Type = EJXItemType::Weapon;
			OldItem.WeaponId = Old->GetDef().Id;
			OldItem.Amount = Old->GetMag();
			DropItem(OldItem, DropAt);
			Old->Destroy();
			Weapons[Slot] = nullptr;
		}
		GiveWeapon(Item.WeaponId, Item.Amount);
		return true;
	}
	case EJXItemType::Ammo: AddAmmo(Item.Ammo, Item.Amount); return true;
	case EJXItemType::Bandage: Bandages += Item.Amount; return true;
	case EJXItemType::FirstAid: FirstAids += Item.Amount; return true;
	case EJXItemType::MedKit: MedKits += Item.Amount; return true;
	case EJXItemType::EnergyDrink: EnergyDrinks += Item.Amount; return true;
	case EJXItemType::FragGrenade: FragGrenades += Item.Amount; return true;
	case EJXItemType::SmokeGrenade: SmokeGrenades += Item.Amount; return true;
	case EJXItemType::FlashGrenade: FlashGrenades += Item.Amount; return true;
	case EJXItemType::Vest:
	case EJXItemType::Helmet:
	{
		const bool bVest = Item.Type == EJXItemType::Vest;
		int32& Level = bVest ? VestLevel : HelmetLevel;
		float& Durability = bVest ? VestDurability : HelmetDurability;
		if (Item.Level < Level || (Item.Level == Level && Item.Durability <= Durability)) return false;
		if (Level > 0 && Durability > 0.f)
		{
			FJXItem Old;
			Old.Type = Item.Type;
			Old.Level = Level;
			Old.Durability = Durability;
			DropItem(Old, DropAt);
		}
		Level = Item.Level;
		Durability = Item.Durability;
		return true;
	}
	default: return false;
	}
}

void AJXCharacter::GiveWeapon(FName WeaponId, int32 Mag)
{
	const FJXWeaponDef* Def = UJXWeaponLibrary::Find(WeaponId);
	if (!Def) return;
	if (Weapons.Num() < SlotCount) Weapons.SetNum(SlotCount);

	int32 Slot = Def->IsPistol() ? 2 : (!Weapons[0] ? 0 : !Weapons[1] ? 1 : -1);
	if (Slot < 0) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJXWeapon* W = GetWorld()->SpawnActor<AJXWeapon>(AJXWeapon::StaticClass(), GetActorTransform(), Params);
	if (!W) return;
	W->Init(*Def, Mag);
	AttachWeapon(W);
	Weapons[Slot] = W;

	// Equip the new gun unless we are already holding a primary and this is a pistol.
	if (CurrentSlot < 0 || CurrentSlot == Slot || (CurrentSlot == 2 && Slot != 2) || !GetCurrentWeapon())
	{
		CurrentSlot = Slot;
	}
	RefreshWeaponVisibility();
}

bool AJXCharacter::HasFreeSlotFor(FName WeaponId) const
{
	const FJXWeaponDef* Def = UJXWeaponLibrary::Find(WeaponId);
	if (!Def) return false;
	if (Def->IsPistol()) return !GetWeaponInSlot(2);
	return !GetWeaponInSlot(0) || !GetWeaponInSlot(1);
}

void AJXCharacter::AttachWeapon(AJXWeapon* W)
{
	const FName Socket = UJXSettings::Get()->HandSocket;
	W->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	W->GetRootComponent()->SetRelativeTransform(W->GetHandOffset());
}

void AJXCharacter::RefreshWeaponVisibility()
{
	const bool bShowAny = DropState == EJXDropState::OnGround && IsAlive();
	for (int32 i = 0; i < Weapons.Num(); ++i)
	{
		if (AJXWeapon* W = Weapons[i])
		{
			W->SetActorHiddenInGame(!(bShowAny && i == CurrentSlot));
		}
	}
}

void AJXCharacter::DropItem(const FJXItem& Item, const FVector& Location)
{
	AJXPickup::SpawnPickup(GetWorld(), Item, Location);
}

void AJXCharacter::DropAllLoot()
{
	const FVector Base = GetActorLocation() - FVector(0.f, 0.f, 80.f);
	int32 Index = 0;
	auto Drop = [this, &Base, &Index](const FJXItem& Item)
	{
		const float Angle = Index * 0.9f;
		const float Dist = 60.f + Index * 12.f;
		DropItem(Item, Base + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f));
		++Index;
	};

	for (AJXWeapon* W : Weapons)
	{
		if (!W) continue;
		FJXItem I;
		I.Type = EJXItemType::Weapon;
		I.WeaponId = W->GetDef().Id;
		I.Amount = W->GetMag();
		Drop(I);
		W->Destroy();
	}
	Weapons.Reset();
	Weapons.SetNum(SlotCount);
	CurrentSlot = -1;

	for (const TPair<EJXAmmoType, int32>& A : Ammo)
	{
		if (A.Value <= 0) continue;
		FJXItem I;
		I.Type = EJXItemType::Ammo;
		I.Ammo = A.Key;
		I.Amount = A.Value;
		Drop(I);
	}
	Ammo.Reset();

	auto DropCount = [&Drop](EJXItemType Type, int32 Count)
	{
		if (Count <= 0) return;
		FJXItem I;
		I.Type = Type;
		I.Amount = Count;
		Drop(I);
	};
	DropCount(EJXItemType::Bandage, Bandages);
	DropCount(EJXItemType::FirstAid, FirstAids);
	DropCount(EJXItemType::MedKit, MedKits);
	DropCount(EJXItemType::EnergyDrink, EnergyDrinks);
	DropCount(EJXItemType::FragGrenade, FragGrenades);
	DropCount(EJXItemType::SmokeGrenade, SmokeGrenades);
	DropCount(EJXItemType::FlashGrenade, FlashGrenades);
	Bandages = FirstAids = MedKits = EnergyDrinks = FragGrenades = SmokeGrenades = FlashGrenades = 0;

	if (VestLevel > 0 && VestDurability > 0.f)
	{
		FJXItem I; I.Type = EJXItemType::Vest; I.Level = VestLevel; I.Durability = VestDurability; Drop(I);
	}
	if (HelmetLevel > 0 && HelmetDurability > 0.f)
	{
		FJXItem I; I.Type = EJXItemType::Helmet; I.Level = HelmetLevel; I.Durability = HelmetDurability; Drop(I);
	}
	VestLevel = HelmetLevel = 0;
}

// ------------------------------------------------------------------ drop plane

void AJXCharacter::EnterPlane(AJXDropPlane* Plane)
{
	if (!Plane) return;
	CurrentPlane = Plane;
	DropState = EJXDropState::InPlane;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetCharacterMovement()->DisableMovement();
	AttachToActor(Plane, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	RefreshWeaponVisibility();
	Plane->AddPassenger(this);
}

void AJXCharacter::EjectFromPlane()
{
	if (DropState != EJXDropState::InPlane) return;
	AJXDropPlane* Plane = CurrentPlane.Get();
	const FVector Exit = Plane ? Plane->GetActorLocation() - FVector(0.f, 0.f, 600.f) : GetActorLocation();
	if (Plane) Plane->RemovePassenger(this);
	CurrentPlane = nullptr;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocation(Exit, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	DropState = EJXDropState::Freefall;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->SetMovementMode(MOVE_Falling);
	Move->AirControl = 1.f;
	Move->MaxWalkSpeed = 2500.f;
	Move->Velocity = Plane ? Plane->GetVelocity() * 0.3f : FVector::ZeroVector;
	RefreshWeaponVisibility();
	UpdateCombatStance();
}

void AJXCharacter::OpenParachute()
{
	if (DropState != EJXDropState::Freefall) return;
	DropState = EJXDropState::Parachute;
	GetCharacterMovement()->MaxWalkSpeed = 1200.f;
}

void AJXCharacter::UpdateDrop(float DeltaSeconds)
{
	if (DropState != EJXDropState::Freefall && DropState != EJXDropState::Parachute) return;
	UCharacterMovementComponent* Move = GetCharacterMovement();

	if (DropState == EJXDropState::Freefall)
	{
		Move->Velocity.Z = FMath::Max(Move->Velocity.Z, -FreefallSpeed);
		// Open automatically when close to the ground.
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(JXChute), false, this);
		const FVector Start = GetActorLocation();
		const bool bNear = GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, AutoParachuteHeight), ECC_Visibility, Q);
		if (bNear) OpenParachute();
	}
	else
	{
		Move->Velocity.Z = FMath::Max(Move->Velocity.Z, -ParachuteFallSpeed);
	}
}

void AJXCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (DropState == EJXDropState::Freefall || DropState == EJXDropState::Parachute)
	{
		DropState = EJXDropState::OnGround;
		GetCharacterMovement()->AirControl = 0.35f;
		UpdateMovementSpeed();
		RefreshWeaponVisibility();
	}
}

// ------------------------------------------------------------------ vehicles

void AJXCharacter::EnterVehicle(APawn* InVehicle, USceneComponent* Seat, bool bHideCharacter)
{
	StopFire();
	SetAiming(false);
	CancelHealing();
	Vehicle = InVehicle;
	GetCharacterMovement()->DisableMovement();
	// A hidden driver has no collision. A visible gunner keeps its mesh so it can still be shot.
	SetActorEnableCollision(!bHideCharacter);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Seat)
	{
		AttachToComponent(Seat, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	SetActorHiddenInGame(bHideCharacter);
	if (bHideCharacter)
	{
		for (AJXWeapon* W : Weapons) if (W) W->SetActorHiddenInGame(true);
	}
}

void AJXCharacter::ExitVehicle(const FVector& ExitLocation)
{
	Vehicle = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (IsAlive())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	RefreshWeaponVisibility();
}

// ------------------------------------------------------------------ feedback

void AJXCharacter::NotifyHitTarget(bool bHeadshot)
{
	if (AJXPlayerController* PC = GetJXController())
	{
		PC->NotifyHitTarget(bHeadshot);
	}
}

void AJXCharacter::ApplyRecoil(float Pitch, float Yaw)
{
	if (!IsLocalHuman() || !Controller) return;
	const float Scale = bAiming ? 0.7f : 1.f;
	FRotator R = Controller->GetControlRotation();
	R.Pitch += Pitch * Scale * FMath::FRandRange(0.8f, 1.2f);
	R.Yaw += Yaw * Scale * FMath::FRandRange(-1.f, 1.f);
	Controller->SetControlRotation(R);
}

void AJXCharacter::ApplyFlash(float Strength)
{
	if (AJXPlayerController* PC = GetJXController())
	{
		PC->NotifyFlash(Strength);
	}
}

float AJXCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!IsAlive() || DropState == EJXDropState::InPlane) return 0.f;

	const UDamageType* Type = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : nullptr;
	const bool bZone = Type && Type->IsA<UJXZoneDamageType>();
	float Damage = DamageAmount;

	if (!bZone)
	{
		bool bHead = false;
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			bHead = UJXCombatLibrary::IsHeadBone(static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.BoneName);
		}
		int32& Level = bHead ? HelmetLevel : VestLevel;
		float& Durability = bHead ? HelmetDurability : VestDurability;
		if (Level > 0 && Durability > 0.f)
		{
			const float Reduction = bHead ? UJXWeaponLibrary::HelmetReduction(Level) : UJXWeaponLibrary::VestReduction(Level);
			Damage *= 1.f - Reduction;
			Durability -= DamageAmount;
			if (Durability <= 0.f)
			{
				Durability = 0.f;
				Level = 0;
			}
		}
	}

	// Let bots fight back against whoever shot them.
	if (EventInstigator && EventInstigator != Controller && !IsPlayerControlled())
	{
		if (AController* C = GetController())
		{
			if (APawn* Attacker = EventInstigator->GetPawn())
			{
				C->SetFocus(Attacker);
			}
		}
	}
	return Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
}

void AJXCharacter::HandleDamaged(float Damage, AController* InstigatedBy)
{
	if (AJXPlayerController* PC = GetJXController())
	{
		PC->NotifyDamaged(Damage);
	}
}

void AJXCharacter::HandleDeath(AActor* Victim, AController* Killer, AActor* DamageCauser)
{
	if (APawn* V = Vehicle.Get())
	{
		if (AJXVehicle* JV = Cast<AJXVehicle>(V)) JV->ForceExit();
	}
	StopFire();
	bAiming = false;
	bHealing = false;
	bFiring = false;
	bForceCombatStance = false;

	DropAllLoot();

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	if (AJXGameMode* GM = GetWorld()->GetAuthGameMode<AJXGameMode>())
	{
		GM->OnCharacterDied(this, Killer);
	}

	if (AJXPlayerController* PC = GetJXController())
	{
		DisableInput(PC);
	}
	else
	{
		DetachFromControllerPendingDestroy();
		SetLifeSpan(30.f);
	}
}

// ------------------------------------------------------------------ tick

void AJXCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsAlive()) return;

	UpdateDrop(DeltaSeconds);
	UpdateHealing();
	if (bFiring || GetWorld()->GetTimeSeconds() - LastShotTime < 1.6f)
	{
		UpdateCombatStance();
	}
	if (IsLocalHuman())
	{
		UpdateInteractable();
		UpdateCamera(DeltaSeconds);
	}
}

void AJXCharacter::UpdateInteractable()
{
	FocusedInteractable = nullptr;
	if (DropState != EJXDropState::OnGround || Vehicle.IsValid()) return;

	TArray<AActor*> Overlaps;
	GetCapsuleComponent()->GetOverlappingActors(Overlaps);
	float BestDist = TNumericLimits<float>::Max();
	for (AActor* A : Overlaps)
	{
		const IJXInteractable* I = Cast<IJXInteractable>(A);
		if (!I || I->GetInteractText(this).IsEmpty()) continue;
		const float D = FVector::DistSquared(A->GetActorLocation(), GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			FocusedInteractable = A;
		}
	}
}

void AJXCharacter::UpdateCamera(float DeltaSeconds)
{
	float Arm = DefaultArmLength;
	float Fov = DefaultFov;
	FVector Offset = DefaultSocketOffset;

	switch (DropState)
	{
	case EJXDropState::InPlane: Arm = 2500.f; Offset = FVector::ZeroVector; break;
	case EJXDropState::Freefall:
	case EJXDropState::Parachute: Arm = 600.f; Offset = FVector(0.f, 0.f, 100.f); break;
	default:
		if (bAiming)
		{
			const AJXWeapon* W = GetCurrentWeapon();
			Arm = 130.f;
			Offset = FVector(0.f, 45.f, 60.f);
			Fov = W ? W->GetDef().AdsFov : 70.f;
		}
		break;
	}
	const float Speed = 12.f;
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, Arm, DeltaSeconds, Speed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, Offset, DeltaSeconds, Speed);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, Fov, DeltaSeconds, Speed));
}
