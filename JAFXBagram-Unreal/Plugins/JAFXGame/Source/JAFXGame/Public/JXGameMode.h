// JAF X BAGRAM - battle royale rules: drop plane, bots, safe zone, last one standing wins.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JXGameMode.generated.h"

class AJXCharacter;
class AJXSafeZone;
class AJXDropPlane;

UCLASS()
class JAFXGAME_API AJXGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJXGameMode();

	/** Seconds after the level loads before the plane takes off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match") float StartDelay = 3.f;
	/** Show the main menu and wait for PLAY before the plane takes off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match") bool bShowMainMenu = true;
	/** Builds the Bagram map automatically when the level does not contain one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match") bool bAutoSpawnBagramMap = true;
	/** Seconds after the match ends before it restarts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match") float RestartDelay = 12.f;

	UFUNCTION(BlueprintPure, Category = "Match") AJXSafeZone* GetSafeZone() const { return Zone; }
	UFUNCTION(BlueprintPure, Category = "Match") AJXDropPlane* GetPlane() const { return Plane; }
	UFUNCTION(BlueprintPure, Category = "Match") int32 GetAliveCount() const;
	UFUNCTION(BlueprintPure, Category = "Match") bool IsMatchOver() const { return bMatchOver; }
	UFUNCTION(BlueprintPure, Category = "Match") float GetMapHalfSize() const;

	void OnCharacterDied(AJXCharacter* Victim, AController* Killer);

	/** True while the main menu is up and the match has not started. */
	bool IsWaitingForMenu() const { return bWaitingForMenu; }
	/** Called by the PLAY button. */
	void RequestBeginMatch();
	/** Set before reloading the level to skip the main menu once (used by "Restart match"). */
	static bool bSkipMenuOnce;

protected:
	virtual void StartPlay() override;

private:
	void BeginMatch();
	void EndMatch(AJXCharacter* Winner);
	void RestartMatch();
	AJXCharacter* SpawnBot(int32 Index, const FVector& Location);
	FVector RandomGroundPoint(const FVector& Center, float Radius) const;

	UPROPERTY() TObjectPtr<AJXSafeZone> Zone;
	UPROPERTY() TObjectPtr<AJXDropPlane> Plane;
	UPROPERTY() TArray<TObjectPtr<AJXCharacter>> Participants;

	bool bMatchStarted = false;
	bool bWaitingForMenu = false;
	bool bMatchOver = false;
	FTimerHandle StartTimer;
	FTimerHandle RestartTimer;
};
