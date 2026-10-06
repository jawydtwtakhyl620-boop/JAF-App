// JAF X BAGRAM - cargo plane that carries everyone across the map at the start.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JXDropPlane.generated.h"

class AJXCharacter;
class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;

UCLASS()
class JAFXGAME_API AJXDropPlane : public AActor
{
	GENERATED_BODY()

public:
	AJXDropPlane();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane") TObjectPtr<UStaticMeshComponent> Fuselage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane") TObjectPtr<UStaticMeshComponent> Wings;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane") TObjectPtr<UStaticMeshComponent> Tail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane") TObjectPtr<UAudioComponent> EngineAudio;

	/** cm/s (6000 = 216 km/h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane") float Speed = 6000.f;

	/** Starts the flight. Bots jump at random points; everyone left jumps at the end. */
	void StartFlight(const FVector& From, const FVector& To);

	void AddPassenger(AJXCharacter* C);
	void RemovePassenger(AJXCharacter* C);

	/** 0..1 along the route. */
	float GetProgress() const;
	FVector GetRouteStart() const { return Start; }
	FVector GetRouteEnd() const { return End; }
	bool IsFlying() const { return bFlying; }

	virtual FVector GetVelocity() const override { return bFlying ? (End - Start).GetSafeNormal() * Speed : FVector::ZeroVector; }

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY() TArray<TObjectPtr<AJXCharacter>> Passengers;
	UPROPERTY() TMap<TObjectPtr<AJXCharacter>, float> BotJumpAt;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Distance = 0.f;
	float Travelled = 0.f;
	bool bFlying = false;
};
