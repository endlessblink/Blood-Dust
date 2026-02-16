#include "IntroSequenceComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Engine/PointLight.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

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

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: [11] Enabling tick, activating"));
	SetComponentTickEnabled(true);
	Activate();
	if (SetupTitleScene())
	{
		if (APlayerCameraManager* CamMgr = CachedPC->PlayerCameraManager)
		{
			CamMgr->StartCameraFade(1.0f, 0.0f, TitleFadeInDuration, FLinearColor::Black, false, false);
		}
		TransitionTo(EIntroState::TitleFadingIn);
		UE_LOG(LogTemp, Display, TEXT("IntroSequence: [12] StartSequence complete, entering TitleFadingIn"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("IntroSequence: Title scene setup failed, skipping to intro animation"));
	StartMainIntroPhase();
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
	if (!TitleMesh)
	{
		return false;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BackdropMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Title/Materials/M_IntroBackdrop_Black.M_IntroBackdrop_Black"));
	UNiagaraSystem* FireSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/FX/NS_FloatingDust.NS_FloatingDust"));

	const FVector CharLoc = CachedCharacter->GetActorLocation();
	const FVector Forward = CachedCharacter->GetActorForwardVector().GetSafeNormal();
	const FVector Right = CachedCharacter->GetActorRightVector().GetSafeNormal();
	const FVector Up = FVector::UpVector;

	const FVector SceneOrigin = CharLoc + Forward * 500.0f + Up * 220.0f;
	const FVector CameraLoc = SceneOrigin - Forward * 520.0f + Up * 70.0f;
	const FRotator CameraRot = (SceneOrigin - CameraLoc).Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = CachedCharacter.Get();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACameraActor* TitleCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraLoc, CameraRot, SpawnParams);
	if (!TitleCamera)
	{
		return false;
	}
	SpawnedTitleActors.Add(TitleCamera);

	AStaticMeshActor* TitleActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SceneOrigin, Forward.Rotation(), SpawnParams);
	if (!TitleActor)
	{
		CleanupTitleScene();
		return false;
	}
	if (UStaticMeshComponent* MeshComp = TitleActor->GetStaticMeshComponent())
	{
		MeshComp->SetStaticMesh(TitleMesh);
		MeshComp->SetWorldScale3D(FVector(1.8f, 1.8f, 1.8f));
		MeshComp->SetMobility(EComponentMobility::Movable);
		MeshComp->SetCastShadow(true);
	}
	SpawnedTitleActors.Add(TitleActor);

	if (CubeMesh)
	{
		const FVector BackdropLoc = SceneOrigin + Forward * 180.0f + Up * 10.0f;
		AStaticMeshActor* BackdropActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), BackdropLoc, Forward.Rotation(), SpawnParams);
		if (BackdropActor)
		{
			if (UStaticMeshComponent* BackdropComp = BackdropActor->GetStaticMeshComponent())
			{
				BackdropComp->SetStaticMesh(CubeMesh);
				BackdropComp->SetWorldScale3D(FVector(24.0f, 24.0f, 20.0f));
				BackdropComp->SetMobility(EComponentMobility::Movable);
				if (BackdropMaterial)
				{
					BackdropComp->SetMaterial(0, BackdropMaterial);
				}
			}
			SpawnedTitleActors.Add(BackdropActor);
		}
	}

	// Warm lights to make dust particles read like embers/fire.
	const TArray<FVector> LightOffsets = {
		Right * 210.0f + Up * 100.0f,
		-Right * 210.0f + Up * 100.0f,
		Forward * 130.0f + Up * 120.0f
	};
	for (const FVector& Offset : LightOffsets)
	{
		APointLight* LightActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), SceneOrigin + Offset, FRotator::ZeroRotator, SpawnParams);
		if (LightActor && LightActor->GetLightComponent())
		{
			if (UPointLightComponent* PointComp = Cast<UPointLightComponent>(LightActor->GetLightComponent()))
			{
				PointComp->SetIntensity(9000.0f);
				PointComp->SetAttenuationRadius(420.0f);
				PointComp->SetLightColor(FLinearColor(1.0f, 0.32f, 0.06f));
				PointComp->SetUseTemperature(true);
				PointComp->SetTemperature(2000.0f);
			}
			SpawnedTitleActors.Add(LightActor);
		}
	}

	if (FireSystem)
	{
		const TArray<FVector> FxOffsets = {
			Right * 180.0f - Up * 10.0f,
			-Right * 180.0f - Up * 10.0f,
			Forward * 60.0f - Up * 20.0f
		};
		for (const FVector& Offset : FxOffsets)
		{
			UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				FireSystem,
				SceneOrigin + Offset,
				FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f),
				FVector(1.6f, 1.6f, 1.6f),
				true,
				true
			);
			if (NiagaraComp)
			{
				SpawnedTitleNiagara.Add(NiagaraComp);
			}
		}
	}

	PreviousViewTarget = CachedPC->GetViewTarget();
	CachedPC->SetViewTarget(TitleCamera);
	return true;
}

void UIntroSequenceComponent::CleanupTitleScene()
{
	for (const TWeakObjectPtr<UNiagaraComponent>& Comp : SpawnedTitleNiagara)
	{
		if (Comp.IsValid())
		{
			Comp->DestroyComponent();
		}
	}
	SpawnedTitleNiagara.Empty();

	for (const TWeakObjectPtr<AActor>& Actor : SpawnedTitleActors)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
		}
	}
	SpawnedTitleActors.Empty();
}

void UIntroSequenceComponent::StartMainIntroPhase()
{
	if (!CachedCharacter.IsValid() || !CachedCamera.IsValid() || !CachedPC.IsValid())
	{
		TransitionTo(EIntroState::Complete);
		return;
	}

	FName ActualHeadBone = FindHeadBone();
	if (ActualHeadBone == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("IntroSequence: No head bone found, skipping sequence"));
		CachedPC->EnableInput(CachedPC.Get());
		TransitionTo(EIntroState::Complete);
		return;
	}

	OriginalCameraSocket = CachedCamera->GetAttachSocketName();

	if (CachedSpringArm.IsValid())
	{
		CachedSpringArm->SetActive(false);
	}

	CachedCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	TrackedBoneName = ActualHeadBone;
	UpdateCameraFromBone();

	AnimationDuration = GettingUpAnimation ? GettingUpAnimation->GetPlayLength() : 0.0f;
	TransitionTo(EIntroState::FadingIn);
}

void UIntroSequenceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTimer += DeltaTime;

	switch (CurrentState)
	{
	case EIntroState::TitleFadingIn:
	{
		if (StateTimer >= TitleFadeInDuration)
		{
			TransitionTo(EIntroState::TitleShowing);
		}
		break;
	}

	case EIntroState::TitleShowing:
	{
		if (StateTimer >= TitleHoldDuration)
		{
			if (CachedPC.IsValid())
			{
				if (APlayerCameraManager* CamMgr = CachedPC->PlayerCameraManager)
				{
					CamMgr->StartCameraFade(0.0f, 1.0f, TitleFadeOutDuration, FLinearColor::Black, false, true);
				}
			}
			TransitionTo(EIntroState::TitleFadingOut);
		}
		break;
	}

	case EIntroState::TitleFadingOut:
	{
		if (StateTimer >= TitleFadeOutDuration)
		{
			CleanupTitleScene();
			if (CachedPC.IsValid())
			{
				if (PreviousViewTarget.IsValid())
				{
					CachedPC->SetViewTarget(PreviousViewTarget.Get());
				}
				else if (CachedCharacter.IsValid())
				{
					CachedPC->SetViewTarget(CachedCharacter.Get());
				}
			}
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
