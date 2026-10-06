#include "JXWeapon.h"
#include "JXCharacter.h"
#include "JXCombatLibrary.h"
#include "JXProjectile.h"
#include "JXSettings.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AJXWeapon::AJXWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMesh->SetupAttachment(Root);
	StaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(Root);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) StaticMesh->SetStaticMesh(Cube.Object);
}

void AJXWeapon::Init(const FJXWeaponDef& InDef, int32 InMag)
{
	Def = InDef;
	Mag = FMath::Clamp(InMag, 0, Def.MagSize);

	const FJXWeaponVisual* Visual = UJXSettings::Get()->FindWeaponVisual(Def.Id);
	USkeletalMesh* Skel = Visual ? Visual->SkeletalMesh.LoadSynchronous() : nullptr;
	UStaticMesh* Static = Visual ? Visual->StaticMesh.LoadSynchronous() : nullptr;
	if (Visual)
	{
		HandOffset = Visual->HandOffset;
		MuzzleSocket = Visual->MuzzleSocket;
		FireSound = Visual->FireSound;
		MuzzleFlash = Visual->MuzzleFlash;
	}

	if (Skel)
	{
		SkeletalMesh->SetSkeletalMesh(Skel);
		StaticMesh->SetVisibility(false);
	}
	else if (Static)
	{
		StaticMesh->SetStaticMesh(Static);
		StaticMesh->SetRelativeScale3D(FVector(1.f));
	}
	else
	{
		// Placeholder: a dark box sized by weapon class, pointing forward along the hand.
		float Length = 0.8f;
		switch (Def.Category)
		{
		case EJXWeaponCategory::Pistol: Length = 0.25f; break;
		case EJXWeaponCategory::SMG: Length = 0.55f; break;
		case EJXWeaponCategory::Sniper: Length = 1.2f; break;
		case EJXWeaponCategory::DMR: Length = 1.0f; break;
		case EJXWeaponCategory::Launcher: Length = 1.0f; break;
		default: break;
		}
		StaticMesh->SetRelativeScale3D(FVector(Length, 0.07f, 0.14f));
		StaticMesh->SetRelativeLocation(FVector(Length * 35.f, 0.f, 0.f));
		if (UMaterialInterface* Base = StaticMesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), Def.Category == EJXWeaponCategory::Launcher
				? FLinearColor(0.15f, 0.2f, 0.1f) : FLinearColor(0.04f, 0.04f, 0.04f));
			StaticMesh->SetMaterial(0, MID);
		}
	}
}

AJXCharacter* AJXWeapon::GetOwnerCharacter() const
{
	return Cast<AJXCharacter>(GetOwner());
}

FVector AJXWeapon::GetMuzzleLocation() const
{
	if (SkeletalMesh->GetSkeletalMeshAsset() && SkeletalMesh->DoesSocketExist(MuzzleSocket))
	{
		return SkeletalMesh->GetSocketLocation(MuzzleSocket);
	}
	if (StaticMesh->GetStaticMesh() && StaticMesh->DoesSocketExist(MuzzleSocket))
	{
		return StaticMesh->GetSocketLocation(MuzzleSocket);
	}
	return GetActorLocation() + GetActorForwardVector() * 60.f;
}

float AJXWeapon::GetCurrentSpread() const
{
	const AJXCharacter* C = GetOwnerCharacter();
	const float Base = (C && C->IsAiming()) ? Def.AdsSpread : Def.HipSpread;
	return Base * (C ? C->GetSpreadMultiplier() : 1.f);
}

float AJXWeapon::GetReloadProgress() const
{
	if (!bReloading || Def.ReloadTime <= 0.f) return 0.f;
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadStartTime) / Def.ReloadTime, 0.f, 1.f);
}

void AJXWeapon::StartFire()
{
	bWantsFire = true;
	if (bReloading) return;

	const float Interval = 60.f / FMath::Max(Def.RPM, 1.f);
	const float Wait = LastFireTime + Interval - GetWorld()->GetTimeSeconds();
	if (Wait <= 0.f)
	{
		FireOnce();
	}
	else if (!GetWorldTimerManager().IsTimerActive(FireTimer))
	{
		// Pressed again before the gun is ready: fire as soon as possible.
		GetWorldTimerManager().SetTimer(FireTimer, this, &AJXWeapon::FireOnce, Wait, false);
	}
}

void AJXWeapon::StopFire()
{
	bWantsFire = false;
	if (Def.bAutomatic)
	{
		GetWorldTimerManager().ClearTimer(FireTimer);
	}
}

void AJXWeapon::FireOnce()
{
	AJXCharacter* Char = GetOwnerCharacter();
	if (!Char || !Char->IsAlive() || bReloading) return;
	if (Def.bAutomatic && !bWantsFire) return;

	if (Mag <= 0)
	{
		StartReload();
		return;
	}

	UWorld* World = GetWorld();
	--Mag;
	LastFireTime = World->GetTimeSeconds();

	FVector ViewLoc;
	FRotator ViewRot;
	if (AController* C = Char->GetController())
	{
		C->GetPlayerViewPoint(ViewLoc, ViewRot);
	}
	else
	{
		Char->GetActorEyesViewPoint(ViewLoc, ViewRot);
	}
	const FVector Dir = ViewRot.Vector();
	// Start the ray level with the character so walls behind a third-person camera are ignored.
	const float Skip = FMath::Max(0.f, FVector::DotProduct(Char->GetActorLocation() - ViewLoc, Dir));
	const FVector AimStart = ViewLoc + Dir * Skip;
	const FVector Muzzle = GetMuzzleLocation();

	if (Def.bProjectile)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(JXRocketAim), false, Char);
		Q.AddIgnoredActor(this);
		const FVector End = AimStart + Dir * Def.Range;
		const FVector Target = World->LineTraceSingleByChannel(Hit, AimStart, End, ECC_Visibility, Q) ? Hit.ImpactPoint : End;
		const FRotator RocketDir = (Target - Muzzle).Rotation();
		AJXProjectile::Launch(World, AJXProjectile::StaticClass(), Muzzle + RocketDir.Vector() * 40.f, RocketDir, Def.ProjectileSpeed, 0.05f, false,
			EJXExplosiveKind::Explosive, Def.ExplosionDamage, Def.ExplosionRadius, 0.f, Char);
	}
	else
	{
		FJXShotParams P;
		P.Causer = Char;
		P.Instigator = Char->GetController();
		P.AimStart = AimStart;
		P.AimDir = Dir;
		P.Muzzle = Muzzle;
		P.Range = Def.Range;
		P.Damage = Def.Damage;
		P.HeadMultiplier = Def.HeadMultiplier;
		P.SpreadDegrees = GetCurrentSpread();
		P.Pellets = Def.Pellets;
		P.Ignore.Add(this);
		const FJXShotResult R = UJXCombatLibrary::FireHitscan(World, P);
		if (R.PawnHits > 0)
		{
			Char->NotifyHitTarget(R.bHeadshot);
		}
	}

	TSoftObjectPtr<USoundBase> Sound = FireSound.IsNull() ? UJXSettings::Get()->DefaultFireSound : FireSound;
	UJXCombatLibrary::PlaySoundAt(this, Sound, Muzzle);
	if (UNiagaraSystem* Flash = MuzzleFlash.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Flash, Muzzle, GetActorRotation());
	}
	Char->ApplyRecoil(Def.RecoilPitch, Def.RecoilYaw);

	if (Def.bAutomatic && bWantsFire)
	{
		GetWorldTimerManager().SetTimer(FireTimer, this, &AJXWeapon::FireOnce, 60.f / FMath::Max(Def.RPM, 1.f), false);
	}
}

bool AJXWeapon::StartReload()
{
	AJXCharacter* Char = GetOwnerCharacter();
	if (!Char || bReloading || Mag >= Def.MagSize || Char->GetAmmo(Def.Ammo) <= 0) return false;
	bReloading = true;
	ReloadStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().ClearTimer(FireTimer);
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &AJXWeapon::FinishReload, Def.ReloadTime, false);
	return true;
}

void AJXWeapon::CancelReload()
{
	bReloading = false;
	GetWorldTimerManager().ClearTimer(ReloadTimer);
}

void AJXWeapon::FinishReload()
{
	bReloading = false;
	if (AJXCharacter* Char = GetOwnerCharacter())
	{
		Mag += Char->TakeAmmo(Def.Ammo, Def.MagSize - Mag);
		if (bWantsFire && Def.bAutomatic)
		{
			StartFire();
		}
	}
}
