// JAF X BAGRAM - base for things a soldier can get into: tanks and the DShK heavy machine gun.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "JXInteractable.h"
#include "JXVehicle.generated.h"

class AJXCharacter;
class UCameraComponent;
class USpringArmComponent;
class USphereComponent;
class UJXHealthComponent;
struct FInputActionValue;

UCLASS(Abstract)
class JAFXGAME_API AJXVehicle : public APawn, public IJXInteractable
{
	GENERATED_BODY()

public:
	AJXVehicle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<USceneComponent> Seat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<USphereComponent> UseArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UJXHealthComponent> Health;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	FString VehicleName = TEXT("Vehicle");

	/** Tanks hide the driver; the DShK gunner stays visible (and can be shot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool bHideOccupant = true;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	AJXCharacter* GetOccupant() const { return Occupant.Get(); }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsDestroyed() const;

	/** Throws the occupant out (death, vehicle destroyed). */
	void ForceExit();

	/** Text for the HUD, e.g. "Cannon ready" or "Overheated". */
	virtual FString GetStatusText() const { return FString(); }

	// IJXInteractable
	virtual FString GetInteractText(const AJXCharacter* By) const override;
	virtual void Interact(AJXCharacter* By) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void StartFire() {}
	virtual void StopFire() {}
	virtual void SwitchWeapon() {}
	virtual void OnDestroyedByDamage(AController* Killer) {}
	virtual FVector GetExitLocation() const;

	UFUNCTION() void HandleDeath(AActor* Victim, AController* Killer, AActor* DamageCauser);

	void InputLook(const FInputActionValue& Value);
	void InputMove(const FInputActionValue& Value);
	void InputMoveStop(const FInputActionValue& Value) { MoveInput = FVector2D::ZeroVector; }
	void InputAimStart() { bZoom = true; }
	void InputAimStop() { bZoom = false; }
	void InputLeave() { ForceExit(); }

	/** Latest movement stick / WASD value. */
	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bZoom = false;
	float DefaultArm = 600.f;

	UPROPERTY() TWeakObjectPtr<AJXCharacter> Occupant;
};
