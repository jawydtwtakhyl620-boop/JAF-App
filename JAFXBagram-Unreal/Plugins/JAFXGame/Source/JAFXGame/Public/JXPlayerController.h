// JAF X BAGRAM - player controller: input setup, touch controls, HUD feedback.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JXPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UTouchInterface;
class SWidget;

/** All input actions. Created in code, so no input assets are needed in the editor. */
USTRUCT()
struct FJXInputActions
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> Look;
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Sprint;
	UPROPERTY() TObjectPtr<UInputAction> Crouch;
	UPROPERTY() TObjectPtr<UInputAction> Fire;
	UPROPERTY() TObjectPtr<UInputAction> Aim;
	UPROPERTY() TObjectPtr<UInputAction> Reload;
	UPROPERTY() TObjectPtr<UInputAction> Interact;
	UPROPERTY() TObjectPtr<UInputAction> Slot1;
	UPROPERTY() TObjectPtr<UInputAction> Slot2;
	UPROPERTY() TObjectPtr<UInputAction> Slot3;
	UPROPERTY() TObjectPtr<UInputAction> NextWeapon;
	UPROPERTY() TObjectPtr<UInputAction> Heal;
	UPROPERTY() TObjectPtr<UInputAction> Throw;
	UPROPERTY() TObjectPtr<UInputAction> CycleThrowable;
	UPROPERTY() TObjectPtr<UInputAction> Pause;
};

/** A touch button drawn by the HUD so players know what each circle does. */
struct FJXTouchLabel
{
	FVector2D Center; // 0..1 of the screen
	FString Text;
};

struct FJXKillFeedEntry
{
	FString Text;
	float Time = 0.f;
};

UCLASS()
class JAFXGAME_API AJXPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	const FJXInputActions& GetInputActions();

	void NotifyHitTarget(bool bHeadshot);
	void NotifyDamaged(float Damage);
	void NotifyFlash(float Strength);
	void AddKillFeed(const FString& Text);
	void ShowCenterMessage(const FString& Title, const FString& Subtitle);

	// ---- Menus (main menu, pause menu, settings) ----
	void ShowMenu(bool bPauseMenu);
	void CloseMenu();
	bool IsMenuOpen() const { return MenuWidget.IsValid(); }
	void TogglePauseMenu();
	void StartMatchFromMenu();
	void RestartMatchFromMenu();
	void QuitFromMenu();

	// ---- Settings (saved between sessions) ----
	/** Camera look speed multiplier (0.2 - 2.0). */
	float LookSensitivity = 1.f;
	void SetLookSensitivity(float Value);
	/** 0 = Low, 1 = Medium, 2 = High, 3 = Epic, -1 = custom. */
	int32 GetGraphicsQuality() const;
	void SetGraphicsQuality(int32 Level);
	bool IsFpsShown() const { return bFpsShown; }
	void ToggleFps();

	// Read by the HUD.
	float LastHitTime = -100.f;
	bool bLastHitHead = false;
	float LastDamageTime = -100.f;
	float FlashAmount = 0.f;
	FString CenterTitle;
	FString CenterSubtitle;
	TArray<FJXKillFeedEntry> KillFeed;
	TArray<FJXTouchLabel> TouchLabels;
	bool bTouchControls = false;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

private:
	void CreateInput();
	void AddMappingContext();
	void SetupTouchInterface();

	UPROPERTY() FJXInputActions Actions;
	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<UTouchInterface> TouchInterface;
	bool bInputCreated = false;
	bool bMappingAdded = false;
	bool bFpsShown = false;
	TSharedPtr<SWidget> MenuWidget;
};
