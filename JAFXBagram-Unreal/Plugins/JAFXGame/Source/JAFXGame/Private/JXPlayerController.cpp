#include "JXPlayerController.h"
#include "JXSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "GameFramework/TouchInterface.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(Outer, Name);
		A->ValueType = Type;
		return A;
	}
}

const FJXInputActions& AJXPlayerController::GetInputActions()
{
	CreateInput();
	AddMappingContext();
	return Actions;
}

void AJXPlayerController::BeginPlay()
{
	Super::BeginPlay();
	CreateInput();
	AddMappingContext();
	SetupTouchInterface();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AJXPlayerController::CreateInput()
{
	if (bInputCreated) return;
	bInputCreated = true;

	using T = EInputActionValueType;
	Actions.Move = MakeAction(this, TEXT("IA_Move"), T::Axis2D);
	Actions.Look = MakeAction(this, TEXT("IA_Look"), T::Axis2D);
	Actions.Jump = MakeAction(this, TEXT("IA_Jump"), T::Boolean);
	Actions.Sprint = MakeAction(this, TEXT("IA_Sprint"), T::Boolean);
	Actions.Crouch = MakeAction(this, TEXT("IA_Crouch"), T::Boolean);
	Actions.Fire = MakeAction(this, TEXT("IA_Fire"), T::Boolean);
	Actions.Aim = MakeAction(this, TEXT("IA_Aim"), T::Boolean);
	Actions.Reload = MakeAction(this, TEXT("IA_Reload"), T::Boolean);
	Actions.Interact = MakeAction(this, TEXT("IA_Interact"), T::Boolean);
	Actions.Slot1 = MakeAction(this, TEXT("IA_Slot1"), T::Boolean);
	Actions.Slot2 = MakeAction(this, TEXT("IA_Slot2"), T::Boolean);
	Actions.Slot3 = MakeAction(this, TEXT("IA_Slot3"), T::Boolean);
	Actions.NextWeapon = MakeAction(this, TEXT("IA_NextWeapon"), T::Boolean);
	Actions.Heal = MakeAction(this, TEXT("IA_Heal"), T::Boolean);
	Actions.Throw = MakeAction(this, TEXT("IA_Throw"), T::Boolean);
	Actions.CycleThrowable = MakeAction(this, TEXT("IA_CycleThrowable"), T::Boolean);

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_JAFX"));

	auto Negate = [this](bool bX, bool bY)
	{
		UInputModifierNegate* N = NewObject<UInputModifierNegate>(this);
		N->bX = bX;
		N->bY = bY;
		N->bZ = false;
		return N;
	};
	auto Swizzle = [this]()
	{
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(this);
		S->Order = EInputAxisSwizzle::YXZ;
		return S;
	};
	auto DeadZone = [this]()
	{
		return NewObject<UInputModifierDeadZone>(this);
	};

	// Movement: WASD + gamepad / on-screen left stick.
	Mapping->MapKey(Actions.Move, EKeys::D);
	Mapping->MapKey(Actions.Move, EKeys::A).Modifiers.Add(Negate(true, false));
	Mapping->MapKey(Actions.Move, EKeys::W).Modifiers.Add(Swizzle());
	{
		FEnhancedActionKeyMapping& S = Mapping->MapKey(Actions.Move, EKeys::S);
		S.Modifiers.Add(Swizzle());
		S.Modifiers.Add(Negate(true, true));
	}
	Mapping->MapKey(Actions.Move, EKeys::Gamepad_Left2D).Modifiers.Add(DeadZone());

	// Look: mouse + gamepad / on-screen right stick (Y inverted like the UE templates).
	Mapping->MapKey(Actions.Look, EKeys::Mouse2D).Modifiers.Add(Negate(false, true));
	{
		FEnhancedActionKeyMapping& G = Mapping->MapKey(Actions.Look, EKeys::Gamepad_Right2D);
		G.Modifiers.Add(DeadZone());
		G.Modifiers.Add(Negate(false, true));
	}

	// Buttons: keyboard/mouse first, then gamepad keys (also used by the touch buttons).
	Mapping->MapKey(Actions.Jump, EKeys::SpaceBar);
	Mapping->MapKey(Actions.Jump, EKeys::Gamepad_FaceButton_Bottom);
	Mapping->MapKey(Actions.Sprint, EKeys::LeftShift);
	Mapping->MapKey(Actions.Sprint, EKeys::Gamepad_LeftThumbstick);
	Mapping->MapKey(Actions.Crouch, EKeys::C);
	Mapping->MapKey(Actions.Crouch, EKeys::LeftControl);
	Mapping->MapKey(Actions.Crouch, EKeys::Gamepad_FaceButton_Right);
	Mapping->MapKey(Actions.Fire, EKeys::LeftMouseButton);
	Mapping->MapKey(Actions.Fire, EKeys::Gamepad_RightTrigger);
	Mapping->MapKey(Actions.Aim, EKeys::RightMouseButton);
	Mapping->MapKey(Actions.Aim, EKeys::Gamepad_LeftTrigger);
	Mapping->MapKey(Actions.Reload, EKeys::R);
	Mapping->MapKey(Actions.Reload, EKeys::Gamepad_FaceButton_Left);
	Mapping->MapKey(Actions.Interact, EKeys::F);
	Mapping->MapKey(Actions.Interact, EKeys::E);
	Mapping->MapKey(Actions.Interact, EKeys::Gamepad_FaceButton_Top);
	Mapping->MapKey(Actions.Slot1, EKeys::One);
	Mapping->MapKey(Actions.Slot2, EKeys::Two);
	Mapping->MapKey(Actions.Slot3, EKeys::Three);
	Mapping->MapKey(Actions.NextWeapon, EKeys::Q);
	Mapping->MapKey(Actions.NextWeapon, EKeys::MouseScrollDown);
	Mapping->MapKey(Actions.NextWeapon, EKeys::Gamepad_RightShoulder);
	Mapping->MapKey(Actions.Heal, EKeys::H);
	Mapping->MapKey(Actions.Heal, EKeys::Gamepad_LeftShoulder);
	Mapping->MapKey(Actions.Throw, EKeys::G);
	Mapping->MapKey(Actions.Throw, EKeys::Gamepad_DPad_Up);
	Mapping->MapKey(Actions.CycleThrowable, EKeys::T);
	Mapping->MapKey(Actions.CycleThrowable, EKeys::Gamepad_DPad_Down);
}

void AJXPlayerController::AddMappingContext()
{
	if (bMappingAdded || !Mapping) return;
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Sub->AddMappingContext(Mapping, 0);
			bMappingAdded = true;
		}
	}
}

void AJXPlayerController::SetupTouchInterface()
{
	bool bWantTouch = UJXSettings::Get()->bForceTouchControls;
#if PLATFORM_IOS || PLATFORM_ANDROID
	bWantTouch = true;
#endif
	if (!bWantTouch || !IsLocalController()) return;

	UTexture2D* Ring = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/MobileResources/HUD/VirtualJoystick_Background.VirtualJoystick_Background"));
	UTexture2D* Knob = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/MobileResources/HUD/VirtualJoystick_Thumb.VirtualJoystick_Thumb"));

	TouchInterface = NewObject<UTouchInterface>(this, TEXT("JAFX_Touch"));
	TouchInterface->ActiveOpacity = 0.9f;
	TouchInterface->InactiveOpacity = 0.55f;
	TouchInterface->TimeUntilDeactive = 0.5f;
	TouchInterface->TimeUntilReset = 2.f;
	TouchInterface->bPreventRecenter = false;

	auto AddStick = [&](FVector2D Center, float Size, FKey X, FKey Y)
	{
		FTouchInputControl C;
		C.Image1 = Knob;
		C.Image2 = Ring;
		C.Center = Center;
		C.VisualSize = FVector2D(Size, Size);
		C.ThumbSize = FVector2D(Size * 0.4f, Size * 0.4f);
		C.InteractionSize = FVector2D(Size * 1.5f, Size * 1.5f);
		C.InputScale = FVector2D(1.f, 1.f);
		C.MainInputKey = X;
		C.AltInputKey = Y;
		TouchInterface->Controls.Add(C);
	};
	auto AddButton = [&](FVector2D Center, float Size, FKey Key, const TCHAR* Label)
	{
		FTouchInputControl C;
		C.Image1 = Ring;
		C.Image2 = Ring;
		C.Center = Center;
		C.VisualSize = FVector2D(Size, Size);
		C.ThumbSize = FVector2D(0.f, 0.f);
		C.InteractionSize = FVector2D(Size * 1.1f, Size * 1.1f);
		C.InputScale = FVector2D(1.f, 1.f);
		C.MainInputKey = Key;
		TouchInterface->Controls.Add(C);
		FJXTouchLabel L;
		L.Center = Center;
		L.Text = Label;
		TouchLabels.Add(L);
	};

	// Layout (fractions of the screen). Left: move stick. Right: look stick + action buttons.
	AddStick(FVector2D(0.13f, 0.72f), 220.f, EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY);
	AddStick(FVector2D(0.87f, 0.72f), 200.f, EKeys::Gamepad_RightX, EKeys::Gamepad_RightY);
	AddButton(FVector2D(0.72f, 0.58f), 150.f, EKeys::Gamepad_RightTrigger, TEXT("FIRE"));
	AddButton(FVector2D(0.07f, 0.38f), 110.f, EKeys::Gamepad_RightTrigger, TEXT("FIRE"));
	AddButton(FVector2D(0.80f, 0.40f), 110.f, EKeys::Gamepad_LeftTrigger, TEXT("AIM"));
	AddButton(FVector2D(0.94f, 0.42f), 100.f, EKeys::Gamepad_FaceButton_Bottom, TEXT("JUMP"));
	AddButton(FVector2D(0.94f, 0.25f), 90.f, EKeys::Gamepad_FaceButton_Right, TEXT("CROUCH"));
	AddButton(FVector2D(0.62f, 0.84f), 95.f, EKeys::Gamepad_FaceButton_Left, TEXT("RELOAD"));
	AddButton(FVector2D(0.58f, 0.60f), 95.f, EKeys::Gamepad_FaceButton_Top, TEXT("USE"));
	AddButton(FVector2D(0.52f, 0.86f), 90.f, EKeys::Gamepad_RightShoulder, TEXT("SWAP"));
	AddButton(FVector2D(0.42f, 0.86f), 90.f, EKeys::Gamepad_LeftShoulder, TEXT("HEAL"));
	AddButton(FVector2D(0.68f, 0.30f), 90.f, EKeys::Gamepad_DPad_Up, TEXT("GRENADE"));
	AddButton(FVector2D(0.26f, 0.45f), 90.f, EKeys::Gamepad_LeftThumbstick, TEXT("SPRINT"));

	ActivateTouchInterface(TouchInterface);
	bTouchControls = true;
}

void AJXPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	FlashAmount = FMath::Max(0.f, FlashAmount - DeltaTime * 0.35f);
	const float Now = GetWorld()->GetTimeSeconds();
	KillFeed.RemoveAll([Now](const FJXKillFeedEntry& E) { return Now - E.Time > 8.f; });
}

void AJXPlayerController::NotifyHitTarget(bool bHeadshot)
{
	LastHitTime = GetWorld()->GetTimeSeconds();
	bLastHitHead = bHeadshot;
}

void AJXPlayerController::NotifyDamaged(float Damage)
{
	LastDamageTime = GetWorld()->GetTimeSeconds();
}

void AJXPlayerController::NotifyFlash(float Strength)
{
	FlashAmount = FMath::Clamp(FMath::Max(FlashAmount, Strength * 1.4f), 0.f, 1.4f);
}

void AJXPlayerController::AddKillFeed(const FString& Text)
{
	FJXKillFeedEntry Entry;
	Entry.Text = Text;
	Entry.Time = GetWorld()->GetTimeSeconds();
	KillFeed.Insert(Entry, 0);
	if (KillFeed.Num() > 5) KillFeed.SetNum(5);
}

void AJXPlayerController::ShowCenterMessage(const FString& Title, const FString& Subtitle)
{
	CenterTitle = Title;
	CenterSubtitle = Subtitle;
}
