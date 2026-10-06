#include "JXHUD.h"
#include "JXCharacter.h"
#include "JXDropPlane.h"
#include "JXGameMode.h"
#include "JXHealthComponent.h"
#include "JXPlayerController.h"
#include "JXSafeZone.h"
#include "JXVehicle.h"
#include "JXWeapon.h"
#include "JXWeaponLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor White(1.f, 1.f, 1.f, 0.95f);
	const FLinearColor Dim(0.f, 0.f, 0.f, 0.45f);
	const FLinearColor Gold(0.95f, 0.75f, 0.2f, 1.f);
	const FLinearColor Red(0.9f, 0.15f, 0.1f, 1.f);
	const FLinearColor ZoneBlue(0.2f, 0.45f, 1.f, 1.f);
}

void AJXHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	S = Canvas->ClipY / 1080.f;

	AJXPlayerController* PC = Cast<AJXPlayerController>(GetOwningPlayerController());
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	AJXCharacter* C = Cast<AJXCharacter>(Pawn);
	AJXVehicle* Vehicle = Cast<AJXVehicle>(Pawn);
	if (Vehicle) C = Vehicle->GetOccupant();

	// Full-screen effects first so text stays readable.
	const float Now = GetWorld()->GetTimeSeconds();
	const float DamageAge = Now - PC->LastDamageTime;
	if (DamageAge < 0.35f)
	{
		DrawRect(FLinearColor(0.7f, 0.f, 0.f, 0.3f * (1.f - DamageAge / 0.35f)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}

	if (C)
	{
		const float Fov = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 90.f;
		if (C->IsAlive() && C->GetDropState() == EJXDropState::OnGround)
		{
			DrawCrosshair(C, Fov);
			DrawHitMarker(PC);
			if (!Vehicle) DrawWeaponPanel(C);
		}
		DrawPlayerStatus(C);
		DrawMinimap(C);
		DrawTopInfo(C);
		DrawDropInfo(C);

		if (C->IsHealing())
		{
			DrawProgress(C->GetHealLabel(), C->GetHealProgress());
		}
		else if (const AJXWeapon* W = C->GetCurrentWeapon())
		{
			if (W->IsReloading()) DrawProgress(TEXT("Reloading"), W->GetReloadProgress());
		}

		const FString Prompt = C->GetInteractPrompt();
		if (!Prompt.IsEmpty() && !Vehicle)
		{
			const FString Key = PC->bTouchControls ? TEXT("[USE] ") : TEXT("[F] ");
			DrawTextCentered(Key + Prompt, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.62f, White, GEngine->GetMediumFont(), 1.2f * S);
		}
	}

	if (Vehicle)
	{
		const FString Status = Vehicle->GetStatusText() + (PC->bTouchControls ? TEXT("   [USE] exit") : TEXT("   [F] exit  [Q] switch weapon"));
		DrawTextCentered(Status, Canvas->ClipX * 0.5f, Canvas->ClipY - 140.f * S, Gold, GEngine->GetMediumFont(), 1.1f * S);
	}

	DrawKillFeed(PC);
	DrawTouchLabels(PC);

	if (PC->FlashAmount > 0.f)
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(PC->FlashAmount, 0.f, 1.f)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
	DrawCenterMessage(PC);
}

void AJXHUD::DrawBar(float X, float Y, float W, float H, float Fill, const FLinearColor& Color)
{
	DrawRect(Dim, X, Y, W, H);
	DrawRect(Color, X, Y, W * FMath::Clamp(Fill, 0.f, 1.f), H);
}

void AJXHUD::DrawTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, UFont* Font, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.7f), CenterX - W * 0.5f + 2.f, Y + 2.f, Font, Scale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, Font, Scale);
}

void AJXHUD::DrawCrosshair(const AJXCharacter* C, float Fov)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const AJXWeapon* W = C->GetCurrentWeapon();
	if (!W && !C->GetVehicle())
	{
		DrawRect(White, CX - 2.f, CY - 2.f, 4.f, 4.f);
		return;
	}
	const float Spread = W ? W->GetCurrentSpread() : 0.8f;
	// Convert the spread angle to pixels on screen.
	const float Gap = FMath::Clamp(FMath::Tan(FMath::DegreesToRadians(Spread)) / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)) * CX, 3.f * S, 120.f * S);
	const float Len = 12.f * S;
	const float T = FMath::Max(2.f, 2.f * S);
	DrawLine(CX, CY - Gap - Len, CX, CY - Gap, White, T);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Len, White, T);
	DrawLine(CX - Gap - Len, CY, CX - Gap, CY, White, T);
	DrawLine(CX + Gap, CY, CX + Gap + Len, CY, White, T);
	DrawRect(White, CX - 1.f, CY - 1.f, 2.f, 2.f);
}

void AJXHUD::DrawHitMarker(const AJXPlayerController* PC)
{
	const float Age = GetWorld()->GetTimeSeconds() - PC->LastHitTime;
	if (Age > 0.18f) return;
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float A = 8.f * S, B = 18.f * S;
	const FLinearColor Col = PC->bLastHitHead ? Red : White;
	const float T = FMath::Max(2.f, 3.f * S);
	DrawLine(CX - B, CY - B, CX - A, CY - A, Col, T);
	DrawLine(CX + B, CY - B, CX + A, CY - A, Col, T);
	DrawLine(CX - B, CY + B, CX - A, CY + A, Col, T);
	DrawLine(CX + B, CY + B, CX + A, CY + A, Col, T);
}

void AJXHUD::DrawPlayerStatus(const AJXCharacter* C)
{
	const float W = 480.f * S, H = 22.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY - 70.f * S;
	const float HP = C->Health ? C->Health->GetHealth() : 0.f;
	DrawBar(X, Y, W, H, HP / 100.f, HP > 30.f ? White : Red);
	DrawTextCentered(FString::Printf(TEXT("%d"), FMath::CeilToInt(HP)), Canvas->ClipX * 0.5f, Y + 1.f * S, FLinearColor::Black, GEngine->GetSmallFont(), 1.2f * S);

	// Armor next to the health bar.
	UFont* Small = GEngine->GetSmallFont();
	if (C->VestLevel > 0)
	{
		DrawText(FString::Printf(TEXT("VEST Lv%d"), C->VestLevel), White, X - 120.f * S, Y, Small, 1.2f * S);
		DrawBar(X - 120.f * S, Y + 18.f * S, 100.f * S, 4.f * S, C->VestDurability / UJXWeaponLibrary::VestDurability(C->VestLevel), White);
	}
	if (C->HelmetLevel > 0)
	{
		DrawText(FString::Printf(TEXT("HELMET Lv%d"), C->HelmetLevel), White, X + W + 20.f * S, Y, Small, 1.2f * S);
		DrawBar(X + W + 20.f * S, Y + 18.f * S, 100.f * S, 4.f * S, C->HelmetDurability / UJXWeaponLibrary::HelmetDurability(C->HelmetLevel), White);
	}

	// Healing items and grenades (bottom left).
	const FString Items = FString::Printf(TEXT("Bandage %d   First Aid %d   Med Kit %d   Drink %d"),
		C->Bandages, C->FirstAids, C->MedKits, C->EnergyDrinks);
	DrawText(Items, White, 30.f * S, Canvas->ClipY - 70.f * S, Small, 1.2f * S);
	const TCHAR* ThrowName = C->SelectedThrowable == EJXItemType::SmokeGrenade ? TEXT("Smoke")
		: C->SelectedThrowable == EJXItemType::FlashGrenade ? TEXT("Flash") : TEXT("Frag");
	const FString Throw = FString::Printf(TEXT("Grenade: %s  (Frag %d  Smoke %d  Flash %d)"), ThrowName, C->FragGrenades, C->SmokeGrenades, C->FlashGrenades);
	DrawText(Throw, White, 30.f * S, Canvas->ClipY - 45.f * S, Small, 1.2f * S);
}

void AJXHUD::DrawWeaponPanel(const AJXCharacter* C)
{
	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();
	const float Right = Canvas->ClipX - 40.f * S;
	float Y = Canvas->ClipY - 190.f * S;

	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		const AJXWeapon* W = C->GetWeaponInSlot(Slot);
		const bool bActive = Slot == C->GetCurrentSlot();
		const FString Line = FString::Printf(TEXT("%d  %s"), Slot + 1, W ? *W->GetDef().DisplayName : TEXT("-"));
		float TW = 0.f, TH = 0.f;
		GetTextSize(Line, TW, TH, Small, 1.2f * S);
		DrawText(Line, bActive ? Gold : FLinearColor(1.f, 1.f, 1.f, 0.6f), Right - TW, Y, Small, 1.2f * S);
		Y += 24.f * S;
	}

	if (const AJXWeapon* W = C->GetCurrentWeapon())
	{
		const FString Ammo = FString::Printf(TEXT("%d / %d"), W->GetMag(), C->GetAmmo(W->GetDef().Ammo));
		float TW = 0.f, TH = 0.f;
		GetTextSize(Ammo, TW, TH, Big, 1.4f * S);
		DrawText(Ammo, W->GetMag() == 0 ? Red : White, Right - TW, Y + 4.f * S, Big, 1.4f * S);
		const FString Info = FString::Printf(TEXT("%s  %s"), *UJXWeaponLibrary::AmmoName(W->GetDef().Ammo), W->GetDef().bAutomatic ? TEXT("AUTO") : TEXT("SINGLE"));
		GetTextSize(Info, TW, TH, Small, 1.f * S);
		DrawText(Info, FLinearColor(1.f, 1.f, 1.f, 0.7f), Right - TW, Y + 50.f * S, Small, 1.f * S);
	}
}

FVector2D AJXHUD::WorldToMap(const FVector2D& World) const
{
	// Map up = world +X (north), map right = world +Y (east).
	const float U = (World.Y + MapHalf) / (2.f * MapHalf);
	const float V = (MapHalf - World.X) / (2.f * MapHalf);
	return MapOrigin + FVector2D(U, V) * MapSize;
}

void AJXHUD::DrawMinimap(const AJXCharacter* C)
{
	const AJXGameMode* GM = GetWorld()->GetAuthGameMode<AJXGameMode>();
	MapHalf = GM ? GM->GetMapHalfSize() : 72000.f;
	MapSize = 230.f * S;
	MapOrigin = FVector2D(Canvas->ClipX - MapSize - 30.f * S, 30.f * S);

	DrawRect(FLinearColor(0.32f, 0.28f, 0.2f, 0.75f), MapOrigin.X, MapOrigin.Y, MapSize, MapSize);
	// Runway (east-west through the middle) as a landmark.
	const FVector2D R1 = WorldToMap(FVector2D(-1500.f, -48000.f));
	const FVector2D R2 = WorldToMap(FVector2D(1500.f, 48000.f));
	DrawRect(FLinearColor(0.2f, 0.2f, 0.2f, 0.9f), R1.X, FMath::Min(R1.Y, R2.Y), R2.X - R1.X, FMath::Max(2.f, FMath::Abs(R2.Y - R1.Y)));

	auto Circle = [this](const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness)
	{
		const int32 Segments = 48;
		FVector2D Prev;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float A = 2.f * PI * i / Segments;
			const FVector2D P = WorldToMap(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
			const FVector2D Clamped(FMath::Clamp(P.X, MapOrigin.X, MapOrigin.X + MapSize), FMath::Clamp(P.Y, MapOrigin.Y, MapOrigin.Y + MapSize));
			if (i > 0) DrawLine(Prev.X, Prev.Y, Clamped.X, Clamped.Y, Color, Thickness);
			Prev = Clamped;
		}
	};

	if (GM)
	{
		if (const AJXSafeZone* Zone = GM->GetSafeZone())
		{
			Circle(Zone->GetCenter(), Zone->GetRadius(), ZoneBlue, 2.f * S);
			Circle(Zone->GetTargetCenter(), Zone->GetTargetRadius(), White, 1.5f * S);
		}
		if (const AJXDropPlane* Plane = GM->GetPlane())
		{
			if (Plane->IsFlying())
			{
				const FVector2D A = WorldToMap(FVector2D(Plane->GetRouteStart()));
				const FVector2D B = WorldToMap(FVector2D(Plane->GetRouteEnd()));
				DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(1.f, 1.f, 1.f, 0.5f), 1.5f * S);
				const FVector2D P = WorldToMap(FVector2D(Plane->GetActorLocation()));
				DrawRect(Gold, P.X - 4.f * S, P.Y - 4.f * S, 8.f * S, 8.f * S);
			}
		}
	}

	// The player: dot + facing line.
	const AActor* Body = C->GetVehicle() ? static_cast<const AActor*>(C->GetVehicle()) : static_cast<const AActor*>(C);
	const FVector2D Me = WorldToMap(FVector2D(Body->GetActorLocation()));
	const FRotator View = C->GetControlRotation().IsNearlyZero() && C->GetVehicle() ? C->GetVehicle()->GetControlRotation() : C->GetControlRotation();
	const FVector Fwd = FRotator(0.f, View.Yaw, 0.f).Vector();
	const FVector2D Tip = Me + FVector2D(Fwd.Y, -Fwd.X) * 14.f * S;
	DrawLine(Me.X, Me.Y, Tip.X, Tip.Y, Gold, 2.f * S);
	DrawRect(Gold, Me.X - 4.f * S, Me.Y - 4.f * S, 8.f * S, 8.f * S);
}

void AJXHUD::DrawTopInfo(const AJXCharacter* C)
{
	const AJXGameMode* GM = GetWorld()->GetAuthGameMode<AJXGameMode>();
	UFont* Med = GEngine->GetMediumFont();
	const float X = MapOrigin.X;
	const float Y = MapOrigin.Y + MapSize + 8.f * S;
	if (GM)
	{
		DrawText(FString::Printf(TEXT("ALIVE %d    KILLS %d"), GM->GetAliveCount(), C->Kills), White, X, Y, Med, 1.f * S);
		if (const AJXSafeZone* Zone = GM->GetSafeZone())
		{
			const bool bOutside = !Zone->IsInside(C->GetVehicle() ? C->GetVehicle()->GetActorLocation() : C->GetActorLocation());
			DrawTextCentered(Zone->GetStatusText() + (bOutside ? TEXT("   -  YOU ARE OUTSIDE THE ZONE") : TEXT("")),
				Canvas->ClipX * 0.5f, 20.f * S, bOutside ? Red : White, Med, 1.1f * S);
		}
	}
}

void AJXHUD::DrawDropInfo(const AJXCharacter* C)
{
	FString Text;
	const bool bTouch = Cast<AJXPlayerController>(GetOwningPlayerController()) && Cast<AJXPlayerController>(GetOwningPlayerController())->bTouchControls;
	const FString Jump = bTouch ? TEXT("JUMP") : TEXT("SPACE");
	switch (C->GetDropState())
	{
	case EJXDropState::InPlane: Text = FString::Printf(TEXT("Press %s to jump out of the plane"), *Jump); break;
	case EJXDropState::Freefall: Text = FString::Printf(TEXT("Press %s to open the parachute"), *Jump); break;
	case EJXDropState::Parachute: Text = TEXT("Parachute open - steer with movement"); break;
	default: return;
	}
	if (C->GetDropState() != EJXDropState::InPlane)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(JXAltitude), false, C);
		const FVector From = C->GetActorLocation();
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, 200000.f), ECC_Visibility, Q))
		{
			Text += FString::Printf(TEXT("   |   %d m"), FMath::RoundToInt(Hit.Distance / 100.f));
		}
	}
	DrawTextCentered(Text, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.7f, Gold, GEngine->GetMediumFont(), 1.3f * S);
}

void AJXHUD::DrawProgress(const FString& Label, float Progress)
{
	const float W = 300.f * S, H = 14.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY * 0.66f;
	DrawBar(X, Y, W, H, Progress, FLinearColor(0.3f, 0.8f, 0.3f, 0.95f));
	DrawTextCentered(Label, Canvas->ClipX * 0.5f, Y - 26.f * S, White, GEngine->GetSmallFont(), 1.2f * S);
}

void AJXHUD::DrawKillFeed(const AJXPlayerController* PC)
{
	float Y = 30.f * S;
	for (const FJXKillFeedEntry& E : PC->KillFeed)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(E.Text, TW, TH, GEngine->GetSmallFont(), 1.2f * S);
		DrawRect(Dim, 26.f * S, Y - 2.f * S, TW + 12.f * S, TH + 4.f * S);
		DrawText(E.Text, White, 32.f * S, Y, GEngine->GetSmallFont(), 1.2f * S);
		Y += TH + 8.f * S;
	}
}

void AJXHUD::DrawCenterMessage(const AJXPlayerController* PC)
{
	if (PC->CenterTitle.IsEmpty()) return;
	const float Y = Canvas->ClipY * 0.3f;
	DrawTextCentered(PC->CenterTitle, Canvas->ClipX * 0.5f, Y, Gold, GEngine->GetLargeFont(), 2.2f * S);
	if (!PC->CenterSubtitle.IsEmpty())
	{
		DrawTextCentered(PC->CenterSubtitle, Canvas->ClipX * 0.5f, Y + 80.f * S, White, GEngine->GetMediumFont(), 1.4f * S);
	}
}

void AJXHUD::DrawTouchLabels(const AJXPlayerController* PC)
{
	if (!PC->bTouchControls) return;
	for (const FJXTouchLabel& L : PC->TouchLabels)
	{
		DrawTextCentered(L.Text, L.Center.X * Canvas->ClipX, L.Center.Y * Canvas->ClipY - 8.f * S, FLinearColor(1.f, 1.f, 1.f, 0.85f), GEngine->GetSmallFont(), 1.1f * S);
	}
}
