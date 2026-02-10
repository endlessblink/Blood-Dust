// Runtime Blueprint Function Library - Helper functions for gameplay logic

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayHelperLibrary.generated.h"

class ACharacter;
class UAnimSequence;

/**
 * Static helper functions for common gameplay operations.
 * These are RUNTIME functions (not editor-only) so they work in packaged builds.
 */
UCLASS()
class GAMEPLAYHELPERS_API UGameplayHelperLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Set the character's max walk speed at runtime.
	 * Wraps CharacterMovementComponent->MaxWalkSpeed = NewSpeed.
	 */
	UFUNCTION(BlueprintCallable, Category="Gameplay|Character", meta=(DefaultToSelf="Character"))
	static void SetCharacterWalkSpeed(ACharacter* Character, float NewSpeed);

	/**
	 * Play an AnimSequence as a one-shot dynamic montage on the character.
	 * Blends in/out smoothly and returns to AnimBP state machine when done.
	 * Uses the DefaultSlot so multiple calls interrupt each other (no stacking).
	 */
	UFUNCTION(BlueprintCallable, Category="Gameplay|Animation", meta=(DefaultToSelf="Character"))
	static void PlayAnimationOneShot(ACharacter* Character, UAnimSequence* AnimSequence, float PlayRate = 1.0f, float BlendIn = 0.25f, float BlendOut = 0.25f);
};
