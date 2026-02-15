#include "IntroSequenceComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UIntroSequenceComponent::UIntroSequenceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bAutoActivate = false;
}

FName UIntroSequenceComponent::FindHeadBone() const
{
	if (!CachedCharacter.IsValid())
	{
		return NAME_None;
	}

	USkeletalMeshComponent* MeshComp = CachedCharacter->GetMesh();
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return NAME_None;
	}

	const FReferenceSkeleton& RefSkel = MeshComp->GetSkeletalMeshAsset()->GetRefSkeleton();

	// Try configured name first, then common fallbacks
	TArray<FName> Candidates = { HeadBoneName, FName("Head"), FName("head"), FName("mixamorig:Head") };
	for (const FName& Name : Candidates)
	{
		if (RefSkel.FindBoneIndex(Name) != INDEX_NONE)
		{
			return Name;
		}
	}

	// Case-insensitive search as last resort
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		FString BoneName = RefSkel.GetBoneName(i).ToString();
		if (BoneName.Contains(TEXT("head"), ESearchCase::IgnoreCase))
		{
			return RefSkel.GetBoneName(i);
		}
	}

	return NAME_None;
}

void UIntroSequenceComponent::StartSequence()
{
	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [1] StartSequence enter"));

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence: No owner character"));
		return;
	}

	CachedCharacter = Character;
	CachedPC = Cast<APlayerController>(Character->GetController());
	if (!CachedPC.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence: No player controller"));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [2] Finding camera/spring arm"));
	CachedCamera = Character->FindComponentByClass<UCameraComponent>();
	CachedSpringArm = Character->FindComponentByClass<USpringArmComponent>();
	if (!CachedCamera.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence: No camera component found, skipping"));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [3] Disabling input"));
	CachedPC->DisableInput(CachedPC.Get());

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [4] Starting camera fade"));
	if (APlayerCameraManager* CamMgr = CachedPC->PlayerCameraManager)
	{
		CamMgr->StartCameraFade(1.0f, 1.0f, 0.01f, FLinearColor::Black, false, true);
	}

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [5] Finding head bone"));
	FName ActualHeadBone = FindHeadBone();
	if (ActualHeadBone == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence: No head bone found, skipping sequence"));
		CachedPC->EnableInput(CachedPC.Get());
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [6] Head bone = %s"), *ActualHeadBone.ToString());

	OriginalCameraSocket = CachedCamera->GetAttachSocketName();

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [7] Deactivating spring arm"));
	if (CachedSpringArm.IsValid())
	{
		CachedSpringArm->SetActive(false);
	}

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [8] Detaching camera"));
	CachedCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	TrackedBoneName = ActualHeadBone;

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [9] UpdateCameraFromBone"));
	UpdateCameraFromBone();

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [10] Caching anim duration"));
	if (GettingUpAnimation)
	{
		AnimationDuration = GettingUpAnimation->GetPlayLength();
		UE_LOG(LogTemp, Display, TEXT("IntroSequence: Animation duration = %.2f"), AnimationDuration);
	}

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [11] Enabling tick, activating"));
	SetComponentTickEnabled(true);
	Activate();
	TransitionTo(EIntroState::FadingIn);
	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [12] StartSequence complete, entering FadingIn"));
}

void UIntroSequenceComponent::UpdateCameraFromBone()
{
	if (!CachedCamera.IsValid() || !CachedCharacter.IsValid())
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = CachedCharacter->GetMesh();
	if (!MeshComp)
	{
		return;
	}

	FVector BoneLocation = MeshComp->GetSocketLocation(TrackedBoneName);
	FVector CharForward = CachedCharacter->GetActorForwardVector();

	// Push camera forward from head bone + slightly above to simulate eye position
	FVector EyePosition = BoneLocation + CharForward * 40.0f + FVector(0.0f, 0.0f, 15.0f);

	// POV pitch: when lying down the head is low → look up at sky;
	// as the robot stands, head rises → pitch tilts down to horizontal
	float CharZ = CachedCharacter->GetActorLocation().Z;
	float HeadHeight = BoneLocation.Z - CharZ;
	float StandingHeight = 160.0f;
	UCapsuleComponent* Capsule = CachedCharacter->GetCapsuleComponent();
	if (Capsule)
	{
		StandingHeight = Capsule->GetScaledCapsuleHalfHeight();
	}
	float HeightRatio = FMath::Clamp(HeadHeight / FMath::Max(StandingHeight, 1.0f), 0.0f, 1.0f);
	float Pitch = FMath::Lerp(15.0f, 0.0f, HeightRatio);

	FRotator CharRot = CachedCharacter->GetActorRotation();
	CachedCamera->SetWorldLocationAndRotation(EyePosition, FRotator(Pitch, CharRot.Yaw, 0.0f));
}

void UIntroSequenceComponent::TransitionTo(EIntroState NewState)
{
	CurrentState = NewState;
	StateTimer = 0.0f;
}

void UIntroSequenceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTimer += DeltaTime;

	switch (CurrentState)
	{
	case EIntroState::FadingIn:
	{
		// Track head bone per-frame (even during black, to be in position when fade reveals)
		UpdateCameraFromBone();

		if (StateTimer >= InitialBlackHoldDuration)
		{
			// Start the fade from black to clear
			if (CachedPC.IsValid())
			{
				if (APlayerCameraManager* CamMgr = CachedPC->PlayerCameraManager)
				{
					CamMgr->StartCameraFade(1.0f, 0.0f, FadeInDuration, FLinearColor::Black, false, false);
				}
			}

			// Play the getting-up animation on DefaultSlot
			UE_LOG(LogTemp, Display, TEXT("IntroSequence: [TICK] Playing animation"));
			if (GettingUpAnimation && CachedCharacter.IsValid())
			{
				USkeletalMeshComponent* MeshComp = CachedCharacter->GetMesh();
				if (MeshComp)
				{
					UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
					UE_LOG(LogTemp, Display, TEXT("IntroSequence: [TICK] AnimInst=%s"), AnimInst ? TEXT("valid") : TEXT("NULL"));
					if (AnimInst)
					{
						AnimInst->PlaySlotAnimationAsDynamicMontage(
							GettingUpAnimation,
							FName("DefaultSlot"),
							0.25f,  // BlendIn
							0.25f,  // BlendOut
							1.0f,   // PlayRate
							1,      // LoopCount
							-1.0f,  // BlendOutTriggerTime
							0.0f    // InTimeToStartMontageAt
						);
						UE_LOG(LogTemp, Display, TEXT("IntroSequence: [TICK] Montage started"));
					}
				}
			}

			// Play getting-up sound effect
			if (GettingUpSound)
			{
				UE_LOG(LogTemp, Display, TEXT("IntroSequence: [TICK] Playing sound"));
				UGameplayStatics::PlaySound2D(GetWorld(), GettingUpSound);
			}

			TransitionTo(EIntroState::PlayingAnimation);
			UE_LOG(LogTemp, Display, TEXT("IntroSequence: [TICK] Transitioned to PlayingAnimation"));
		}
		break;
	}

	case EIntroState::PlayingAnimation:
	{
		// Track head bone per-frame for 1st-person POV
		UpdateCameraFromBone();

		// Wait for animation to nearly finish (leave blend-out margin)
		float TargetTime = FMath::Max(AnimationDuration - 0.25f, 0.5f);
		if (StateTimer >= TargetTime)
		{
			// Capture camera world transform before drifting to 3rd-person
			if (CachedCamera.IsValid())
			{
				CapturedCameraTransform = CachedCamera->GetComponentTransform();
			}

			// Re-enable spring arm so it starts calculating its endpoint
			if (CachedSpringArm.IsValid())
			{
				CachedSpringArm->SetActive(true);
			}

			TransitionTo(EIntroState::DriftingCamera);
		}
		break;
	}

	case EIntroState::DriftingCamera:
	{
		float Alpha = FMath::Clamp(StateTimer / FMath::Max(CameraDriftDuration, 0.01f), 0.0f, 1.0f);
		float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

		if (CachedCamera.IsValid() && CachedSpringArm.IsValid())
		{
			// Spring arm socket gives us the target 3rd-person camera position
			FTransform TargetTransform = CachedSpringArm->GetSocketTransform(USpringArmComponent::SocketName);

			FVector LerpedLocation = FMath::Lerp(
				CapturedCameraTransform.GetLocation(),
				TargetTransform.GetLocation(),
				EasedAlpha
			);
			FQuat LerpedRotation = FQuat::Slerp(
				CapturedCameraTransform.GetRotation(),
				TargetTransform.GetRotation(),
				EasedAlpha
			);

			CachedCamera->SetWorldLocationAndRotation(LerpedLocation, LerpedRotation.Rotator());
		}

		if (Alpha >= 1.0f)
		{
			// Re-attach camera to spring arm at its original socket
			if (CachedCamera.IsValid() && CachedSpringArm.IsValid())
			{
				CachedCamera->AttachToComponent(
					CachedSpringArm.Get(),
					FAttachmentTransformRules::SnapToTargetNotIncludingScale,
					USpringArmComponent::SocketName
				);
			}

			TransitionTo(EIntroState::Complete);
		}
		break;
	}

	case EIntroState::Complete:
	{
		if (CachedPC.IsValid())
		{
			CachedPC->EnableInput(CachedPC.Get());
		}

		SetComponentTickEnabled(false);
		Deactivate();
		break;
	}

	default:
		break;
	}
}
