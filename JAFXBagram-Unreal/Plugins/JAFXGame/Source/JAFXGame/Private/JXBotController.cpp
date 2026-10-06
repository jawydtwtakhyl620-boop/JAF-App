#include "JXBotController.h"
#include "JXCharacter.h"
#include "JXGameMode.h"
#include "JXSafeZone.h"
#include "JXWeapon.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

AJXBotController::AJXBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
}

void AJXBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AJXCharacter* Bot = GetBot())
	{
		Bot->BotSpreadMultiplier = AimError;
	}
	ThinkTimer = FMath::FRandRange(0.f, 0.5f);
}

AJXCharacter* AJXBotController::GetBot() const
{
	return Cast<AJXCharacter>(GetPawn());
}

bool AJXBotController::CanSee(const AActor* Other) const
{
	const AJXCharacter* Bot = GetBot();
	if (!Bot || !Other) return false;
	const FVector Eye = Bot->GetPawnViewLocation();
	const FVector To = Other->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	if (FVector::DistSquared(Eye, To) > FMath::Square(SightRange)) return false;

	// 140 degree field of view, but always notice someone very close.
	const FVector Dir = (To - Eye).GetSafeNormal();
	const bool bClose = FVector::DistSquared(Eye, To) < FMath::Square(1500.f);
	if (!bClose && FVector::DotProduct(Bot->GetActorForwardVector(), Dir) < 0.34f) return false;

	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(JXBotSight), false, Bot);
	Q.AddIgnoredActor(Other);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, To, ECC_Visibility, Q);
}

AJXCharacter* AJXBotController::FindTarget() const
{
	const AJXCharacter* Bot = GetBot();
	AJXCharacter* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AJXCharacter> It(GetWorld()); It; ++It)
	{
		AJXCharacter* C = *It;
		if (C == Bot || !C->IsAlive() || C->GetDropState() == EJXDropState::InPlane) continue;
		const float D = FVector::DistSquared(C->GetActorLocation(), Bot->GetActorLocation());
		if (D < BestDist && CanSee(C))
		{
			BestDist = D;
			Best = C;
		}
	}
	return Best;
}

void AJXBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AJXCharacter* Bot = GetBot();
	if (!Bot || !Bot->IsAlive()) return;
	if (Bot->GetDropState() != EJXDropState::OnGround) return; // still in the plane or falling

	ThinkTimer -= DeltaSeconds;
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = 0.3f;
		AJXCharacter* Seen = FindTarget();
		// Someone shot us (the character sets our focus): fight back even if not visible yet.
		if (!Seen)
		{
			if (AJXCharacter* Attacker = Cast<AJXCharacter>(GetFocusActor()))
			{
				if (Attacker->IsAlive() && Attacker != Target.Get())
				{
					LastKnownLocation = Attacker->GetActorLocation();
					bHasLastKnown = true;
				}
			}
		}
		if (Seen != Target.Get()) SeenTime = 0.f;
		Target = Seen;
		if (Seen)
		{
			LastKnownLocation = Seen->GetActorLocation();
			bHasLastKnown = true;
		}
	}

	if (Target.IsValid())
	{
		Fight(DeltaSeconds);
	}
	else
	{
		Bot->StopFire();
		Bot->SetForceCombatStance(false);
		ClearFocus(EAIFocusPriority::Gameplay);
		Roam();
	}
}

void AJXBotController::Fight(float DeltaSeconds)
{
	AJXCharacter* Bot = GetBot();
	AJXCharacter* T = Target.Get();
	SetFocus(T);
	Bot->SetForceCombatStance(true);
	SeenTime += DeltaSeconds;

	AJXWeapon* W = Bot->GetCurrentWeapon();
	if (!W)
	{
		// Unarmed: run away from the enemy.
		Bot->StopFire();
		const FVector Away = Bot->GetActorLocation() + (Bot->GetActorLocation() - T->GetActorLocation()).GetSafeNormal2D() * 2000.f;
		MoveToLocation(Away, 100.f);
		return;
	}
	if (W->GetMag() <= 0 && !W->IsReloading())
	{
		Bot->StopFire();
		if (!W->StartReload())
		{
			Bot->NextWeapon();
		}
		return;
	}

	const float Dist = FVector::Dist(Bot->GetActorLocation(), T->GetActorLocation());
	if (Dist > W->GetDef().Range)
	{
		Bot->StopFire();
		MoveToActor(T, W->GetDef().Range * 0.6f);
		return;
	}

	// Shoot in bursts after a reaction delay.
	BurstTimer -= DeltaSeconds;
	if (SeenTime > ReactionTime && BurstTimer <= 0.f)
	{
		Bot->StartFire();
		BurstTimer = W->GetDef().bAutomatic ? FMath::FRandRange(0.25f, 0.6f) : 0.05f;
	}
	else if (BurstTimer < -0.4f || !W->GetDef().bAutomatic)
	{
		Bot->StopFire();
		if (BurstTimer < -0.4f) BurstTimer = FMath::FRandRange(0.f, 0.3f);
	}

	// Strafe sideways while fighting so bots are harder to hit.
	StrafeTimer -= DeltaSeconds;
	if (StrafeTimer <= 0.f)
	{
		StrafeTimer = FMath::FRandRange(0.8f, 2.f);
		const FVector Side = FVector::CrossProduct((T->GetActorLocation() - Bot->GetActorLocation()).GetSafeNormal2D(), FVector::UpVector);
		const float Want = W->GetDef().Category == EJXWeaponCategory::Shotgun ? 600.f : 2000.f;
		FVector Goal = Bot->GetActorLocation() + Side * FMath::FRandRange(-500.f, 500.f);
		if (Dist > Want * 1.5f) Goal += (T->GetActorLocation() - Bot->GetActorLocation()).GetSafeNormal2D() * 600.f;
		MoveToLocation(Goal, 50.f, true, true, false, true);
	}
}

void AJXBotController::Roam()
{
	AJXCharacter* Bot = GetBot();
	const bool bIdle = GetMoveStatus() == EPathFollowingStatus::Idle;
	IdleTimer += GetWorld()->GetDeltaSeconds();

	// Stay inside the next safe zone.
	if (AJXGameMode* GM = GetWorld()->GetAuthGameMode<AJXGameMode>())
	{
		if (AJXSafeZone* Zone = GM->GetSafeZone())
		{
			const FVector2D C = Zone->GetTargetCenter();
			const float R = Zone->GetTargetRadius();
			const FVector2D P(Bot->GetActorLocation().X, Bot->GetActorLocation().Y);
			if (R > 0.f && FVector2D::Distance(P, C) > R * 0.85f)
			{
				if (bIdle || IdleTimer > 4.f)
				{
					IdleTimer = 0.f;
					bHasLastKnown = false;
					MoveToRandomPoint(FVector(C.X, C.Y, Bot->GetActorLocation().Z), R * 0.5f);
				}
				return;
			}
		}
	}

	if (bHasLastKnown)
	{
		bHasLastKnown = false;
		MoveToLocation(LastKnownLocation, 200.f);
		return;
	}

	if (bIdle && IdleTimer > FMath::FRandRange(0.5f, 3.f))
	{
		IdleTimer = 0.f;
		MoveToRandomPoint(Bot->GetActorLocation(), 6000.f);
	}
}

bool AJXBotController::MoveToRandomPoint(const FVector& Origin, float Radius)
{
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Point;
	if (Nav && Nav->GetRandomReachablePointInRadius(Origin, Radius, Point))
	{
		return MoveToLocation(Point.Location, 150.f) != EPathFollowingRequestResult::Failed;
	}
	// No navigation mesh: walk straight there.
	const FVector Goal = Origin + FVector(FMath::FRandRange(-Radius, Radius), FMath::FRandRange(-Radius, Radius), 0.f);
	return MoveToLocation(Goal, 150.f, true, false) != EPathFollowingRequestResult::Failed;
}
