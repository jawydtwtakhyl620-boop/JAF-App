// JAF X BAGRAM - an item lying on the ground.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JXInteractable.h"
#include "JXTypes.h"
#include "JXPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class URotatingMovementComponent;

UCLASS()
class JAFXGAME_API AJXPickup : public AActor, public IJXInteractable
{
	GENERATED_BODY()

public:
	AJXPickup();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<URotatingMovementComponent> Spinner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ExposeOnSpawn = true))
	FJXItem Item;

	void SetItem(const FJXItem& InItem);

	/** Spawns a pickup on the ground below Location. */
	static AJXPickup* SpawnPickup(UWorld* World, const FJXItem& Item, const FVector& Location);

	// IJXInteractable
	virtual FString GetInteractText(const AJXCharacter* By) const override;
	virtual void Interact(AJXCharacter* By) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool IsAutoPickup() const;
	void UpdateVisual();
};

/** Place in a level (for hand-made maps): fills its box with random loot on the floor. */
UCLASS()
class JAFXGAME_API AJXLootSpawner : public AActor
{
	GENERATED_BODY()

public:
	AJXLootSpawner();

	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<class UBoxComponent> Area;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = 0))
	int32 Count = 6;

protected:
	virtual void BeginPlay() override;
};
