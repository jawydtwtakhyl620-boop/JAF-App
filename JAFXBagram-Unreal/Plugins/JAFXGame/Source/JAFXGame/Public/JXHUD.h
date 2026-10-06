// JAF X BAGRAM - on-screen HUD drawn with the canvas (works without any UI assets).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "JXHUD.generated.h"

class AJXCharacter;
class AJXPlayerController;

UCLASS()
class JAFXGAME_API AJXHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCrosshair(const AJXCharacter* C, float Fov);
	void DrawHitMarker(const AJXPlayerController* PC);
	void DrawPlayerStatus(const AJXCharacter* C);
	void DrawWeaponPanel(const AJXCharacter* C);
	void DrawMinimap(const AJXCharacter* C);
	void DrawTopInfo(const AJXCharacter* C);
	void DrawKillFeed(const AJXPlayerController* PC);
	void DrawCenterMessage(const AJXPlayerController* PC);
	void DrawTouchLabels(const AJXPlayerController* PC);
	void DrawProgress(const FString& Label, float Progress);
	void DrawDropInfo(const AJXCharacter* C);
	void DrawBar(float X, float Y, float W, float H, float Fill, const FLinearColor& Color);
	void DrawTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, class UFont* Font, float Scale);
	FVector2D WorldToMap(const FVector2D& World) const;

	float S = 1.f; // UI scale for the current resolution
	FVector2D MapOrigin = FVector2D::ZeroVector;
	float MapSize = 200.f;
	float MapHalf = 72000.f;
};
