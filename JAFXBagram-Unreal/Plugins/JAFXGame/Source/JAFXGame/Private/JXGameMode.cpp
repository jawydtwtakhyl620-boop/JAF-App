#include "JXGameMode.h"
#include "JXBagramMap.h"
#include "JXBotController.h"
#include "JXCharacter.h"
#include "JXDropPlane.h"
#include "JXHUD.h"
#include "JXPlayerController.h"
#include "JXSafeZone.h"
#include "JXSettings.h"
#include "JXWeaponLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* BotNames[] = {
		TEXT("Shahin"), TEXT("Toofan"), TEXT("Arash"), TEXT("Sepehr"), TEXT("Kaveh"), TEXT("Babak"), TEXT("Nima"),
		TEXT("Omid"), TEXT("Parsa"), TEXT("Ramin"), TEXT("Saman"), TEXT("Taha"), TEXT("Yasin"), TEXT("Zaman"),
		TEXT("Hamid"), TEXT("Farid"), TEXT("Jalal"), TEXT("Karim"), TEXT("Mehdi"), TEXT("Navid"), TEXT("Rostam"),
		TEXT("Sohrab"), TEXT("Wahid"), TEXT("Behzad"), TEXT("Dariush"), TEXT("Elyas"), TEXT("Haris"), TEXT("Idris"),
		TEXT("Mirwais"), TEXT("Ahmad"), TEXT("Bilal"), TEXT("Daud"), TEXT("Fawad"), TEXT("Ghani"), TEXT("Hekmat")
	};

	FString NameOf(const AController* C, const AJXCharacter* Pawn)
	{
		if (C && C->PlayerState && !C->PlayerState->GetPlayerName().IsEmpty() && C->IsPlayerController())
		{
			return C->PlayerState->GetPlayerName();
		}
		if (Pawn && Pawn->Tags.Num() > 0) return Pawn->Tags[0].ToString();
		return TEXT("Soldier");
	}
}

bool AJXGameMode::bSkipMenuOnce = false;

AJXGameMode::AJXGameMode()
{
	DefaultPawnClass = AJXCharacter::StaticClass();
	PlayerControllerClass = AJXPlayerController::StaticClass();
	HUDClass = AJXHUD::StaticClass();
}

float AJXGameMode::GetMapHalfSize() const
{
	return UJXSettings::Get()->MapHalfSize;
}

void AJXGameMode::StartPlay()
{
	if (bAutoSpawnBagramMap)
	{
		TActorIterator<AJXBagramMap> Existing(GetWorld());
		if (!Existing)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<AJXBagramMap>(AJXBagramMap::StaticClass(), FTransform::Identity, P);
		}
	}
	// Decide before Super::StartPlay so the player controller sees it in its BeginPlay.
	bWaitingForMenu = bShowMainMenu && !bSkipMenuOnce;
	bSkipMenuOnce = false;
	Super::StartPlay();
	if (!bWaitingForMenu)
	{
		GetWorldTimerManager().SetTimer(StartTimer, this, &AJXGameMode::BeginMatch, FMath::Max(StartDelay, 0.1f), false);
	}
}

void AJXGameMode::RequestBeginMatch()
{
	if (!bWaitingForMenu) return;
	bWaitingForMenu = false;
	GetWorldTimerManager().SetTimer(StartTimer, this, &AJXGameMode::BeginMatch, 0.5f, false);
}

AJXCharacter* AJXGameMode::SpawnBot(int32 Index, const FVector& Location)
{
	const UJXSettings* S = UJXSettings::Get();
	UClass* Class = S->BotClass.LoadSynchronous();
	if (!Class) Class = DefaultPawnClass ? DefaultPawnClass.Get() : AJXCharacter::StaticClass();
	if (!Class->IsChildOf(AJXCharacter::StaticClass())) Class = AJXCharacter::StaticClass();

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AJXCharacter* Bot = GetWorld()->SpawnActor<AJXCharacter>(Class, Location, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), P);
	if (!Bot) return nullptr;

	Bot->Tags.Add(FName(BotNames[Index % UE_ARRAY_COUNT(BotNames)]));
	Bot->AIControllerClass = AJXBotController::StaticClass();
	Bot->SpawnDefaultController();

	// Bots start with a random gun so the first fights happen quickly.
	FRandomStream Rng(FMath::Rand());
	FName WeaponId = UJXWeaponLibrary::RandomWeaponId(Rng);
	if (WeaponId == TEXT("RPG7J") || WeaponId == TEXT("AWM")) WeaponId = TEXT("M4A");
	if (const FJXWeaponDef* D = UJXWeaponLibrary::Find(WeaponId))
	{
		Bot->GiveWeapon(WeaponId, D->MagSize);
		Bot->AddAmmo(D->Ammo, D->MagSize * 3);
	}
	FJXItem Bandage;
	Bandage.Type = EJXItemType::Bandage;
	Bandage.Amount = 3;
	Bot->PickUp(Bandage);
	return Bot;
}

FVector AJXGameMode::RandomGroundPoint(const FVector& Center, float Radius) const
{
	if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Point;
		if (Nav->GetRandomReachablePointInRadius(Center, Radius, Point))
		{
			return Point.Location + FVector(0.f, 0.f, 100.f);
		}
	}
	return Center + FVector(FMath::FRandRange(-Radius, Radius), FMath::FRandRange(-Radius, Radius), 300.f);
}

void AJXGameMode::BeginMatch()
{
	if (bMatchStarted) return;
	bMatchStarted = true;

	UWorld* World = GetWorld();
	const UJXSettings* S = UJXSettings::Get();
	const float Half = S->MapHalfSize;

	// Humans.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AJXCharacter* C = Cast<AJXCharacter>(It->Get() ? It->Get()->GetPawn() : nullptr))
		{
			Participants.Add(C);
			// A pistol so new players can defend themselves after landing.
			C->GiveWeapon(TEXT("P92"), 15);
			C->AddAmmo(EJXAmmoType::A9mm, 15);
		}
	}

	// Bots.
	for (int32 i = 0; i < S->BotCount; ++i)
	{
		const FVector Spot = S->bUseDropPlane ? FVector(0.f, 0.f, S->PlaneAltitude + 5000.f + i * 300.f) : RandomGroundPoint(FVector::ZeroVector, Half * 0.9f);
		if (AJXCharacter* Bot = SpawnBot(i, Spot))
		{
			Participants.Add(Bot);
		}
	}

	// Safe zone around a random point near the middle, big enough to cover the whole map.
	FActorSpawnParameters ZP;
	ZP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Zone = World->SpawnActor<AJXSafeZone>(AJXSafeZone::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, ZP);
	if (Zone)
	{
		const FVector2D Center(FMath::FRandRange(-Half, Half) * 0.25f, FMath::FRandRange(-Half, Half) * 0.25f);
		Zone->StartZone(Center, Half * 1.45f);
	}

	if (!S->bUseDropPlane)
	{
		for (AJXCharacter* C : Participants)
		{
			if (C && C->IsPlayerControlled())
			{
				C->SetActorLocation(RandomGroundPoint(FVector::ZeroVector, Half * 0.8f), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
		return;
	}

	// Plane route: a random straight line across the map.
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
	const FVector Side(-Dir.Y, Dir.X, 0.f);
	const FVector Offset = Side * FMath::FRandRange(-Half, Half) * 0.4f;
	const FVector From = Offset - Dir * Half * 1.3f + FVector(0.f, 0.f, S->PlaneAltitude);
	const FVector To = Offset + Dir * Half * 1.3f + FVector(0.f, 0.f, S->PlaneAltitude);

	UClass* PlaneClass = S->PlaneClass.LoadSynchronous();
	if (!PlaneClass) PlaneClass = AJXDropPlane::StaticClass();
	Plane = World->SpawnActor<AJXDropPlane>(PlaneClass, From, Dir.Rotation(), ZP);
	if (!Plane) return;

	for (AJXCharacter* C : Participants)
	{
		if (C) C->EnterPlane(Plane);
	}
	Plane->StartFlight(From, To);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AJXPlayerController* PC = Cast<AJXPlayerController>(It->Get()))
		{
			PC->ShowCenterMessage(TEXT("JAF X BAGRAM"), TEXT("Press JUMP to leave the plane"));
			FTimerHandle Clear;
			TWeakObjectPtr<AJXPlayerController> Weak(PC);
			World->GetTimerManager().SetTimer(Clear, [Weak]()
			{
				if (Weak.IsValid()) Weak->ShowCenterMessage(FString(), FString());
			}, 5.f, false);
		}
	}
}

int32 AJXGameMode::GetAliveCount() const
{
	int32 N = 0;
	for (const AJXCharacter* C : Participants)
	{
		if (C && C->IsAlive()) ++N;
	}
	return N;
}

void AJXGameMode::OnCharacterDied(AJXCharacter* Victim, AController* Killer)
{
	if (!Victim) return;
	AJXCharacter* KillerPawn = Killer ? Cast<AJXCharacter>(Killer->GetPawn()) : nullptr;
	if (KillerPawn && KillerPawn != Victim) KillerPawn->Kills++;

	const int32 Alive = GetAliveCount();
	const FString VictimName = NameOf(Victim->GetController(), Victim);
	const FString Line = KillerPawn && KillerPawn != Victim
		? FString::Printf(TEXT("%s  >  %s"), *NameOf(Killer, KillerPawn), *VictimName)
		: FString::Printf(TEXT("%s died"), *VictimName);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AJXPlayerController* PC = Cast<AJXPlayerController>(It->Get()))
		{
			PC->AddKillFeed(Line);
			if (PC->GetPawn() == Victim && !bMatchOver)
			{
				PC->ShowCenterMessage(TEXT("YOU DIED"), FString::Printf(TEXT("Rank #%d   Kills: %d"), Alive + 1, Victim->Kills));
			}
		}
	}

	if (bMatchOver) return;

	// Single-player: restart once the human is dead.
	bool bAnyHumanAlive = false;
	AJXCharacter* LastAlive = nullptr;
	for (AJXCharacter* C : Participants)
	{
		if (!C || !C->IsAlive()) continue;
		LastAlive = C;
		if (C->IsPlayerControlled() || (C->GetVehicle() && C->GetVehicle()->IsPlayerControlled())) bAnyHumanAlive = true;
	}
	if (Alive <= 1)
	{
		EndMatch(LastAlive);
	}
	else if (!bAnyHumanAlive)
	{
		bMatchOver = true;
		GetWorldTimerManager().SetTimer(RestartTimer, this, &AJXGameMode::RestartMatch, 8.f, false);
	}
}

void AJXGameMode::EndMatch(AJXCharacter* Winner)
{
	bMatchOver = true;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AJXPlayerController* PC = Cast<AJXPlayerController>(It->Get());
		if (!PC) continue;
		const bool bWon = Winner && (PC->GetPawn() == Winner || (Winner->GetVehicle() && PC->GetPawn() == Winner->GetVehicle()));
		if (bWon)
		{
			PC->ShowCenterMessage(TEXT("CHAMPION OF BAGRAM!"), FString::Printf(TEXT("#1   Kills: %d"), Winner->Kills));
		}
		else if (PC->CenterTitle.IsEmpty())
		{
			PC->ShowCenterMessage(TEXT("MATCH OVER"), Winner ? FString::Printf(TEXT("Winner: %s"), *NameOf(Winner->GetController(), Winner)) : FString());
		}
	}
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AJXGameMode::RestartMatch, RestartDelay, false);
}

void AJXGameMode::RestartMatch()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
