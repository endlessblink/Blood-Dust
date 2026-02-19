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
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

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
	UE_LOG(LogTemp, Display, TEXT("IntroSequence[v2-seqfix]: [1] StartSequence enter"));

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

	UE_LOG(LogTemp, Display, TEXT("IntroSequence[v2-seqfix]: [2] Finding camera/spring arm"));
	CachedCamera = Character->FindComponentByClass<UCameraComponent>();
	CachedSpringArm = Character->FindComponentByClass<USpringArmComponent>();
	if (!CachedCamera.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence: No camera component found, skipping"));
		return;
	}
	// Fundamental viewport safety: always run player camera unconstrained.
	CachedCamera->bConstrainAspectRatio = false;
	CachedCamera->AspectRatio = 16.0f / 9.0f;

	UE_LOG(LogTemp, Display, TEXT("IntroSequence[v2-seqfix]: [3] Disabling input"));
	CachedPC->DisableInput(CachedPC.Get());

	UE_LOG(LogTemp, Display, TEXT("IntroSequence[v2-seqfix]: [4] Starting camera fade"));
	if (APlayerCameraManager* CamMgr = CachedPC->PlayerCameraManager)
	{
		CamMgr->StartCameraFade(1.0f, 1.0f, 0.01f, FLinearColor::Black, false, true);
	}

	SetComponentTickEnabled(true);
	Activate();

	// Deterministic path: always attempt title prelude first.
	if (SetupTitleScene())
	{
		TransitionTo(EIntroState::TitleFadingIn);
		return;
	}
	UE_LOG(LogTemp, Error, TEXT("IntroSequence[v2-seqfix]: Prelude setup failed. Aborting intro sequence."));
	if (CachedPC.IsValid())
	{
		if (APlayerCameraManager* CamMgr = CachedPC->PlayerCameraManager)
		{
			CamMgr->StartCameraFade(1.0f, 0.0f, 0.2f, FLinearColor::Black, false, false);
		}
		CachedPC->EnableInput(CachedPC.Get());
	}
	SetComponentTickEnabled(false);
	Deactivate();
}

bool UIntroSequenceComponent::SetupTitleScene()
{
	if (!CachedCharacter.IsValid() || !CachedPC.IsValid())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UStaticMesh* TitleMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Title/Meshes/SM_Title_BloodAndRust.SM_Title_BloodAndRust"));
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* TitleMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Title/Materials/M_Title_BloodAndRust.M_Title_BloodAndRust"));
	UMaterialInterface* BackdropMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Title/Materials/M_IntroBackdrop_Black.M_IntroBackdrop_Black"));

	if (!TitleMesh || !CubeMesh || !BackdropMat)
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence[v2-seqfix]: Title setup failed, missing title/backdrop assets"));
		return false;
	}

	const FVector CharLoc = CachedCharacter->GetActorLocation();
	const FVector Forward = CachedCharacter->GetActorForwardVector().GetSafeNormal();
	const FVector Right = CachedCharacter->GetActorRightVector().GetSafeNormal();

	const FVector TitleLoc = CharLoc + Forward * 650.0f + FVector(0.0f, 0.0f, 130.0f);
	const FVector CameraLoc = TitleLoc - Forward * 500.0f + FVector(0.0f, 0.0f, 40.0f);
	const FRotator CameraRot = (TitleLoc - CameraLoc).Rotation();

	TitleCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraLoc, CameraRot);
	if (!TitleCamera.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence[v2-seqfix]: Title setup failed, camera spawn"));
		CleanupTitleScene();
		return false;
	}
	if (UCameraComponent* TitleCamComp = TitleCamera->GetCameraComponent())
	{
		// Prevent any pillarbox/letterbox carryover from intro camera.
		TitleCamComp->bConstrainAspectRatio = false;
		TitleCamComp->AspectRatio = 16.0f / 9.0f;
		TitleCamComp->FieldOfView = 55.0f;
	}

	TitleMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), TitleLoc, FRotator(0.0f, CachedCharacter->GetActorRotation().Yaw + 180.0f, 0.0f));
	if (!TitleMeshActor.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence[v2-seqfix]: Title setup failed, mesh spawn"));
		CleanupTitleScene();
		return false;
	}
	TitleMeshActor->SetActorScale3D(FVector(2.2f));
	if (UStaticMeshComponent* MeshComp = TitleMeshActor->GetStaticMeshComponent())
	{
		MeshComp->SetStaticMesh(TitleMesh);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetCastShadow(false);
		if (TitleMat)
		{
			MeshComp->SetMaterial(0, TitleMat);
		}
	}

	const FVector CamForward = TitleCamera->GetActorForwardVector().GetSafeNormal();
	// Backdrop must be BEHIND the title from camera perspective (not in front of it).
	const FVector BackdropLoc = TitleLoc + CamForward * 260.0f;
	const FRotator BackdropRot = FRotationMatrix::MakeFromY(CamForward).Rotator();
	TitleBackdropActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), BackdropLoc, BackdropRot);
	if (!TitleBackdropActor.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence[v2-seqfix]: Title setup failed, backdrop spawn"));
		CleanupTitleScene();
		return false;
	}
	TitleBackdropActor->SetActorScale3D(FVector(60.0f, 0.05f, 40.0f));
	if (UStaticMeshComponent* BackdropComp = TitleBackdropActor->GetStaticMeshComponent())
	{
		BackdropComp->SetStaticMesh(CubeMesh);
		BackdropComp->SetMaterial(0, BackdropMat);
		BackdropComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BackdropComp->SetCastShadow(false);
	}

	TitleLightA = World->SpawnActor<APointLight>(APointLight::StaticClass(), TitleLoc + Right * 180.0f + FVector(0.0f, 0.0f, -60.0f), FRotator::ZeroRotator);
	TitleLightB = World->SpawnActor<APointLight>(APointLight::StaticClass(), TitleLoc - Right * 180.0f + FVector(0.0f, 0.0f, -60.0f), FRotator::ZeroRotator);
	for (TWeakObjectPtr<APointLight> Light : { TitleLightA, TitleLightB })
	{
		if (Light.IsValid())
		{
			if (UPointLightComponent* PointComp = Cast<UPointLightComponent>(Light->GetLightComponent()))
			{
				PointComp->SetIntensity(4500.0f);
				PointComp->SetLightColor(FLinearColor(1.0f, 0.42f, 0.08f));
				PointComp->SetAttenuationRadius(420.0f);
			}
		}
	}

	UNiagaraSystem* FXSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/FX/NS_FloatingDust.NS_FloatingDust"));
	if (FXSystem)
	{
		TitleFXA = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, FXSystem, TitleLoc + Right * 140.0f + FVector(0.0f, 0.0f, -55.0f),
			FRotator::ZeroRotator, FVector(0.9f), true, true, ENCPoolMethod::None, true);
		TitleFXB = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, FXSystem, TitleLoc - Right * 140.0f + FVector(0.0f, 0.0f, -55.0f),
			FRotator::ZeroRotator, FVector(0.9f), true, true, ENCPoolMethod::None, true);
	}

	OriginalViewTarget = CachedPC->GetViewTarget();
	CachedPC->SetViewTargetWithBlend(TitleCamera.Get(), 0.0f);
	if (APlayerCameraManager* CamMgr = CachedPC->PlayerCameraManager)
	{
		CamMgr->StartCameraFade(1.0f, 0.0f, TitleFadeInDuration, FLinearColor::Black, false, false);
	}
	UE_LOG(LogTemp, Display, TEXT("IntroSequence[v2-seqfix]: Title prelude started"));

	// Safety cleanup: if any state transition is interrupted, force-remove title scene actors.
	const float SafetyDelay = FMath::Max(TitleFadeInDuration + TitleHoldDuration + TitleFadeOutDuration + 1.0f, 2.0f);
	TWeakObjectPtr<UIntroSequenceComponent> WeakThis(this);
	FTimerHandle SafetyCleanupTimer;
	World->GetTimerManager().SetTimer(
		SafetyCleanupTimer,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->CleanupTitleScene();
			}
		},
		SafetyDelay,
		false
	);

	return true;
}

void UIntroSequenceComponent::CleanupTitleScene()
{
	if (TitleFXA.IsValid())
	{
		TitleFXA->Deactivate();
		TitleFXA->DestroyComponent();
	}
	if (TitleFXB.IsValid())
	{
		TitleFXB->Deactivate();
		TitleFXB->DestroyComponent();
	}
	if (TitleLightA.IsValid())
	{
		TitleLightA->Destroy();
	}
	if (TitleLightB.IsValid())
	{
		TitleLightB->Destroy();
	}
	if (TitleMeshActor.IsValid())
	{
		TitleMeshActor->Destroy();
	}
	if (TitleBackdropActor.IsValid())
	{
		TitleBackdropActor->Destroy();
	}
	if (TitleCamera.IsValid())
	{
		TitleCamera->Destroy();
	}
}

void UIntroSequenceComponent::StartMainIntroPhase()
{
	// Ensure no title-prelude actors can leak into gameplay view.
	CleanupTitleScene();
	if (CachedPC.IsValid() && CachedCharacter.IsValid())
	{
		CachedPC->SetViewTargetWithBlend(CachedCharacter.Get(), 0.0f);
	}
	if (CachedCamera.IsValid())
	{
		CachedCamera->bConstrainAspectRatio = false;
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
	case EIntroState::TitleFadingIn:
	{
		if (StateTimer >= FMath::Max(TitleFadeInDuration, 0.01f))
		{
			UE_LOG(LogTemp, Display, TEXT("IntroSequence[v2-seqfix]: TitleFadingIn complete -> TitleShowing"));
			TransitionTo(EIntroState::TitleShowing);
		}
		break;
	}

	case EIntroState::TitleShowing:
	{
		if (StateTimer >= FMath::Max(TitleHoldDuration, 0.01f))
		{
			UE_LOG(LogTemp, Display, TEXT("IntroSequence[v2-seqfix]: TitleShowing complete -> TitleFadingOut"));
			if (CachedPC.IsValid() && CachedPC->PlayerCameraManager)
			{
				CachedPC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, TitleFadeOutDuration, FLinearColor::Black, false, true);
			}
			TransitionTo(EIntroState::TitleFadingOut);
		}
		break;
	}

	case EIntroState::TitleFadingOut:
	{
		if (StateTimer >= FMath::Max(TitleFadeOutDuration, 0.01f))
		{
			UE_LOG(LogTemp, Display, TEXT("IntroSequence[v2-seqfix]: TitleFadingOut complete -> MainIntro"));
			if (CachedPC.IsValid())
			{
				AActor* ViewTarget = OriginalViewTarget.IsValid() ? OriginalViewTarget.Get() : CachedCharacter.Get();
				if (ViewTarget)
				{
					CachedPC->SetViewTargetWithBlend(ViewTarget, 0.0f);
				}
			}
			CleanupTitleScene();
			StartMainIntroPhase();
		}
		break;
	}

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
				UGameplayStatics::PlaySound2D(GetWorld(), GettingUpSound, 0.7f, 1.0f);
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
		CleanupTitleScene();
		if (CachedPC.IsValid() && CachedCharacter.IsValid())
		{
			CachedPC->SetViewTargetWithBlend(CachedCharacter.Get(), 0.0f);
		}
		if (CachedCamera.IsValid())
		{
			CachedCamera->bConstrainAspectRatio = false;
		}
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
