// JAF X BAGRAM - the soldier. Used by the player and by bots.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JXTypes.h"
#include "JXCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UJXHealthComponent;
class AJXWeapon;
class AJXDropPlane;
class AJXPlayerController;
struct FInputActionValue;

UCLASS()
class JAFXGAME_API AJXCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AJXCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UJXHealthComponent> Health;

	// ---------- Movement tuning (cm/s) ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float RunSpeed = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float SprintSpeed = 630.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float AimSpeed = 280.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float HealingSpeed = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float FreefallSpeed = 5500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float ParachuteFallSpeed = 550.f;
	/** Parachute opens by itself below this height above ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float AutoParachuteHeight = 15000.f;

	// ---------- Inventory ----------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 Bandages = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 FirstAids = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 MedKits = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 EnergyDrinks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 FragGrenades = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 SmokeGrenades = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 FlashGrenades = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") EJXItemType SelectedThrowable = EJXItemType::FragGrenade;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 VestLevel = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") float VestDurability = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") int32 HelmetLevel = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory") float HelmetDurability = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 Kills = 0;

	/** Extra weapon spread for bots so they are not perfect shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI") float BotSpreadMultiplier = 1.f;

	// ---------- Queries ----------
	UFUNCTION(BlueprintPure, Category = "JAFX") bool IsAlive() const;
	UFUNCTION(BlueprintPure, Category = "JAFX") bool IsAiming() const { return bAiming; }
	UFUNCTION(BlueprintPure, Category = "JAFX") EJXDropState GetDropState() const { return DropState; }
	UFUNCTION(BlueprintPure, Category = "JAFX") AJXWeapon* GetCurrentWeapon() const;
	UFUNCTION(BlueprintPure, Category = "JAFX") AJXWeapon* GetWeaponInSlot(int32 Slot) const;
	UFUNCTION(BlueprintPure, Category = "JAFX") int32 GetCurrentSlot() const { return CurrentSlot; }
	UFUNCTION(BlueprintPure, Category = "JAFX") int32 GetAmmo(EJXAmmoType Type) const;
	UFUNCTION(BlueprintPure, Category = "JAFX") int32 GetThrowableCount(EJXItemType Type) const;
	UFUNCTION(BlueprintPure, Category = "JAFX") bool IsHealing() const { return bHealing; }
	UFUNCTION(BlueprintPure, Category = "JAFX") float GetHealProgress() const;
	UFUNCTION(BlueprintPure, Category = "JAFX") APawn* GetVehicle() const { return Vehicle.Get(); }
	float GetSpreadMultiplier() const;
	/** Prompt for the nearest usable thing, e.g. "Pick up K-47". */
	FString GetInteractPrompt() const;
	FString GetHealLabel() const;

	/** Removes up to Wanted rounds and returns how many were taken. */
	int32 TakeAmmo(EJXAmmoType Type, int32 Wanted);
	void AddAmmo(EJXAmmoType Type, int32 Amount);

	// ---------- Actions (input and AI) ----------
	UFUNCTION(BlueprintCallable, Category = "JAFX") void StartFire();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void StopFire();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void SetAiming(bool bNewAiming);
	UFUNCTION(BlueprintCallable, Category = "JAFX") void Reload();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void Interact();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void EquipSlot(int32 Slot);
	UFUNCTION(BlueprintCallable, Category = "JAFX") void NextWeapon();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void UseBestHeal();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void ThrowGrenade();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void CycleThrowable();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void ToggleCrouch();
	UFUNCTION(BlueprintCallable, Category = "JAFX") void SetSprinting(bool bNewSprinting);
	/** Jump on the ground, leave the plane, or open the parachute. */
	UFUNCTION(BlueprintCallable, Category = "JAFX") void JumpOrDrop();
	/** Bots: turn to face the aim direction while fighting. */
	void SetForceCombatStance(bool bForce);

	// ---------- Items ----------
	/** Adds an item; returns false if it was not taken. A replaced weapon/armor is dropped at DropLocation. */
	bool PickUp(const FJXItem& Item);
	void GiveWeapon(FName WeaponId, int32 Mag);
	/** True when the weapon could be picked up without replacing another one. */
	bool HasFreeSlotFor(FName WeaponId) const;

	// ---------- Drop plane ----------
	void EnterPlane(AJXDropPlane* Plane);
	void EjectFromPlane();
	void OpenParachute();

	// ---------- Vehicles / mounted guns ----------
	void EnterVehicle(APawn* InVehicle, USceneComponent* Seat, bool bHideCharacter);
	void ExitVehicle(const FVector& ExitLocation);

	// ---------- Feedback ----------
	void NotifyHitTarget(bool bHeadshot);
	void ApplyRecoil(float Pitch, float Yaw);
	void ApplyFlash(float Strength);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION() void HandleDeath(AActor* Victim, AController* Killer, AActor* DamageCauser);
	UFUNCTION() void HandleDamaged(float Damage, AController* InstigatedBy);

	void InputMove(const FInputActionValue& Value);
	void InputLook(const FInputActionValue& Value);
	void InputJumpReleased() { StopJumping(); }
	void InputSprintStart() { SetSprinting(true); }
	void InputSprintStop() { SetSprinting(false); }
	void InputAimStart() { SetAiming(true); }
	void InputAimStop() { SetAiming(false); }
	void InputSlot1() { EquipSlot(0); }
	void InputSlot2() { EquipSlot(1); }
	void InputSlot3() { EquipSlot(2); }

private:
	void ApplyDefaultMesh();
	void UpdateInteractable();
	void UpdateCamera(float DeltaSeconds);
	void UpdateDrop(float DeltaSeconds);
	void UpdateHealing();
	void UpdateMovementSpeed();
	void UpdateCombatStance();
	void AttachWeapon(AJXWeapon* Weapon);
	void RefreshWeaponVisibility();
	void CancelHealing();
	void DropItem(const FJXItem& Item, const FVector& Location);
	void DropAllLoot();
	bool IsLocalHuman() const;
	AJXPlayerController* GetJXController() const;

	UPROPERTY() TArray<TObjectPtr<AJXWeapon>> Weapons;
	UPROPERTY() TMap<EJXAmmoType, int32> Ammo;
	UPROPERTY() TWeakObjectPtr<AActor> FocusedInteractable;
	UPROPERTY() TWeakObjectPtr<AJXDropPlane> CurrentPlane;
	UPROPERTY() TWeakObjectPtr<APawn> Vehicle;

	int32 CurrentSlot = -1;
	bool bAiming = false;
	bool bSprinting = false;
	bool bFiring = false;
	bool bForceCombatStance = false;
	float LastShotTime = -100.f;
	EJXDropState DropState = EJXDropState::OnGround;

	bool bHealing = false;
	EJXItemType HealingItem = EJXItemType::Bandage;
	float HealStartTime = 0.f;
	float HealDuration = 0.f;

	float DefaultArmLength = 300.f;
	float DefaultFov = 90.f;
	FVector DefaultSocketOffset = FVector::ZeroVector;
};
