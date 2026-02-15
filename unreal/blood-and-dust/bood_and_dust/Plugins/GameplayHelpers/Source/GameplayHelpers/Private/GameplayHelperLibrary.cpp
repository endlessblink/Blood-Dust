#include "GameplayHelperLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "TimerManager.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"

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

void UGameplayHelperLibrary::PlayAnimationOneShot(ACharacter* Character, UAnimSequence* AnimSequence, float PlayRate, float BlendIn, float BlendOut, bool bStopMovement)
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

	// If a montage is already playing in the DefaultSlot, ignore the new request
	if (AnimInst->Montage_IsPlaying(nullptr))
	{
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
	TSharedPtr<SBox> HealthFillBox;
	TSharedPtr<SBorder> HealthFillBorder;
	TSharedPtr<STextBlock> HealthText;
	TSharedPtr<SWidget> DeathOverlay;
	TSharedPtr<SBorder> DamageFlashBorder;
	float MaxHealth = 100.0f;
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
	Dead
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
	EEnemyAIState PrevAnimState = EEnemyAIState::Return; // Sentinel: differs from initial Idle to trigger first-frame anim
	EEnemyAIState PreHitReactState = EEnemyAIState::Idle; // state to restore after hit react
	bool bInitialized = false;
	bool bHealthInitialized = false;
	bool bDeathAnimStarted = false;
	UAnimSequence* CurrentPlayingAnim = nullptr; // avoid redundant SetAnimation calls

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
	UAnimSequence* ChosenMoveAnim = nullptr;
	UAnimSequence* ChosenDeathAnim = nullptr;

	// Idle behavior
	EIdleBehavior CurrentIdleBehavior = EIdleBehavior::Stand;
	float IdleBehaviorTimer = 0.0f;
	float NextIdleBehaviorTime = 0.0f;
	FVector IdleWanderTarget = FVector::ZeroVector;
	bool bIdleBehaviorActive = false;
	float IdleScreamEndTime = 0.0f;
};
static TMap<TWeakObjectPtr<AActor>, FEnemyAIStateData> EnemyAIStates;

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

	for (ACharacter* Victim : HitCharacters)
	{
		if (!IsValid(Victim))
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
				// Just disable capsule collision so the enemy stops blocking
				UCapsuleComponent* Capsule = Victim->GetCapsuleComponent();
				if (Capsule)
				{
					Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	UAnimSequence* IdleAnim, UAnimSequence* WalkAnim,
	UAnimSequence* DeathAnim, UAnimSequence* HitReactAnim,
	UAnimSequence* AttackAnim2, UAnimSequence* AttackAnim3,
	UAnimSequence* RunAnim, UAnimSequence* CrawlAnim,
	UAnimSequence* ScreamAnim, UAnimSequence* DeathAnim2)
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

			// Select movement animation based on personality
			if (State.Personality == EEnemyPersonality::Crawler && CrawlAnim)
				State.ChosenMoveAnim = CrawlAnim;
			else if (State.Personality == EEnemyPersonality::Berserker && RunAnim)
				State.ChosenMoveAnim = RunAnim;
			else
				State.ChosenMoveAnim = WalkAnim;

			// Select death animation variety
			TArray<UAnimSequence*> AvailableDeaths;
			if (DeathAnim) AvailableDeaths.Add(DeathAnim);
			if (DeathAnim2) AvailableDeaths.Add(DeathAnim2);
			if (AvailableDeaths.Num() > 0)
				State.ChosenDeathAnim = AvailableDeaths[FMath::RandRange(0, AvailableDeaths.Num() - 1)];

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
			InitMoveComp->BrakingDecelerationWalking = 2000.0f;
			InitMoveComp->GroundFriction = 8.0f;
			InitMoveComp->bOrientRotationToMovement = false; // We handle rotation manually
			InitMoveComp->SetMovementMode(MOVE_Falling); // Gravity settles onto ground
		}

		// AnimBP handles locomotion (Idle ↔ Walk driven by CMC velocity).
		// Do NOT use AnimationSingleNode — it conflicts with PlayAnimationOneShot montages.
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

	USkeletalMeshComponent* MeshComp = Enemy->GetMesh();
	UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();

	// DEBUG: Show per-instance values with personality
	if (GEngine && DistToPlayer < 4000.f)
	{
		const TCHAR* StateNames[] = { TEXT("Idle"), TEXT("Chase"), TEXT("Attack"), TEXT("Return"), TEXT("HitReact"), TEXT("Dead") };
		const TCHAR* PersonalityNames[] = { TEXT("NORMAL"), TEXT("BERSERKER"), TEXT("STALKER"), TEXT("BRUTE"), TEXT("CRAWLER") };
		int32 StateIdx = FMath::Clamp((int32)State.CurrentState, 0, 5);
		int32 PersIdx = FMath::Clamp((int32)State.Personality, 0, 4);
		GEngine->AddOnScreenDebugMessage(
			(int32)Enemy->GetUniqueID(), 0.0f, FColor::Cyan,
			FString::Printf(TEXT("%s [%s] Spd=%.0f Aggro=%.0f Dmg=x%.1f State=%s Dist=%.0f Atk=%s Move=%s"),
				*Enemy->GetName(),
				PersonalityNames[PersIdx],
				MoveSpeed * State.SpeedMultiplier,
				AggroRange * State.AggroRangeMultiplier,
				State.DamageMultiplier,
				StateNames[StateIdx],
				DistToPlayer,
				State.ChosenAttackAnim ? *State.ChosenAttackAnim->GetName() : TEXT("NONE"),
				State.ChosenMoveAnim ? *State.ChosenMoveAnim->GetName() : TEXT("NONE")));
	}

	// --- DEATH STATE ---
	if (HP <= 0.f)
	{
		if (!State.bDeathAnimStarted)
		{
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

			// Play death animation — PlayAnimation auto-switches to SingleNode (fine for dying enemy)
			UAnimSequence* UsedDeathAnim = State.ChosenDeathAnim ? State.ChosenDeathAnim : DeathAnim;
			if (UsedDeathAnim && MeshComp)
			{
				MeshComp->PlayAnimation(UsedDeathAnim, false);
			}
		}
		else
		{
			// Wait for death anim to finish, then destroy
			UAnimSequence* UsedDeathAnim = State.ChosenDeathAnim ? State.ChosenDeathAnim : DeathAnim;
			float DeathAnimLen = UsedDeathAnim ? UsedDeathAnim->GetPlayLength() : 1.5f;
			if ((CurrentTime - State.DeathStartTime) > DeathAnimLen + 0.3f)
			{
				EnemyAIStates.Remove(Key);
				Enemy->Destroy();
			}
		}
		return; // Don't process AI when dead/dying
	}

	// --- HIT REACTION DETECTION ---
	if (State.PreviousHealth > 0.f && HP < State.PreviousHealth && HP > 0.f)
	{
		// Took damage — trigger hit react
		if (State.CurrentState != EEnemyAIState::HitReact && HitReactAnim)
		{
			State.PreHitReactState = State.CurrentState;
			State.CurrentState = EEnemyAIState::HitReact;
			State.HitReactStartTime = CurrentTime;

			// Cancel any playing attack montage so hit react base anim is visible
			if (MeshComp)
			{
				if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
				{
					AnimInst->Montage_Stop(0.1f); // Quick blend out
				}
			}

			// Play hit react as montage overlay (blends back to AnimBP locomotion when done)
			if (HitReactAnim)
			{
				PlayAnimationOneShot(Enemy, HitReactAnim, 1.0f, 0.1f, 0.15f, false);
			}
		}
	}
	State.PreviousHealth = HP;

	// HitReact -> previous state after animation finishes
	if (State.CurrentState == EEnemyAIState::HitReact)
	{
		float HitReactLen = HitReactAnim ? FMath::Min(HitReactAnim->GetPlayLength(), 0.8f) : 0.5f;
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
		if (State.CurrentState == EEnemyAIState::Idle && DistToPlayer < AggroRange * State.AggroRangeMultiplier)
		{
			State.bIdleBehaviorActive = false; // Cancel any idle behavior
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

			// Play personality-selected attack animation
			UAnimSequence* UsedAttackAnim = State.ChosenAttackAnim ? State.ChosenAttackAnim : AttackAnim;
			if (UsedAttackAnim)
			{
				PlayAnimationOneShot(Enemy, UsedAttackAnim, 1.0f, 0.15f, 0.15f, false);
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

	case EEnemyAIState::Idle:
	default:
	{
		State.IdleBehaviorTimer += DeltaTime;

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
				// Play scream as montage overlay on AnimBP
				PlayAnimationOneShot(Enemy, ScreamAnim, 1.0f, 0.15f, 0.15f, false);
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

		// Read initial health as max; auto-init to 100 if CDO default didn't propagate
		FProperty* HealthProp = Player->GetClass()->FindPropertyByName(FName("Health"));
		if (HealthProp)
		{
			void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Player);
			if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp))
				PlayerHUD.MaxHealth = FP->GetPropertyValue(ValPtr);
			else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp))
				PlayerHUD.MaxHealth = (float)DP->GetPropertyValue(ValPtr);

			// Safety: if Health is 0, set it to 100 so player doesn't die on first tick
			if (PlayerHUD.MaxHealth <= 0.f)
			{
				PlayerHUD.MaxHealth = 100.f;
				if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp))
					FP->SetPropertyValue(ValPtr, 100.f);
				else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp))
					DP->SetPropertyValue(ValPtr, 100.0);
				UE_LOG(LogTemp, Warning, TEXT("ManagePlayerHUD: Player Health was 0, auto-initialized to 100"));
			}
		}
		else
		{
			PlayerHUD.MaxHealth = 100.f;
		}

		// Font styles — Dutch Golden Age aesthetic
		FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
		FSlateFontInfo HealthNumFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
		FSlateFontInfo DeathTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 58);
		FSlateFontInfo DeathSubFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);
		FSlateFontInfo DeathRuleFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);

		// Palette constants
		const FLinearColor DarkUmber(0.03f, 0.02f, 0.01f, 0.92f);
		const FLinearColor MidOchre(0.27f, 0.15f, 0.06f, 1.0f);
		const FLinearColor GoldHighlight(0.55f, 0.35f, 0.10f, 1.0f);
		const FLinearColor WarmParchment(0.75f, 0.60f, 0.38f, 1.0f);
		const FLinearColor GildedEdge(0.40f, 0.28f, 0.10f, 0.80f);
		const FLinearColor DeepCrimson(0.45f, 0.06f, 0.03f, 1.0f);
		const FLinearColor BurntSienna(0.50f, 0.15f, 0.05f, 1.0f);

		// Build Slate widget tree — Dutch Golden Age ornate style
		TSharedRef<SOverlay> Root = SNew(SOverlay)

			// Health bar (bottom-left, gilded frame style)
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(36.0f, 0.0f, 0.0f, 48.0f)
			[
				SNew(SVerticalBox)

				// Label row: decorative rule + "VITAE" + decorative rule
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("\x2500\x2500\x2500  "))))
						.Font(DeathRuleFont)
						.ColorAndOpacity(FSlateColor(GildedEdge))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("V I T A E")))
						.Font(LabelFont)
						.ColorAndOpacity(FSlateColor(GoldHighlight))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("  \x2500\x2500\x2500"))))
						.Font(DeathRuleFont)
						.ColorAndOpacity(FSlateColor(GildedEdge))
					]
				]

				// Health bar with gilded frame (outer border → inner border → fill)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					// Outer gilded frame
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(GildedEdge)
					.Padding(FMargin(2.0f))
					[
						// Inner dark frame
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
						.BorderBackgroundColor(DarkUmber)
						.Padding(FMargin(2.0f))
						[
							SNew(SBox)
							.WidthOverride(240.0f)
							.HeightOverride(16.0f)
							[
								SNew(SOverlay)

								// Dark background
								+ SOverlay::Slot()
								[
									SNew(SBorder)
									.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
									.BorderBackgroundColor(FLinearColor(0.05f, 0.03f, 0.02f, 0.95f))
									.Padding(0)
								]

								// Health fill
								+ SOverlay::Slot()
								.HAlign(HAlign_Left)
								[
									SAssignNew(PlayerHUD.HealthFillBox, SBox)
									.WidthOverride(240.0f)
									.HeightOverride(16.0f)
									[
										SAssignNew(PlayerHUD.HealthFillBorder, SBorder)
										.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
										.BorderBackgroundColor(BurntSienna)
										.Padding(0)
									]
								]

								// Overlay: health number centered on bar
								+ SOverlay::Slot()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SAssignNew(PlayerHUD.HealthText, STextBlock)
									.Text(FText::FromString(FString::Printf(TEXT("%d / %d"), (int32)PlayerHUD.MaxHealth, (int32)PlayerHUD.MaxHealth)))
									.Font(HealthNumFont)
									.ColorAndOpacity(FSlateColor(WarmParchment))
									.ShadowOffset(FVector2D(1.0f, 1.0f))
									.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
								]
							]
						]
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

	if (PlayerHUD.HealthFillBox.IsValid())
	{
		PlayerHUD.HealthFillBox->SetWidthOverride(240.0f * Pct);
	}

	if (PlayerHUD.HealthFillBorder.IsValid())
	{
		// Dutch Golden Age palette: burnt sienna → golden ochre → warm amber
		FLinearColor BarColor;
		if (Pct > 0.6f)
			BarColor = FLinearColor(0.45f, 0.30f, 0.08f, 0.95f);  // Warm amber/ochre
		else if (Pct > 0.3f)
			BarColor = FLinearColor(0.55f, 0.25f, 0.06f, 0.95f);  // Golden ochre
		else
			BarColor = FLinearColor(0.50f, 0.10f, 0.04f, 0.95f);  // Burnt sienna

		PlayerHUD.HealthFillBorder->SetBorderBackgroundColor(BarColor);
	}

	if (PlayerHUD.HealthText.IsValid())
	{
		PlayerHUD.HealthText->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), FMath::Max((int32)HP, 0), (int32)PlayerHUD.MaxHealth)));
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
					// Reset HUD state before level transition
					PlayerHUD = FPlayerHUDState();

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

				// Read current intensity
				UPointLightComponent* LightComp = Light->GetPointLightComponent();
				if (LightComp)
				{
					CP.OriginalIntensity = LightComp->Intensity;
				}

				GameFlow.Checkpoints.Add(CP);
			}
			else if (Name.Contains(TEXT("Portal_Light")))
			{
				GameFlow.PortalLightActor = Light;
				GameFlow.PortalLocation = Light->GetActorLocation();
			}
		}

		GameFlow.TotalCheckpoints = GameFlow.Checkpoints.Num();

		UE_LOG(LogTemp, Log, TEXT("ManageGameFlow: Found %d breadcrumb lights, portal=%s"),
			GameFlow.TotalCheckpoints,
			GameFlow.PortalLightActor.IsValid() ? *GameFlow.PortalLightActor->GetName() : TEXT("NONE"));
	}

	// Skip if already won or dead
	if (GameFlow.bVictory || PlayerHUD.bDead) return;

	double CurrentTime = World->GetTimeSeconds();
	FVector PlayerLoc = Player->GetActorLocation();

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
						UPointLightComponent* LightComp = Light->GetPointLightComponent();
						if (LightComp)
						{
							float Alpha = Elapsed / 0.3f;
							float NewIntensity = FMath::Lerp(CP.OriginalIntensity, CP.OriginalIntensity * 10.0f, Alpha);
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
				GameFlow.CheckpointsCollected++;

				// Trigger golden flash
				GameFlow.GoldenFlashStartTime = CurrentTime;

				// Trigger checkpoint text
				GameFlow.CheckpointTextStartTime = CurrentTime;
				GameFlow.CheckpointDisplayText = FString::Printf(
					TEXT("S O U L   R E C O V E R E D   ( %d / %d )"),
					GameFlow.CheckpointsCollected, GameFlow.TotalCheckpoints);

				UE_LOG(LogTemp, Log, TEXT("ManageGameFlow: Checkpoint collected %d/%d"),
					GameFlow.CheckpointsCollected, GameFlow.TotalCheckpoints);
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
