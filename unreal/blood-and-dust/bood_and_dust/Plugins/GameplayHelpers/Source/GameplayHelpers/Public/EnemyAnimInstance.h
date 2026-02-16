// C++ AnimInstance for enemy locomotion via BlendSpace1D.
// Provides smoothed Speed variable for continuous idle/walk blending.
// Eliminates state-machine animation resets.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemyAnimInstance.generated.h"

class ACharacter;
class UCharacterMovementComponent;

UCLASS()
class GAMEPLAYHELPERS_API UEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Smoothed ground speed for BlendSpace1D (0 = idle, ~300 = walk). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Speed = 0.0f;

	/** True when the enemy is dead — freezes the current pose. */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsDead = false;

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UCharacterMovementComponent> CachedCMC;
};
