#include "JXSafeZone.h"
#include "JXCharacter.h"
#include "JXSettings.h"
#include "JXTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AJXSafeZone::AJXSafeZone()
{
	PrimaryActorTick.bCanEverTick = true;

	Wall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wall"));
	RootComponent = Wall;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) Wall->SetStaticMesh(Cyl.Object);
	Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wall->SetCastShadow(false);

	// Timings tuned for a ~1.4 km map and 25 players (about 15 minutes per match).
	auto Add = [this](float Wait, float Shrink, float Fraction, float Dps)
	{
		FJXZonePhase P;
		P.WaitTime = Wait;
		P.ShrinkTime = Shrink;
		P.RadiusFraction = Fraction;
		P.DamagePerSecond = Dps;
		Phases.Add(P);
	};
	Add(120.f, 90.f, 0.55f, 0.6f);
	Add(90.f, 70.f, 0.32f, 1.f);
	Add(75.f, 55.f, 0.18f, 2.f);
	Add(60.f, 45.f, 0.09f, 4.f);
	Add(45.f, 35.f, 0.04f, 7.f);
	Add(30.f, 30.f, 0.0f, 10.f);
}

void AJXSafeZone::BeginPlay()
{
	Super::BeginPlay();
	// The wall is drawn from outside only (single-sided cylinder), like a curtain around the circle.
	UMaterialInterface* Mat = UJXSettings::Get()->ZoneMaterial.LoadSynchronous();
	if (Mat)
	{
		Wall->SetMaterial(0, Mat);
	}
	else if (UMaterialInterface* Base = Wall->GetMaterial(0))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.25f, 0.9f));
		Wall->SetMaterial(0, MID);
	}
	Wall->SetVisibility(false);
}

void AJXSafeZone::StartZone(const FVector2D& Center, float Radius)
{
	StartRadius = Radius;
	CurCenter = TargetCenter = FromCenter = Center;
	CurRadius = TargetRadius = FromRadius = Radius;
	bStarted = true;
	Wall->SetVisibility(true);
	BeginPhase(0);
	GetWorldTimerManager().SetTimer(DamageTimer, this, &AJXSafeZone::DamageOutside, 1.f, true);
	UpdateWall();
}

void AJXSafeZone::BeginPhase(int32 Index)
{
	PhaseIndex = Index;
	bShrinking = false;
	PhaseTimer = 0.f;
	if (!Phases.IsValidIndex(Index)) return;

	FromCenter = CurCenter;
	FromRadius = CurRadius;
	TargetRadius = StartRadius * Phases[Index].RadiusFraction;
	// The next circle always lies fully inside the current one.
	const float MaxOffset = FMath::Max(0.f, CurRadius - TargetRadius);
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Dist = FMath::Sqrt(FMath::FRand()) * MaxOffset;
	TargetCenter = CurCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Dist;
}

void AJXSafeZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bStarted || !Phases.IsValidIndex(PhaseIndex)) return;

	const FJXZonePhase& P = Phases[PhaseIndex];
	PhaseTimer += DeltaSeconds;
	if (!bShrinking && PhaseTimer >= P.WaitTime)
	{
		bShrinking = true;
		PhaseTimer = 0.f;
		DamagePerSecond = P.DamagePerSecond;
	}
	if (bShrinking)
	{
		const float Alpha = P.ShrinkTime > 0.f ? FMath::Clamp(PhaseTimer / P.ShrinkTime, 0.f, 1.f) : 1.f;
		CurCenter = FMath::Lerp(FromCenter, TargetCenter, Alpha);
		CurRadius = FMath::Lerp(FromRadius, TargetRadius, Alpha);
		UpdateWall();
		if (Alpha >= 1.f) BeginPhase(PhaseIndex + 1);
	}
}

void AJXSafeZone::UpdateWall()
{
	// Basic cylinder is 100 x 100 x 100 uu; make it 3 km tall.
	const float Diameter = FMath::Max(CurRadius * 2.f, 1.f);
	Wall->SetWorldLocation(FVector(CurCenter.X, CurCenter.Y, 50000.f));
	Wall->SetWorldScale3D(FVector(Diameter / 100.f, Diameter / 100.f, 3000.f));
}

bool AJXSafeZone::IsInside(const FVector& Location) const
{
	return FVector2D::Distance(FVector2D(Location.X, Location.Y), CurCenter) <= CurRadius;
}

FString AJXSafeZone::GetStatusText() const
{
	if (!bStarted) return FString();
	if (!Phases.IsValidIndex(PhaseIndex)) return TEXT("Final zone");
	if (bShrinking) return TEXT("ZONE SHRINKING!");
	const int32 Left = FMath::CeilToInt(Phases[PhaseIndex].WaitTime - PhaseTimer);
	return FString::Printf(TEXT("Zone shrinks in %d:%02d"), Left / 60, Left % 60);
}

void AJXSafeZone::DamageOutside()
{
	for (TActorIterator<AJXCharacter> It(GetWorld()); It; ++It)
	{
		AJXCharacter* C = *It;
		if (!C->IsAlive() || C->GetDropState() == EJXDropState::InPlane) continue;
		const FVector Loc = C->GetVehicle() ? C->GetVehicle()->GetActorLocation() : C->GetActorLocation();
		if (!IsInside(Loc))
		{
			C->TakeDamage(DamagePerSecond, FDamageEvent(UJXZoneDamageType::StaticClass()), nullptr, this);
		}
	}
}
