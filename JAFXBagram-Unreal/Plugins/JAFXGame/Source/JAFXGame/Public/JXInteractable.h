// JAF X BAGRAM - anything the player can use with the Interact button.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "JXInteractable.generated.h"

class AJXCharacter;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UJXInteractable : public UInterface
{
	GENERATED_BODY()
};

class JAFXGAME_API IJXInteractable
{
	GENERATED_BODY()

public:
	/** Text for the on-screen prompt, e.g. "Pick up K-47". Empty = not usable right now. */
	virtual FString GetInteractText(const AJXCharacter* By) const { return FString(); }
	virtual void Interact(AJXCharacter* By) {}
};
