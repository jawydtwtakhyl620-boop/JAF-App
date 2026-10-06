#include "JXPickup.h"
#include "JXCharacter.h"
#include "JXSettings.h"
#include "JXWeaponLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AJXPickup::AJXPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(130.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);

	Spinner = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spinner"));
	Spinner->RotationRate = FRotator(0.f, 60.f, 0.f);
	Spinner->SetUpdatedComponent(Mesh);
}

AJXPickup* AJXPickup::SpawnPickup(UWorld* World, const FJXItem& InItem, const FVector& Location)
{
	if (!World) return nullptr;
	// Snap to the floor below.
	FVector Spot = Location;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(JXPickupFloor), false);
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 100), Location - FVector(0, 0, 5000), ECC_Visibility, Q))
	{
		Spot = Hit.ImpactPoint;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJXPickup* Pickup = World->SpawnActor<AJXPickup>(AJXPickup::StaticClass(), Spot + FVector(0, 0, 25), FRotator::ZeroRotator, P);
	if (Pickup) Pickup->SetItem(InItem);
	return Pickup;
}

void AJXPickup::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AJXPickup::OnOverlap);
	UpdateVisual();
}

void AJXPickup::SetItem(const FJXItem& InItem)
{
	Item = InItem;
	UpdateVisual();
}

bool AJXPickup::IsAutoPickup() const
{
	// Ammo, healing and grenades are collected by walking over them; weapons and armor need the button.
	return Item.Type != EJXItemType::Weapon && Item.Type != EJXItemType::Vest && Item.Type != EJXItemType::Helmet;
}

FString AJXPickup::GetInteractText(const AJXCharacter* By) const
{
	return FString::Printf(TEXT("Pick up %s"), *UJXWeaponLibrary::ItemLabel(Item));
}

void AJXPickup::Interact(AJXCharacter* By)
{
	if (By && By->PickUp(Item))
	{
		Destroy();
	}
}

void AJXPickup::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AJXCharacter* C = Cast<AJXCharacter>(OtherActor);
	if (!C || !C->IsAlive() || C->GetDropState() != EJXDropState::OnGround) return;
	// Players auto-collect consumables only. Bots also take armor and weapons when they have room
	// (never swapping weapons, otherwise they would keep picking up what they just dropped).
	const bool bBotWants = !C->IsPlayerControlled() &&
		(Item.Type != EJXItemType::Weapon || C->HasFreeSlotFor(Item.WeaponId));
	if (IsAutoPickup() || bBotWants)
	{
		Interact(C);
	}
}

void AJXPickup::UpdateVisual()
{
	if (!Mesh) return;
	FVector Scale(0.3f);
	FLinearColor Color(0.8f, 0.8f, 0.8f);
	UStaticMesh* Custom = nullptr;

	switch (Item.Type)
	{
	case EJXItemType::Weapon:
	{
		if (const FJXWeaponVisual* V = UJXSettings::Get()->FindWeaponVisual(Item.WeaponId))
		{
			Custom = V->StaticMesh.LoadSynchronous();
		}
		Scale = FVector(0.9f, 0.08f, 0.18f);
		Color = FLinearColor(0.05f, 0.05f, 0.05f);
		break;
	}
	case EJXItemType::Ammo:
		Scale = FVector(0.3f, 0.2f, 0.2f);
		switch (Item.Ammo)
		{
		case EJXAmmoType::A556: Color = FLinearColor(0.2f, 0.6f, 0.2f); break;
		case EJXAmmoType::A762: Color = FLinearColor(0.8f, 0.55f, 0.1f); break;
		case EJXAmmoType::A9mm: Color = FLinearColor(0.85f, 0.8f, 0.2f); break;
		case EJXAmmoType::A45: Color = FLinearColor(0.5f, 0.3f, 0.7f); break;
		case EJXAmmoType::A12Gauge: Color = FLinearColor(0.8f, 0.15f, 0.1f); break;
		case EJXAmmoType::A300: Color = FLinearColor(0.15f, 0.35f, 0.8f); break;
		default: Color = FLinearColor(0.3f, 0.3f, 0.3f); break;
		}
		break;
	case EJXItemType::Bandage: Scale = FVector(0.2f); Color = FLinearColor(0.95f, 0.92f, 0.85f); break;
	case EJXItemType::FirstAid: Scale = FVector(0.35f, 0.25f, 0.15f); Color = FLinearColor(0.9f, 0.9f, 0.9f); break;
	case EJXItemType::MedKit: Scale = FVector(0.5f, 0.35f, 0.2f); Color = FLinearColor(0.9f, 0.1f, 0.1f); break;
	case EJXItemType::EnergyDrink: Scale = FVector(0.1f, 0.1f, 0.25f); Color = FLinearColor(0.1f, 0.5f, 0.9f); break;
	case EJXItemType::Vest: Scale = FVector(0.5f, 0.2f, 0.6f); Color = FLinearColor(0.3f, 0.32f, 0.2f) * (0.7f + 0.2f * Item.Level); break;
	case EJXItemType::Helmet: Scale = FVector(0.3f); Color = FLinearColor(0.25f, 0.3f, 0.2f) * (0.7f + 0.2f * Item.Level); break;
	case EJXItemType::FragGrenade: Scale = FVector(0.12f); Color = FLinearColor(0.2f, 0.3f, 0.15f); break;
	case EJXItemType::SmokeGrenade: Scale = FVector(0.1f, 0.1f, 0.2f); Color = FLinearColor(0.6f, 0.6f, 0.6f); break;
	case EJXItemType::FlashGrenade: Scale = FVector(0.1f, 0.1f, 0.2f); Color = FLinearColor(0.15f, 0.15f, 0.15f); break;
	}

	if (Custom)
	{
		Mesh->SetStaticMesh(Custom);
		Mesh->SetRelativeScale3D(FVector(1.f));
		return;
	}
	Mesh->SetRelativeScale3D(Scale);
	if (UMaterialInterface* Base = Mesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Base);
		if (!MID)
		{
			MID = UMaterialInstanceDynamic::Create(Base, this);
			Mesh->SetMaterial(0, MID);
		}
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

AJXLootSpawner::AJXLootSpawner()
{
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->SetBoxExtent(FVector(500.f, 500.f, 200.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Area;
}

void AJXLootSpawner::BeginPlay()
{
	Super::BeginPlay();
	FRandomStream Rng(FMath::Rand());
	const FVector Extent = Area->GetScaledBoxExtent();
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Local(Rng.FRandRange(-Extent.X, Extent.X), Rng.FRandRange(-Extent.Y, Extent.Y), Extent.Z);
		const FVector World = GetActorTransform().TransformPosition(Local);
		const FJXItem Item = UJXWeaponLibrary::RandomLoot(Rng);
		AJXPickup::SpawnPickup(GetWorld(), Item, World);
		if (Item.Type == EJXItemType::Weapon)
		{
			if (const FJXWeaponDef* D = UJXWeaponLibrary::Find(Item.WeaponId))
			{
				FJXItem AmmoItem;
				AmmoItem.Type = EJXItemType::Ammo;
				AmmoItem.Ammo = D->Ammo;
				AmmoItem.Amount = UJXWeaponLibrary::AmmoPackSize(D->Ammo);
				AJXPickup::SpawnPickup(GetWorld(), AmmoItem, World + FVector(60.f, 0.f, 0.f));
			}
		}
	}
}
