#include "MCPHelperLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"

void UMCPHelperLibrary::SetCharacterWalkSpeed(ACharacter* Character, float NewSpeed)
{
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPHelperLibrary::SetCharacterWalkSpeed: Character is null"));
		return;
	}

	UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
	if (!MovementComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPHelperLibrary::SetCharacterWalkSpeed: No CharacterMovementComponent"));
		return;
	}

	MovementComp->MaxWalkSpeed = NewSpeed;
}

void UMCPHelperLibrary::PlayAnimationOneShot(ACharacter* Character, UAnimSequence* AnimSequence, float PlayRate, float BlendIn, float BlendOut)
{
	if (!Character || !AnimSequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPHelperLibrary::PlayAnimationOneShot: Character or AnimSequence is null"));
		return;
	}

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPHelperLibrary::PlayAnimationOneShot: No SkeletalMeshComponent"));
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPHelperLibrary::PlayAnimationOneShot: No AnimInstance"));
		return;
	}

	// Play as dynamic montage in DefaultSlot - blends in, plays once, blends out
	// Then the AnimBP state machine resumes automatically
	AnimInst->PlaySlotAnimationAsDynamicMontage(
		AnimSequence,
		FName("DefaultSlot"),
		BlendIn,
		BlendOut,
		PlayRate,
		1,      // LoopCount = 1 (play once)
		-1.0f,  // BlendOutTriggerTime = auto
		0.0f    // InTimeToStartMontageAt
	);
}
