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
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

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
};
static FPlayerHUDState PlayerHUD;

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

			// Ragdoll: enable physics on skeletal mesh
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

// --- Enemy AI State Machine ---

enum class EEnemyAIState : uint8
{
	Idle,
	Chase,
	Attack,
	Return
};

struct FEnemyAIStateData
{
	FVector SpawnLocation;
	double LastAttackTime = 0.0;
	EEnemyAIState CurrentState = EEnemyAIState::Idle;
	EEnemyAIState PrevAnimState = EEnemyAIState::Return; // Sentinel: differs from initial Idle to trigger first-frame anim
	bool bInitialized = false;
	bool bHealthInitialized = false;
};
static TMap<TWeakObjectPtr<AActor>, FEnemyAIStateData> EnemyAIStates;

void UGameplayHelperLibrary::UpdateEnemyAI(ACharacter* Enemy, float AggroRange, float AttackRange,
	float LeashDistance, float MoveSpeed, float AttackCooldown,
	float AttackDamage, float AttackRadius, UAnimSequence* AttackAnim,
	UAnimSequence* IdleAnim, UAnimSequence* WalkAnim)
{
	if (!Enemy) return;
	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	// Get/init state
	TWeakObjectPtr<AActor> Key(Enemy);
	FEnemyAIStateData& State = EnemyAIStates.FindOrAdd(Key);
	if (!State.bInitialized)
	{
		State.SpawnLocation = Enemy->GetActorLocation();
		State.bInitialized = true;

		// Ensure movement works without an AI Controller
		UCharacterMovementComponent* InitMoveComp = Enemy->GetCharacterMovement();
		if (InitMoveComp)
		{
			InitMoveComp->bRunPhysicsWithNoController = true;
		}

		// Clear any AnimBP and switch to SingleNode mode so we control animations directly
		USkeletalMeshComponent* InitMesh = Enemy->GetMesh();
		if (InitMesh)
		{
			InitMesh->SetAnimInstanceClass(nullptr);
			InitMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (IdleAnim)
			{
				InitMesh->SetAnimation(IdleAnim);
				InitMesh->Play(true);
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
	float DistToSpawn = FVector::Dist(Enemy->GetActorLocation(), State.SpawnLocation);
	double CurrentTime = World->GetTimeSeconds();
	float DeltaTime = World->GetDeltaSeconds();

	// Check if already dead (Health <= 0), auto-init Health if 0 (CDO propagation may fail on placed instances)
	FProperty* HealthProp = Enemy->GetClass()->FindPropertyByName(FName("Health"));
	if (HealthProp)
	{
		void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Enemy);
		float HP = 0.f;
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

		if (HP <= 0.f) return; // Actually dead
	}

	// Set movement speed
	UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();
	if (MoveComp) MoveComp->MaxWalkSpeed = MoveSpeed;

	// --- STATE TRANSITIONS (with hysteresis) ---

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

	// IDLE -> CHASE when player enters aggro range
	if (State.CurrentState == EEnemyAIState::Idle && DistToPlayer < AggroRange)
	{
		State.CurrentState = EEnemyAIState::Chase;
	}

	// CHASE -> ATTACK when close enough
	if (State.CurrentState == EEnemyAIState::Chase && DistToPlayer < AttackRange)
	{
		State.CurrentState = EEnemyAIState::Attack;
	}

	// ATTACK -> CHASE when player moves out of attack range (with hysteresis buffer)
	if (State.CurrentState == EEnemyAIState::Attack && DistToPlayer > AttackRange * 1.5f)
	{
		State.CurrentState = EEnemyAIState::Chase;
	}

	// CHASE -> IDLE when player escapes aggro range (with hysteresis)
	if (State.CurrentState == EEnemyAIState::Chase && DistToPlayer > AggroRange * 1.2f)
	{
		State.CurrentState = EEnemyAIState::Return;
	}

	// --- STATE BEHAVIORS ---

	switch (State.CurrentState)
	{
	case EEnemyAIState::Return:
	{
		FVector DirToSpawn = State.SpawnLocation - Enemy->GetActorLocation();
		DirToSpawn.Z = 0.0f; // Horizontal only — let gravity handle vertical
		DirToSpawn = DirToSpawn.GetSafeNormal();
		Enemy->AddMovementInput(DirToSpawn, 1.0f);
		FRotator TargetRot = FRotator(0.f, DirToSpawn.Rotation().Yaw, 0.f);
		FRotator CurrentRot = Enemy->GetActorRotation();
		Enemy->SetActorRotation(FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 8.0f));
		break;
	}

	case EEnemyAIState::Attack:
	{
		// Face player (horizontal only)
		FVector DirToPlayer = Player->GetActorLocation() - Enemy->GetActorLocation();
		DirToPlayer.Z = 0.0f;
		DirToPlayer = DirToPlayer.GetSafeNormal();
		FRotator TargetRot = FRotator(0.f, DirToPlayer.Rotation().Yaw, 0.f);
		FRotator CurrentRot = Enemy->GetActorRotation();
		Enemy->SetActorRotation(FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 10.0f));

		// Attack with cooldown
		if ((CurrentTime - State.LastAttackTime) >= AttackCooldown)
		{
			State.LastAttackTime = CurrentTime;

			// Play attack animation (if provided)
			if (AttackAnim)
			{
				PlayAnimationOneShot(Enemy, AttackAnim, 1.0f, 0.15f, 0.15f, false);
			}

			// Deal damage to player
			ApplyMeleeDamage(Enemy, AttackDamage, AttackRadius, 30000.f);
		}
		break;
	}

	case EEnemyAIState::Chase:
	{
		FVector DirToPlayer = Player->GetActorLocation() - Enemy->GetActorLocation();
		DirToPlayer.Z = 0.0f; // Horizontal only — let gravity handle vertical
		DirToPlayer = DirToPlayer.GetSafeNormal();
		Enemy->AddMovementInput(DirToPlayer, 1.0f);
		FRotator TargetRot = FRotator(0.f, DirToPlayer.Rotation().Yaw, 0.f);
		FRotator CurrentRot = Enemy->GetActorRotation();
		Enemy->SetActorRotation(FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 8.0f));
		break;
	}

	case EEnemyAIState::Idle:
	default:
		// Do nothing - AnimBP handles idle animation
		break;
	}

	// --- ANIMATION CONTROL (direct, bypasses AnimBP) ---
	USkeletalMeshComponent* MeshComp = Enemy->GetMesh();
	if (MeshComp)
	{
		// Determine desired animation based on current state
		UAnimSequence* DesiredAnim = IdleAnim;
		bool bLooping = true;

		switch (State.CurrentState)
		{
		case EEnemyAIState::Chase:
		case EEnemyAIState::Return:
			DesiredAnim = WalkAnim ? WalkAnim : IdleAnim;
			bLooping = true;
			break;

		case EEnemyAIState::Attack:
			// Only switch to attack anim when actually attacking (cooldown just fired)
			if (AttackAnim && (CurrentTime - State.LastAttackTime) < 0.1)
			{
				DesiredAnim = AttackAnim;
				bLooping = false;
			}
			else if (AttackAnim && State.PrevAnimState == EEnemyAIState::Attack)
			{
				// Keep attack anim playing until it finishes, then go to idle
				UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
				if (AnimInst && AnimInst->Montage_IsPlaying(nullptr))
				{
					// Attack montage still playing, don't override
					DesiredAnim = nullptr;
				}
				else
				{
					DesiredAnim = IdleAnim;
					bLooping = true;
				}
			}
			break;

		case EEnemyAIState::Idle:
		default:
			DesiredAnim = IdleAnim;
			bLooping = true;
			break;
		}

		// Switch animation only when state changes or we have a new anim to play
		if (DesiredAnim && State.PrevAnimState != State.CurrentState)
		{
			MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			MeshComp->SetAnimation(DesiredAnim);
			MeshComp->SetPlayRate(1.0f);
			MeshComp->Play(bLooping);
		}

		State.PrevAnimState = State.CurrentState;
	}
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

		// Read initial health as max
		FProperty* HealthProp = Player->GetClass()->FindPropertyByName(FName("Health"));
		if (HealthProp)
		{
			void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Player);
			if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp))
				PlayerHUD.MaxHealth = FP->GetPropertyValue(ValPtr);
			else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp))
				PlayerHUD.MaxHealth = (float)DP->GetPropertyValue(ValPtr);
		}
		if (PlayerHUD.MaxHealth <= 0.f) PlayerHUD.MaxHealth = 100.f;

		// Font styles (UE 5.7 API)
		FSlateFontInfo BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
		FSlateFontInfo RegularFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);
		FSlateFontInfo BigBoldFont = FCoreStyle::GetDefaultFontStyle("Bold", 52);
		FSlateFontInfo SmallRegularFont = FCoreStyle::GetDefaultFontStyle("Regular", 20);

		// Build Slate widget tree
		TSharedRef<SOverlay> Root = SNew(SOverlay)

			// Health bar (bottom-left)
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(40.0f, 0.0f, 0.0f, 60.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(PlayerHUD.HealthText, STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("HEALTH  %d / %d"), (int32)PlayerHUD.MaxHealth, (int32)PlayerHUD.MaxHealth)))
					.Font(BoldFont)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.7f)))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(280.0f)
					.HeightOverride(22.0f)
					[
						SNew(SOverlay)

						// Background
						+ SOverlay::Slot()
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
							.BorderBackgroundColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.85f))
							.Padding(0)
						]

						// Health fill
						+ SOverlay::Slot()
						.HAlign(HAlign_Left)
						[
							SAssignNew(PlayerHUD.HealthFillBox, SBox)
							.WidthOverride(280.0f)
							.HeightOverride(22.0f)
							[
								SAssignNew(PlayerHUD.HealthFillBorder, SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
								.BorderBackgroundColor(FLinearColor(0.6f, 0.18f, 0.1f, 0.95f))
								.Padding(0)
							]
						]
					]
				]
			]

			// Death overlay (hidden initially)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(PlayerHUD.DeathOverlay, SOverlay)
				.Visibility(EVisibility::Collapsed)

				// Dark background
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.01f, 0.01f, 0.75f))
					.Padding(0)
				]

				// Centered death text
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("YOU DIED")))
						.Font(BigBoldFont)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.12f, 0.08f)))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 24.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Restarting...")))
						.Font(SmallRegularFont)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.6f, 0.45f)))
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
				.BorderBackgroundColor(FLinearColor(0.8f, 0.05f, 0.02f, 0.0f))
				.Padding(0)
				.Visibility(EVisibility::HitTestInvisible)
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
		PlayerHUD.HealthFillBox->SetWidthOverride(280.0f * Pct);
	}

	if (PlayerHUD.HealthFillBorder.IsValid())
	{
		// Color: dark red -> golden -> green based on health %
		FLinearColor BarColor;
		if (Pct > 0.6f)
			BarColor = FLinearColor(0.25f, 0.55f, 0.12f, 0.95f);  // Muted green
		else if (Pct > 0.3f)
			BarColor = FLinearColor(0.7f, 0.55f, 0.1f, 0.95f);    // Golden ochre
		else
			BarColor = FLinearColor(0.6f, 0.18f, 0.1f, 0.95f);    // Dark red

		PlayerHUD.HealthFillBorder->SetBorderBackgroundColor(BarColor);
	}

	if (PlayerHUD.HealthText.IsValid())
	{
		PlayerHUD.HealthText->SetText(FText::FromString(
			FString::Printf(TEXT("HEALTH  %d / %d"), FMath::Max((int32)HP, 0), (int32)PlayerHUD.MaxHealth)));
	}

	// Damage flash fade (0.3s)
	if (PlayerHUD.DamageFlashBorder.IsValid() && PlayerHUD.DamageFlashStartTime > 0.0)
	{
		float Elapsed = (float)(World->GetTimeSeconds() - PlayerHUD.DamageFlashStartTime);
		if (Elapsed < 0.3f)
		{
			PlayerHUD.DamageFlashBorder->SetBorderBackgroundColor(
				FLinearColor(0.8f, 0.05f, 0.02f, FMath::Lerp(0.45f, 0.0f, Elapsed / 0.3f)));
		}
		else
		{
			PlayerHUD.DamageFlashBorder->SetBorderBackgroundColor(
				FLinearColor(0.8f, 0.05f, 0.02f, 0.0f));
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
