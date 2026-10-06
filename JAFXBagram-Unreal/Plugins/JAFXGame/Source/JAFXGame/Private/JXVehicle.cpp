#include "JXVehicle.h"
#include "JXCharacter.h"
#include "JXHealthComponent.h"
#include "JXPlayerController.h"
#include "JXTypes.h"
#include "Engine/DamageEvents.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

AJXVehicle::AJXVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	// Subclasses create the root component and attach these to it.
	Seat = CreateDefaultSubobject<USceneComponent>(TEXT("Seat"));
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->bUsePawnControlRotation = true;
	Boom->TargetArmLength = 600.f;
	Boom->bDoCollisionTest = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);

	UseArea = CreateDefaultSubobject<USphereComponent>(TEXT("UseArea"));
	UseArea->InitSphereRadius(300.f);
	UseArea->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	UseArea->SetGenerateOverlapEvents(true);

	Health = CreateDefaultSubobject<UJXHealthComponent>(TEXT("Health"));
	Health->bTakesZoneDamage = false;

	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AJXVehicle::BeginPlay()
{
	Super::BeginPlay();
	DefaultArm = Boom->TargetArmLength;
	Health->OnDeath.AddDynamic(this, &AJXVehicle::HandleDeath);
}

bool AJXVehicle::IsDestroyed() const
{
	return Health && Health->IsDead();
}

FString AJXVehicle::GetInteractText(const AJXCharacter* By) const
{
	if (Occupant.IsValid() || IsDestroyed()) return FString();
	return FString::Printf(TEXT("Use %s"), *VehicleName);
}

void AJXVehicle::Interact(AJXCharacter* By)
{
	if (!By || Occupant.IsValid() || IsDestroyed()) return;
	APlayerController* PC = Cast<APlayerController>(By->GetController());
	if (!PC) return; // bots do not drive

	Occupant = By;
	By->EnterVehicle(this, Seat, bHideOccupant);
	const FRotator Keep = PC->GetControlRotation();
	PC->Possess(this);
	PC->SetControlRotation(Keep);
}

FVector AJXVehicle::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -350.f + FVector(0.f, 0.f, 120.f);
}

void AJXVehicle::ForceExit()
{
	AJXCharacter* C = Occupant.Get();
	if (!C) return;
	StopFire();
	Occupant = nullptr;
	MoveInput = FVector2D::ZeroVector;
	bZoom = false;

	AController* Ctrl = GetController();
	C->ExitVehicle(GetExitLocation());
	if (Ctrl)
	{
		const FRotator Keep = Ctrl->GetControlRotation();
		Ctrl->Possess(C);
		Ctrl->SetControlRotation(Keep);
	}
}

void AJXVehicle::HandleDeath(AActor* Victim, AController* Killer, AActor* DamageCauser)
{
	AJXCharacter* C = Occupant.Get();
	ForceExit();
	OnDestroyedByDamage(Killer);
	if (C && C->IsAlive() && bHideOccupant)
	{
		// Whoever is inside a destroyed vehicle does not survive.
		C->TakeDamage(1000.f, FDamageEvent(UJXExplosiveDamageType::StaticClass()), Killer, this);
	}
}

void AJXVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	AJXPlayerController* PC = Cast<AJXPlayerController>(GetController());
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!PC || !EIC) return;

	const FJXInputActions& A = PC->GetInputActions();
	EIC->BindAction(A.Look, ETriggerEvent::Triggered, this, &AJXVehicle::InputLook);
	EIC->BindAction(A.Move, ETriggerEvent::Triggered, this, &AJXVehicle::InputMove);
	EIC->BindAction(A.Move, ETriggerEvent::Completed, this, &AJXVehicle::InputMoveStop);
	EIC->BindAction(A.Fire, ETriggerEvent::Started, this, &AJXVehicle::StartFire);
	EIC->BindAction(A.Fire, ETriggerEvent::Completed, this, &AJXVehicle::StopFire);
	EIC->BindAction(A.Fire, ETriggerEvent::Canceled, this, &AJXVehicle::StopFire);
	EIC->BindAction(A.Aim, ETriggerEvent::Started, this, &AJXVehicle::InputAimStart);
	EIC->BindAction(A.Aim, ETriggerEvent::Completed, this, &AJXVehicle::InputAimStop);
	EIC->BindAction(A.Interact, ETriggerEvent::Started, this, &AJXVehicle::InputLeave);
	EIC->BindAction(A.NextWeapon, ETriggerEvent::Started, this, &AJXVehicle::SwitchWeapon);
}

void AJXVehicle::InputLook(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	float Scale = bZoom ? 0.4f : 1.f;
	if (const AJXPlayerController* PC = Cast<AJXPlayerController>(GetController())) Scale *= PC->LookSensitivity;
	AddControllerYawInput(V.X * Scale);
	AddControllerPitchInput(V.Y * Scale);
}

void AJXVehicle::InputMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
}
