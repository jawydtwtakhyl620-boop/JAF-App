#include "JXTank.h"
#include "JXCharacter.h"
#include "JXCombatLibrary.h"
#include "JXHealthComponent.h"
#include "JXProjectile.h"
#include "JXSettings.h"
#include "JXTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, FVector Location, FVector SizeMeters)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Mesh) C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Location);
		C->SetRelativeScale3D(SizeMeters); // basic shapes are 1 m
		return C;
	}
}

AJXTank::AJXTank()
{
	VehicleName = TEXT("Tank");
	bHideOccupant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeF.Succeeded() ? CubeF.Object : nullptr;
	UStaticMesh* Cyl = CylF.Succeeded() ? CylF.Object : nullptr;

	// The collision box floats 40 cm above the ground so the tank drives over curbs and the runway edge.
	// Its centre sits GroundClearance + 90 cm above the ground; the visible parts reach down to the ground.
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetBoxExtent(FVector(350.f, 180.f, 90.f));
	Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = Body;

	Hull = MakePart(this, Body, TEXT("Hull"), Cube, FVector(0.f, 0.f, -40.f), FVector(6.6f, 3.0f, 1.3f));
	TrackLeft = MakePart(this, Body, TEXT("TrackLeft"), Cube, FVector(0.f, -165.f, -80.f), FVector(7.0f, 0.6f, 1.0f));
	TrackRight = MakePart(this, Body, TEXT("TrackRight"), Cube, FVector(0.f, 165.f, -80.f), FVector(7.0f, 0.6f, 1.0f));

	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(Body);
	TurretPivot->SetRelativeLocation(FVector(-30.f, 0.f, 25.f));
	Turret = MakePart(this, TurretPivot, TEXT("Turret"), Cyl, FVector(0.f, 0.f, 35.f), FVector(2.8f, 2.8f, 0.8f));

	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(TurretPivot);
	GunPivot->SetRelativeLocation(FVector(130.f, 0.f, 40.f));
	Cannon = MakePart(this, GunPivot, TEXT("Cannon"), Cube, FVector(200.f, 0.f, 0.f), FVector(4.0f, 0.22f, 0.22f));

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(GunPivot);
	Muzzle->SetRelativeLocation(FVector(410.f, 0.f, 0.f));

	Seat->SetupAttachment(Body);
	Boom->SetupAttachment(TurretPivot);
	Boom->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Boom->TargetArmLength = 900.f;
	Boom->SocketOffset = FVector(0.f, 0.f, 120.f);
	UseArea->SetupAttachment(Body);
	UseArea->SetSphereRadius(480.f);

	// Bullets barely scratch it; 4 rockets or about 5 grenades destroy it.
	Health->MaxHealth = 1500.f;
	Health->BulletMultiplier = 0.02f;
	Health->ExplosiveMultiplier = 3.f;
}

void AJXTank::BeginPlay()
{
	Super::BeginPlay();
	// Desert camouflage for the placeholder shapes (ignored once real meshes are assigned).
	const FLinearColor Sand(0.42f, 0.36f, 0.22f);
	const FLinearColor Dark(0.12f, 0.12f, 0.1f);
	for (UStaticMeshComponent* Part : { Hull.Get(), Turret.Get(), Cannon.Get(), TrackLeft.Get(), TrackRight.Get() })
	{
		if (!Part) continue;
		if (UMaterialInterface* Base = Part->GetMaterial(0))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), (Part == TrackLeft.Get() || Part == TrackRight.Get()) ? Dark : Sand);
			Part->SetMaterial(0, MID);
		}
	}
}

FString AJXTank::GetStatusText() const
{
	const float Left = CannonReload - (GetWorld()->GetTimeSeconds() - LastCannonTime);
	const FString Weapon = bUseMachineGun ? TEXT("Machine gun") : (Left > 0.f ? FString::Printf(TEXT("Cannon reloading %.1fs"), Left) : TEXT("Cannon READY"));
	return FString::Printf(TEXT("TANK  armor %d  |  %s  |  %d km/h"),
		FMath::RoundToInt(Health->GetHealth()), *Weapon, FMath::RoundToInt(FMath::Abs(Speed) * 0.036f));
}

void AJXTank::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsDestroyed()) return;
	Drive(DeltaSeconds);
	AimTurret(DeltaSeconds);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, bZoom ? 35.f : 90.f, DeltaSeconds, 10.f));
}

void AJXTank::Drive(float DeltaSeconds)
{
	const float Throttle = GetController() ? MoveInput.Y : 0.f;
	const float Steer = GetController() ? MoveInput.X : 0.f;

	const float Target = Throttle >= 0.f ? Throttle * MaxSpeed : Throttle * ReverseSpeed;
	Speed = FMath::FInterpConstantTo(Speed, Target, DeltaSeconds, Acceleration);

	if (FMath::Abs(Steer) > 0.05f)
	{
		// Tracks let it turn on the spot; reversing flips steering like a real tank.
		const float Dir = Speed < -10.f ? -1.f : 1.f;
		AddActorWorldRotation(FRotator(0.f, Steer * TurnRate * Dir * DeltaSeconds, 0.f));
	}

	if (FMath::Abs(Speed) > 1.f)
	{
		const FVector Delta = GetActorForwardVector().GetSafeNormal2D() * Speed * DeltaSeconds;
		FHitResult Hit;
		AddActorWorldOffset(Delta, true, &Hit);
		if (Hit.bBlockingHit)
		{
			AJXCharacter* Victim = Cast<AJXCharacter>(Hit.GetActor());
			if (Victim && Victim->IsAlive() && FMath::Abs(Speed) > 250.f)
			{
				Victim->TakeDamage(RunOverDamage, FDamageEvent(UJXExplosiveDamageType::StaticClass()), GetController(), this);
			}
			else
			{
				Speed *= 0.3f;
			}
		}
	}
	SnapToGround();
}

void AJXTank::SnapToGround()
{
	UWorld* World = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(JXTankGround), false, this);
	if (AJXCharacter* C = GetOccupant()) Q.AddIgnoredActor(C);

	auto GroundZ = [&](const FVector& P, float& OutZ)
	{
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 400.f), P - FVector(0, 0, 3000.f), ECC_Visibility, Q))
		{
			OutZ = Hit.ImpactPoint.Z;
			return true;
		}
		return false;
	};

	const FVector Loc = GetActorLocation();
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	float Front = 0.f, Back = 0.f;
	const bool bFront = GroundZ(Loc + Fwd * 300.f, Front);
	const bool bBack = GroundZ(Loc - Fwd * 300.f, Back);
	if (!bFront || !bBack) return;

	const float GroundClearance = 40.f;
	const float Z = (Front + Back) * 0.5f + Body->GetScaledBoxExtent().Z + GroundClearance;
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Front - Back, 600.f));
	SetActorLocation(FVector(Loc.X, Loc.Y, FMath::FInterpTo(Loc.Z, Z, GetWorld()->GetDeltaSeconds(), 12.f)));
	SetActorRotation(FRotator(FMath::Clamp(Pitch, -25.f, 25.f), GetActorRotation().Yaw, 0.f));
}

void AJXTank::AimTurret(float DeltaSeconds)
{
	AController* C = GetController();
	if (!C) return;
	const FRotator Aim = C->GetControlRotation();
	const FRotator Current = TurretPivot->GetComponentRotation();
	const float NewYaw = FMath::FixedTurn(Current.Yaw, Aim.Yaw, TurretTurnRate * DeltaSeconds);
	TurretPivot->SetWorldRotation(FRotator(GetActorRotation().Pitch, NewYaw, 0.f));
	const float Pitch = FMath::ClampAngle(FRotator::NormalizeAxis(Aim.Pitch), -8.f, 20.f);
	GunPivot->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
}

void AJXTank::StartFire()
{
	if (IsDestroyed() || !GetController()) return;
	if (bUseMachineGun)
	{
		bFiringMG = true;
		if (!GetWorldTimerManager().IsTimerActive(MGTimer)) FireMachineGun();
	}
	else
	{
		FireCannon();
	}
}

void AJXTank::StopFire()
{
	bFiringMG = false;
	GetWorldTimerManager().ClearTimer(MGTimer);
}

void AJXTank::FireCannon()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastCannonTime < CannonReload) return;
	LastCannonTime = Now;

	const FVector From = Muzzle->GetComponentLocation();
	const FRotator Dir = Muzzle->GetComponentRotation();
	AJXProjectile* Shell = AJXProjectile::Launch(GetWorld(), AJXProjectile::StaticClass(), From, Dir, ShellSpeed, 0.15f, false,
		EJXExplosiveKind::Explosive, CannonDamage, CannonRadius, 0.f, this);
	if (Shell)
	{
		Shell->Collision->IgnoreActorWhenMoving(this, true);
		if (AJXCharacter* C = GetOccupant()) Shell->Collision->IgnoreActorWhenMoving(C, true);
	}
	UJXCombatLibrary::PlaySoundAt(this, CannonSound.IsNull() ? UJXSettings::Get()->ExplosionSound : CannonSound, From);
	Speed -= 150.f; // recoil
}

void AJXTank::FireMachineGun()
{
	AController* C = GetController();
	if (!bFiringMG || !C) return;

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
	P.Range = 60000.f;
	P.Damage = MachineGunDamage;
	P.SpreadDegrees = 1.2f;
	P.Ignore.Add(GetOccupant());
	const FJXShotResult R = UJXCombatLibrary::FireHitscan(GetWorld(), P);
	if (R.PawnHits > 0)
	{
		if (AJXCharacter* Driver = GetOccupant()) Driver->NotifyHitTarget(R.bHeadshot);
	}
	UJXCombatLibrary::PlaySoundAt(this, UJXSettings::Get()->DefaultFireSound, P.Muzzle, 0.8f);
	GetWorldTimerManager().SetTimer(MGTimer, this, &AJXTank::FireMachineGun, 60.f / FMath::Max(MachineGunRPM, 1.f), false);
}

void AJXTank::OnDestroyedByDamage(AController* Killer)
{
	Speed = 0.f;
	UJXCombatLibrary::Explode(this, GetActorLocation() + FVector(0, 0, 100.f), 150.f, 800.f, this, Killer);
	// Burnt-out wreck.
	for (UStaticMeshComponent* Part : { Hull.Get(), TrackLeft.Get(), TrackRight.Get(), Turret.Get(), Cannon.Get() })
	{
		if (!Part) continue;
		if (UMaterialInterface* Base = Part->GetMaterial(0))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.03f, 0.03f, 0.03f));
			Part->SetMaterial(0, MID);
		}
	}
}

