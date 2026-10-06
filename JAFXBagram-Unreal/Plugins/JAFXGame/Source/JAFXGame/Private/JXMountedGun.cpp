#include "JXMountedGun.h"
#include "JXCharacter.h"
#include "JXCombatLibrary.h"
#include "JXHealthComponent.h"
#include "JXSettings.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AJXMountedGun::AJXMountedGun()
{
	VehicleName = TEXT("DShK");
	bHideOccupant = false;

	Base = CreateDefaultSubobject<UBoxComponent>(TEXT("Base"));
	Base->SetBoxExtent(FVector(40.f, 40.f, 50.f));
	Base->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = Base;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Tripod = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tripod"));
	Tripod->SetupAttachment(Base);
	Tripod->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cylinder.Succeeded()) Tripod->SetStaticMesh(Cylinder.Object);
	Tripod->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.f));

	YawPivot = CreateDefaultSubobject<USceneComponent>(TEXT("YawPivot"));
	YawPivot->SetupAttachment(Base);
	YawPivot->SetRelativeLocation(FVector(0.f, 0.f, 60.f));

	PitchPivot = CreateDefaultSubobject<USceneComponent>(TEXT("PitchPivot"));
	PitchPivot->SetupAttachment(YawPivot);

	Barrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
	Barrel->SetupAttachment(PitchPivot);
	Barrel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cube.Succeeded()) Barrel->SetStaticMesh(Cube.Object);
	Barrel->SetRelativeLocation(FVector(50.f, 0.f, 0.f));
	Barrel->SetRelativeScale3D(FVector(1.6f, 0.14f, 0.2f));

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(PitchPivot);
	Muzzle->SetRelativeLocation(FVector(135.f, 0.f, 0.f));

	// Gunner stands behind the gun.
	Seat->SetupAttachment(YawPivot);
	Seat->SetRelativeLocation(FVector(-110.f, 0.f, 36.f));

	Boom->SetupAttachment(Base);
	Boom->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Boom->TargetArmLength = 260.f;
	Boom->SocketOffset = FVector(0.f, 40.f, 40.f);

	UseArea->SetupAttachment(Base);
	UseArea->SetSphereRadius(220.f);

	// Mounted guns cannot be destroyed.
	Health->MaxHealth = 1.0e9f;
	Health->BulletMultiplier = 0.f;
	Health->ExplosiveMultiplier = 0.f;
}

FVector AJXMountedGun::GetExitLocation() const
{
	return Seat->GetComponentLocation() - YawPivot->GetForwardVector() * 60.f + FVector(0.f, 0.f, 50.f);
}

FString AJXMountedGun::GetStatusText() const
{
	if (bOverheated) return TEXT("DShK OVERHEATED");
	return FString::Printf(TEXT("DShK  heat %d%%"), FMath::RoundToInt(Heat * 100.f));
}

void AJXMountedGun::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Heat = FMath::Max(0.f, Heat - CoolPerSecond * DeltaSeconds);
	if (bOverheated && Heat < 0.3f) bOverheated = false;

	if (AController* C = GetController())
	{
		const FRotator Aim = C->GetControlRotation();
		YawPivot->SetWorldRotation(FRotator(0.f, Aim.Yaw, 0.f));
		const float Pitch = FMath::ClampAngle(FRotator::NormalizeAxis(Aim.Pitch), -15.f, 45.f);
		PitchPivot->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
	}

	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, bZoom ? 40.f : 90.f, DeltaSeconds, 10.f));
}

void AJXMountedGun::StartFire()
{
	bFiring = true;
	if (!GetWorldTimerManager().IsTimerActive(FireTimer))
	{
		FireOnce();
	}
}

void AJXMountedGun::StopFire()
{
	bFiring = false;
	GetWorldTimerManager().ClearTimer(FireTimer);
}

void AJXMountedGun::FireOnce()
{
	AController* C = GetController();
	if (!bFiring || !C || bOverheated) return;

	FVector ViewLoc;
	FRotator ViewRot;
	C->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector Dir = ViewRot.Vector();
	const float Skip = FMath::Max(0.f, FVector::DotProduct(Muzzle->GetComponentLocation() - ViewLoc, Dir));

	FJXShotParams P;
	P.Causer = this;
	P.Instigator = C;
	P.AimStart = ViewLoc + Dir * Skip;
	P.AimDir = Dir;
	P.Muzzle = Muzzle->GetComponentLocation();
	P.Range = Range;
	P.Damage = Damage;
	P.HeadMultiplier = 2.f;
	P.SpreadDegrees = SpreadDegrees * (bZoom ? 0.6f : 1.f);
	P.Ignore.Add(GetOccupant());
	const FJXShotResult R = UJXCombatLibrary::FireHitscan(GetWorld(), P);
	if (R.PawnHits > 0)
	{
		if (AJXCharacter* Gunner = GetOccupant()) Gunner->NotifyHitTarget(R.bHeadshot);
	}

	UJXCombatLibrary::PlaySoundAt(this, FireSound.IsNull() ? UJXSettings::Get()->DefaultFireSound : FireSound, P.Muzzle);

	Heat += HeatPerShot;
	if (Heat >= 1.f)
	{
		Heat = 1.f;
		bOverheated = true;
		return;
	}
	GetWorldTimerManager().SetTimer(FireTimer, this, &AJXMountedGun::FireOnce, 60.f / FMath::Max(RPM, 1.f), false);
}
