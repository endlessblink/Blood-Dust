// Intro camera sequence component: 1st-person head-bone POV during "getting up" animation,
// fade from black, then smooth camera drift back to 3rd-person spring arm.
// Dynamically created by UGameplayHelperLibrary::StartIntroSequence().

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IntroSequenceComponent.generated.h"

class ACharacter;
class APlayerController;
class UCameraComponent;
class USpringArmComponent;
class UAnimSequence;
class USoundBase;
class UAudioComponent;

UENUM()
enum class EIntroState : uint8
{
	PendingStart,
	FadingIn,
	PlayingAnimation,
	DriftingCamera,
	Complete
};

UCLASS()
class GAMEPLAYHELPERS_API UIntroSequenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UIntroSequenceComponent();

	UPROPERTY()
	UAnimSequence* GettingUpAnimation = nullptr;

	UPROPERTY()
	FName HeadBoneName = FName("Head");

	UPROPERTY()
	float FadeInDuration = 1.0f;

	UPROPERTY()
	float CameraDriftDuration = 1.5f;

	UPROPERTY()
	float InitialBlackHoldDuration = 0.3f;

	UPROPERTY()
	USoundBase* GettingUpSound = nullptr;

	void StartSequence();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	EIntroState CurrentState = EIntroState::PendingStart;
	float StateTimer = 0.0f;
	float AnimationDuration = 0.0f;

	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UCameraComponent> CachedCamera;
	TWeakObjectPtr<USpringArmComponent> CachedSpringArm;
	TWeakObjectPtr<APlayerController> CachedPC;

	FTransform CapturedCameraTransform;
	FName OriginalCameraSocket;
	FName TrackedBoneName;
	TArray<TWeakObjectPtr<UAudioComponent>> PausedAudioComponents;

	void UpdateCameraFromBone();
	FName FindHeadBone() const;
	void TransitionTo(EIntroState NewState);
};
