#include "JXDropPlane.h"
#include "JXCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AJXDropPlane::AJXDropPlane()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// Placeholder cargo plane (about 50 m long). Replace with a real mesh in a Blueprint child.
	Fuselage = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fuselage"));
	Fuselage->SetupAttachment(Root);
	if (Cyl.Succeeded()) Fuselage->SetStaticMesh(Cyl.Object);
	Fuselage->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Fuselage->SetRelativeScale3D(FVector(6.f, 6.f, 50.f));

	Wings = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wings"));
	Wings->SetupAttachment(Root);
	if (Cube.Succeeded()) Wings->SetStaticMesh(Cube.Object);
	Wings->SetRelativeLocation(FVector(200.f, 0.f, 200.f));
	Wings->SetRelativeScale3D(FVector(8.f, 50.f, 0.6f));

	Tail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tail"));
	Tail->SetupAttachment(Root);
	if (Cube.Succeeded()) Tail->SetStaticMesh(Cube.Object);
	Tail->SetRelativeLocation(FVector(-2300.f, 0.f, 600.f));
	Tail->SetRelativeScale3D(FVector(5.f, 0.6f, 9.f));

	for (UStaticMeshComponent* C : { Fuselage.Get(), Wings.Get(), Tail.Get() })
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(Root);
	EngineAudio->bAutoActivate = false;
}

void AJXDropPlane::StartFlight(const FVector& From, const FVector& To)
{
	Start = From;
	End = To;
	Distance = FVector::Dist(From, To);
	Travelled = 0.f;
	bFlying = true;
	SetActorLocation(From);
	SetActorRotation((To - From).Rotation());
	if (EngineAudio->Sound) EngineAudio->Play();

	// Bots leave the plane somewhere between 10% and 90% of the route.
	for (AJXCharacter* C : Passengers)
	{
		if (C && !C->IsPlayerControlled())
		{
			BotJumpAt.Add(C, FMath::FRandRange(0.1f, 0.9f));
		}
	}
}

void AJXDropPlane::AddPassenger(AJXCharacter* C)
{
	if (C) Passengers.AddUnique(C);
}

void AJXDropPlane::RemovePassenger(AJXCharacter* C)
{
	Passengers.Remove(C);
	BotJumpAt.Remove(C);
}

float AJXDropPlane::GetProgress() const
{
	return Distance > 0.f ? FMath::Clamp(Travelled / Distance, 0.f, 1.f) : 0.f;
}

void AJXDropPlane::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFlying) return;

	Travelled += Speed * DeltaSeconds;
	SetActorLocation(FMath::Lerp(Start, End, GetProgress()));
	const float P = GetProgress();

	// Copy: ejecting removes the passenger from the arrays.
	TArray<AJXCharacter*> Leaving;
	for (const TPair<TObjectPtr<AJXCharacter>, float>& It : BotJumpAt)
	{
		if (It.Key && P >= It.Value) Leaving.Add(It.Key);
	}
	if (P >= 0.97f)
	{
		for (AJXCharacter* C : Passengers) if (C) Leaving.AddUnique(C);
	}
	for (AJXCharacter* C : Leaving)
	{
		C->EjectFromPlane();
	}

	if (P >= 1.f)
	{
		bFlying = false;
		SetLifeSpan(10.f);
	}
}
