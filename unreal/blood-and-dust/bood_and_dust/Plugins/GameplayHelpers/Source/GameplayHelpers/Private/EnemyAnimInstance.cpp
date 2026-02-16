#include "EnemyAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "Animation/BlendSpace1D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"

void UEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	UE_LOG(LogTemp, Warning, TEXT("EnemyAnimInstance::Init class=%s"), *GetClass()->GetName());

	APawn* Owner = TryGetPawnOwner();
	if (ACharacter* Char = Cast<ACharacter>(Owner))
	{
		CachedCharacter = Char;
		CachedCMC = Char->GetCharacterMovement();

		// === COMPREHENSIVE DIAGNOSTIC for animation debugging ===
		USkeletalMeshComponent* MeshComp = Char->GetMesh();
		if (MeshComp)
		{
			// Mesh asset
			USkeletalMesh* MeshAsset = MeshComp->GetSkeletalMeshAsset();
			FString MeshName = MeshAsset ? MeshAsset->GetPathName() : TEXT("NULL");

			// Skeleton
			USkeleton* MeshSkel = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
			FString SkelName = MeshSkel ? MeshSkel->GetPathName() : TEXT("NULL");
			int32 SkelBones = MeshSkel ? MeshSkel->GetReferenceSkeleton().GetNum() : 0;

			// AnimClass
			UClass* AnimClass = MeshComp->AnimClass;
			FString AnimClassName = AnimClass ? AnimClass->GetPathName() : TEXT("NULL");

			// Animation mode
			int32 AnimMode = (int32)MeshComp->GetAnimationMode();

			// Is this the active AnimInstance?
			UAnimInstance* ActiveAnim = MeshComp->GetAnimInstance();
			bool bIsActive = (ActiveAnim == this);

			// bPauseAnims, bNoSkeletonUpdate
			bool bPaused = MeshComp->bPauseAnims;
			bool bNoSkelUpdate = MeshComp->bNoSkeletonUpdate;

			UE_LOG(LogTemp, Warning, TEXT("ANIM DIAG [%s] owner=%s"),
				*GetClass()->GetName(), *Char->GetName());
			UE_LOG(LogTemp, Warning, TEXT("  Mesh: %s"), *MeshName);
			UE_LOG(LogTemp, Warning, TEXT("  Skeleton: %s (%d bones)"), *SkelName, SkelBones);
			UE_LOG(LogTemp, Warning, TEXT("  AnimClass: %s"), *AnimClassName);
			UE_LOG(LogTemp, Warning, TEXT("  AnimMode: %d (0=BP, 1=SingleNode, 2=Custom)"), AnimMode);
			UE_LOG(LogTemp, Warning, TEXT("  ActiveAnimInstance: %s (isThis=%s)"),
				ActiveAnim ? *ActiveAnim->GetClass()->GetName() : TEXT("NULL"),
				bIsActive ? TEXT("YES") : TEXT("NO"));
			UE_LOG(LogTemp, Warning, TEXT("  bPauseAnims=%d bNoSkeletonUpdate=%d"),
				bPaused ? 1 : 0, bNoSkelUpdate ? 1 : 0);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ANIM DIAG [%s]: GetMesh() returned NULL!"), *GetClass()->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ANIM DIAG [%s]: No Character owner (pawn=%s)"),
			*GetClass()->GetName(), Owner ? *Owner->GetName() : TEXT("NULL"));
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

	// Cache BlendSpace node struct pointers for direct driving (one-time lookup)
	if (!bBSLookupDone)
	{
		bBSLookupDone = true;
		for (TFieldIterator<FStructProperty> It(GetClass()); It; ++It)
		{
			UScriptStruct* Struct = It->Struct;
			if (!Struct) continue;

			FString StructName = Struct->GetName();
			if (StructName.Contains(TEXT("BlendSpace")))
			{
				FProperty* XProp = Struct->FindPropertyByName(FName("X"));
				if (XProp)
				{
					CachedBSNodeProp = *It;
					CachedBSXProp = XProp;
					UE_LOG(LogTemp, Log, TEXT("EnemyAnimInstance [%s]: Found BlendSpace node '%s' with X property — direct driving enabled"),
						*GetClass()->GetName(), *It->GetName());
					break;
				}
			}
		}
		if (!CachedBSNodeProp)
		{
			UE_LOG(LogTemp, Warning, TEXT("EnemyAnimInstance [%s]: No BlendSpace node found — AnimGraph pin binding must work"),
				*GetClass()->GetName());
		}
	}

	// Set LocSpeed BP variable for the EventGraph/pin binding path.
	// NativeUpdateAnimation runs BEFORE BlueprintUpdateAnimation.
	// The EventGraph ALSO sets LocSpeed. We set it here too as a belt-and-suspenders.
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

void UEnemyAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// === FIX: NativeThreadSafeUpdateAnimation runs BEFORE AnimGraph pin binding ===
	// Actual evaluation order on worker thread:
	//   1. NativeThreadSafeUpdateAnimation (HERE)
	//   2. FExposedValueHandler copies LocSpeed → BlendSpacePlayer.X (pin binding)
	//   3. BlendSpacePlayer evaluates with X
	//
	// The EventGraph may have set LocSpeed to 0 (if TryGetPawnOwner returned null).
	// We MUST write LocSpeed = Speed HERE so pin binding copies the correct value.
	// Writing BS.X directly is useless — pin binding overwrites it in step 2.

	// FIX: Set LocSpeed BP variable = Speed (so pin binding gets correct value)
	if (FProperty* LocSpeedProp = GetClass()->FindPropertyByName(FName("LocSpeed")))
	{
		void* ValPtr = LocSpeedProp->ContainerPtrToValuePtr<void>(this);
		if (FFloatProperty* FP = CastField<FFloatProperty>(LocSpeedProp))
		{
			FP->SetPropertyValue(ValPtr, Speed);
		}
		else if (FDoubleProperty* DP = CastField<FDoubleProperty>(LocSpeedProp))
		{
			DP->SetPropertyValue(ValPtr, (double)Speed);
		}
	}

	// Also still write BS.X directly (belt-and-suspenders — in case there's no pin binding)
	if (CachedBSNodeProp && CachedBSXProp)
	{
		void* NodePtr = CachedBSNodeProp->ContainerPtrToValuePtr<void>(this);
		void* XPtr = CachedBSXProp->ContainerPtrToValuePtr<void>(NodePtr);

		// Read current X before we write (diagnostic)
		float OldX = 0.0f;
		if (FFloatProperty* FP = CastField<FFloatProperty>(CachedBSXProp))
		{
			OldX = FP->GetPropertyValue(XPtr);
			FP->SetPropertyValue(XPtr, Speed);
		}
		else if (FDoubleProperty* DP = CastField<FDoubleProperty>(CachedBSXProp))
		{
			OldX = (float)DP->GetPropertyValue(XPtr);
			DP->SetPropertyValue(XPtr, (double)Speed);
		}

		// Reduced diagnostic: log once every ~600 frames when moving
		DiagFrameCounter++;
		if (Speed > 3.0f && DiagFrameCounter % 600 == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("EnemyAnim [%s] Speed=%.1f BS.X=%.1f"),
				*GetClass()->GetName(), Speed, Speed);
		}
	}
}
