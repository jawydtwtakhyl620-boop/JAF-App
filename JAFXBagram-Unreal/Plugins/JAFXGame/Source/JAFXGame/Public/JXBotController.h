// JAF X BAGRAM - simple battle royale AI: loot while wandering, stay in the zone, fight anyone it sees.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "JXBotController.generated.h"

class AJXCharacter;

UCLASS()
class JAFXGAME_API AJXBotController : public AAIController
{
	GENERATED_BODY()

public:
	AJXBotController();

	/** How far bots can spot enemies (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot") float SightRange = 9000.f;
	/** Seconds between seeing an enemy and starting to shoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot") float ReactionTime = 0.6f;
	/** Weapon spread multiplier (higher = worse aim). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot") float AimError = 2.5f;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	AJXCharacter* GetBot() const;
	AJXCharacter* FindTarget() const;
	bool CanSee(const AActor* Other) const;
	void Fight(float DeltaSeconds);
	void Roam();
	bool MoveToRandomPoint(const FVector& Origin, float Radius);

	UPROPERTY() TWeakObjectPtr<AJXCharacter> Target;
	FVector LastKnownLocation = FVector::ZeroVector;
	bool bHasLastKnown = false;
	float ThinkTimer = 0.f;
	float SeenTime = 0.f;
	float BurstTimer = 0.f;
	float StrafeTimer = 0.f;
	float IdleTimer = 0.f;
};
