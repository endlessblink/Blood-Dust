#include "GameplayHelperLibrary.h"
#include "IntroSequenceComponent.h"
#include "EnemyAnimInstance.h"
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
#include "Kismet/KismetSystemLibrary.h"
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
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr bool kEnableEnemySfx = false;
	constexpr bool kEnablePlayerMovementSfx = false;
	constexpr float kSfxGlobalVolume = 0.75f;
	constexpr float kPlayerStepVolume = 0.0f;
	constexpr float kEnemyHitVolume = 0.36f;
	constexpr float kEnemyStepVolume = 0.24f;
	constexpr float kUiSfxVolume = 0.75f;
}

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

	UAnimMontage* Montage = AnimInst->PlaySlotAnimationAsDynamicMontage(
		AnimSequence,
		FName("DefaultSlot"),
		BlendIn,
		BlendOut,
		PlayRate,
		1,      // LoopCount = 1 (play once)
		-1.0f,  // BlendOutTriggerTime = auto
		0.0f    // InTimeToStartMontageAt
	);

	// Keep one authoritative SFX path.
	// Animation SoundNotifies are handled by the animation system directly.

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
	TSharedPtr<STextBlock> VictoryActionText;
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
	TWeakObjectPtr<AActor> PortalTriggerActor;
	FVector PortalLocation = FVector::ZeroVector;
	float PortalTriggerRadius = 500.0f;
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

// --- Minimap ---

// Custom Slate widget that draws markers directly via OnPaint.
// No RenderTransform, no layout-based positioning — just direct draw calls.
// This guarantees markers stay within the widget's painted geometry.
class SMinimapMarkerLayer : public SLeafWidget
{
public:
	struct FMarkerData
	{
		FVector2D Position; // pixel position within widget
		float Size;
		const FSlateBrush* Brush;
		bool bVisible;
	};

	SLATE_BEGIN_ARGS(SMinimapMarkerLayer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetCanTick(false);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D::ZeroVector; // fills parent
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		for (const FMarkerData& M : Markers)
		{
			if (!M.bVisible || !M.Brush) continue;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(M.Size, M.Size),
					FSlateLayoutTransform(M.Position)),
				M.Brush,
				ESlateDrawEffect::None,
				FLinearColor::White
			);
		}
		return LayerId;
	}

	void SetMarkerCount(int32 Count)
	{
		Markers.SetNum(Count);
	}

	void SetMarker(int32 Index, const FVector2D& Pos, float Size, const FSlateBrush* Brush, bool bVisible)
	{
		if (Index < Markers.Num())
		{
			Markers[Index] = { Pos, Size, Brush, bVisible };
		}
	}

	void RequestRepaint()
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}

private:
	TArray<FMarkerData> Markers;
};

struct FMinimapState
{
	TWeakObjectPtr<UWorld> OwnerWorld;
	bool bCreated = false;

	// Textures
	TStrongObjectPtr<UTexture2D> MapTexture;
	FSlateBrush MapBrush;

	// Dot brushes (rounded circles)
	FSlateBrush PlayerGlowBrush;    // large semi-transparent orange halo
	FSlateBrush PlayerDotBrush;     // solid bright orange center
	FSlateBrush CheckpointActiveBrush;
	FSlateBrush CheckpointCollectedBrush;

	// Widget references
	TSharedPtr<SOverlay> RootWidget;
	TSharedPtr<SMinimapMarkerLayer> MarkerLayer;

	// Marker slot layout: [checkpoints 0..15] [player glow 16] [player dot 17]
	static constexpr int32 MaxCheckpointMarkers = 16;
	static constexpr int32 PlayerGlowSlot = MaxCheckpointMarkers;     // 16
	static constexpr int32 PlayerDotSlot = MaxCheckpointMarkers + 1;  // 17
	static constexpr int32 TotalMarkers = PlayerDotSlot + 1;          // 18

	// World bounds for coordinate mapping (auto-detected from landscape)
	FVector2D WorldMin = FVector2D(-15000, -15000);
	FVector2D WorldMax = FVector2D(15000, 15000);
};
static FMinimapState MinimapState;

// Minimap display constants (2x size)
static constexpr float MinimapWidth = 700.0f;
static constexpr float MinimapHeight = 436.0f;  // 700 * (637/1024) preserving aspect ratio
static constexpr float MinimapPlayerGlowSize = 36.0f;  // glow halo behind player
static constexpr float MinimapPlayerMarkerSize = 14.0f;
static constexpr float MinimapCheckpointMarkerSize = 12.0f;

// Inner map area (fraction of widget where markers move, inside the ornate frame)
// Tightened inward to keep markers well within the painted map, off the frame border
static constexpr float MinimapInnerLeft   = 0.175f;
static constexpr float MinimapInnerRight  = 0.825f;
static constexpr float MinimapInnerTop    = 0.150f;
static constexpr float MinimapInnerBottom = 0.660f;

// Coordinate mapping: adjust these to match map orientation to world
static constexpr bool bMinimapSwapXY = false;
static constexpr bool bMinimapFlipX  = false;
static constexpr bool bMinimapFlipY  = true;  // world Y+ typically = screen down, so flip

static FVector2D WorldToMinimapPos(const FVector& WorldPos, float MarkerSize)
{
	float NormX = (WorldPos.X - MinimapState.WorldMin.X) /
		FMath::Max(MinimapState.WorldMax.X - MinimapState.WorldMin.X, 1.0);
	float NormY = (WorldPos.Y - MinimapState.WorldMin.Y) /
		FMath::Max(MinimapState.WorldMax.Y - MinimapState.WorldMin.Y, 1.0);

	NormX = FMath::Clamp(NormX, 0.f, 1.f);
	NormY = FMath::Clamp(NormY, 0.f, 1.f);

	if (bMinimapSwapXY) Swap(NormX, NormY);
	if (bMinimapFlipX) NormX = 1.0f - NormX;
	if (bMinimapFlipY) NormY = 1.0f - NormY;

	// Absolute pixel coords within the full minimap widget (includes frame offset)
	float FrameL = MinimapWidth * MinimapInnerLeft;
	float FrameT = MinimapHeight * MinimapInnerTop;
	float FrameR = MinimapWidth * MinimapInnerRight;
	float FrameB = MinimapHeight * MinimapInnerBottom;
	float InnerW = FrameR - FrameL;
	float InnerH = FrameB - FrameT;

	float PixelX = FrameL + NormX * InnerW - MarkerSize * 0.5f;
	float PixelY = FrameT + NormY * InnerH - MarkerSize * 0.5f;

	// Clamp so marker stays entirely within the inner frame area
	PixelX = FMath::Clamp(PixelX, FrameL, FrameR - MarkerSize);
	PixelY = FMath::Clamp(PixelY, FrameT, FrameB - MarkerSize);

	return FVector2D(PixelX, PixelY);
}

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
	double LastHitReactEndTime = 0.0; // Stagger immunity: cooldown after hit-react ends
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
	double LastEnemyStepTime = 0.0;
	double LastGettingHitSfxTime = -100.0;

	// Floating health bar
	TWeakObjectPtr<UWidgetComponent> HealthBarComponent;
	float MaxHealth = 100.f;

	bool bPendingDamage = false;
	double PendingDamageTime = 0.0;
	float PendingDamageAmount = 0.f;
	float PendingDamageRadius = 0.f;

	// Diagnostic frame counter for periodic logging
	int32 DiagFrameCounter = 0;
};
static TMap<TWeakObjectPtr<AActor>, FEnemyAIStateData> EnemyAIStates;

// --- Per-Enemy-Type Sound Cache ---

struct FEnemyTypeSounds
{
	TArray<USoundBase*> HitSounds;
	TArray<USoundBase*> GettingHitSounds;
	TArray<USoundBase*> StepsSounds;
};

static TMap<FString, FEnemyTypeSounds> EnemyTypeSoundCache;

static FString GetEnemyTypeKey(AActor* Actor)
{
	if (!Actor) return FString();
	FString ClassName = Actor->GetClass()->GetName();
	if (ClassName.Contains(TEXT("Bell"), ESearchCase::IgnoreCase)) return TEXT("Bell");
	if (ClassName.Contains(TEXT("KingBot"), ESearchCase::IgnoreCase) || ClassName.Contains(TEXT("Kingbot"), ESearchCase::IgnoreCase)) return TEXT("KingBot");
	if (ClassName.Contains(TEXT("Gigantus"), ESearchCase::IgnoreCase) || ClassName.Contains(TEXT("Giganto"), ESearchCase::IgnoreCase) || ClassName.Contains(TEXT("Gigant"), ESearchCase::IgnoreCase)) return TEXT("Gigantus");
	return FString();
}

enum class EEnemySoundType : uint8 { Hit, GettingHit, Steps };

static void AddSoundIfExists(TArray<USoundBase*>& OutArray, const TCHAR* ObjectPath)
{
	USoundBase* Sound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, ObjectPath));
	if (Sound)
	{
		OutArray.Add(Sound);
	}
}

static void LoadNoamTrimEnemySounds(FEnemyTypeSounds& OutSounds, const FString& TypeKey)
{
	// Policy: enemy SFX must come from the noam-trim set.
	// In Content, those correspond to the numbered variants in each enemy folder.
	if (TypeKey == TEXT("Bell"))
	{
		AddSoundIfExists(OutSounds.HitSounds, TEXT("/Game/Audio/SFX/Bell/S_Bell_Hit_1.S_Bell_Hit_1"));
		AddSoundIfExists(OutSounds.GettingHitSounds, TEXT("/Game/Audio/SFX/Bell/S_Bell_GettingHit_1.S_Bell_GettingHit_1"));
		AddSoundIfExists(OutSounds.GettingHitSounds, TEXT("/Game/Audio/SFX/Bell/S_Bell_GettingHit_2.S_Bell_GettingHit_2"));
		AddSoundIfExists(OutSounds.StepsSounds, TEXT("/Game/Audio/SFX/Bell/S_Bell_Steps_1.S_Bell_Steps_1"));
		return;
	}

	if (TypeKey == TEXT("KingBot"))
	{
		AddSoundIfExists(OutSounds.HitSounds, TEXT("/Game/Audio/SFX/Kingbot/S_Kingbot_Hit_1.S_Kingbot_Hit_1"));
		AddSoundIfExists(OutSounds.GettingHitSounds, TEXT("/Game/Audio/SFX/Kingbot/S_Kingbot_GettingHit_1.S_Kingbot_GettingHit_1"));
		AddSoundIfExists(OutSounds.StepsSounds, TEXT("/Game/Audio/SFX/Kingbot/S_Kingbot_Steps_1.S_Kingbot_Steps_1"));
		return;
	}

	if (TypeKey == TEXT("Gigantus"))
	{
		AddSoundIfExists(OutSounds.HitSounds, TEXT("/Game/Audio/SFX/Gigantus/S_Gigantus_Hit_1.S_Gigantus_Hit_1"));
		AddSoundIfExists(OutSounds.GettingHitSounds, TEXT("/Game/Audio/SFX/Gigantus/S_Gigantus_GettingHit_1.S_Gigantus_GettingHit_1"));
		AddSoundIfExists(OutSounds.StepsSounds, TEXT("/Game/Audio/SFX/Gigantus/S_Gigantus_Steps_1.S_Gigantus_Steps_1"));
		return;
	}
}

static FEnemyTypeSounds* GetOrLoadEnemyTypeSounds(const FString& TypeKey)
{
	if (TypeKey.IsEmpty()) return nullptr;
	if (FEnemyTypeSounds* Existing = EnemyTypeSoundCache.Find(TypeKey))
		return Existing;

	FEnemyTypeSounds NewSounds;
	LoadNoamTrimEnemySounds(NewSounds, TypeKey);

	UE_LOG(LogTemp, Log, TEXT("EnemyTypeSounds: Loaded '%s' — Hit=%d GettingHit=%d Steps=%d"),
		*TypeKey, NewSounds.HitSounds.Num(), NewSounds.GettingHitSounds.Num(), NewSounds.StepsSounds.Num());

	EnemyTypeSoundCache.Add(TypeKey, NewSounds);
	return EnemyTypeSoundCache.Find(TypeKey);
}

static USoundBase* PickRandomSound(const TArray<USoundBase*>& Sounds)
{
	if (Sounds.Num() == 0) return nullptr;
	if (Sounds.Num() == 1) return Sounds[0];
	return Sounds[FMath::RandRange(0, Sounds.Num() - 1)];
}

static void PlayEnemyTypeSound(UWorld* World, AActor* EnemyActor, EEnemySoundType SoundType, float PitchScale = 1.0f)
{
	if (!kEnableEnemySfx) return;
	if (!World || !EnemyActor) return;
	FString TypeKey = GetEnemyTypeKey(EnemyActor);
	FEnemyTypeSounds* Sounds = GetOrLoadEnemyTypeSounds(TypeKey);
	if (!Sounds) return;

	USoundBase* Sound = nullptr;
	switch (SoundType)
	{
	case EEnemySoundType::Hit:        Sound = PickRandomSound(Sounds->HitSounds); break;
	case EEnemySoundType::GettingHit: Sound = PickRandomSound(Sounds->GettingHitSounds); break;
	case EEnemySoundType::Steps:      Sound = PickRandomSound(Sounds->StepsSounds); break;
	}
	if (Sound)
	{
		const bool bStep = (SoundType == EEnemySoundType::Steps);
		float BaseVolume = bStep ? kEnemyStepVolume : kEnemyHitVolume;
		// Bell steps: keep subtle but audible. Bell hits: slightly emphasized.
		if (TypeKey == TEXT("Bell"))
		{
			BaseVolume = bStep ? 0.24f : 0.46f;
		}
		const float Pitch = (bStep ? FMath::FRandRange(0.96f, 1.04f) : FMath::FRandRange(0.92f, 1.08f)) * PitchScale;
		const FVector SoundLoc = EnemyActor->GetActorLocation();
		UGameplayStatics::PlaySoundAtLocation(World, Sound, SoundLoc, BaseVolume, Pitch);
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

// --- Player Footstep System (distance-based, not AnimNotify) ---
// Tracks actual movement distance and plays alternating L/R footstep sounds.
// This is superior to AnimNotify for locomotion because it decouples sound
// timing from animation cycle length — footsteps sync to real movement.

struct FPlayerFootstepState
{
	TWeakObjectPtr<UWorld> OwnerWorld;
	bool bInitialized = false;
	FVector LastPosition = FVector::ZeroVector;
	float DistanceAccumulated = 0.0f;
	bool bLeftFoot = true;
	double LastStepTime = 0.0;
	USoundBase* WalkL = nullptr;
	USoundBase* WalkR = nullptr;
	USoundBase* HitSound1 = nullptr;
	USoundBase* HitSound2 = nullptr;
	USoundBase* DeathSound = nullptr;
	bool bSoundsLoaded = false;
};
static FPlayerFootstepState PlayerFootsteps;

static void UpdatePlayerFootsteps(ACharacter* Player)
{
	if (!kEnablePlayerMovementSfx)
	{
		return;
	}
	if (!Player) return;
	UWorld* World = Player->GetWorld();
	if (!World) return;

	// Reset if world changed (level restart)
	if (PlayerFootsteps.bInitialized && (!PlayerFootsteps.OwnerWorld.IsValid() || PlayerFootsteps.OwnerWorld.Get() != World))
	{
		PlayerFootsteps = FPlayerFootstepState();
	}

	// Load sounds once
	if (!PlayerFootsteps.bSoundsLoaded)
	{
		PlayerFootsteps.bSoundsLoaded = true;
		PlayerFootsteps.WalkL = Cast<USoundBase>(StaticLoadObject(
			USoundBase::StaticClass(), nullptr,
			TEXT("/Game/Audio/SFX/Hero/S_Hero_Walk_L.S_Hero_Walk_L")));
		PlayerFootsteps.WalkR = Cast<USoundBase>(StaticLoadObject(
			USoundBase::StaticClass(), nullptr,
			TEXT("/Game/Audio/SFX/Hero/S_Hero_Walk_R.S_Hero_Walk_R")));
		PlayerFootsteps.HitSound1 = Cast<USoundBase>(StaticLoadObject(
			USoundBase::StaticClass(), nullptr,
			TEXT("/Game/Audio/SFX/Hero/S_Hero_Hit_1.S_Hero_Hit_1")));
		PlayerFootsteps.HitSound2 = Cast<USoundBase>(StaticLoadObject(
			USoundBase::StaticClass(), nullptr,
			TEXT("/Game/Audio/SFX/Hero/S_Hero_Hit_2.S_Hero_Hit_2")));
		PlayerFootsteps.DeathSound = Cast<USoundBase>(StaticLoadObject(
			USoundBase::StaticClass(), nullptr,
			TEXT("/Game/Audio/SFX/Hero/S_Hero_Death.S_Hero_Death")));

		UE_LOG(LogTemp, Log, TEXT("PlayerFootsteps: WalkL=%s WalkR=%s Hit1=%s Hit2=%s Death=%s"),
			PlayerFootsteps.WalkL ? TEXT("OK") : TEXT("MISSING"),
			PlayerFootsteps.WalkR ? TEXT("OK") : TEXT("MISSING"),
			PlayerFootsteps.HitSound1 ? TEXT("OK") : TEXT("MISSING"),
			PlayerFootsteps.HitSound2 ? TEXT("OK") : TEXT("MISSING"),
			PlayerFootsteps.DeathSound ? TEXT("OK") : TEXT("MISSING"));
	}

	if (!PlayerFootsteps.bInitialized)
	{
		PlayerFootsteps.bInitialized = true;
		PlayerFootsteps.OwnerWorld = World;
		PlayerFootsteps.LastPosition = Player->GetActorLocation();
		return; // Skip first frame to establish baseline position
	}

	UCharacterMovementComponent* CMC = Player->GetCharacterMovement();
	if (!CMC) return;

	// Only play footsteps when grounded (not jumping/falling)
	if (!CMC->IsMovingOnGround()) return;

	// Calculate horizontal distance moved this frame
	FVector CurrentPos = Player->GetActorLocation();
	FVector Delta = CurrentPos - PlayerFootsteps.LastPosition;
	Delta.Z = 0.0f;
	float DistThisFrame = Delta.Size();
	PlayerFootsteps.LastPosition = CurrentPos;

	// Need minimum speed to count as walking (filters idle drift)
	float Speed = CMC->Velocity.Size2D();
	if (Speed < 10.0f)
	{
		PlayerFootsteps.DistanceAccumulated = 0.0f;
		return;
	}

	// Step distance scales with speed: longer strides when running
	// Walking (~400) = ~110 cm per step, Running (~800) = ~140 cm per step
	float StepDistance = FMath::Lerp(110.0f, 140.0f,
		FMath::Clamp((Speed - 200.0f) / 600.0f, 0.0f, 1.0f));

	// Minimum time between steps prevents machine-gun footsteps on frame spikes
	constexpr float MinStepInterval = 0.2f;
	double CurrentTime = World->GetTimeSeconds();

	PlayerFootsteps.DistanceAccumulated += DistThisFrame;

	if (PlayerFootsteps.DistanceAccumulated >= StepDistance
		&& (CurrentTime - PlayerFootsteps.LastStepTime) >= MinStepInterval)
	{
		USoundBase* StepSound = PlayerFootsteps.bLeftFoot
			? PlayerFootsteps.WalkL
			: PlayerFootsteps.WalkR;

		if (StepSound)
		{
			// Slight pitch variation for naturalness
			float PitchVar = FMath::FRandRange(0.95f, 1.05f);
			UGameplayStatics::PlaySoundAtLocation(
				World, StepSound, Player->GetActorLocation(), kPlayerStepVolume, PitchVar);
		}

		PlayerFootsteps.bLeftFoot = !PlayerFootsteps.bLeftFoot;
		PlayerFootsteps.DistanceAccumulated = 0.0f;
		PlayerFootsteps.LastStepTime = CurrentTime;
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

	// --- FORWARD CONE FILTER ---
	// Melee attacks are directional: only hit targets roughly in front of the attacker.
	// This prevents hitting enemies behind or beside the attacker.
	{
		FVector Fwd = Attacker->GetActorForwardVector();
		Fwd.Z = 0.f;
		if (!Fwd.IsNearlyZero())
		{
			Fwd.Normalize();
			TSet<ACharacter*> InCone;
			for (ACharacter* HC : HitCharacters)
			{
				FVector ToTarget = HC->GetActorLocation() - Origin;
				ToTarget.Z = 0.f;
				float Dist = ToTarget.Size();
				if (Dist < 1.f) { InCone.Add(HC); continue; } // Overlapping = always hit
				FVector Dir = ToTarget / Dist;
				// cos(80°) ≈ 0.17 → ~160° cone in front of attacker
				if (FVector::DotProduct(Fwd, Dir) > 0.17f)
				{
					InCone.Add(HC);
				}
			}
			HitCharacters = InCone;
		}
	}

	// Determine if attacker is the player (for friendly-fire prevention)
	ACharacter* PlayerCharDmg = UGameplayStatics::GetPlayerCharacter(World, 0);
	bool bAttackerIsPlayer = (Attacker == PlayerCharDmg);

	// --- SINGLE TARGET for player melee ---
	// Player attacks hit only the closest enemy in the cone.
	// This prevents AoE splash damage on grouped enemies.
	if (bAttackerIsPlayer && HitCharacters.Num() > 1)
	{
		ACharacter* Closest = nullptr;
		float ClosestDistSq = FLT_MAX;
		for (ACharacter* HC : HitCharacters)
		{
			float DSq = FVector::DistSquared(Origin, HC->GetActorLocation());
			if (DSq < ClosestDistSq)
			{
				ClosestDistSq = DSq;
				Closest = HC;
			}
		}
		HitCharacters.Empty();
		if (Closest) HitCharacters.Add(Closest);
	}

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
		// Enemy durability tuning for player attacks
		if (bAttackerIsPlayer && !bVictimIsPlayer)
		{
			const FString VictimClass = Victim->GetClass()->GetName();
			if (VictimClass.Contains(TEXT("Bell"), ESearchCase::IgnoreCase))
			{
				EffectiveDamage *= 0.60f;
			}
			else if (VictimClass.Contains(TEXT("KingBot"), ESearchCase::IgnoreCase))
			{
				EffectiveDamage *= 0.85f;
			}
			else if (VictimClass.Contains(TEXT("Giganto"), ESearchCase::IgnoreCase)
				|| VictimClass.Contains(TEXT("Gigantus"), ESearchCase::IgnoreCase))
			{
				EffectiveDamage *= 0.80f;
			}
		}
		CurrentHealth -= EffectiveDamage;

		// Enemy attack-hit SFX: play only on confirmed damage to player.
		// This keeps timing tied to real hits and prevents random far-away punch sounds.
		if (!bAttackerIsPlayer && bVictimIsPlayer)
		{
			static TMap<TWeakObjectPtr<AActor>, double> LastEnemyHitSfxTime;
			const TWeakObjectPtr<AActor> AttackerKey(Attacker);
			const double Now = World->GetTimeSeconds();
			double& LastTime = LastEnemyHitSfxTime.FindOrAdd(AttackerKey);
			if ((Now - LastTime) >= 0.40)
			{
				PlayEnemyTypeSound(World, Attacker, EEnemySoundType::Hit);
				LastTime = Now;
			}
		}

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

			// Player hit reaction animation (non-lethal only)
			static UAnimSequence* PlayerHitAnim = nullptr;
			static bool bPlayerHitAnimTried = false;
			if (!bPlayerHitAnimTried)
			{
				bPlayerHitAnimTried = true;
				PlayerHitAnim = Cast<UAnimSequence>(StaticLoadObject(
					UAnimSequence::StaticClass(), nullptr,
					TEXT("/Game/Characters/Robot/Animations/getting-hit.getting-hit")));
			}
			if (PlayerHitAnim)
			{
				PlayAnimationOneShot(PlayerCharFlash, PlayerHitAnim, 1.0f, 0.06f, 0.12f, false, true);
			}
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

		// Per-instance base randomization (kept tight to avoid locomotion foot sliding).
		State.SpeedMultiplier = FMath::FRandRange(0.85f, 1.15f);
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
				State.SpeedMultiplier *= 1.2f;         // Fast, but still within run clip range
				State.AggroRangeMultiplier *= 0.5f;    // Only reacts when close
				State.AttackCooldownJitter -= 1.0f;    // Attacks rapidly
				State.ReactionDelay *= 0.2f;           // Near-instant reaction
				State.DamageMultiplier = 0.7f;         // Lower damage per hit
				break;
			case EEnemyPersonality::Stalker:
				State.SpeedMultiplier *= 0.8f;         // Slow, menacing approach
				State.AggroRangeMultiplier *= 2.0f;    // Notices player from far
				State.ReactionDelay *= 2.5f;           // Long stare before moving
				State.WobbleAmplitude *= 2.0f;         // Weaving approach
				State.DamageMultiplier = 1.0f;
				break;
			case EEnemyPersonality::Brute:
				State.SpeedMultiplier *= 0.9f;         // Slow and heavy
				State.WobbleAmplitude *= 0.2f;         // Charges straight
				State.AttackCooldownJitter += 0.5f;    // Slower attacks
				State.DamageMultiplier = 1.8f;         // Hits HARD
				break;
			case EEnemyPersonality::Crawler:
				State.SpeedMultiplier *= 0.75f;        // Creeping
				State.AggroRangeMultiplier *= 1.4f;    // Aware
				State.ReactionDelay *= 0.5f;           // Quick to start crawling
				State.AnimPlayRateVariation *= 0.8f;   // Slower anim
				State.DamageMultiplier = 1.2f;
				break;
			default: // Normal
				State.DamageMultiplier = 1.0f;
				break;
			}

			// KingBot-specific stability tuning to reduce "wobbly" locomotion feel.
			const FString InitClassName = Enemy->GetClass()->GetName();
			if (InitClassName.Contains(TEXT("KingBot"), ESearchCase::IgnoreCase))
			{
				State.WobbleAmplitude *= 0.25f;
				State.SpeedMultiplier = FMath::Clamp(State.SpeedMultiplier, 0.95f, 1.05f);
				State.AnimPlayRateVariation = 1.0f;
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
			// Supports multiple naming conventions:
			//   Bell/KingBot:  {Type}/Animations/{Type}_{AnimName}
			//   Giganto-style: {Type}/Anim_{AnimName} (root folder, Anim_ prefix)
			{
				FString AnimClassName = Enemy->GetClass()->GetName();
				AnimClassName.RemoveFromEnd(TEXT("_C"));
				FString AnimEnemyType = AnimClassName;
				AnimEnemyType.RemoveFromStart(TEXT("BP_"));

				// Base paths: standard subfolder + root folder
				FString AnimSubPath = FString::Printf(TEXT("/Game/Characters/Enemies/%s/Animations/"), *AnimEnemyType);
				FString AnimRootPath = FString::Printf(TEXT("/Game/Characters/Enemies/%s/"), *AnimEnemyType);

				// Try loading from multiple naming conventions
				auto TryLoadAnimMulti = [&](const FString& AnimSuffix) -> UAnimSequence* {
					// 1. Standard: {Type}/Animations/{Type}_{Suffix}
					FString Name1 = AnimEnemyType + TEXT("_") + AnimSuffix;
					FString Path1 = AnimSubPath + Name1 + TEXT(".") + Name1;
					UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *Path1);
					if (Anim) return Anim;

					// 2. Giganto-style: {Type}/Anim_{Suffix}
					FString Name2 = TEXT("Anim_") + AnimSuffix;
					FString Path2 = AnimRootPath + Name2 + TEXT(".") + Name2;
					Anim = LoadObject<UAnimSequence>(nullptr, *Path2);
					if (Anim) return Anim;

					// 3. Root folder with Type prefix: {Type}/{Type}_{Suffix}
					FString Path3 = AnimRootPath + Name1 + TEXT(".") + Name1;
					Anim = LoadObject<UAnimSequence>(nullptr, *Path3);
					return Anim;
				};

				// Hit-react: prefer BodyBlock, then TakingPunch
				UAnimSequence* HitReactCandidate = TryLoadAnimMulti(TEXT("BodyBlock"));
				if (!HitReactCandidate) HitReactCandidate = TryLoadAnimMulti(TEXT("TakingPunch"));
				if (HitReactCandidate)
				{
					State.ChosenHitReactAnim = HitReactCandidate;
				}
				else
				{
					State.ChosenHitReactAnim = HitReactAnim; // fallback to BP param
				}

				// Death: try multiple naming variants
				{
					UAnimSequence* DeathCandidate = TryLoadAnimMulti(TEXT("Death"));
					if (!DeathCandidate) DeathCandidate = TryLoadAnimMulti(TEXT("Dying"));
					if (!DeathCandidate) DeathCandidate = TryLoadAnimMulti(TEXT("ZombieDying"));
					if (!DeathCandidate) DeathCandidate = TryLoadAnimMulti(TEXT("RifleHitBack"));
					if (DeathCandidate) State.ChosenDeathAnim = DeathCandidate;
				}

				// Attack: override BP param if convention finds one
				{
					UAnimSequence* AttackCandidate = TryLoadAnimMulti(TEXT("ZombieAttack"));
					if (!AttackCandidate) AttackCandidate = TryLoadAnimMulti(TEXT("Punching"));
					if (!AttackCandidate) AttackCandidate = TryLoadAnimMulti(TEXT("Biting"));
					if (!AttackCandidate) AttackCandidate = TryLoadAnimMulti(TEXT("NeckBite"));
					if (!AttackCandidate) AttackCandidate = TryLoadAnimMulti(TEXT("ZombieStandUp")); // Giganto: use standup as attack
					if (AttackCandidate) State.ChosenAttackAnim = AttackCandidate;
				}

				UE_LOG(LogTemp, Log, TEXT("UpdateEnemyAI [%s]: Anim discovery — HitReact=%s, Death=%s, Attack=%s"),
					*AnimEnemyType,
					State.ChosenHitReactAnim ? *State.ChosenHitReactAnim->GetName() : TEXT("NONE"),
					State.ChosenDeathAnim ? *State.ChosenDeathAnim->GetName() : TEXT("NONE"),
					State.ChosenAttackAnim ? *State.ChosenAttackAnim->GetName() : TEXT("NONE"));
			}

			// Initialize idle behavior timer (staggered per instance)
			State.NextIdleBehaviorTime = FMath::FRandRange(2.0f, 8.0f);
			State.IdleBehaviorTimer = 0.0f;
		}

		// Snap to ground on first tick using WorldStatic trace (hits landscape, ignores HISM grass)
		{
			// Use capsule half-height for ground offset — NOT GetActorBounds().
			// GetActorBounds includes mesh scale (e.g. 3x Bell mesh) which produces
			// a massive offset and floats the character far above the terrain.
			// For ACharacter, actor location = capsule center, so feet = surface + HalfHeight.
			UCapsuleComponent* SnapCapsule = Enemy->GetCapsuleComponent();
			float SnapOffset = SnapCapsule ? SnapCapsule->GetScaledCapsuleHalfHeight() : 90.0f;

			FVector Loc = Enemy->GetActorLocation();

			FHitResult SnapHit;
			FVector SnapStart = FVector(Loc.X, Loc.Y, Loc.Z + 5000.0f);
			FVector SnapEnd = FVector(Loc.X, Loc.Y, Loc.Z - 5000.0f);
			FCollisionQueryParams SnapParams;
			SnapParams.AddIgnoredActor(Enemy);

			if (World->LineTraceSingleByChannel(SnapHit, SnapStart, SnapEnd, ECC_WorldStatic, SnapParams))
			{
				FVector SnappedLoc = FVector(Loc.X, Loc.Y, SnapHit.Location.Z + SnapOffset);
				Enemy->SetActorLocation(SnappedLoc, false, nullptr, ETeleportType::TeleportPhysics);
				UE_LOG(LogTemp, Warning, TEXT("EnemyAI INIT [%s]: SnapOffset=%.1f SurfaceZ=%.1f NewZ=%.1f CapsuleHH=%.1f CapsuleR=%.1f"),
					*Enemy->GetName(), SnapOffset, SnapHit.Location.Z, SnappedLoc.Z,
					SnapCapsule ? SnapCapsule->GetScaledCapsuleHalfHeight() : -1.f,
					SnapCapsule ? SnapCapsule->GetScaledCapsuleRadius() : -1.f);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("EnemyAI INIT [%s]: Ground trace MISSED! No landscape below."), *Enemy->GetName());
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
			InitMoveComp->SetMovementMode(MOVE_Walking);        // Start on ground (snap already placed us)
			// Force CMC to recognize floor immediately (prevents one-frame fall after snap)
			InitMoveComp->FindFloor(Enemy->GetActorLocation(), InitMoveComp->CurrentFloor, false);
		}

		// === Perplexity-recommended proactive fixes (2026-02-16) ===
		// Force-fix ALL known non-animation causes of gliding on spawned instances.
		UE_LOG(LogTemp, Warning, TEXT("UpdateEnemyAI INIT BUILD_ID=2026-02-16-v16 enemy=%s"), *Enemy->GetName());
		USkeletalMeshComponent* InitMesh = Enemy->GetMesh();
		if (InitMesh)
		{
			// FIX 1: Force VisibilityBasedAnimTickOption to always tick.
			// Default can be ONLY_TICK_POSE_WHEN_RENDERED which culls anim updates.
			InitMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

			// FIX 2: Ensure anims are not paused
			InitMesh->bPauseAnims = false;

			// FIX 3: Ensure component tick is enabled
			InitMesh->SetComponentTickEnabled(true);

			// FIX 4: ALWAYS force SetAnimInstanceClass on spawned instances.
			// CDO component defaults may not propagate AnimClass reliably.
			// SetAnimInstanceClass clears+reinits the anim instance and sets mode to AnimationBlueprint.
			FString ClassName = Enemy->GetClass()->GetName();
			ClassName.RemoveFromEnd(TEXT("_C"));
			FString EnemyType = ClassName;
			EnemyType.RemoveFromStart(TEXT("BP_"));

			// Bell-specific skeleton compatibility enforcement:
			// never run Bell with the deprecated SK_Bell_New_Skeleton.
			if (EnemyType == TEXT("Bell"))
			{
				USkeleton* TargetBellSkeleton = LoadObject<USkeleton>(
					nullptr,
					TEXT("/Game/Characters/Enemies/Bell/SK_Bell_Skeleton.SK_Bell_Skeleton"));

				if (TargetBellSkeleton)
				{
					USkeletalMesh* CurrentMesh = InitMesh->GetSkeletalMeshAsset();
					USkeleton* CurrentSkel = CurrentMesh ? CurrentMesh->GetSkeleton() : nullptr;

					if (CurrentSkel != TargetBellSkeleton)
					{
						USkeletalMesh* CompatibleMesh = nullptr;
						const TCHAR* BellMeshCandidates[] = {
							TEXT("/Game/Characters/Enemies/Bell/SK_Bell.SK_Bell"),
							TEXT("/Game/Characters/Enemies/Bell/SK_Bell_Anim.SK_Bell_Anim"),
							TEXT("/Game/Characters/Enemies/Bell/SK_Bell_New.SK_Bell_New")
						};

						for (const TCHAR* Path : BellMeshCandidates)
						{
							if (USkeletalMesh* Candidate = LoadObject<USkeletalMesh>(nullptr, Path))
							{
								if (Candidate->GetSkeleton() == TargetBellSkeleton)
								{
									CompatibleMesh = Candidate;
									break;
								}
							}
						}

						if (CompatibleMesh)
						{
							InitMesh->SetSkeletalMesh(CompatibleMesh);
							UE_LOG(LogTemp, Warning, TEXT("EnemyAI INIT [%s]: Switched Bell mesh to %s for SK_Bell_Skeleton compatibility"),
								*Enemy->GetName(), *CompatibleMesh->GetPathName());
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("EnemyAI INIT [%s]: No Bell mesh found with SK_Bell_Skeleton"), *Enemy->GetName());
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EnemyAI INIT [%s]: Failed to load SK_Bell_Skeleton"), *Enemy->GetName());
				}
			}

			FString AnimBPPath = FString::Printf(
				TEXT("/Game/Characters/Enemies/%s/ABP_BG_%s.ABP_BG_%s_C"),
				*EnemyType, *EnemyType, *EnemyType
			);

			UClass* AnimBPClass = LoadObject<UClass>(nullptr, *AnimBPPath);
			if (AnimBPClass)
			{
				// Always force-assign — don't trust CDO propagation
				InitMesh->SetAnimInstanceClass(AnimBPClass);
				UE_LOG(LogTemp, Warning, TEXT("EnemyAI INIT [%s]: Force-assigned AnimBP %s, Mode=%d, bPauseAnims=%d, VisTick=%d"),
					*Enemy->GetName(), *AnimBPPath,
					(int32)InitMesh->GetAnimationMode(),
					(int32)InitMesh->bPauseAnims,
					(int32)InitMesh->VisibilityBasedAnimTickOption);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("EnemyAI INIT [%s]: FAILED to load AnimBP at %s"), *Enemy->GetName(), *AnimBPPath);
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

	// === DIAGNOSTIC LOGGING (every 120 frames per enemy) ===
	if (++State.DiagFrameCounter >= 120)
	{
		State.DiagFrameCounter = 0;
		UCharacterMovementComponent* DiagCMC = Enemy->GetCharacterMovement();
		USkeletalMeshComponent* DiagMesh = Enemy->GetMesh();
		UAnimInstance* DiagAnim = DiagMesh ? DiagMesh->GetAnimInstance() : nullptr;

		static const TCHAR* StateNames[] = { TEXT("Idle"), TEXT("Chase"), TEXT("Attack"), TEXT("Return"), TEXT("HitReact"), TEXT("Dead"), TEXT("Patrol") };
		int32 StateIdx = FMath::Clamp((int32)State.CurrentState, 0, 6);

		float DiagVel = Enemy->GetVelocity().Size();
		float DiagVel2D = Enemy->GetVelocity().Size2D();
		float DiagMaxSpeed = DiagCMC ? DiagCMC->MaxWalkSpeed : -1.f;
		int32 DiagMoveMode = DiagCMC ? (int32)DiagCMC->MovementMode : -1;
		bool DiagHasFloor = DiagCMC ? DiagCMC->CurrentFloor.bBlockingHit : false;
		bool DiagAnimValid = DiagAnim != nullptr;
		FString DiagAnimClass = DiagAnim ? DiagAnim->GetClass()->GetName() : TEXT("NULL");

		// Check AnimInstance Speed AND LocSpeed properties via reflection
		float DiagAnimSpeed = -1.f;
		float DiagLocSpeed = -1.f;
		if (DiagAnim)
		{
			FProperty* SpeedProp = DiagAnim->GetClass()->FindPropertyByName(FName("Speed"));
			if (SpeedProp)
			{
				void* SpeedPtr = SpeedProp->ContainerPtrToValuePtr<void>(DiagAnim);
				if (FFloatProperty* FP = CastField<FFloatProperty>(SpeedProp))
					DiagAnimSpeed = FP->GetPropertyValue(SpeedPtr);
				else if (FDoubleProperty* DP = CastField<FDoubleProperty>(SpeedProp))
					DiagAnimSpeed = (float)DP->GetPropertyValue(SpeedPtr);
			}
			FProperty* LocSpeedProp = DiagAnim->GetClass()->FindPropertyByName(FName("LocSpeed"));
			if (LocSpeedProp)
			{
				void* LocSpeedPtr = LocSpeedProp->ContainerPtrToValuePtr<void>(DiagAnim);
				if (FFloatProperty* FP = CastField<FFloatProperty>(LocSpeedProp))
					DiagLocSpeed = FP->GetPropertyValue(LocSpeedPtr);
				else if (FDoubleProperty* DP = CastField<FDoubleProperty>(LocSpeedProp))
					DiagLocSpeed = (float)DP->GetPropertyValue(LocSpeedPtr);
			}
		}

		// Perplexity-recommended diagnostics: AnimationMode, bPauseAnims, VisibilityBasedAnimTickOption, AnimClass
		int32 DiagAnimMode = DiagMesh ? (int32)DiagMesh->GetAnimationMode() : -1;
		bool DiagPauseAnims = DiagMesh ? DiagMesh->bPauseAnims : false;
		int32 DiagVisTick = DiagMesh ? (int32)DiagMesh->VisibilityBasedAnimTickOption : -1;
		FString DiagAnimBPClass = (DiagMesh && DiagMesh->GetAnimClass()) ? DiagMesh->GetAnimClass()->GetName() : TEXT("NONE");
		bool DiagMeshTickEnabled = DiagMesh ? DiagMesh->IsComponentTickEnabled() : false;
		bool DiagHasController = Enemy->GetController() != nullptr;

		// Location delta tracking for CMC vs actual movement comparison
		FVector CurLoc = Enemy->GetActorLocation();
		float LocDelta = FVector::Dist(CurLoc, State.SpawnLocation); // rough distance from spawn

		UE_LOG(LogTemp, Warning, TEXT("DIAG [%s] State=%s MoveMode=%d HasFloor=%d Vel=%.1f Vel2D=%.1f MaxSpeed=%.1f AnimMode=%d AnimClass=%s AnimBP=%s Speed=%.1f LocSpeed=%.1f bPause=%d VisTick=%d MeshTick=%d HasCtrl=%d Dist=%.0f"),
			*Enemy->GetName(), StateNames[StateIdx], DiagMoveMode, DiagHasFloor,
			DiagVel, DiagVel2D, DiagMaxSpeed,
			DiagAnimMode, *DiagAnimClass, *DiagAnimBPClass, DiagAnimSpeed, DiagLocSpeed,
			(int32)DiagPauseAnims, DiagVisTick, (int32)DiagMeshTickEnabled,
			(int32)DiagHasController, DistToPlayer);
	}

	// Check Health, auto-init if 0, handle death + hit reactions
	FProperty* HealthProp = Enemy->GetClass()->FindPropertyByName(FName("Health"));
	float HP = 100.f;
	if (HealthProp)
	{
		void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Enemy);
		HP = 0.f;
		if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp)) HP = FP->GetPropertyValue(ValPtr);
		else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp)) HP = (float)DP->GetPropertyValue(ValPtr);

		// Always override HP on first tick based on enemy type.
		// Blueprint default (100) is ignored — C++ controls type-based HP scaling.
		if (!State.bHealthInitialized)
		{
			State.bHealthInitialized = true;
			HP = 110.f; // Bell: should survive several player hits
			FString ClassName = Enemy->GetClass()->GetName();
			if (ClassName.Contains(TEXT("KingBot")))
			{
				HP = 200.f; // KingBot: mid-tier
			}
			else if (ClassName.Contains(TEXT("Giganto")))
			{
				HP = 450.f; // Giganto: tank, hard to kill
			}
			if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp)) FP->SetPropertyValue(ValPtr, HP);
			else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp)) DP->SetPropertyValue(ValPtr, (double)HP);
			UE_LOG(LogTemp, Log, TEXT("UpdateEnemyAI: %s HP initialized to %.0f"), *ClassName, HP);
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
	UCapsuleComponent* LiveCapsule = Enemy->GetCapsuleComponent();
	if (HP > 0.f && LiveCapsule)
	{
		// Keep live enemies physically blocking the player while in combat.
		LiveCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
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
			State.bPendingDamage = false;
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

			// Signal AnimInstance to freeze Speed updates (BlendSpace stays at last pose)
			if (MeshComp)
			{
				if (UEnemyAnimInstance* EnemyAnim = Cast<UEnemyAnimInstance>(MeshComp->GetAnimInstance()))
				{
					EnemyAnim->bIsDead = true;
				}
			}

			// Play per-type enemy death sound: reuse GettingHit, but slower pitch.
			PlayEnemyTypeSound(World, Enemy, EEnemySoundType::GettingHit, 0.78f);
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
		// Stagger immunity: 0.5s cooldown after last hit-react to prevent infinite stagger lock
		bool bStaggerImmune = (State.LastHitReactEndTime > 0.0 && (CurrentTime - State.LastHitReactEndTime) < 0.5);
		if (State.CurrentState != EEnemyAIState::HitReact && !bStaggerImmune)
		{
			State.PreHitReactState = State.CurrentState;
			State.CurrentState = EEnemyAIState::HitReact;
			State.HitReactStartTime = CurrentTime;
			State.bPendingDamage = false;

			// Play hit react when available; otherwise still force a brief stagger.
			if (UsedHitReactAnim)
			{
				PlayAnimationOneShot(Enemy, UsedHitReactAnim, 1.0f, 0.1f, 0.15f, false, /*bForceInterrupt=*/true);
			}
			else if (MoveComp)
			{
				MoveComp->StopMovementImmediately();
			}

			// Avoid audio spam when multiple hits land in a tight window.
			if ((CurrentTime - State.LastGettingHitSfxTime) >= 0.45)
			{
				PlayEnemyTypeSound(World, Enemy, EEnemySoundType::GettingHit);
				State.LastGettingHitSfxTime = CurrentTime;
			}
		}
	}
	State.PreviousHealth = HP;

	// HitReact -> previous state after animation finishes
	if (State.CurrentState == EEnemyAIState::HitReact)
	{
		UAnimSequence* HitReactForLen = State.ChosenHitReactAnim ? State.ChosenHitReactAnim : HitReactAnim;
		float HitReactLen = HitReactForLen ? FMath::Min(HitReactForLen->GetPlayLength(), 0.5f) : 0.5f;
		if ((CurrentTime - State.HitReactStartTime) > HitReactLen)
		{
			State.CurrentState = State.PreHitReactState;
			State.LastHitReactEndTime = CurrentTime;

			// CRITICAL: Stop the hit-react montage so the Slot node releases
			// back to BlendSpace locomotion. Without this, the montage blocks
			// the BlendSpace output on DefaultSlot indefinitely.
			USkeletalMeshComponent* HRMesh = Enemy->GetMesh();
			if (HRMesh)
			{
				if (UAnimInstance* HRAnim = HRMesh->GetAnimInstance())
				{
					HRAnim->Montage_Stop(0.15f);
				}
			}
		}
	}

	// Update CMC walk speed (includes per-instance variation)
	if (MoveComp)
	{
		const float RawSpeed = MoveSpeed * State.SpeedMultiplier;
		// Keep authored speed profile; animation graph should adapt to movement, not vice-versa.
		MoveComp->MaxWalkSpeed = RawSpeed;
	}

	// Process pending damage (delayed from attack windup)
	if (State.bPendingDamage && CurrentTime >= State.PendingDamageTime)
	{
		// Only deal damage if still in Attack state (cancelled by hit-react, death, etc.)
		if (State.CurrentState == EEnemyAIState::Attack)
		{
			ApplyMeleeDamage(Enemy, State.PendingDamageAmount, State.PendingDamageRadius, 30000.f);
		}
		State.bPendingDamage = false;
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
				// Attack SFX handled by AnimNotify baked into animation assets
			}

			// Queue delayed damage — gives player a window to stun/interrupt the attack
			State.bPendingDamage = true;
			float WindupDelay = 0.50f;
			if (UsedAttackAnim)
			{
				// Land hit around the "contact" part of the swing.
				WindupDelay = FMath::Clamp(UsedAttackAnim->GetPlayLength() * 0.58f, 0.30f, 0.85f);
			}
			// Per-enemy timing overrides for better visual sync.
			const FString EnemyClassName = Enemy->GetClass()->GetName();
			if (EnemyClassName.Contains(TEXT("Giganto"), ESearchCase::IgnoreCase)
				|| EnemyClassName.Contains(TEXT("Gigantus"), ESearchCase::IgnoreCase))
			{
				WindupDelay = UsedAttackAnim ? FMath::Clamp(UsedAttackAnim->GetPlayLength() * 0.68f, 0.45f, 1.10f) : 0.70f;
			}
			else if (EnemyClassName.Contains(TEXT("KingBot"), ESearchCase::IgnoreCase))
			{
				WindupDelay = UsedAttackAnim ? FMath::Clamp(UsedAttackAnim->GetPlayLength() * 0.60f, 0.35f, 0.90f) : 0.55f;
			}
			else if (EnemyClassName.Contains(TEXT("Bell"), ESearchCase::IgnoreCase))
			{
				WindupDelay = UsedAttackAnim ? FMath::Clamp(UsedAttackAnim->GetPlayLength() * 0.52f, 0.28f, 0.75f) : 0.42f;
			}
			State.PendingDamageTime = CurrentTime + WindupDelay;
			State.PendingDamageAmount = AttackDamage * State.DamageMultiplier;
			State.PendingDamageRadius = AttackRadius;
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
					// Attack SFX handled by AnimNotify baked into animation assets
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

		// Load health bar textures — UVRegion crops out black padding at render time
		// Content bounds measured from 2816x1536 source PNGs (getbbox)
		const float SrcW = 2816.0f, SrcH = 1536.0f;
		const float ContentL = 120.0f, ContentT = 116.0f;
		const float ContentR = 2746.0f, ContentB = 1239.0f;
		const float ContentW = ContentR - ContentL;  // 2626px actual content
		const float ContentH = ContentB - ContentT;  // 1123px actual content

		const float HealthBarWidth = 500.0f;
		const float HealthBarHeight = HealthBarWidth * (ContentH / ContentW); // ~214px, content-only aspect

		// UV sub-region tells Slate to render ONLY the content area, ignoring padding
		const FBox2f ContentUV(
			FVector2f(ContentL / SrcW, ContentT / SrcH),   // top-left  (0.0426, 0.0755)
			FVector2f(ContentR / SrcW, ContentB / SrcH));   // bot-right (0.9751, 0.8066)

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
				Brush.SetUVRegion(ContentUV);
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
				.Clipping(EWidgetClipping::ClipToBounds)
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
						SAssignNew(PlayerHUD.VictoryActionText, STextBlock)
						.Text(FText::FromString(TEXT("R - Restart Run    Esc - Exit")))
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
	// With UVRegion crop at col 120-2746 (2626px content), tube is at content-local 1215-2320
	const float HBDisplayWidth = 500.0f;
	const float TubeLeftPx = ((1335.0f - 120.0f) / 2626.0f) * HBDisplayWidth;   // ~231px
	const float TubeRightPx = ((2440.0f - 120.0f) / 2626.0f) * HBDisplayWidth;  // ~442px
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

	// --- Player Footsteps (distance-based) ---
	UpdatePlayerFootsteps(Player);

	// Handle death
	if (HP <= 0.0f)
	{
		PlayerHUD.bDead = true;

		// Play death sound
		if (PlayerFootsteps.DeathSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				World, PlayerFootsteps.DeathSound, Player->GetActorLocation(), 1.0f, 1.0f);
		}

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
					MinimapState = FMinimapState();
					EnemyAIStates.Empty();
					BlockingActors.Empty();
					MusicSystem = FMusicState();
					PlayerFootsteps = FPlayerFootstepState();
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
		GameFlow.PortalTriggerRadius = 500.0f;

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
			else if (Name.Contains(TEXT("Portal_Light")))
			{
				GameFlow.PortalLightActor = Light;
				GameFlow.PortalLocation = Light->GetActorLocation();
			}
			else if (Name.Contains(TEXT("Portal_Beacon")) && !GameFlow.PortalLightActor.IsValid())
			{
				// Fallback only: prefer explicit Portal_Light when both exist.
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

		// Prefer explicit portal trigger actor when available.
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor)) continue;

			const FString Name = Actor->GetName();
			if (Name.Contains(TEXT("PortalTrigger")) || Name.Contains(TEXT("BP_PortalTrigger")))
			{
				GameFlow.PortalTriggerActor = Actor;
				GameFlow.PortalLocation = Actor->GetActorLocation();

				FVector BoundsOrigin = FVector::ZeroVector;
				FVector BoundsExtent = FVector::ZeroVector;
				Actor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
				const float RadiusFromBounds = FMath::Max(BoundsExtent.X, BoundsExtent.Y);
				GameFlow.PortalTriggerRadius = FMath::Max(RadiusFromBounds + 150.0f, 500.0f);
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

	// If dead, don't update game-flow logic (death screen handles restart)
	if (PlayerHUD.bDead) return;

	// Victory mode: wait for explicit player choice (no auto-reset).
	if (GameFlow.bVictory)
	{
		APlayerController* PC = Cast<APlayerController>(Player->GetController());
		if (!PC) return;

		if (PC->WasInputKeyJustPressed(EKeys::R))
		{
			// Reset both HUD and GameFlow state before level transition.
			PlayerHUD = FPlayerHUDState();
			GameFlow = FGameFlowState();
			EnemyAIStates.Empty();
			BlockingActors.Empty();
			MusicSystem = FMusicState();
			PlayerFootsteps = FPlayerFootstepState();
			EnemyTypeSoundCache.Empty();

			FString LevelName = UGameplayStatics::GetCurrentLevelName(World, true);
			UGameplayStatics::OpenLevel(World, FName(*LevelName));
		}
		else if (PC->WasInputKeyJustPressed(EKeys::Escape))
		{
			UKismetSystemLibrary::QuitGame(World, PC, EQuitPreference::Quit, false);
		}
		return;
	}

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
				GameFlow.CheckpointsCollected = FMath::Min(GameFlow.CheckpointsCollected + 1, GameFlow.TotalCheckpoints);
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
					UGameplayStatics::PlaySound2D(World, CheckpointSound, kUiSfxVolume, 1.0f);
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

	// Keep counter synchronized with actual checkpoint states to avoid
	// stale "not all recovered" gate blocking when actors were invalidated externally.
	int32 CollectedByState = 0;
	for (const FCheckpointData& CP : GameFlow.Checkpoints)
	{
		if (CP.State == ECheckpointState::Collected)
		{
			CollectedByState++;
		}
	}
	if (CollectedByState != GameFlow.CheckpointsCollected)
	{
		GameFlow.CheckpointsCollected = CollectedByState;
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

	// --- Victory condition ---
	const bool bAllCheckpointsRecovered =
		(GameFlow.TotalCheckpoints <= 0) || (GameFlow.CheckpointsCollected >= GameFlow.TotalCheckpoints);
	const bool bHasPortalTarget =
		GameFlow.PortalTriggerActor.IsValid() || GameFlow.PortalLightActor.IsValid() || !GameFlow.PortalLocation.IsZero();
	if (bAllCheckpointsRecovered)
	{
		bool bReachedVictory = false;
		if (!bHasPortalTarget)
		{
			// Fallback: if no portal actor was found in this level, complete immediately.
			UE_LOG(LogTemp, Warning, TEXT("ManageGameFlow: No portal target found; completing victory on all checkpoints recovered"));
			bReachedVictory = true;
		}
		else
		{
			// Use stored location (portal light may have been destroyed by checkpoint logic if it happened to match)
			FVector PortalLoc = GameFlow.PortalLocation;
			float TriggerRadius = GameFlow.PortalTriggerRadius;
			if (GameFlow.PortalTriggerActor.IsValid())
			{
				PortalLoc = GameFlow.PortalTriggerActor->GetActorLocation();
				FVector BoundsOrigin = FVector::ZeroVector;
				FVector BoundsExtent = FVector::ZeroVector;
				GameFlow.PortalTriggerActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
				TriggerRadius = FMath::Max(FMath::Max(BoundsExtent.X, BoundsExtent.Y) + 150.0f, 500.0f);
			}
			if (GameFlow.PortalLightActor.IsValid())
			{
				PortalLoc = GameFlow.PortalLightActor->GetActorLocation();
				TriggerRadius = FMath::Max(TriggerRadius, 500.0f);
			}

			const float DistToPortal2D = FVector::Dist2D(PlayerLoc, PortalLoc);
			bReachedVictory = (DistToPortal2D < TriggerRadius);
		}

		if (bReachedVictory)
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
				UGameplayStatics::PlaySound2D(World, VictorySound, kUiSfxVolume, 1.0f);
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

			// Freeze gameplay but keep menu-selection keys active.
			APlayerController* PC = Cast<APlayerController>(Player->GetController());
			if (PC)
			{
				PC->SetIgnoreMoveInput(true);
				PC->SetIgnoreLookInput(true);
				PC->SetInputMode(FInputModeUIOnly());
				PC->bShowMouseCursor = true;
			}
		}
	}
}

void UGameplayHelperLibrary::StartIntroSequence(ACharacter* Character, UAnimSequence* GettingUpAnimation,
	USoundBase* GettingUpSound, FName HeadBoneName, float FadeInDuration, float CameraDriftDuration, float InitialBlackHoldDuration)
{
	if (!Character || !GettingUpAnimation)
	{
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("IntroSequence: Creating component on %s"), *Character->GetName());

	// Immediately go black so player doesn't see the level before intro
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (PC)
	{
		PC->DisableInput(PC);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 1.0f, 0.01f, FLinearColor::Black, false, true);
		}
	}

	UIntroSequenceComponent* IntroComp = NewObject<UIntroSequenceComponent>(Character);
	if (!GettingUpSound)
	{
		GettingUpSound = Cast<USoundBase>(StaticLoadObject(
			USoundBase::StaticClass(), nullptr,
			TEXT("/Game/Audio/SFX/Hero/S_Robot_GettingUp.S_Robot_GettingUp")));
	}
	IntroComp->GettingUpAnimation = GettingUpAnimation;
	IntroComp->GettingUpSound = GettingUpSound;
	IntroComp->HeadBoneName = HeadBoneName;
	IntroComp->FadeInDuration = FadeInDuration;
	IntroComp->CameraDriftDuration = CameraDriftDuration;
	IntroComp->InitialBlackHoldDuration = InitialBlackHoldDuration;
	IntroComp->bEnableTitlePrelude = true;
	IntroComp->TitleFadeInDuration = 1.0f;
	IntroComp->TitleHoldDuration = 6.0f;
	IntroComp->TitleFadeOutDuration = 1.0f;
	IntroComp->RegisterComponent();
	Character->AddInstanceComponent(IntroComp);

	// Defer StartSequence by 0.5s to let camera manager, input, and physics fully initialize
	TWeakObjectPtr<UIntroSequenceComponent> WeakComp = IntroComp;
	FTimerHandle TimerHandle;
	Character->GetWorldTimerManager().SetTimer(TimerHandle, [WeakComp]()
	{
		if (WeakComp.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("IntroSequence: Deferred StartSequence firing"));
			WeakComp->StartSequence();
		}
	}, 0.5f, false);
}

// ==================== MINIMAP ====================

void UGameplayHelperLibrary::ManageMinimap(ACharacter* Player)
{
	if (!Player) return;
	UWorld* World = Player->GetWorld();
	if (!World) return;

	// Reset if world changed (level restart)
	if (MinimapState.bCreated && (!MinimapState.OwnerWorld.IsValid() || MinimapState.OwnerWorld.Get() != World))
	{
		MinimapState = FMinimapState();
	}

	// Create minimap on first call
	if (!MinimapState.bCreated)
	{
		UGameViewportClient* GVC = World->GetGameViewport();
		if (!GVC) return;

		MinimapState.OwnerWorld = World;

		// Auto-detect world bounds from landscape actors
		bool bFoundLandscape = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("Landscape")))
			{
				FBox Bounds = It->GetComponentsBoundingBox();
				if (!bFoundLandscape)
				{
					MinimapState.WorldMin = FVector2D(Bounds.Min.X, Bounds.Min.Y);
					MinimapState.WorldMax = FVector2D(Bounds.Max.X, Bounds.Max.Y);
					bFoundLandscape = true;
				}
				else
				{
					MinimapState.WorldMin.X = FMath::Min(MinimapState.WorldMin.X, Bounds.Min.X);
					MinimapState.WorldMin.Y = FMath::Min(MinimapState.WorldMin.Y, Bounds.Min.Y);
					MinimapState.WorldMax.X = FMath::Max(MinimapState.WorldMax.X, Bounds.Max.X);
					MinimapState.WorldMax.Y = FMath::Max(MinimapState.WorldMax.Y, Bounds.Max.Y);
				}
			}
		}

		if (bFoundLandscape)
		{
			UE_LOG(LogTemp, Log, TEXT("ManageMinimap: Landscape bounds X[%.0f..%.0f] Y[%.0f..%.0f]"),
				MinimapState.WorldMin.X, MinimapState.WorldMax.X,
				MinimapState.WorldMin.Y, MinimapState.WorldMax.Y);
		}

		// Load minimap texture
		UTexture2D* MapTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/T_Minimap.T_Minimap"));
		if (MapTex)
		{
			MinimapState.MapTexture = TStrongObjectPtr<UTexture2D>(MapTex);
			MinimapState.MapBrush.SetResourceObject(MapTex);
			MinimapState.MapBrush.ImageSize = FVector2D(MinimapWidth, MinimapHeight);
			MinimapState.MapBrush.DrawAs = ESlateBrushDrawType::Image;
			MinimapState.MapBrush.Tiling = ESlateBrushTileType::NoTile;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ManageMinimap: Failed to load /Game/UI/Textures/T_Minimap"));
		}

		// Player: bright orange dot with soft glow halo
		MinimapState.PlayerGlowBrush = FSlateRoundedBoxBrush(
			FLinearColor(1.0f, 0.50f, 0.05f, 0.35f), MinimapPlayerGlowSize * 0.5f);
		MinimapState.PlayerDotBrush = FSlateRoundedBoxBrush(
			FLinearColor(1.0f, 0.55f, 0.05f, 1.0f), MinimapPlayerMarkerSize * 0.5f);
		// Checkpoints: bright white with slight blue tint (stands out on golden map)
		MinimapState.CheckpointActiveBrush = FSlateRoundedBoxBrush(
			FLinearColor(1.0f, 1.0f, 0.85f, 0.9f), MinimapCheckpointMarkerSize * 0.5f,
			FLinearColor(0.80f, 0.65f, 0.20f, 1.0f), 2.0f); // gold outline
		MinimapState.CheckpointCollectedBrush = FSlateRoundedBoxBrush(
			FLinearColor(0.25f, 0.20f, 0.10f, 0.3f), MinimapCheckpointMarkerSize * 0.5f);

		// Create custom marker layer (draws markers directly via OnPaint — no RenderTransform)
		TSharedRef<SMinimapMarkerLayer> Markers = SNew(SMinimapMarkerLayer);
		Markers->SetMarkerCount(FMinimapState::TotalMarkers);
		MinimapState.MarkerLayer = Markers;

		// Assemble root widget (anchored top-right)
		TSharedRef<SOverlay> Root = SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(0, 20, 20, 0)
			[
				SNew(SBox)
				.WidthOverride(MinimapWidth)
				.HeightOverride(MinimapHeight)
				[
					SNew(SOverlay)
					// Layer 0: Map background image
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(&MinimapState.MapBrush)
					]
					// Layer 1: Custom paint marker layer (draws circles at pixel positions)
					+ SOverlay::Slot()
					[
						Markers
					]
				]
			];

		MinimapState.RootWidget = Root;
		GVC->AddViewportWidgetContent(TSharedRef<SWidget>(Root));
		MinimapState.bCreated = true;

		UE_LOG(LogTemp, Log, TEXT("ManageMinimap: Created minimap widget (%.0fx%.0f)"),
			MinimapWidth, MinimapHeight);
	}

	// Hide minimap when player is dead
	{
		FProperty* HealthProp = Player->GetClass()->FindPropertyByName(FName("Health"));
		if (HealthProp)
		{
			void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Player);
			float HP = 0.f;
			if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp))
				HP = FP->GetPropertyValue(ValPtr);
			else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp))
				HP = (float)DP->GetPropertyValue(ValPtr);

			if (HP <= 0.f)
			{
				if (MinimapState.RootWidget.IsValid())
					MinimapState.RootWidget->SetVisibility(EVisibility::Collapsed);
				return;
			}
		}
	}

	// Ensure visible
	if (MinimapState.RootWidget.IsValid())
		MinimapState.RootWidget->SetVisibility(EVisibility::Visible);

	if (!MinimapState.MarkerLayer.IsValid()) return;
	SMinimapMarkerLayer& ML = *MinimapState.MarkerLayer;

	// --- Update markers: soul checkpoints + player only ---

	// Checkpoint (soul) markers — slots 0..15
	if (GameFlow.bInitialized)
	{
		int32 CPCount = FMath::Min(GameFlow.Checkpoints.Num(), (int32)FMinimapState::MaxCheckpointMarkers);
		for (int32 i = 0; i < CPCount; i++)
		{
			const FCheckpointData& CP = GameFlow.Checkpoints[i];
			FVector2D Pos = WorldToMinimapPos(CP.Location, MinimapCheckpointMarkerSize);
			const FSlateBrush* Brush = (CP.State == ECheckpointState::Collected)
				? &MinimapState.CheckpointCollectedBrush
				: &MinimapState.CheckpointActiveBrush;
			ML.SetMarker(i, Pos, MinimapCheckpointMarkerSize, Brush, true);
		}
		for (int32 i = CPCount; i < FMinimapState::MaxCheckpointMarkers; i++)
			ML.SetMarker(i, FVector2D::ZeroVector, 0, nullptr, false);
	}
	else
	{
		for (int32 i = 0; i < FMinimapState::MaxCheckpointMarkers; i++)
			ML.SetMarker(i, FVector2D::ZeroVector, 0, nullptr, false);
	}

	// Player glow halo — slot 16 (drawn behind the dot)
	FVector2D PlayerGlowPos = WorldToMinimapPos(Player->GetActorLocation(), MinimapPlayerGlowSize);
	ML.SetMarker(FMinimapState::PlayerGlowSlot, PlayerGlowPos, MinimapPlayerGlowSize, &MinimapState.PlayerGlowBrush, true);

	// Player dot — slot 17 (drawn last = on top)
	FVector2D PlayerDotPos = WorldToMinimapPos(Player->GetActorLocation(), MinimapPlayerMarkerSize);
	ML.SetMarker(FMinimapState::PlayerDotSlot, PlayerDotPos, MinimapPlayerMarkerSize, &MinimapState.PlayerDotBrush, true);

	ML.RequestRepaint();
}
