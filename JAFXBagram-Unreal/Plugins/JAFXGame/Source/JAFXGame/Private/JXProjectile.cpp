#include "JXProjectile.h"
#include "JXCharacter.h"
#include "JXCombatLibrary.h"
#include "JXSettings.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AJXProjectile::AJXProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.16f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 5000.f;
	Movement->MaxSpeed = 30000.f;
	Movement->bRotationFollowsVelocity = true;

	InitialLifeSpan = 30.f;
}

AJXProjectile* AJXProjectile::Launch(UWorld* World, TSubclassOf<AJXProjectile> Class, const FVector& Location, const FRotator& Direction,
	float Speed, float Gravity, bool bBounce, EJXExplosiveKind InKind, float InDamage, float InRadius, float Fuse, APawn* InstigatorPawn)
{
	if (!World) return nullptr;
	if (!Class) Class = AJXProjectile::StaticClass();

	const FTransform Xform(Direction, Location);
	AJXProjectile* P = World->SpawnActorDeferred<AJXProjectile>(Class, Xform, InstigatorPawn, InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!P) return nullptr;

	P->Kind = InKind;
	P->Damage = InDamage;
	P->Radius = InRadius;
	P->FuseTime = Fuse;
	P->Movement->InitialSpeed = Speed;
	P->Movement->MaxSpeed = FMath::Max(Speed, 1.f);
	P->Movement->ProjectileGravityScale = Gravity;
	P->Movement->bShouldBounce = bBounce;
	P->Movement->Bounciness = 0.3f;
	P->Movement->Friction = 0.4f;
	P->Movement->bRotationFollowsVelocity = !bBounce;
	P->FinishSpawning(Xform);
	return P;
}

void AJXProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Inst = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Inst, true);
	}
	// Bouncing grenades rely on their fuse instead of exploding on impact.
	if (!Movement->bShouldBounce)
	{
		Movement->OnProjectileStop.AddDynamic(this, &AJXProjectile::OnStopped);
	}
	if (FuseTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(FuseTimer, this, &AJXProjectile::Detonate, FuseTime, false);
	}
}

void AJXProjectile::OnStopped(const FHitResult& ImpactResult)
{
	if (FuseTime <= 0.f)
	{
		Detonate();
	}
}

void AJXProjectile::Detonate()
{
	if (bDetonated) return;
	bDetonated = true;

	UWorld* World = GetWorld();
	const FVector Loc = GetActorLocation();
	const UJXSettings* Settings = UJXSettings::Get();
	AController* InstigatorController = GetInstigatorController();

	switch (Kind)
	{
	case EJXExplosiveKind::Explosive:
		UJXCombatLibrary::Explode(this, Loc, Damage, Radius, this, InstigatorController);
		break;

	case EJXExplosiveKind::Smoke:
	{
		UJXCombatLibrary::SpawnEffect(this, Settings->SmokeEffect, Loc, FRotator::ZeroRotator, 1.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AJXSmokeCloud* Cloud = World->SpawnActor<AJXSmokeCloud>(AJXSmokeCloud::StaticClass(), Loc + FVector(0, 0, 200), FRotator::ZeroRotator, Params))
		{
			Cloud->Init(SmokeDuration, Settings->SmokeEffect.IsNull());
		}
		break;
	}

	case EJXExplosiveKind::Flash:
		for (TActorIterator<AJXCharacter> It(World); It; ++It)
		{
			AJXCharacter* C = *It;
			if (!C->IsAlive()) continue;
			const FVector Eye = C->GetPawnViewLocation();
			const float Dist = FVector::Dist(Eye, Loc);
			if (Dist > Radius) continue;
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(JXFlash), false, this);
			Q.AddIgnoredActor(C);
			if (World->LineTraceSingleByChannel(Hit, Loc, Eye, ECC_Visibility, Q)) continue;
			const FVector ToFlash = (Loc - Eye).GetSafeNormal();
			const float Facing = FVector::DotProduct(C->GetViewRotation().Vector(), ToFlash) > 0.2f ? 1.f : 0.45f;
			C->ApplyFlash(Facing * (1.f - Dist / Radius * 0.6f));
		}
		break;
	}
	Destroy();
}

AJXSmokeCloud::AJXSmokeCloud()
{
	Blocker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blocker"));
	RootComponent = Blocker;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Blocker->SetStaticMesh(Sphere.Object);
	Blocker->SetRelativeScale3D(FVector(9.f)); // 9 m diameter
	// Only blocks line of sight, so AI cannot see through smoke but players walk through.
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Blocker->SetCastShadow(false);
}

void AJXSmokeCloud::Init(float Duration, bool bShowFallbackMesh)
{
	Blocker->SetHiddenInGame(!bShowFallbackMesh);
	SetLifeSpan(Duration);
}
