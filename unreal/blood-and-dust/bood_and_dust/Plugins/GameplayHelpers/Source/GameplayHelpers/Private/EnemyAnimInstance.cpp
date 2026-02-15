#include "EnemyAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

void UEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* Owner = TryGetPawnOwner();
	if (ACharacter* Char = Cast<ACharacter>(Owner))
	{
		CachedCharacter = Char;
		CachedCMC = Char->GetCharacterMovement();
	}
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (bIsDead)
	{
		return; // Pose is frozen
	}

	if (!CachedCMC.IsValid())
	{
		// Re-cache if character was re-possessed or component changed
		APawn* Owner = TryGetPawnOwner();
		if (ACharacter* Char = Cast<ACharacter>(Owner))
		{
			CachedCharacter = Char;
			CachedCMC = Char->GetCharacterMovement();
		}

		if (!CachedCMC.IsValid())
		{
			return;
		}
	}

	// Target speed from CMC ground velocity (ignore Z for jumps/falls)
	float TargetSpeed = CachedCMC->Velocity.Size2D();

	// Smooth interpolation — prevents jitter from causing animation pops
	Speed = FMath::FInterpTo(Speed, TargetSpeed, DeltaSeconds, 5.0f);

	// Dead-zone snap: tiny velocities from physics settling → treat as zero
	if (Speed < 3.0f)
	{
		Speed = 0.0f;
	}

	// Also set LocSpeed BP variable for AnimGraph BlendSpace binding.
	// The EventGraph also computes LocSpeed, but C++ is more reliable —
	// this ensures the BlendSpace always gets the correct speed value.
	if (FProperty* Prop = GetClass()->FindPropertyByName(FName("LocSpeed")))
	{
		void* ValPtr = Prop->ContainerPtrToValuePtr<void>(this);
		if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
		{
			FP->SetPropertyValue(ValPtr, Speed);
		}
		else if (FDoubleProperty* DP = CastField<FDoubleProperty>(Prop))
		{
			DP->SetPropertyValue(ValPtr, (double)Speed);
		}
	}
}
