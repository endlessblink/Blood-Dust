#include "GameplayHelperLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "TimerManager.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Engine/Texture2D.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

void UGameplayHelperLibrary::SetCharacterWalkSpeed(ACharacter* Character, float NewSpeed)
{
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	MovementComp->MaxWalkSpeed = NewSpeed;
}

void UGameplayHelperLibrary::PlayAnimationOneShot(ACharacter* Character, UAnimSequence* AnimSequence, float PlayRate, float BlendIn, float BlendOut, bool bStopMovement, bool bForceInterrupt)
{
	if (!Character || !AnimSequence)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	if (!MeshComp)
	{
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		return;
	}

	if (bForceInterrupt)
	{
		// Instantly stop any playing montage so the new one can start immediately.
		// Used for hit-react, scream, and other interrupting animations.
		AnimInst->Montage_Stop(0.0f);
	}
	else if (AnimInst->Montage_IsPlaying(nullptr))
	{
		// Non-interrupt mode: if a montage is already playing, ignore the new request.
		// This prevents attack animations from restarting on rapid triggers.
		return;
	}

	// Stop movement during the animation if requested
	UCharacterMovementComponent* MovementComp = nullptr;
	float SavedSpeed = 0.0f;
	if (bStopMovement)
	{
		MovementComp = Character->GetCharacterMovement();
		if (MovementComp)
		{
			SavedSpeed = MovementComp->MaxWalkSpeed;
			MovementComp->MaxWalkSpeed = 0.0f;
			MovementComp->StopMovementImmediately();
		}
	}

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

	// Set timer to restore movement speed after animation completes
	if (bStopMovement && MovementComp)
	{
		float Duration = AnimSequence->GetPlayLength() / FMath::Max(PlayRate, 0.01f);
		// Restore slightly before animation ends (during blend out) for smoother feel
		float RestoreTime = FMath::Max(Duration - BlendOut, 0.1f);

		FTimerHandle TimerHandle;
		TWeakObjectPtr<UCharacterMovementComponent> WeakMoveComp(MovementComp);
		Character->GetWorldTimerManager().SetTimer(
			TimerHandle,
			[WeakMoveComp, SavedSpeed]()
			{
				if (WeakMoveComp.IsValid())
				{
					WeakMoveComp->MaxWalkSpeed = SavedSpeed;
				}
			},
			RestoreTime,
			false // Don't loop
		);
	}
}

void UGameplayHelperLibrary::AddInputMappingContextToCharacter(ACharacter* Character, UInputMappingContext* MappingContext, int32 Priority)
{
	if (!Character || !MappingContext)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	Subsystem->AddMappingContext(MappingContext, Priority);

	// Ensure game viewport captures keyboard input — without this,
	// keyboard doesn't register until user clicks in the viewport
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
}

// --- Blocking State ---
static TSet<TWeakObjectPtr<AActor>> BlockingActors;

// --- Player Health HUD ---

struct FPlayerHUDState
{
	TWeakObjectPtr<UWorld> OwnerWorld;
	TSharedPtr<SWidget> RootWidget;
	TSharedPtr<SWidget> DeathOverlay;

	// Texture-based health bar (3-layer: base → fill → frame mask)
	TStrongObjectPtr<UTexture2D> BaseBarTexture;
	TStrongObjectPtr<UTexture2D> FillBarTexture;
	TStrongObjectPtr<UTexture2D> FrameBarTexture;
	FSlateBrush BaseBrush;   // Layer 0: frame + dark empty tube
	FSlateBrush FillBrush;   // Layer 1: golden liquid (clipped by HP%)
	FSlateBrush FrameBrush;  // Layer 2: frame with transparent tube hole (masks fill)
	TSharedPtr<SBox> HealthClipBox;
	TSharedPtr<SBorder> DamageFlashBorder;
	float MaxHealth = 50.0f;
	double DamageFlashStartTime = 0.0;
	bool bCreated = false;
	bool bDead = false;

	// Game flow UI
	TSharedPtr<SBorder> GoldenFlashBorder;
	TSharedPtr<STextBlock> CheckpointText;
	TSharedPtr<SWidget> VictoryOverlay;
	TSharedPtr<STextBlock> VictoryCheckpointText;
};
static FPlayerHUDState PlayerHUD;

// --- Game Flow (Checkpoint + Victory) ---

enum class ECheckpointState : uint8
{
	Active,      // Waiting for player
	Collecting,  // Intensity ramp animation
	Collected    // Destroyed
};

struct FCheckpointData
{
	TWeakObjectPtr<AActor> LightActor;
	FVector Location;
	ECheckpointState State = ECheckpointState::Active;
	double CollectStartTime = 0.0;
	float OriginalIntensity = 3000.0f;
	float OriginalAttenuationRadius = 1000.0f;
	bool bIsBeacon = false; // true if this is the current "next" checkpoint beacon
};

struct FGameFlowState
{
	TWeakObjectPtr<UWorld> OwnerWorld;
	bool bInitialized = false;
	bool bVictory = false;
	TArray<FCheckpointData> Checkpoints;
	int32 CheckpointsCollected = 0;
	int32 TotalCheckpoints = 0;
	TWeakObjectPtr<AActor> PortalLightActor;
	FVector PortalLocation = FVector::ZeroVector;
	double GoldenFlashStartTime = 0.0;
	double CheckpointTextStartTime = 0.0;
	double VictoryStartTime = 0.0;
	FString CheckpointDisplayText;

	// Directional light dimming
	TWeakObjectPtr<AActor> DirectionalLightActor;
	float OriginalDirLightIntensity = 1.0f;
	float DimPerCheckpoint = 0.15f; // 15% dimmer per checkpoint
};
static FGameFlowState GameFlow;

// --- Enemy AI State Machine ---

enum class EEnemyAIState : uint8
{
	Idle,
	Chase,
	Attack,
	Return,
	HitReact,
	Dead,
	Patrol
};

enum class EEnemyPersonality : uint8
{
	Normal,     // Balanced stats, standard behavior
	Berserker,  // Fast, close aggro, aggressive, punching
	Stalker,    // Slow approach, wide aggro, biting attacks
	Brute,      // Slow, high damage, zombie attacks
	Crawler     // Crawl movement, low profile, biting
};

enum class EIdleBehavior : uint8
{
	Stand,
	LookAround,
	Wander,
	Scream
};

struct FEnemyAIStateData
{
	FVector SpawnLocation;
	double LastAttackTime = 0.0;
	double HitReactStartTime = 0.0;
	double DeathStartTime = 0.0;
	float PreviousHealth = -1.f; // -1 = uninitialized
	EEnemyAIState CurrentState = EEnemyAIState::Idle;
	EEnemyAIState PreHitReactState = EEnemyAIState::Idle; // state to restore after hit react
	bool bInitialized = false;
	bool bHealthInitialized = false;
	bool bDeathAnimStarted = false;
	bool bDeathBreakStarted = false;
	double DeathBreakStartTime = 0.0;
	TArray<TWeakObjectPtr<AActor>> DebrisActors;

	// Per-instance randomization for organic behavior
	float SpeedMultiplier = 1.0f;
	float AggroRangeMultiplier = 1.0f;
	float ReactionDelay = 0.0f;
	float AttackCooldownJitter = 0.0f;
	float WobblePhase = 0.0f;
	float WobbleAmplitude = 0.0f;
	float AnimPlayRateVariation = 1.0f;
	double AggroStartTime = 0.0;
	bool bAggroReactionDone = false;

	// Personality archetype (assigned once on init)
	EEnemyPersonality Personality = EEnemyPersonality::Normal;
	float DamageMultiplier = 1.0f;

	// Per-instance selected animations (chosen from available pool on init)
	UAnimSequence* ChosenAttackAnim = nullptr;
	UAnimSequence* ChosenDeathAnim = nullptr;
	UAnimSequence* ChosenHitReactAnim = nullptr;

	// Idle behavior
	EIdleBehavior CurrentIdleBehavior = EIdleBehavior::Stand;
	float IdleBehaviorTimer = 0.0f;
	float NextIdleBehaviorTime = 0.0f;
	FVector IdleWanderTarget = FVector::ZeroVector;
	bool bIdleBehaviorActive = false;
	float IdleScreamEndTime = 0.0f;

	// Patrol behavior
	FVector PatrolTarget = FVector::ZeroVector;
	float PatrolPauseTimer = 0.0f;
	float PatrolPauseDuration = 0.0f;
	bool bPatrolPausing = false;

	// Combat partner (visual fighting)
	double LastPartnerAttackTime = 0.0;
	float PartnerAttackCooldown = 0.0f;
	TWeakObjectPtr<AActor> AutoDiscoveredPartner;
	bool bPartnerSearchDone = false;

	// Floating health bar
	TWeakObjectPtr<UWidgetComponent> HealthBarComponent;
	float MaxHealth = 100.f;
};
static TMap<TWeakObjectPtr<AActor>, FEnemyAIStateData> EnemyAIStates;

// --- Per-Enemy-Type Sound Cache ---

struct FEnemyTypeSounds
{
	USoundBase* Hit = nullptr;
	USoundBase* GettingHit = nullptr;
	USoundBase* Steps = nullptr;
};

static TMap<FString, FEnemyTypeSounds> EnemyTypeSoundCache;

static FString GetEnemyTypeKey(AActor* Actor)
{
	if (!Actor) return FString();
	FString ClassName = Actor->GetClass()->GetName();
	if (ClassName.Contains(TEXT("Bell")))    return TEXT("Bell");
	if (ClassName.Contains(TEXT("KingBot"))) return TEXT("Kingbot");
	if (ClassName.Contains(TEXT("Giganto"))) return TEXT("Gigantus");
	return FString();
}

enum class EEnemySoundType : uint8 { Hit, GettingHit, Steps };

static FEnemyTypeSounds* GetOrLoadEnemyTypeSounds(const FString& TypeKey)
{
	if (TypeKey.IsEmpty()) return nullptr;
	if (FEnemyTypeSounds* Existing = EnemyTypeSoundCache.Find(TypeKey))
		return Existing;

	FEnemyTypeSounds NewSounds;
	FString HitPath = FString::Printf(TEXT("/Game/Audio/SFX/%s/S_%s_Hit.S_%s_Hit"), *TypeKey, *TypeKey, *TypeKey);
	FString GettingHitPath = FString::Printf(TEXT("/Game/Audio/SFX/%s/S_%s_GettingHit.S_%s_GettingHit"), *TypeKey, *TypeKey, *TypeKey);
	FString StepsPath = FString::Printf(TEXT("/Game/Audio/SFX/%s/S_%s_Steps.S_%s_Steps"), *TypeKey, *TypeKey, *TypeKey);

	NewSounds.Hit = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *HitPath));
	NewSounds.GettingHit = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *GettingHitPath));
	NewSounds.Steps = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *StepsPath));

	UE_LOG(LogTemp, Log, TEXT("EnemyTypeSounds: Loaded '%s' — Hit=%s GettingHit=%s Steps=%s"),
		*TypeKey,
		NewSounds.Hit ? TEXT("OK") : TEXT("MISSING"),
		NewSounds.GettingHit ? TEXT("OK") : TEXT("MISSING"),
		NewSounds.Steps ? TEXT("OK") : TEXT("MISSING"));

	EnemyTypeSoundCache.Add(TypeKey, NewSounds);
	return EnemyTypeSoundCache.Find(TypeKey);
}

static void PlayEnemyTypeSound(UWorld* World, AActor* EnemyActor, EEnemySoundType SoundType)
{
	if (!World || !EnemyActor) return;
	FString TypeKey = GetEnemyTypeKey(EnemyActor);
	FEnemyTypeSounds* Sounds = GetOrLoadEnemyTypeSounds(TypeKey);
	if (!Sounds) return;

	USoundBase* Sound = nullptr;
	switch (SoundType)
	{
	case EEnemySoundType::Hit:        Sound = Sounds->Hit; break;
	case EEnemySoundType::GettingHit: Sound = Sounds->GettingHit; break;
	case EEnemySoundType::Steps:      Sound = Sounds->Steps; break;
	}
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Sound, EnemyActor->GetActorLocation(), 1.0f, 1.0f);
	}
}

// --- Dynamic Music Crossfade ---

struct FMusicState
{
	TWeakObjectPtr<UWorld> OwnerWorld;
	bool bInitialized = false;
	TWeakObjectPtr<UAudioComponent> ExplorationComp;
	TWeakObjectPtr<UAudioComponent> CombatComp;
	bool bInCombat = false;
	double LastCombatEnemyTime = 0.0;
	static constexpr float CombatCooldown = 5.0f;
	USoundBase* ExplorationMusic = nullptr;
	USoundBase* CombatMusic = nullptr;
	bool bSoundsLoaded = false;
};
static FMusicState MusicSystem;

static void UpdateMusicCrossfade(UWorld* World)
{
	if (!World) return;

	if (MusicSystem.bInitialized && (!MusicSystem.OwnerWorld.IsValid() || MusicSystem.OwnerWorld.Get() != World))
	{
		MusicSystem = FMusicState();
	}

	if (!MusicSystem.bSoundsLoaded)
	{
		MusicSystem.bSoundsLoaded = true;
		MusicSystem.ExplorationMusic = Cast<USoundBase>(StaticLoadObject(
			USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/Music/S_Main_Theme.S_Main_Theme")));
		MusicSystem.CombatMusic = Cast<USoundBase>(StaticLoadObject(
			USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/Music/S_Action_1.S_Action_1")));
	}

	if (!MusicSystem.bInitialized)
	{
		MusicSystem.bInitialized = true;
		MusicSystem.OwnerWorld = World;

		if (MusicSystem.ExplorationMusic)
		{
			MusicSystem.ExplorationComp = UGameplayStatics::CreateSound2D(
				World, MusicSystem.ExplorationMusic, 1.0f, 1.0f, 0.0f, nullptr, false, false);
			if (MusicSystem.ExplorationComp.IsValid())
				MusicSystem.ExplorationComp->FadeIn(2.0f, 1.0f);
		}

		if (MusicSystem.CombatMusic)
		{
			MusicSystem.CombatComp = UGameplayStatics::CreateSound2D(
				World, MusicSystem.CombatMusic, 0.0f, 1.0f, 0.0f, nullptr, false, false);
			if (MusicSystem.CombatComp.IsValid())
			{
				MusicSystem.CombatComp->Play();
				MusicSystem.CombatComp->SetVolumeMultiplier(0.0f);
			}
		}
	}

	double CurrentTime = World->GetTimeSeconds();
	bool bAnyCombat = false;
	for (const auto& Pair : EnemyAIStates)
	{
		if (!Pair.Key.IsValid()) continue;
		const FEnemyAIStateData& StateData = Pair.Value;
		if (StateData.CurrentState == EEnemyAIState::Chase || StateData.CurrentState == EEnemyAIState::Attack)
		{
			bAnyCombat = true;
			break;
		}
	}

	if (bAnyCombat)
		MusicSystem.LastCombatEnemyTime = CurrentTime;

	bool bShouldBeCombat = (CurrentTime - MusicSystem.LastCombatEnemyTime) < MusicSystem.CombatCooldown
		&& MusicSystem.LastCombatEnemyTime > 0.0;

	if (bShouldBeCombat && !MusicSystem.bInCombat)
	{
		MusicSystem.bInCombat = true;
		if (MusicSystem.ExplorationComp.IsValid())
			MusicSystem.ExplorationComp->FadeOut(1.5f, 0.0f);
		if (MusicSystem.CombatComp.IsValid())
			MusicSystem.CombatComp->FadeIn(1.0f, 1.0f);
	}
	else if (!bShouldBeCombat && MusicSystem.bInCombat)
	{
		MusicSystem.bInCombat = false;
		if (MusicSystem.CombatComp.IsValid())
			MusicSystem.CombatComp->FadeOut(2.0f, 0.0f);
		if (MusicSystem.ExplorationComp.IsValid())
			MusicSystem.ExplorationComp->FadeIn(3.0f, 1.0f);
	}
}

void UGameplayHelperLibrary::ApplyMeleeDamage(ACharacter* Attacker, float Damage, float Radius, float KnockbackImpulse)
{
	if (!Attacker)
	{
		return;
	}

	UWorld* World = Attacker->GetWorld();
	if (!World)
	{
		return;
	}

	// Sphere overlap to find nearby pawns
	FVector Origin = Attacker->GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Attacker);

	World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, Sphere, Params);

	// Deduplicate actors (multiple components can overlap)
	TSet<ACharacter*> HitCharacters;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActor == Attacker)
		{
			continue;
		}
		ACharacter* HitChar = Cast<ACharacter>(HitActor);
		if (HitChar)
		{
			HitCharacters.Add(HitChar);
		}
	}

	// Determine if attacker is the player (for friendly-fire prevention)
	ACharacter* PlayerCharDmg = UGameplayStatics::GetPlayerCharacter(World, 0);
	bool bAttackerIsPlayer = (Attacker == PlayerCharDmg);

	for (ACharacter* Victim : HitCharacters)
	{
		if (!IsValid(Victim))
		{
			continue;
		}

		// Prevent enemy-to-enemy friendly fire: AI enemies should only damage the player
		bool bVictimIsPlayer = (Victim == PlayerCharDmg);
		if (!bAttackerIsPlayer && !bVictimIsPlayer)
		{
			continue;
		}

		// Find "Health" property via reflection (supports both float and double BP variables)
		FProperty* HealthProp = Victim->GetClass()->FindPropertyByName(FName("Health"));
		if (!HealthProp)
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyMeleeDamage: %s has no 'Health' variable, skipping"), *Victim->GetName());
			continue;
		}

		void* ValuePtr = HealthProp->ContainerPtrToValuePtr<void>(Victim);
		float CurrentHealth = 0.0f;

		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(HealthProp))
		{
			CurrentHealth = FloatProp->GetPropertyValue(ValuePtr);
		}
		else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(HealthProp))
		{
			CurrentHealth = (float)DoubleProp->GetPropertyValue(ValuePtr);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyMeleeDamage: %s 'Health' is not float/double, skipping"), *Victim->GetName());
			continue;
		}

		// Already dead
		if (CurrentHealth <= 0.0f)
		{
			continue;
		}

		// Apply damage
		// Check blocking (75% damage reduction)
		float EffectiveDamage = Damage;
		if (BlockingActors.Contains(TWeakObjectPtr<AActor>(Victim)))
		{
			EffectiveDamage = Damage * 0.25f;
			UE_LOG(LogTemp, Log, TEXT("ApplyMeleeDamage: %s blocked! %.0f -> %.0f"),
				*Victim->GetName(), Damage, EffectiveDamage);
		}
		CurrentHealth -= EffectiveDamage;

		// NOTE: Attack SFX removed from here — will be added via AnimNotify later
		// for proper animation-synced timing.

		// Write back
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(HealthProp))
		{
			FloatProp->SetPropertyValue(ValuePtr, CurrentHealth);
		}
		else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(HealthProp))
		{
			DoubleProp->SetPropertyValue(ValuePtr, (double)CurrentHealth);
		}

		UE_LOG(LogTemp, Log, TEXT("ApplyMeleeDamage: %s took %.0f damage, health now %.0f"), *Victim->GetName(), Damage, CurrentHealth);

		// Trigger red flash if victim is player
		ACharacter* PlayerCharFlash = UGameplayStatics::GetPlayerCharacter(World, 0);
		if (Victim == PlayerCharFlash && CurrentHealth > 0.f)
		{
			PlayerHUD.DamageFlashStartTime = World->GetTimeSeconds();
		}

		// Blood VFX (graceful null if asset missing)
		static UNiagaraSystem* BloodFX = nullptr;
		static bool bBloodLoadAttempted = false;
		if (!bBloodLoadAttempted)
		{
			bBloodLoadAttempted = true;
			BloodFX = Cast<UNiagaraSystem>(StaticLoadObject(
				UNiagaraSystem::StaticClass(), nullptr,
				TEXT("/Game/FX/NS_BloodBurst.NS_BloodBurst")));
		}
		if (BloodFX)
		{
			FVector HitLoc = Victim->GetActorLocation();
			HitLoc.Z += 80.f;
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, BloodFX, HitLoc, FRotator::ZeroRotator,
				FVector(1.f), true, true, ENCPoolMethod::None, true);
		}

		if (CurrentHealth <= 0.0f)
		{
			// --- DEATH ---
			UE_LOG(LogTemp, Log, TEXT("ApplyMeleeDamage: %s died!"), *Victim->GetName());

			// Check if this enemy is managed by UpdateEnemyAI (has state entry)
			TWeakObjectPtr<AActor> VictimKey(Victim);
			bool bManagedByAI = EnemyAIStates.Contains(VictimKey);

			if (bManagedByAI)
			{
				// Let UpdateEnemyAI handle death animation + cleanup on next tick
				// Don't fully disable capsule — enemy needs floor support during death anim
				// Just ignore pawns so the dead enemy doesn't block player movement
				UCapsuleComponent* Capsule = Victim->GetCapsuleComponent();
				if (Capsule)
				{
					Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
				}
			}
			else
			{
				// Non-AI enemy or player: use ragdoll path
				USkeletalMeshComponent* MeshComp = Victim->GetMesh();
				if (MeshComp)
				{
					MeshComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
					MeshComp->SetSimulatePhysics(true);

					// Knockback impulse away from attacker
					FVector KnockDir = (Victim->GetActorLocation() - Origin).GetSafeNormal();
					KnockDir.Z = 0.3f; // Slight upward arc
					KnockDir.Normalize();
					MeshComp->AddImpulse(KnockDir * KnockbackImpulse);
				}

				// Disable capsule collision so ragdoll doesn't fight it
				UCapsuleComponent* Capsule = Victim->GetCapsuleComponent();
				if (Capsule)
				{
					Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}

				// Stop movement
				Victim->GetCharacterMovement()->DisableMovement();

				// Check if victim is the player — don't destroy, let ManagePlayerHUD handle death screen
				ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0);
				if (Victim == PlayerChar)
				{
					// Disable player input
					APlayerController* PC = Cast<APlayerController>(Victim->GetController());
					if (PC)
					{
						PC->DisableInput(PC);
					}
					// ManagePlayerHUD will detect HP<=0 on next tick and show death screen
				}
				else
				{
					// Enemy: delayed destroy (1.5 seconds for ragdoll to settle)
					TWeakObjectPtr<ACharacter> WeakVictim(Victim);
					FTimerHandle DestroyTimer;
					World->GetTimerManager().SetTimer(
						DestroyTimer,
						[WeakVictim]()
						{
							if (WeakVictim.IsValid())
							{
								WeakVictim->Destroy();
							}
						},
						1.5f,
						false
					);
				}
			}
		}
	}
}

void UGameplayHelperLibrary::UpdateEnemyAI(ACharacter* Enemy, float AggroRange, float AttackRange,
	float LeashDistance, float MoveSpeed, float AttackCooldown,
	float AttackDamage, float AttackRadius, UAnimSequence* AttackAnim,
	UAnimSequence* DeathAnim, UAnimSequence* HitReactAnim,
	UAnimSequence* AttackAnim2, UAnimSequence* AttackAnim3,
	UAnimSequence* ScreamAnim, UAnimSequence* DeathAnim2,
	bool bIgnorePlayer, float PatrolRadius, AActor* CombatPartner)
{
	if (!Enemy) return;
	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	// Get/init state
	TWeakObjectPtr<AActor> Key(Enemy);
	FEnemyAIStateData& State = EnemyAIStates.FindOrAdd(Key);
	if (!State.bInitialized)
	{
		State.bInitialized = true;

		// Per-instance base randomization
		State.SpeedMultiplier = FMath::FRandRange(0.65f, 1.35f);
		State.AggroRangeMultiplier = FMath::FRandRange(0.7f, 1.3f);
		State.ReactionDelay = FMath::FRandRange(0.1f, 1.5f);
		State.AttackCooldownJitter = FMath::FRandRange(-0.5f, 1.0f);
		State.WobblePhase = FMath::FRandRange(0.0f, 2.0f * PI);
		State.WobbleAmplitude = FMath::FRandRange(30.0f, 80.0f);
		State.AnimPlayRateVariation = FMath::FRandRange(0.8f, 1.2f);

		// --- PERSONALITY ARCHETYPE ASSIGNMENT ---
		{
			float PersonalityRoll = FMath::FRand();
			if (PersonalityRoll < 0.30f)
				State.Personality = EEnemyPersonality::Normal;
			else if (PersonalityRoll < 0.45f)
				State.Personality = EEnemyPersonality::Berserker;
			else if (PersonalityRoll < 0.65f)
				State.Personality = EEnemyPersonality::Stalker;
			else if (PersonalityRoll < 0.80f)
				State.Personality = EEnemyPersonality::Brute;
			else
				State.Personality = EEnemyPersonality::Crawler;

			// Personality-specific stat multipliers (stack on top of base random)
			switch (State.Personality)
			{
			case EEnemyPersonality::Berserker:
				State.SpeedMultiplier *= 1.6f;         // Very fast
				State.AggroRangeMultiplier *= 0.5f;    // Only reacts when close
				State.AttackCooldownJitter -= 1.0f;    // Attacks rapidly
				State.ReactionDelay *= 0.2f;           // Near-instant reaction
				State.DamageMultiplier = 0.7f;         // Lower damage per hit
				break;
			case EEnemyPersonality::Stalker:
				State.SpeedMultiplier *= 0.55f;        // Slow, menacing approach
				State.AggroRangeMultiplier *= 2.0f;    // Notices player from far
				State.ReactionDelay *= 2.5f;           // Long stare before moving
				State.WobbleAmplitude *= 2.0f;         // Weaving approach
				State.DamageMultiplier = 1.0f;
				break;
			case EEnemyPersonality::Brute:
				State.SpeedMultiplier *= 0.8f;         // Slow and heavy
				State.WobbleAmplitude *= 0.2f;         // Charges straight
				State.AttackCooldownJitter += 0.5f;    // Slower attacks
				State.DamageMultiplier = 1.8f;         // Hits HARD
				break;
			case EEnemyPersonality::Crawler:
				State.SpeedMultiplier *= 0.4f;         // Creeping
				State.AggroRangeMultiplier *= 1.4f;    // Aware
				State.ReactionDelay *= 0.5f;           // Quick to start crawling
				State.AnimPlayRateVariation *= 0.8f;   // Slower anim
				State.DamageMultiplier = 1.2f;
				break;
			default: // Normal
				State.DamageMultiplier = 1.0f;
				break;
			}

			// Select attack animation based on personality + available pool
			TArray<UAnimSequence*> AvailableAttacks;
			if (AttackAnim) AvailableAttacks.Add(AttackAnim);
			if (AttackAnim2) AvailableAttacks.Add(AttackAnim2);
			if (AttackAnim3) AvailableAttacks.Add(AttackAnim3);

			if (AvailableAttacks.Num() > 0)
			{
				switch (State.Personality)
				{
				case EEnemyPersonality::Stalker:
					// Prefer secondary (biting) if available
					State.ChosenAttackAnim = AvailableAttacks.Num() > 1
						? AvailableAttacks[1] : AvailableAttacks[0];
					break;
				case EEnemyPersonality::Brute:
					// Prefer tertiary (heavy zombie attack) if available
					State.ChosenAttackAnim = AvailableAttacks.Last();
					break;
				default:
					// Random from full pool
					State.ChosenAttackAnim = AvailableAttacks[FMath::RandRange(0, AvailableAttacks.Num() - 1)];
					break;
				}
			}

			// Select death animation variety
			TArray<UAnimSequence*> AvailableDeaths;
			if (DeathAnim) AvailableDeaths.Add(DeathAnim);
			if (DeathAnim2) AvailableDeaths.Add(DeathAnim2);
			if (AvailableDeaths.Num() > 0)
				State.ChosenDeathAnim = AvailableDeaths[FMath::RandRange(0, AvailableDeaths.Num() - 1)];

			// --- ANIMATION OVERRIDE BY NAME CONVENTION ---
			// Load correct animations from enemy's folder, overriding Blueprint params if found.
			// This ensures the right anim plays even if the Blueprint has wrong assignments.
			{
				FString AnimClassName = Enemy->GetClass()->GetName();
				AnimClassName.RemoveFromEnd(TEXT("_C"));
				FString AnimEnemyType = AnimClassName;
				AnimEnemyType.RemoveFromStart(TEXT("BP_"));
				FString AnimBasePath = FString::Printf(TEXT("/Game/Characters/Enemies/%s/Animations/"), *AnimEnemyType);

				auto TryLoadAnimSeq = [&](const FString& Name) -> UAnimSequence* {
					FString FullPath = AnimBasePath + Name + TEXT(".") + Name;
					return LoadObject<UAnimSequence>(nullptr, *FullPath);
				};

				// Hit-react: prefer BodyBlock (impact reaction), then TakingPunch
				UAnimSequence* HitReactCandidate = TryLoadAnimSeq(AnimEnemyType + TEXT("_BodyBlock"));
				if (!HitReactCandidate) HitReactCandidate = TryLoadAnimSeq(AnimEnemyType + TEXT("_TakingPunch"));
				if (HitReactCandidate)
				{
					State.ChosenHitReactAnim = HitReactCandidate;
				}
				else
				{
					State.ChosenHitReactAnim = HitReactAnim; // fallback to BP param
				}

				// Death: ALWAYS try name convention (overrides BP params for correctness)
				{
					UAnimSequence* DeathCandidate = TryLoadAnimSeq(AnimEnemyType + TEXT("_Death"));
					if (!DeathCandidate) DeathCandidate = TryLoadAnimSeq(AnimEnemyType + TEXT("_Dying"));
					if (!DeathCandidate) DeathCandidate = TryLoadAnimSeq(AnimEnemyType + TEXT("_ZombieDying"));
					if (!DeathCandidate) DeathCandidate = TryLoadAnimSeq(AnimEnemyType + TEXT("_RifleHitBack"));
					if (DeathCandidate) State.ChosenDeathAnim = DeathCandidate;
				}
			}

			// Initialize idle behavior timer (staggered per instance)
			State.NextIdleBehaviorTime = FMath::FRandRange(2.0f, 8.0f);
			State.IdleBehaviorTimer = 0.0f;
		}

		// Snap to ground on first tick using WorldStatic trace (hits landscape, ignores HISM grass)
		{
			// Use visual bounds (capsule + mesh combined) to correctly ground
			// skeletal meshes whose pivot isn't at feet (e.g. Meshy AI exports)
			FVector BoundsOrigin, BoundsExtent;
			Enemy->GetActorBounds(false, BoundsOrigin, BoundsExtent);

			FVector Loc = Enemy->GetActorLocation();
			float VisualBottomOffset = Loc.Z - (BoundsOrigin.Z - BoundsExtent.Z);

			FHitResult SnapHit;
			FVector SnapStart = FVector(Loc.X, Loc.Y, Loc.Z + 5000.0f);
			FVector SnapEnd = FVector(Loc.X, Loc.Y, Loc.Z - 5000.0f);
			FCollisionQueryParams SnapParams;
			SnapParams.AddIgnoredActor(Enemy);

			if (World->LineTraceSingleByChannel(SnapHit, SnapStart, SnapEnd, ECC_WorldStatic, SnapParams))
			{
				FVector SnappedLoc = FVector(Loc.X, Loc.Y, SnapHit.Location.Z + VisualBottomOffset);
				Enemy->SetActorLocation(SnappedLoc, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		State.SpawnLocation = Enemy->GetActorLocation();

		// Ensure capsule collision is correct — CMC needs QueryAndPhysics to detect floors
		UCapsuleComponent* InitCapsule = Enemy->GetCapsuleComponent();
		if (InitCapsule)
		{
			InitCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			InitCapsule->SetCollisionObjectType(ECC_Pawn);
			InitCapsule->SetCollisionResponseToAllChannels(ECR_Block);
			InitCapsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}

		// Configure CMC — gravity pulls to ground, CMC handles floor detection naturally
		UCharacterMovementComponent* InitMoveComp = Enemy->GetCharacterMovement();
		if (InitMoveComp)
		{
			InitMoveComp->SetComponentTickEnabled(true);
			InitMoveComp->GravityScale = 3.0f;
			InitMoveComp->MaxWalkSpeed = MoveSpeed;
			InitMoveComp->MaxAcceleration = 4096.0f;           // Fast ramp-up to full speed
			InitMoveComp->BrakingDecelerationWalking = 300.0f; // Slow ramp-down (smooth stop)
			InitMoveComp->GroundFriction = 6.0f;               // Slightly less friction
			InitMoveComp->MaxStepHeight = 20.0f;               // Can't step onto other characters
			InitMoveComp->bOrientRotationToMovement = false;    // We handle rotation manually
			InitMoveComp->SetAvoidanceEnabled(true);             // RVO avoidance
			InitMoveComp->AvoidanceWeight = 0.5f;
			InitMoveComp->SetMovementMode(MOVE_Falling);        // Gravity settles onto ground
		}

		// Ensure AnimBP is assigned for locomotion state machine + montage slot.
		// If set_skeletal_animation was ever called, it may have cleared the AnimBP class.
		USkeletalMeshComponent* InitMesh = Enemy->GetMesh();
		if (InitMesh)
		{
			UAnimInstance* ExistingAnim = InitMesh->GetAnimInstance();
			if (!ExistingAnim || InitMesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
			{
				// Derive AnimBP path from Blueprint class name: BP_Bell -> ABP_BG_Bell
				FString ClassName = Enemy->GetClass()->GetName();
				// Strip _C suffix if present (generated class name)
				ClassName.RemoveFromEnd(TEXT("_C"));
				// Extract enemy type: BP_Bell -> Bell, BP_KingBot -> KingBot
				FString EnemyType = ClassName;
				EnemyType.RemoveFromStart(TEXT("BP_"));

				FString AnimBPPath = FString::Printf(
					TEXT("/Game/Characters/Enemies/%s/ABP_BG_%s.ABP_BG_%s_C"),
					*EnemyType, *EnemyType, *EnemyType
				);

				UClass* AnimBPClass = LoadObject<UClass>(nullptr, *AnimBPPath);
				if (AnimBPClass)
				{
					InitMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
					InitMesh->SetAnimInstanceClass(AnimBPClass);
					UE_LOG(LogTemp, Log, TEXT("UpdateEnemyAI: Assigned AnimBP %s to %s"), *AnimBPPath, *Enemy->GetName());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("UpdateEnemyAI: Could not load AnimBP at %s for %s"), *AnimBPPath, *Enemy->GetName());
				}
			}
		}
	}

	// Clean up dead entries periodically (every 100th call)
	static int32 CleanupCounter = 0;
	if (++CleanupCounter > 100)
	{
		CleanupCounter = 0;
		for (auto It = EnemyAIStates.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid()) It.RemoveCurrent();
		}
	}


	// Find player
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Player) return;

	float DistToPlayer = FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation());
	float DistToSpawn = FVector::Dist2D(Enemy->GetActorLocation(), State.SpawnLocation);
	double CurrentTime = World->GetTimeSeconds();
	float DeltaTime = World->GetDeltaSeconds();

	// Check Health, auto-init if 0, handle death + hit reactions
	FProperty* HealthProp = Enemy->GetClass()->FindPropertyByName(FName("Health"));
	float HP = 100.f;
	if (HealthProp)
	{
		void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Enemy);
		HP = 0.f;
		if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp)) HP = FP->GetPropertyValue(ValPtr);
		else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp)) HP = (float)DP->GetPropertyValue(ValPtr);

		// Auto-initialize: Health==0 on first tick means CDO default didn't propagate
		if (HP == 0.f && !State.bHealthInitialized)
		{
			State.bHealthInitialized = true;
			HP = 100.f;
			if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp)) FP->SetPropertyValue(ValPtr, HP);
			else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp)) DP->SetPropertyValue(ValPtr, (double)HP);
		}
	}

	// --- FLOATING HEALTH BAR ---
	// Create UWidgetComponent on first tick (after health property is found)
	if (!State.HealthBarComponent.IsValid() && HP > 0.f && HealthProp)
	{
		static TWeakObjectPtr<UClass> CachedWidgetClass;
		static bool bWidgetClassLoadAttempted = false;
		if (!bWidgetClassLoadAttempted)
		{
			bWidgetClassLoadAttempted = true;
			CachedWidgetClass = LoadClass<UUserWidget>(nullptr,
				TEXT("/Game/UI/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
			if (!CachedWidgetClass.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("UpdateEnemyAI: Failed to load WBP_EnemyHealthBar widget class"));
			}
		}

		if (CachedWidgetClass.IsValid())
		{
			UWidgetComponent* WC = NewObject<UWidgetComponent>(Enemy, NAME_None);
			WC->SetupAttachment(Enemy->GetRootComponent());

			// Position above head: capsule half-height + offset
			UCapsuleComponent* Cap = Enemy->GetCapsuleComponent();
			float HeadZ = Cap ? Cap->GetScaledCapsuleHalfHeight() + 30.f : 120.f;
			WC->SetRelativeLocation(FVector(0, 0, HeadZ));

			WC->SetWidgetSpace(EWidgetSpace::Screen);
			WC->SetDrawSize(FVector2D(120, 10));
			WC->SetPivot(FVector2D(0.5f, 0.5f));
			WC->SetWidgetClass(CachedWidgetClass.Get());
			WC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WC->SetVisibility(false); // Hidden until damaged
			WC->RegisterComponent();

			State.HealthBarComponent = WC;
			State.MaxHealth = HP;

			UE_LOG(LogTemp, Log, TEXT("UpdateEnemyAI: Created health bar for %s (MaxHP=%.0f, HeadZ=%.0f)"),
				*Enemy->GetName(), State.MaxHealth, HeadZ);
		}
	}

	// Update health bar percent + visibility
	if (State.HealthBarComponent.IsValid())
	{
		UUserWidget* Widget = State.HealthBarComponent->GetWidget();
		if (Widget)
		{
			UProgressBar* Bar = Cast<UProgressBar>(
				Widget->WidgetTree->FindWidget(FName("HealthFill")));
			if (Bar)
			{
				Bar->SetPercent(FMath::Clamp(HP / State.MaxHealth, 0.f, 1.f));
			}
		}

		// Show only when damaged, hide at full health
		bool bShouldShow = (HP > 0.f && HP < State.MaxHealth);
		State.HealthBarComponent->SetVisibility(bShouldShow);
	}

	USkeletalMeshComponent* MeshComp = Enemy->GetMesh();
	UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();

	// DEBUG: Show per-instance values with personality
	if (GEngine && DistToPlayer < 4000.f)
	{
		const TCHAR* StateNames[] = { TEXT("Idle"), TEXT("Chase"), TEXT("Attack"), TEXT("Return"), TEXT("HitReact"), TEXT("Dead"), TEXT("Patrol") };
		const TCHAR* PersonalityNames[] = { TEXT("NORMAL"), TEXT("BERSERKER"), TEXT("STALKER"), TEXT("BRUTE"), TEXT("CRAWLER") };
		int32 StateIdx = FMath::Clamp((int32)State.CurrentState, 0, 6);
		int32 PersIdx = FMath::Clamp((int32)State.Personality, 0, 4);
		GEngine->AddOnScreenDebugMessage(
			(int32)Enemy->GetUniqueID(), 0.0f, FColor::Cyan,
			FString::Printf(TEXT("%s [%s] Spd=%.0f Aggro=%.0f Dmg=x%.1f State=%s Dist=%.0f Atk=%s"),
				*Enemy->GetName(),
				PersonalityNames[PersIdx],
				MoveSpeed * State.SpeedMultiplier,
				AggroRange * State.AggroRangeMultiplier,
				State.DamageMultiplier,
				StateNames[StateIdx],
				DistToPlayer,
				State.ChosenAttackAnim ? *State.ChosenAttackAnim->GetName() : TEXT("NONE")));
	}

	// --- DEATH STATE ---
	if (HP <= 0.f)
	{
		if (!State.bDeathAnimStarted)
		{
			// Hide + destroy health bar on death
			if (State.HealthBarComponent.IsValid())
			{
				State.HealthBarComponent->SetVisibility(false);
				State.HealthBarComponent->DestroyComponent();
				State.HealthBarComponent = nullptr;
			}

			State.bDeathAnimStarted = true;
			State.CurrentState = EEnemyAIState::Dead;
			State.DeathStartTime = CurrentTime;

			// Stop movement
			if (MoveComp) MoveComp->DisableMovement();

			// Cancel any playing attack montage so death anim is visible
			if (MeshComp)
			{
				if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
				{
					AnimInst->Montage_Stop(0.0f); // Instant stop, no blend
				}
			}

			// Play death animation via montage — keeps AnimBP alive (no SingleNode mode switch)
			UAnimSequence* UsedDeathAnim = State.ChosenDeathAnim ? State.ChosenDeathAnim : DeathAnim;
			if (UsedDeathAnim && MeshComp)
			{
				if (UAnimInstance* DeathAnimInst = MeshComp->GetAnimInstance())
				{
					DeathAnimInst->PlaySlotAnimationAsDynamicMontage(
						UsedDeathAnim, FName("DefaultSlot"),
						0.1f,   // BlendInTime
						0.0f,   // BlendOutTime — no blend-out so pose holds
						1.0f,   // PlayRate
						1,      // LoopCount — play once
						-1.f,   // BlendOutTriggerTime
						0.0f    // StartAt
					);
				}
			}
			else if (MeshComp)
			{
				// No death animation available — freeze current pose and skip to break-apart
				MeshComp->bPauseAnims = true;
			}

			// Play per-type enemy death sound (reuse GettingHit as death cry)
			PlayEnemyTypeSound(World, Enemy, EEnemySoundType::GettingHit);
		}
		else
		{
			// Wait for death anim to finish, then break apart into rock debris
			UAnimSequence* UsedDeathAnim = State.ChosenDeathAnim ? State.ChosenDeathAnim : DeathAnim;
			float DeathAnimLen = UsedDeathAnim ? UsedDeathAnim->GetPlayLength() : 0.0f;
			float TimeSinceDeath = (float)(CurrentTime - State.DeathStartTime);

			// Freeze animation pose after death anim finishes (prevents snapping back to idle)
			if (TimeSinceDeath >= DeathAnimLen - 0.05f && MeshComp && !MeshComp->bPauseAnims)
			{
				MeshComp->bPauseAnims = true;
			}

			if (!State.bDeathBreakStarted && TimeSinceDeath > DeathAnimLen + 0.3f)
			{
				// Start break-apart phase: hide enemy, spawn physics debris
				State.bDeathBreakStarted = true;
				State.DeathBreakStartTime = CurrentTime;

				// Now fully disable capsule (enemy is done animating)
				UCapsuleComponent* DeathCapsule = Enemy->GetCapsuleComponent();
				if (DeathCapsule)
				{
					DeathCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}

				// Hide the enemy mesh
				if (MeshComp)
				{
					MeshComp->SetVisibility(false);
				}

				// Load rock meshes for debris (cached static - loaded once across all deaths)
				static TArray<UStaticMesh*> DebrisRockMeshes;
				static bool bDebrisMeshesLoaded = false;
				if (!bDebrisMeshesLoaded)
				{
					bDebrisMeshesLoaded = true;
					const TCHAR* MeshPaths[] = {
						TEXT("/Game/Meshes/Rocks/rock_moss_set_01_rock01.rock_moss_set_01_rock01"),
						TEXT("/Game/Meshes/Rocks/rock_moss_set_01_rock02.rock_moss_set_01_rock02"),
						TEXT("/Game/Meshes/Rocks/rock_moss_set_01_rock03.rock_moss_set_01_rock03"),
						TEXT("/Game/Meshes/Rocks/rock_moss_set_01_rock04.rock_moss_set_01_rock04"),
						TEXT("/Game/Meshes/Rocks/rock_moss_set_01_rock05.rock_moss_set_01_rock05"),
						TEXT("/Game/Meshes/Rocks/rock_moss_set_01_rock06.rock_moss_set_01_rock06"),
					};
					for (const TCHAR* Path : MeshPaths)
					{
						UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
						if (Mesh)
						{
							DebrisRockMeshes.Add(Mesh);
						}
					}
				}

				// Spawn 5 debris rock chunks at chest height
				if (DebrisRockMeshes.Num() > 0)
				{
					FVector EnemyLoc = Enemy->GetActorLocation();
					EnemyLoc.Z += 50.f;

					for (int32 i = 0; i < 5; i++)
					{
						FVector SpawnOffset(
							FMath::FRandRange(-40.f, 40.f),
							FMath::FRandRange(-40.f, 40.f),
							FMath::FRandRange(0.f, 60.f)
						);
						FVector SpawnLoc = EnemyLoc + SpawnOffset;
						FRotator SpawnRot(
							FMath::FRandRange(0.f, 360.f),
							FMath::FRandRange(0.f, 360.f),
							FMath::FRandRange(0.f, 360.f)
						);

						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						SpawnParams.ObjectFlags = RF_Transient;

						AActor* Debris = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);
						if (!Debris) continue;

						UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(Debris, NAME_None, RF_Transient);
						SMC->SetStaticMesh(DebrisRockMeshes[FMath::RandRange(0, DebrisRockMeshes.Num() - 1)]);

						float DebrisScale = FMath::FRandRange(0.15f, 0.5f);
						SMC->SetRelativeScale3D(FVector(DebrisScale));

						Debris->SetRootComponent(SMC);
						SMC->RegisterComponent();

						// Physics setup
						SMC->SetCollisionProfileName(TEXT("PhysicsActor"));
						SMC->SetSimulatePhysics(true);

						// Outward + upward impulse for burst scatter
						FVector ImpulseDir = FVector(
							FMath::FRandRange(-1.f, 1.f),
							FMath::FRandRange(-1.f, 1.f),
							FMath::FRandRange(0.8f, 2.0f)
						).GetSafeNormal();
						float ImpulseMag = FMath::FRandRange(20000.f, 50000.f);
						SMC->AddImpulse(ImpulseDir * ImpulseMag);

						State.DebrisActors.Add(TWeakObjectPtr<AActor>(Debris));
					}
				}
			}

			// Cleanup debris + enemy after 3 seconds
			if (State.bDeathBreakStarted)
			{
				constexpr float BreakCleanupDelay = 3.0f;
				float TimeSinceBreak = (float)(CurrentTime - State.DeathBreakStartTime);

				if (TimeSinceBreak >= BreakCleanupDelay)
				{
					for (auto& WeakDebris : State.DebrisActors)
					{
						if (WeakDebris.IsValid())
						{
							WeakDebris->Destroy();
						}
					}
					State.DebrisActors.Empty();
					EnemyAIStates.Remove(Key);
					Enemy->Destroy();
				}
			}
		}
		return; // Don't process AI when dead/dying
	}

	// --- HIT REACTION DETECTION ---
	if (State.PreviousHealth > 0.f && HP < State.PreviousHealth && HP > 0.f)
	{
		// Took damage — trigger hit react
		UAnimSequence* UsedHitReactAnim = State.ChosenHitReactAnim ? State.ChosenHitReactAnim : HitReactAnim;
		if (State.CurrentState != EEnemyAIState::HitReact && UsedHitReactAnim)
		{
			State.PreHitReactState = State.CurrentState;
			State.CurrentState = EEnemyAIState::HitReact;
			State.HitReactStartTime = CurrentTime;

			// Play hit react — bForceInterrupt instantly stops any attack montage
			PlayAnimationOneShot(Enemy, UsedHitReactAnim, 1.0f, 0.1f, 0.15f, false, /*bForceInterrupt=*/true);

			// Play getting-hit sound immediately when enemy takes damage
			PlayEnemyTypeSound(World, Enemy, EEnemySoundType::GettingHit);
		}
	}
	State.PreviousHealth = HP;

	// HitReact -> previous state after animation finishes
	if (State.CurrentState == EEnemyAIState::HitReact)
	{
		UAnimSequence* HitReactForLen = State.ChosenHitReactAnim ? State.ChosenHitReactAnim : HitReactAnim;
		float HitReactLen = HitReactForLen ? FMath::Min(HitReactForLen->GetPlayLength(), 1.5f) : 0.8f;
		if ((CurrentTime - State.HitReactStartTime) > HitReactLen)
		{
			State.CurrentState = State.PreHitReactState;
		}
	}

	// Update CMC walk speed (includes per-instance variation)
	if (MoveComp)
	{
		MoveComp->MaxWalkSpeed = MoveSpeed * State.SpeedMultiplier;
	}

	// --- STATE TRANSITIONS (with hysteresis) ---
	// Skip transitions during hit react (enemy is stunned)
	if (State.CurrentState != EEnemyAIState::HitReact)
	{
		// Force RETURN if too far from spawn (highest priority)
		if (DistToSpawn > LeashDistance && State.CurrentState != EEnemyAIState::Return)
		{
			State.CurrentState = EEnemyAIState::Return;
		}

		// RETURN -> IDLE when close to spawn
		if (State.CurrentState == EEnemyAIState::Return && DistToSpawn < 150.f)
		{
			State.CurrentState = EEnemyAIState::Idle;
		}

		// IDLE -> CHASE when player enters aggro range (with per-instance variation)
		if (!bIgnorePlayer && State.CurrentState == EEnemyAIState::Idle && DistToPlayer < AggroRange * State.AggroRangeMultiplier)
		{
			State.bIdleBehaviorActive = false; // Cancel any idle behavior
			State.AggroStartTime = CurrentTime;
			State.bAggroReactionDone = false;
			State.CurrentState = EEnemyAIState::Chase;
		}

		// IDLE -> PATROL when patrol is configured (with random delay)
		if (State.CurrentState == EEnemyAIState::Idle && PatrolRadius > 0.0f && !State.bIdleBehaviorActive)
		{
			if (State.IdleBehaviorTimer > State.NextIdleBehaviorTime)
			{
				// Pick random point within patrol radius of spawn
				FVector2D RandDir2D = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal();
				float WanderDist = FMath::FRandRange(PatrolRadius * 0.3f, PatrolRadius);
				State.PatrolTarget = State.SpawnLocation + FVector(RandDir2D.X * WanderDist, RandDir2D.Y * WanderDist, 0.f);
				State.bPatrolPausing = false;
				State.CurrentState = EEnemyAIState::Patrol;
			}
		}

		// PATROL -> CHASE when player enters aggro range (if not ignoring)
		if (!bIgnorePlayer && State.CurrentState == EEnemyAIState::Patrol && DistToPlayer < AggroRange * State.AggroRangeMultiplier)
		{
			State.AggroStartTime = CurrentTime;
			State.bAggroReactionDone = false;
			State.CurrentState = EEnemyAIState::Chase;
		}

		// CHASE -> ATTACK when close enough (with windup delay before first strike)
		if (State.CurrentState == EEnemyAIState::Chase && DistToPlayer < AttackRange)
		{
			State.CurrentState = EEnemyAIState::Attack;
			// Force a 0.5s windup before the first attack fires
			State.LastAttackTime = CurrentTime - (AttackCooldown + State.AttackCooldownJitter) + 0.5;
		}

		// ATTACK -> CHASE when player moves out of attack range (with hysteresis buffer)
		if (State.CurrentState == EEnemyAIState::Attack && DistToPlayer > AttackRange * 1.5f)
		{
			State.CurrentState = EEnemyAIState::Chase;
		}

		// CHASE -> IDLE when player escapes aggro range (with hysteresis and per-instance variation)
		if (State.CurrentState == EEnemyAIState::Chase && DistToPlayer > AggroRange * State.AggroRangeMultiplier * 1.2f)
		{
			State.CurrentState = EEnemyAIState::Return;
		}
	} // end skip transitions during HitReact

	// --- STATE BEHAVIORS ---

	switch (State.CurrentState)
	{
	case EEnemyAIState::Return:
	{
		FVector DirToSpawn = State.SpawnLocation - Enemy->GetActorLocation();
		FVector HorizDir = FVector(DirToSpawn.X, DirToSpawn.Y, 0.0f).GetSafeNormal();
		if (!HorizDir.IsNearlyZero())
		{
			Enemy->AddMovementInput(HorizDir, 1.0f);
			FRotator TargetRot = FRotator(0.f, HorizDir.Rotation().Yaw, 0.f);
			Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, 8.0f));
		}
		break;
	}

	case EEnemyAIState::Attack:
	{
		// Face player
		FVector DirToPlayer = Player->GetActorLocation() - Enemy->GetActorLocation();
		FVector HorizDir = FVector(DirToPlayer.X, DirToPlayer.Y, 0.0f).GetSafeNormal();
		if (!HorizDir.IsNearlyZero())
		{
			FRotator TargetRot = FRotator(0.f, HorizDir.Rotation().Yaw, 0.f);
			FRotator CurrentRot = Enemy->GetActorRotation();
			Enemy->SetActorRotation(FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 10.0f));
		}

		// Attack with cooldown (with per-instance jitter)
		if ((CurrentTime - State.LastAttackTime) >= (AttackCooldown + State.AttackCooldownJitter))
		{
			State.LastAttackTime = CurrentTime;

			// Use personality-assigned attack animation (consistent per-enemy, no random cycling)
			UAnimSequence* UsedAttackAnim = State.ChosenAttackAnim ? State.ChosenAttackAnim : AttackAnim;
			if (UsedAttackAnim)
			{
				PlayAnimationOneShot(Enemy, UsedAttackAnim, 1.0f, 0.15f, 0.2f, false);

				// Play attack SFX delayed to sync with animation impact (~40% of anim length)
				float SfxDelay = UsedAttackAnim->GetPlayLength() * 0.4f;
				FTimerHandle SfxTimerHandle;
				TWeakObjectPtr<AActor> WeakEnemy(Enemy);
				TWeakObjectPtr<UWorld> WeakWorld(World);
				Enemy->GetWorldTimerManager().SetTimer(
					SfxTimerHandle,
					[WeakEnemy, WeakWorld]()
					{
						if (WeakEnemy.IsValid() && WeakWorld.IsValid())
						{
							PlayEnemyTypeSound(WeakWorld.Get(), WeakEnemy.Get(), EEnemySoundType::Hit);
						}
					},
					SfxDelay,
					false
				);
			}

			// Deal damage scaled by personality
			ApplyMeleeDamage(Enemy, AttackDamage * State.DamageMultiplier, AttackRadius, 30000.f);
		}
		break;
	}

	case EEnemyAIState::Chase:
	{
		// Reaction delay — enemy "notices" player but takes a moment to start moving
		if (!State.bAggroReactionDone)
		{
			if ((CurrentTime - State.AggroStartTime) < State.ReactionDelay)
			{
				// Still reacting — face player but don't move yet
				FVector DirToPlayer = Player->GetActorLocation() - Enemy->GetActorLocation();
				FVector HDir = FVector(DirToPlayer.X, DirToPlayer.Y, 0.0f).GetSafeNormal();
				if (!HDir.IsNearlyZero())
				{
					FRotator TargetRot = FRotator(0.f, HDir.Rotation().Yaw, 0.f);
					Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, 5.0f));
				}
				break;
			}
			State.bAggroReactionDone = true;
		}

		FVector DirToPlayer = Player->GetActorLocation() - Enemy->GetActorLocation();
		FVector HorizDir = FVector(DirToPlayer.X, DirToPlayer.Y, 0.0f).GetSafeNormal();
		float HorizDistToPlayer = FVector(DirToPlayer.X, DirToPlayer.Y, 0.0f).Size();
		// Stop approaching when within 80% of attack range to prevent overshooting
		if (HorizDistToPlayer > AttackRange * 0.8f)
		{
			if (!HorizDir.IsNearlyZero())
			{
				// Lateral wobble for organic pathing — each enemy has unique phase
				FVector WobbleDir = FVector(-HorizDir.Y, HorizDir.X, 0.0f);
				float WobbleOffset = FMath::Sin((float)CurrentTime * 2.5f + State.WobblePhase) * 0.15f;
				FVector FinalDir = (HorizDir + WobbleDir * WobbleOffset).GetSafeNormal();
				Enemy->AddMovementInput(FinalDir, 1.0f);
				FRotator TargetRot = FRotator(0.f, FinalDir.Rotation().Yaw, 0.f);
				Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, 6.0f));
			}
		}
		else
		{
			// Close enough - just face the player
			if (!HorizDir.IsNearlyZero())
			{
				FRotator TargetRot = FRotator(0.f, HorizDir.Rotation().Yaw, 0.f);
				Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, 8.0f));
			}
		}
		break;
	}

	case EEnemyAIState::HitReact:
	{
		// Face player while stunned, no movement
		FVector DirToPlayer = Player->GetActorLocation() - Enemy->GetActorLocation();
		FVector HorizDir = FVector(DirToPlayer.X, DirToPlayer.Y, 0.0f).GetSafeNormal();
		if (!HorizDir.IsNearlyZero())
		{
			FRotator TargetRot = FRotator(0.f, HorizDir.Rotation().Yaw, 0.f);
			Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, 6.0f));
		}
		break;
	}

	case EEnemyAIState::Patrol:
	{
		if (State.bPatrolPausing)
		{
			// Pausing at patrol point
			State.PatrolPauseTimer += DeltaTime;
			if (State.PatrolPauseTimer >= State.PatrolPauseDuration)
			{
				// Done pausing — return to Idle (will re-enter Patrol next idle timeout)
				State.bPatrolPausing = false;
				State.CurrentState = EEnemyAIState::Idle;
				State.IdleBehaviorTimer = 0.0f;
				State.NextIdleBehaviorTime = FMath::FRandRange(1.0f, 3.0f); // Short wait before next patrol
			}
		}
		else
		{
			// Walking to patrol point
			FVector Dir = State.PatrolTarget - Enemy->GetActorLocation();
			FVector HDir = FVector(Dir.X, Dir.Y, 0.0f);
			float Dist = HDir.Size();

			if (Dist > 80.f)
			{
				FVector Normal = HDir.GetSafeNormal();
				Enemy->AddMovementInput(Normal, 0.4f); // Slow patrol walk
				FRotator TargetRot = FRotator(0.f, Normal.Rotation().Yaw, 0.f);
				Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, 3.0f));
			}
			else
			{
				// Reached patrol point — pause
				State.bPatrolPausing = true;
				State.PatrolPauseTimer = 0.0f;
				State.PatrolPauseDuration = FMath::FRandRange(2.0f, 5.0f);
			}
		}
		break;
	}

	case EEnemyAIState::Idle:
	default:
	{
		State.IdleBehaviorTimer += DeltaTime;

		// Auto-discover combat partner: find nearest same-class enemy within 500 units
		AActor* EffectiveCombatPartner = CombatPartner;
		if (!EffectiveCombatPartner && !State.bPartnerSearchDone)
		{
			State.bPartnerSearchDone = true;
			const float PartnerSearchRadius = 500.0f;
			float BestDist = PartnerSearchRadius;
			for (auto& Pair : EnemyAIStates)
			{
				AActor* Other = Pair.Key.Get();
				if (!Other || Other == Enemy || !IsValid(Other)) continue;
				if (Other->GetClass() != Enemy->GetClass()) continue;
				float Dist = FVector::Dist(Enemy->GetActorLocation(), Other->GetActorLocation());
				if (Dist < BestDist)
				{
					// Check the other enemy also doesn't already have a partner
					FEnemyAIStateData& OtherState = Pair.Value;
					if (!OtherState.AutoDiscoveredPartner.IsValid())
					{
						BestDist = Dist;
						State.AutoDiscoveredPartner = Other;
					}
				}
			}
			if (State.AutoDiscoveredPartner.IsValid())
			{
				// Set mutual partnership
				FEnemyAIStateData* OtherData = EnemyAIStates.Find(State.AutoDiscoveredPartner);
				if (OtherData)
				{
					OtherData->AutoDiscoveredPartner = Enemy;
					OtherData->bPartnerSearchDone = true;
				}
			}
		}
		if (!EffectiveCombatPartner && State.AutoDiscoveredPartner.IsValid())
		{
			EffectiveCombatPartner = State.AutoDiscoveredPartner.Get();
		}

		// Combat partner behavior — face partner and attack periodically
		if (EffectiveCombatPartner && IsValid(EffectiveCombatPartner))
		{
			FVector DirToPartner = EffectiveCombatPartner->GetActorLocation() - Enemy->GetActorLocation();
			FVector HorizDir = FVector(DirToPartner.X, DirToPartner.Y, 0.0f).GetSafeNormal();
			if (!HorizDir.IsNearlyZero())
			{
				FRotator TargetRot = FRotator(0.f, HorizDir.Rotation().Yaw, 0.f);
				Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, 6.0f));
			}

			// Initialize cooldown on first tick
			if (State.PartnerAttackCooldown == 0.0f)
			{
				State.PartnerAttackCooldown = FMath::FRandRange(2.0f, 4.0f);
				State.LastPartnerAttackTime = CurrentTime - FMath::FRandRange(0.0f, State.PartnerAttackCooldown); // Stagger start
			}

			// Play attack montage on cooldown
			if ((CurrentTime - State.LastPartnerAttackTime) >= State.PartnerAttackCooldown)
			{
				State.LastPartnerAttackTime = CurrentTime;
				State.PartnerAttackCooldown = FMath::FRandRange(2.5f, 5.0f); // Randomize next cooldown

				// Pick random attack from pool each time
				TArray<UAnimSequence*> PartnerAttackPool;
				if (AttackAnim) PartnerAttackPool.Add(AttackAnim);
				if (AttackAnim2) PartnerAttackPool.Add(AttackAnim2);
				if (AttackAnim3) PartnerAttackPool.Add(AttackAnim3);
				UAnimSequence* UsedAttackAnim = PartnerAttackPool.Num() > 0
					? PartnerAttackPool[FMath::RandRange(0, PartnerAttackPool.Num() - 1)] : nullptr;
				if (UsedAttackAnim)
				{
					PlayAnimationOneShot(Enemy, UsedAttackAnim, State.AnimPlayRateVariation, 0.15f, 0.15f, false);

					// Play attack SFX delayed for partner combat too
					float PartnerSfxDelay = UsedAttackAnim->GetPlayLength() * 0.4f;
					FTimerHandle PartnerSfxTimer;
					TWeakObjectPtr<AActor> WeakPartnerEnemy(Enemy);
					TWeakObjectPtr<UWorld> WeakPartnerWorld(World);
					Enemy->GetWorldTimerManager().SetTimer(
						PartnerSfxTimer,
						[WeakPartnerEnemy, WeakPartnerWorld]()
						{
							if (WeakPartnerEnemy.IsValid() && WeakPartnerWorld.IsValid())
							{
								PlayEnemyTypeSound(WeakPartnerWorld.Get(), WeakPartnerEnemy.Get(), EEnemySoundType::Hit);
							}
						},
						PartnerSfxDelay,
						false
					);
				}
			}

			break; // Skip normal idle behaviors — combat partner takes over
		}

		// Pick a new idle behavior when timer expires
		if (!State.bIdleBehaviorActive && State.IdleBehaviorTimer >= State.NextIdleBehaviorTime)
		{
			float Roll = FMath::FRand();
			if (ScreamAnim && Roll < 0.12f)
			{
				State.CurrentIdleBehavior = EIdleBehavior::Scream;
				State.bIdleBehaviorActive = true;
				State.IdleScreamEndTime = CurrentTime + ScreamAnim->GetPlayLength();
				State.IdleBehaviorTimer = 0.0f;
				// Play scream — bForceInterrupt ensures it starts even if another montage lingers
				PlayAnimationOneShot(Enemy, ScreamAnim, 1.0f, 0.15f, 0.15f, false, /*bForceInterrupt=*/true);
			}
			else if (Roll < 0.35f)
			{
				State.CurrentIdleBehavior = EIdleBehavior::LookAround;
				State.bIdleBehaviorActive = true;
				State.IdleBehaviorTimer = 0.0f;
			}
			else if (Roll < 0.60f)
			{
				State.CurrentIdleBehavior = EIdleBehavior::Wander;
				State.bIdleBehaviorActive = true;
				State.IdleBehaviorTimer = 0.0f;
				// Random point near spawn
				FVector2D RandDir = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal();
				float WanderDist = FMath::FRandRange(80.f, 250.f);
				State.IdleWanderTarget = State.SpawnLocation + FVector(RandDir.X * WanderDist, RandDir.Y * WanderDist, 0.f);
			}
			else
			{
				// Stand still — reset timer for next attempt
				State.IdleBehaviorTimer = 0.0f;
				State.NextIdleBehaviorTime = FMath::FRandRange(3.0f, 8.0f);
			}
		}

		// Execute active idle behavior
		if (State.bIdleBehaviorActive)
		{
			switch (State.CurrentIdleBehavior)
			{
			case EIdleBehavior::LookAround:
			{
				// Slowly pan rotation with sinusoidal motion
				float TurnRate = FMath::Sin(State.IdleBehaviorTimer * 1.5f) * 50.0f;
				FRotator CurrentRot = Enemy->GetActorRotation();
				CurrentRot.Yaw += TurnRate * DeltaTime;
				Enemy->SetActorRotation(CurrentRot);

				if (State.IdleBehaviorTimer > 3.5f)
				{
					State.bIdleBehaviorActive = false;
					State.IdleBehaviorTimer = 0.0f;
					State.NextIdleBehaviorTime = FMath::FRandRange(4.0f, 10.0f);
				}
				break;
			}
			case EIdleBehavior::Wander:
			{
				FVector Dir = State.IdleWanderTarget - Enemy->GetActorLocation();
				FVector HDir = FVector(Dir.X, Dir.Y, 0.0f);
				float Dist = HDir.Size();

				if (Dist > 50.f && State.IdleBehaviorTimer < 5.0f)
				{
					FVector Normal = HDir.GetSafeNormal();
					Enemy->AddMovementInput(Normal, 0.4f); // Slow wander
					FRotator TargetRot = FRotator(0.f, Normal.Rotation().Yaw, 0.f);
					Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, 3.0f));
				}
				else
				{
					State.bIdleBehaviorActive = false;
					State.IdleBehaviorTimer = 0.0f;
					State.NextIdleBehaviorTime = FMath::FRandRange(3.0f, 7.0f);
				}
				break;
			}
			case EIdleBehavior::Scream:
			{
				if (CurrentTime > State.IdleScreamEndTime)
				{
					State.bIdleBehaviorActive = false;
					State.IdleBehaviorTimer = 0.0f;
					State.NextIdleBehaviorTime = FMath::FRandRange(8.0f, 15.0f);
				}
				break;
			}
			default:
				State.bIdleBehaviorActive = false;
				break;
			}
		}
		break;
	}
	}

	// CMC handles ground tracking via gravity + floor detection (capsule collision enforced at init)

	// Animation is driven by AnimBP (locomotion state machine reads CMC velocity).
	// One-shot animations (attack, death, hit react, scream) use PlayAnimationOneShot montages.
}

void UGameplayHelperLibrary::ManagePlayerHUD(ACharacter* Player)
{
	if (!Player) return;
	UWorld* World = Player->GetWorld();
	if (!World) return;

	// Reset if world changed (level restart)
	if (PlayerHUD.bCreated && (!PlayerHUD.OwnerWorld.IsValid() || PlayerHUD.OwnerWorld.Get() != World))
	{
		PlayerHUD = FPlayerHUDState();
	}

	// Create HUD on first call
	if (!PlayerHUD.bCreated)
	{
		UGameViewportClient* GVC = World->GetGameViewport();
		if (!GVC) return;

		PlayerHUD.OwnerWorld = World;

		// Read initial health as max; auto-init to 50 if CDO default didn't propagate
		FProperty* HealthProp = Player->GetClass()->FindPropertyByName(FName("Health"));
		if (HealthProp)
		{
			void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Player);
			if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp))
				PlayerHUD.MaxHealth = FP->GetPropertyValue(ValPtr);
			else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp))
				PlayerHUD.MaxHealth = (float)DP->GetPropertyValue(ValPtr);

			// Safety: if Health is 0, set it to 50 so player doesn't die on first tick
			if (PlayerHUD.MaxHealth <= 0.f)
			{
				PlayerHUD.MaxHealth = 50.f;
				if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp))
					FP->SetPropertyValue(ValPtr, 50.f);
				else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp))
					DP->SetPropertyValue(ValPtr, 50.0);
				UE_LOG(LogTemp, Warning, TEXT("ManagePlayerHUD: Player Health was 0, auto-initialized to 50"));
			}
		}
		else
		{
			PlayerHUD.MaxHealth = 50.f;
		}

		// Load health bar textures
		const float HealthBarWidth = 500.0f;
		const float HealthBarHeight = HealthBarWidth * (1536.0f / 2816.0f); // ~273px, preserves 2816:1536 aspect

		UTexture2D* BaseTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/T_HB_Base.T_HB_Base"));
		UTexture2D* FillTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/T_HB_Fill.T_HB_Fill"));
		UTexture2D* FrameTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/T_HB_Frame.T_HB_Frame"));

		if (BaseTex && FillTex && FrameTex)
		{
			PlayerHUD.BaseBarTexture = TStrongObjectPtr<UTexture2D>(BaseTex);
			PlayerHUD.FillBarTexture = TStrongObjectPtr<UTexture2D>(FillTex);
			PlayerHUD.FrameBarTexture = TStrongObjectPtr<UTexture2D>(FrameTex);

			auto SetupBrush = [&](FSlateBrush& Brush, UTexture2D* Tex) {
				Brush.SetResourceObject(Tex);
				Brush.ImageSize = FVector2D(HealthBarWidth, HealthBarHeight);
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Brush.Tiling = ESlateBrushTileType::NoTile;
			};
			SetupBrush(PlayerHUD.BaseBrush, BaseTex);
			SetupBrush(PlayerHUD.FillBrush, FillTex);
			SetupBrush(PlayerHUD.FrameBrush, FrameTex);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ManagePlayerHUD: Failed to load health bar textures (Base=%d, Fill=%d, Frame=%d)"),
				BaseTex != nullptr, FillTex != nullptr, FrameTex != nullptr);
		}

		// Font styles — Dutch Golden Age aesthetic
		FSlateFontInfo DeathTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 58);
		FSlateFontInfo DeathSubFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);
		FSlateFontInfo DeathRuleFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);

		// Palette constants
		const FLinearColor DarkUmber(0.03f, 0.02f, 0.01f, 0.92f);
		const FLinearColor GildedEdge(0.40f, 0.28f, 0.10f, 0.80f);
		const FLinearColor DeepCrimson(0.45f, 0.06f, 0.03f, 1.0f);

		// Build Slate widget tree — Dutch Golden Age ornate style
		TSharedRef<SOverlay> Root = SNew(SOverlay)

			// Health bar (bottom-left, painted texture overlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(20.0f, 0.0f, 0.0f, 20.0f)
			[
				SNew(SBox)
				.WidthOverride(HealthBarWidth)
				.HeightOverride(HealthBarHeight)
				[
					SNew(SOverlay)

					// Layer 0: Base — frame + dark empty tube (always full size)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(&PlayerHUD.BaseBrush)
					]

					// Layer 1: Fill — golden liquid (clipped by HP%, transparent elsewhere)
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					[
						SAssignNew(PlayerHUD.HealthClipBox, SBox)
						.WidthOverride(HealthBarWidth)
						.Clipping(EWidgetClipping::ClipToBounds)
						[
							SNew(SImage)
							.Image(&PlayerHUD.FillBrush)
							.DesiredSizeOverride(FVector2D(HealthBarWidth, HealthBarHeight))
						]
					]

					// Layer 2: Frame mask — frame with transparent tube hole (masks fill overflow)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(&PlayerHUD.FrameBrush)
					]
				]
			]

			// Death overlay — chiaroscuro darkness
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(PlayerHUD.DeathOverlay, SOverlay)
				.Visibility(EVisibility::Collapsed)

				// Deep umber vignette
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.015f, 0.01f, 0.88f))
					.Padding(0)
				]

				// Centered death text with decorative rules
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					// Upper decorative rule
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 12.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550"))))
						.Font(DeathRuleFont)
						.ColorAndOpacity(FSlateColor(GildedEdge))
					]

					// Main death text
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("YOU DIED")))
						.Font(DeathTitleFont)
						.ColorAndOpacity(FSlateColor(DeepCrimson))
						.ShadowOffset(FVector2D(2.0f, 2.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
					]

					// Lower decorative rule
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550"))))
						.Font(DeathRuleFont)
						.ColorAndOpacity(FSlateColor(GildedEdge))
					]

					// Subtitle
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 20.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Restarting...")))
						.Font(DeathSubFont)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.40f, 0.22f, 0.8f)))
					]
				]
			]

			// Damage flash (fullscreen red flash on hit)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(PlayerHUD.DamageFlashBorder, SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor(FLinearColor(0.50f, 0.10f, 0.03f, 0.0f))
				.Padding(0)
				.Visibility(EVisibility::HitTestInvisible)
			]

			// Golden flash overlay (checkpoint collection)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(PlayerHUD.GoldenFlashBorder, SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor(FLinearColor(0.55f, 0.35f, 0.10f, 0.0f))
				.Padding(0)
				.Visibility(EVisibility::HitTestInvisible)
			]

			// Checkpoint text overlay (centered)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SAssignNew(PlayerHUD.CheckpointText, STextBlock)
				.Text(FText::FromString(TEXT("")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.35f, 0.10f, 0.0f)))
				.ShadowOffset(FVector2D(2.0f, 2.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
				.Visibility(EVisibility::HitTestInvisible)
			]

			// Victory overlay (THE ESCAPE — LEVEL COMPLETE)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(PlayerHUD.VictoryOverlay, SOverlay)
				.Visibility(EVisibility::Collapsed)

				// Deep warm umber vignette
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FLinearColor(0.08f, 0.05f, 0.02f, 0.90f))
					.Padding(0)
				]

				// Centered victory content
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					// Upper decorative double rule
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550"))))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.28f, 0.10f, 0.80f)))
					]

					// "THE ESCAPE" title
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("THE ESCAPE")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 64))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.35f, 0.10f, 1.0f)))
						.ShadowOffset(FVector2D(3.0f, 3.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
					]

					// Lower decorative double rule
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 16.0f, 0.0f, 24.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550\x2550"))))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.28f, 0.10f, 0.80f)))
					]

					// "LEVEL COMPLETE"
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("LEVEL COMPLETE")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.40f, 0.22f, 1.0f)))
					]

					// "X / Y Souls Recovered" (dynamic)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 32.0f)
					[
						SAssignNew(PlayerHUD.VictoryCheckpointText, STextBlock)
						.Text(FText::FromString(TEXT("0 / 0 Souls Recovered")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.28f, 0.10f, 0.65f)))
					]

					// "Restarting..."
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Restarting...")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.40f, 0.22f, 0.60f)))
					]
				]
			];

		PlayerHUD.RootWidget = Root;
		GVC->AddViewportWidgetContent(TSharedRef<SWidget>(Root));
		PlayerHUD.bCreated = true;
	}

	// If dead, don't update anymore (death screen is already showing)
	if (PlayerHUD.bDead) return;

	// Read current health
	FProperty* HealthProp = Player->GetClass()->FindPropertyByName(FName("Health"));
	if (!HealthProp) return;

	void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Player);
	float HP = 0.f;
	if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp)) HP = FP->GetPropertyValue(ValPtr);
	else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp)) HP = (float)DP->GetPropertyValue(ValPtr);

	// Update health bar
	float Pct = FMath::Clamp(HP / PlayerHUD.MaxHealth, 0.0f, 1.0f);

	// Tube mapping: liquid occupies cols 1335-2440 of 2816px source
	// Map HP% linearly to the tube region only
	const float HBDisplayWidth = 500.0f;
	const float TubeLeftPx = (1335.0f / 2816.0f) * HBDisplayWidth;   // ~237px
	const float TubeRightPx = (2440.0f / 2816.0f) * HBDisplayWidth;  // ~433px
	float ClipWidth = TubeLeftPx + (TubeRightPx - TubeLeftPx) * Pct;

	if (PlayerHUD.HealthClipBox.IsValid())
	{
		PlayerHUD.HealthClipBox->SetWidthOverride(ClipWidth);
	}

	// Damage flash fade (0.3s)
	if (PlayerHUD.DamageFlashBorder.IsValid() && PlayerHUD.DamageFlashStartTime > 0.0)
	{
		float Elapsed = (float)(World->GetTimeSeconds() - PlayerHUD.DamageFlashStartTime);
		if (Elapsed < 0.3f)
		{
			PlayerHUD.DamageFlashBorder->SetBorderBackgroundColor(
				FLinearColor(0.50f, 0.10f, 0.03f, FMath::Lerp(0.40f, 0.0f, Elapsed / 0.3f)));
		}
		else
		{
			PlayerHUD.DamageFlashBorder->SetBorderBackgroundColor(
				FLinearColor(0.50f, 0.10f, 0.03f, 0.0f));
			PlayerHUD.DamageFlashStartTime = 0.0;
		}
	}

	// --- Dynamic Music Crossfade ---
	UpdateMusicCrossfade(World);

	// Handle death
	if (HP <= 0.0f)
	{
		PlayerHUD.bDead = true;

		// Show death overlay
		if (PlayerHUD.DeathOverlay.IsValid())
		{
			PlayerHUD.DeathOverlay->SetVisibility(EVisibility::Visible);
		}

		// Restart level after 3 seconds
		FTimerHandle RestartTimer;
		TWeakObjectPtr<UWorld> WeakWorld(World);
		World->GetTimerManager().SetTimer(
			RestartTimer,
			[WeakWorld]()
			{
				if (WeakWorld.IsValid())
				{
					// Reset all state before level transition
					PlayerHUD = FPlayerHUDState();
					GameFlow = FGameFlowState();
					EnemyAIStates.Empty();
					BlockingActors.Empty();
					MusicSystem = FMusicState();
					EnemyTypeSoundCache.Empty();

					FString LevelName = UGameplayStatics::GetCurrentLevelName(WeakWorld.Get(), true);
					UGameplayStatics::OpenLevel(WeakWorld.Get(), FName(*LevelName));
				}
			},
			3.0f,
			false
		);
	}
}

void UGameplayHelperLibrary::SetPlayerBlocking(ACharacter* Character, bool bBlocking)
{
	if (!Character) return;

	TWeakObjectPtr<AActor> Key(Character);
	if (bBlocking)
	{
		BlockingActors.Add(Key);
		UE_LOG(LogTemp, Log, TEXT("SetPlayerBlocking: %s is now BLOCKING"), *Character->GetName());
	}
	else
	{
		BlockingActors.Remove(Key);
		UE_LOG(LogTemp, Log, TEXT("SetPlayerBlocking: %s stopped blocking"), *Character->GetName());
	}
}

bool UGameplayHelperLibrary::IsPlayerBlocking(ACharacter* Character)
{
	if (!Character) return false;
	return BlockingActors.Contains(TWeakObjectPtr<AActor>(Character));
}

void UGameplayHelperLibrary::ManageGameFlow(ACharacter* Player)
{
	if (!Player) return;
	UWorld* World = Player->GetWorld();
	if (!World) return;

	// Reset if world changed (level restart)
	if (GameFlow.bInitialized && (!GameFlow.OwnerWorld.IsValid() || GameFlow.OwnerWorld.Get() != World))
	{
		GameFlow = FGameFlowState();
	}

	// Initialize: discover checkpoint and portal lights
	if (!GameFlow.bInitialized)
	{
		GameFlow.bInitialized = true;
		GameFlow.OwnerWorld = World;
		GameFlow.CheckpointsCollected = 0;

		for (TActorIterator<APointLight> It(World); It; ++It)
		{
			APointLight* Light = *It;
			if (!IsValid(Light)) continue;

			FString Name = Light->GetName();

			if (Name.Contains(TEXT("Breadcrumb_Light")))
			{
				FCheckpointData CP;
				CP.LightActor = Light;
				CP.Location = Light->GetActorLocation();
				CP.State = ECheckpointState::Active;

				// Read current intensity and attenuation
				UPointLightComponent* LightComp = Cast<UPointLightComponent>(Light->GetLightComponent());
				if (LightComp)
				{
					CP.OriginalIntensity = LightComp->Intensity;
					CP.OriginalAttenuationRadius = LightComp->AttenuationRadius;
				}

				GameFlow.Checkpoints.Add(CP);
			}
			else if (Name.Contains(TEXT("Portal_Light")) || Name.Contains(TEXT("Portal_Beacon")))
			{
				GameFlow.PortalLightActor = Light;
				GameFlow.PortalLocation = Light->GetActorLocation();
			}
		}

		GameFlow.TotalCheckpoints = GameFlow.Checkpoints.Num();

		// Sort checkpoints by distance from PlayerStart for correct order
		FVector PlayerStartLoc = Player->GetActorLocation();
		GameFlow.Checkpoints.Sort([&PlayerStartLoc](const FCheckpointData& A, const FCheckpointData& B)
		{
			return FVector::Dist(A.Location, PlayerStartLoc) < FVector::Dist(B.Location, PlayerStartLoc);
		});

		// Find directional light for progressive dimming
		for (TActorIterator<ADirectionalLight> DirIt(World); DirIt; ++DirIt)
		{
			ADirectionalLight* DirLight = *DirIt;
			if (IsValid(DirLight))
			{
				GameFlow.DirectionalLightActor = DirLight;
				UDirectionalLightComponent* DirComp = Cast<UDirectionalLightComponent>(DirLight->GetLightComponent());
				if (DirComp)
				{
					GameFlow.OriginalDirLightIntensity = DirComp->Intensity;
				}
				break;
			}
		}

		// Activate beacon on the first checkpoint (so player knows where to go)
		for (FCheckpointData& CP : GameFlow.Checkpoints)
		{
			if (CP.State == ECheckpointState::Active && CP.LightActor.IsValid())
			{
				CP.bIsBeacon = true;
				APointLight* Light = Cast<APointLight>(CP.LightActor.Get());
				if (Light)
				{
					UPointLightComponent* LC = Cast<UPointLightComponent>(Light->GetLightComponent());
					if (LC)
					{
						LC->SetIntensity(CP.OriginalIntensity * 5.0f);
						LC->SetAttenuationRadius(CP.OriginalAttenuationRadius * 3.0f);
						LC->SetLightColor(FLinearColor(0.70f, 0.43f, 0.12f)); // Warm golden beacon
					}
				}
				break;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("ManageGameFlow: Found %d breadcrumb lights, portal=%s, dirlight=%s"),
			GameFlow.TotalCheckpoints,
			GameFlow.PortalLightActor.IsValid() ? *GameFlow.PortalLightActor->GetName() : TEXT("NONE"),
			GameFlow.DirectionalLightActor.IsValid() ? *GameFlow.DirectionalLightActor->GetName() : TEXT("NONE"));
	}

	// Skip if already won or dead
	if (GameFlow.bVictory || PlayerHUD.bDead) return;

	double CurrentTime = World->GetTimeSeconds();
	FVector PlayerLoc = Player->GetActorLocation();

	// --- Beacon pulse on next active checkpoint (sinusoidal glow) ---
	for (FCheckpointData& CP : GameFlow.Checkpoints)
	{
		if (CP.State == ECheckpointState::Active && CP.bIsBeacon && CP.LightActor.IsValid())
		{
			APointLight* Light = Cast<APointLight>(CP.LightActor.Get());
			if (Light)
			{
				UPointLightComponent* LC = Cast<UPointLightComponent>(Light->GetLightComponent());
				if (LC)
				{
					float BaseIntensity = CP.OriginalIntensity * 5.0f;
					float Pulse = FMath::Sin((float)CurrentTime * 2.5f) * 0.3f + 0.7f; // Oscillates 0.4..1.0
					LC->SetIntensity(BaseIntensity * Pulse);
				}
			}
			break; // Only pulse the current beacon
		}
	}

	// --- Checkpoint collection logic ---
	for (FCheckpointData& CP : GameFlow.Checkpoints)
	{
		if (CP.State == ECheckpointState::Active)
		{
			if (!CP.LightActor.IsValid())
			{
				CP.State = ECheckpointState::Collected;
				continue;
			}

			float Dist = FVector::Dist(PlayerLoc, CP.Location);
			if (Dist < 400.0f)
			{
				// Start collecting
				CP.State = ECheckpointState::Collecting;
				CP.CollectStartTime = CurrentTime;
				UE_LOG(LogTemp, Log, TEXT("ManageGameFlow: Collecting checkpoint at %s"), *CP.Location.ToString());
			}
		}
		else if (CP.State == ECheckpointState::Collecting)
		{
			float Elapsed = (float)(CurrentTime - CP.CollectStartTime);

			if (Elapsed < 0.3f)
			{
				// Ramp intensity up
				if (CP.LightActor.IsValid())
				{
					APointLight* Light = Cast<APointLight>(CP.LightActor.Get());
					if (Light)
					{
						UPointLightComponent* LightComp = Cast<UPointLightComponent>(Light->GetLightComponent());
						if (LightComp)
						{
							float Alpha = Elapsed / 0.3f;
							float NewIntensity = FMath::InterpEaseOut(CP.OriginalIntensity, CP.OriginalIntensity * 10.0f, Alpha, 2.0f);
							LightComp->SetIntensity(NewIntensity);
						}
					}
				}
			}
			else
			{
				// Collection complete — destroy the light
				if (CP.LightActor.IsValid())
				{
					CP.LightActor->Destroy();
				}
				CP.State = ECheckpointState::Collected;
				CP.bIsBeacon = false;
				GameFlow.CheckpointsCollected++;

				// Play checkpoint chime
				static USoundBase* CheckpointSound = nullptr;
				static bool bCheckpointSoundLoadAttempted = false;
				if (!bCheckpointSoundLoadAttempted)
				{
					bCheckpointSoundLoadAttempted = true;
					CheckpointSound = Cast<USoundBase>(StaticLoadObject(
						USoundBase::StaticClass(), nullptr,
						TEXT("/Game/Audio/SFX/S_Checkpoint_Chime.S_Checkpoint_Chime")));
				}
				if (CheckpointSound)
				{
					UGameplayStatics::PlaySound2D(World, CheckpointSound, 1.0f, 1.0f);
				}

				// Trigger golden flash
				GameFlow.GoldenFlashStartTime = CurrentTime;

				// Trigger checkpoint text
				GameFlow.CheckpointTextStartTime = CurrentTime;
				GameFlow.CheckpointDisplayText = FString::Printf(
					TEXT("S O U L   R E C O V E R E D   ( %d / %d )"),
					GameFlow.CheckpointsCollected, GameFlow.TotalCheckpoints);

				// --- Dim directional light progressively ---
				if (GameFlow.DirectionalLightActor.IsValid())
				{
					ADirectionalLight* DirLight = Cast<ADirectionalLight>(GameFlow.DirectionalLightActor.Get());
					if (DirLight)
					{
						UDirectionalLightComponent* DirComp = Cast<UDirectionalLightComponent>(DirLight->GetLightComponent());
						if (DirComp)
						{
							float DimFraction = 1.0f - GameFlow.DimPerCheckpoint * GameFlow.CheckpointsCollected;
							DimFraction = FMath::Max(DimFraction, 0.15f); // Don't go below 15%
							DirComp->SetIntensity(GameFlow.OriginalDirLightIntensity * DimFraction);
						}
					}
				}

				// --- Activate beacon on next uncollected checkpoint ---
				for (FCheckpointData& NextCP : GameFlow.Checkpoints)
				{
					if (NextCP.State == ECheckpointState::Active && NextCP.LightActor.IsValid())
					{
						NextCP.bIsBeacon = true;
						APointLight* NextLight = Cast<APointLight>(NextCP.LightActor.Get());
						if (NextLight)
						{
							UPointLightComponent* NextLC = Cast<UPointLightComponent>(NextLight->GetLightComponent());
							if (NextLC)
							{
								NextLC->SetIntensity(NextCP.OriginalIntensity * 5.0f);
								NextLC->SetAttenuationRadius(NextCP.OriginalAttenuationRadius * 3.0f);
								NextLC->SetLightColor(FLinearColor(0.70f, 0.43f, 0.12f));
							}
						}
						break; // Only the next one
					}
				}

				UE_LOG(LogTemp, Log, TEXT("ManageGameFlow: Checkpoint collected %d/%d, dimming light to %.0f%%"),
					GameFlow.CheckpointsCollected, GameFlow.TotalCheckpoints,
					(1.0f - GameFlow.DimPerCheckpoint * GameFlow.CheckpointsCollected) * 100.0f);
			}
		}
	}

	// --- Golden flash animation (0.5s) ---
	if (PlayerHUD.GoldenFlashBorder.IsValid() && GameFlow.GoldenFlashStartTime > 0.0)
	{
		float Elapsed = (float)(CurrentTime - GameFlow.GoldenFlashStartTime);
		if (Elapsed < 0.5f)
		{
			float Alpha;
			if (Elapsed < 0.1f)
			{
				Alpha = FMath::Lerp(0.0f, 0.35f, Elapsed / 0.1f);
			}
			else
			{
				Alpha = FMath::Lerp(0.35f, 0.0f, (Elapsed - 0.1f) / 0.4f);
			}
			PlayerHUD.GoldenFlashBorder->SetBorderBackgroundColor(
				FLinearColor(0.55f, 0.35f, 0.10f, Alpha));
		}
		else
		{
			PlayerHUD.GoldenFlashBorder->SetBorderBackgroundColor(
				FLinearColor(0.55f, 0.35f, 0.10f, 0.0f));
			GameFlow.GoldenFlashStartTime = 0.0;
		}
	}

	// --- Checkpoint text animation (1.5s) ---
	if (PlayerHUD.CheckpointText.IsValid() && GameFlow.CheckpointTextStartTime > 0.0)
	{
		float Elapsed = (float)(CurrentTime - GameFlow.CheckpointTextStartTime);
		if (Elapsed < 1.5f)
		{
			float Alpha;
			if (Elapsed < 0.2f)
			{
				Alpha = FMath::Lerp(0.0f, 1.0f, Elapsed / 0.2f);
			}
			else if (Elapsed < 0.8f)
			{
				Alpha = 1.0f;
			}
			else
			{
				Alpha = FMath::Lerp(1.0f, 0.0f, (Elapsed - 0.8f) / 0.7f);
			}
			PlayerHUD.CheckpointText->SetText(FText::FromString(GameFlow.CheckpointDisplayText));
			PlayerHUD.CheckpointText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.55f, 0.35f, 0.10f, Alpha)));
		}
		else
		{
			PlayerHUD.CheckpointText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.55f, 0.35f, 0.10f, 0.0f)));
			GameFlow.CheckpointTextStartTime = 0.0;
		}
	}

	// --- Victory condition: reach the portal ---
	if (GameFlow.PortalLightActor.IsValid() || !GameFlow.PortalLocation.IsZero())
	{
		// Use stored location (portal light may have been destroyed by checkpoint logic if it happened to match)
		FVector PortalLoc = GameFlow.PortalLocation;
		if (GameFlow.PortalLightActor.IsValid())
		{
			PortalLoc = GameFlow.PortalLightActor->GetActorLocation();
		}

		float DistToPortal = FVector::Dist(PlayerLoc, PortalLoc);
		if (DistToPortal < 500.0f)
		{
			GameFlow.bVictory = true;
			GameFlow.VictoryStartTime = CurrentTime;

			// Play victory fanfare
			static USoundBase* VictorySound = nullptr;
			static bool bVictorySoundLoadAttempted = false;
			if (!bVictorySoundLoadAttempted)
			{
				bVictorySoundLoadAttempted = true;
				VictorySound = Cast<USoundBase>(StaticLoadObject(
					USoundBase::StaticClass(), nullptr,
					TEXT("/Game/Audio/SFX/S_Victory_Fanfare.S_Victory_Fanfare")));
			}
			if (VictorySound)
			{
				UGameplayStatics::PlaySound2D(World, VictorySound, 1.0f, 1.0f);
			}

			UE_LOG(LogTemp, Log, TEXT("ManageGameFlow: VICTORY! %d/%d souls recovered"),
				GameFlow.CheckpointsCollected, GameFlow.TotalCheckpoints);

			// Show victory overlay
			if (PlayerHUD.VictoryOverlay.IsValid())
			{
				PlayerHUD.VictoryOverlay->SetVisibility(EVisibility::Visible);
			}

			// Update checkpoint counter text
			if (PlayerHUD.VictoryCheckpointText.IsValid())
			{
				PlayerHUD.VictoryCheckpointText->SetText(FText::FromString(
					FString::Printf(TEXT("%d / %d Souls Recovered"),
						GameFlow.CheckpointsCollected, GameFlow.TotalCheckpoints)));
			}

			// Disable player input
			APlayerController* PC = Cast<APlayerController>(Player->GetController());
			if (PC)
			{
				PC->DisableInput(PC);
			}

			// Restart level after 5 seconds
			FTimerHandle VictoryRestartTimer;
			TWeakObjectPtr<UWorld> WeakWorld(World);
			World->GetTimerManager().SetTimer(
				VictoryRestartTimer,
				[WeakWorld]()
				{
					if (WeakWorld.IsValid())
					{
						// Reset both HUD and GameFlow state before level transition
						PlayerHUD = FPlayerHUDState();
						GameFlow = FGameFlowState();
						EnemyAIStates.Empty();
						BlockingActors.Empty();
						MusicSystem = FMusicState();
						EnemyTypeSoundCache.Empty();

						FString LevelName = UGameplayStatics::GetCurrentLevelName(WeakWorld.Get(), true);
						UGameplayStatics::OpenLevel(WeakWorld.Get(), FName(*LevelName));
					}
				},
				5.0f,
				false
			);
		}
	}
}
