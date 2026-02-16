#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace1D.h"

// BUILD_ID: bump this every time you change plugin code and rebuild.
// Search for this exact string in the editor log to confirm the new binary is loaded.
#define GAMEPLAY_HELPERS_BUILD_ID TEXT("GameplayHelpers BUILD_ID=2026-02-16-v21")

class FGameplayHelpersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogTemp, Warning, TEXT("=== %s LOADED ==="), GAMEPLAY_HELPERS_BUILD_ID);

		// One-shot skeleton MERGE fix for Bell enemy.
		//
		// Root cause: Bell's mesh (SK_Bell_New, 33 bones) uses SK_Bell_New_Skeleton,
		// but all Bell animations use SK_Bell_Skeleton (24 bones).
		// Two separate USkeleton objects from different imports.
		//
		// Fix approach:
		// 1. MergeAllBonesToBoneTree: add the mesh's 33 bones INTO SK_Bell_Skeleton
		//    (keeps all 24 existing animation bones + adds 9 extra mesh bones)
		// 2. SetSkeleton: retarget SK_Bell_New mesh to use SK_Bell_Skeleton
		// 3. Retarget AnimBP + BlendSpace to SK_Bell_Skeleton
		// After Ctrl+S, SK_Bell_New_Skeleton has zero references and can be deleted.
		//
		// Deferred 5s to ensure assets are loaded in editor context.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) -> bool
		{
			// Load mesh
			USkeletalMesh* BellMesh = LoadObject<USkeletalMesh>(nullptr,
				TEXT("/Game/Characters/Enemies/Bell/SK_Bell_New.SK_Bell_New"));
			// Load target skeleton (the one all animations use)
			USkeleton* AnimSkel = LoadObject<USkeleton>(nullptr,
				TEXT("/Game/Characters/Enemies/Bell/SK_Bell_Skeleton.SK_Bell_Skeleton"));
			// Load old skeleton (the one we want to eliminate)
			USkeleton* OldSkel = LoadObject<USkeleton>(nullptr,
				TEXT("/Game/Characters/Enemies/Bell/SK_Bell_New_Skeleton.SK_Bell_New_Skeleton"));

			if (!BellMesh || !AnimSkel)
			{
				UE_LOG(LogTemp, Error, TEXT("BELL SKEL MERGE: Failed to load mesh or AnimSkel — skipping"));
				return false;
			}

			USkeleton* CurrentMeshSkel = BellMesh->GetSkeleton();
			UE_LOG(LogTemp, Warning, TEXT("BELL SKEL MERGE: Mesh skeleton='%s' (%d bones), Target AnimSkel='%s' (%d bones)"),
				CurrentMeshSkel ? *CurrentMeshSkel->GetName() : TEXT("NULL"),
				CurrentMeshSkel ? CurrentMeshSkel->GetReferenceSkeleton().GetNum() : 0,
				*AnimSkel->GetName(), AnimSkel->GetReferenceSkeleton().GetNum());

			if (CurrentMeshSkel == AnimSkel)
			{
				UE_LOG(LogTemp, Log, TEXT("BELL SKEL MERGE: Mesh already uses AnimSkel — no merge needed"));
			}
			else
			{
				// Step 1: Merge mesh bones into AnimSkel
				// Adds any bones from BellMesh that don't already exist in AnimSkel.
				// Existing animation bones are preserved; new bones are appended.
				//
				// The bone hierarchies DIFFER (mesh has Chest→Spine, skeleton has
				// Chest→Spine02→Spine01→Spine). MergeAllBonesToBoneTree calls
				// IsCompatibleMesh which does a parent-chain check that fails.
				// Temporarily set the CVar to allow incompatible hierarchies.
				IConsoleVariable* MergeCVar = IConsoleManager::Get().FindConsoleVariable(
					TEXT("a.Skeleton.AllowIncompatibleSkeletalMeshMerge"));
				bool bOldCVarValue = MergeCVar ? MergeCVar->GetBool() : false;
				if (MergeCVar)
				{
					MergeCVar->Set(true);
					UE_LOG(LogTemp, Warning, TEXT("BELL SKEL MERGE: Set AllowIncompatibleSkeletalMeshMerge=true"));
				}

				int32 PreMergeBones = AnimSkel->GetReferenceSkeleton().GetNum();
				bool bMerged = AnimSkel->MergeAllBonesToBoneTree(BellMesh, /*bShowProgress=*/ false);
				int32 PostMergeBones = AnimSkel->GetReferenceSkeleton().GetNum();

				// Restore CVar
				if (MergeCVar)
				{
					MergeCVar->Set(bOldCVarValue);
				}

				UE_LOG(LogTemp, Warning, TEXT("BELL SKEL MERGE: MergeAllBonesToBoneTree result=%s, bones %d -> %d (+%d)"),
					bMerged ? TEXT("SUCCESS") : TEXT("FAILED"),
					PreMergeBones, PostMergeBones, PostMergeBones - PreMergeBones);

				if (!bMerged)
				{
					UE_LOG(LogTemp, Error, TEXT("BELL SKEL MERGE: Merge failed — aborting"));
					return false;
				}

				// Step 2: Retarget mesh to AnimSkel
				BellMesh->SetSkeleton(AnimSkel);
				BellMesh->MarkPackageDirty();
				AnimSkel->MarkPackageDirty();

				UE_LOG(LogTemp, Warning, TEXT("BELL SKEL MERGE: Mesh skeleton changed from '%s' to '%s'"),
					CurrentMeshSkel ? *CurrentMeshSkel->GetName() : TEXT("NULL"),
					*AnimSkel->GetName());
			}

			// Step 3: Fix AnimBP target skeleton
			UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr,
				TEXT("/Game/Characters/Enemies/Bell/ABP_BG_Bell.ABP_BG_Bell"));
			if (AnimBP)
			{
				USkeleton* ABPSkel = AnimBP->TargetSkeleton.Get();
				if (ABPSkel != AnimSkel)
				{
					UE_LOG(LogTemp, Warning, TEXT("BELL SKEL MERGE: AnimBP TargetSkeleton was '%s', changing to '%s'"),
						ABPSkel ? *ABPSkel->GetName() : TEXT("NULL"),
						*AnimSkel->GetName());
					AnimBP->TargetSkeleton = AnimSkel;
					AnimBP->MarkPackageDirty();
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("BELL SKEL MERGE: AnimBP already targets AnimSkel"));
				}
			}

			// Step 4: Fix BlendSpace skeleton
			UBlendSpace1D* BS = LoadObject<UBlendSpace1D>(nullptr,
				TEXT("/Game/Characters/Enemies/Bell/BS_BG_Bell_Locomotion.BS_BG_Bell_Locomotion"));
			if (BS)
			{
				USkeleton* BSSkel = BS->GetSkeleton();
				if (BSSkel != AnimSkel)
				{
					UE_LOG(LogTemp, Warning, TEXT("BELL SKEL MERGE: BlendSpace skeleton was '%s', changing to '%s'"),
						BSSkel ? *BSSkel->GetName() : TEXT("NULL"),
						*AnimSkel->GetName());
					BS->SetSkeleton(AnimSkel);
					BS->MarkPackageDirty();
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("BELL SKEL MERGE: BlendSpace already uses AnimSkel"));
				}
			}

			// Diagnostic: log final state
			USkeleton* FinalMeshSkel = BellMesh->GetSkeleton();
			UE_LOG(LogTemp, Warning, TEXT("BELL SKEL MERGE: === FINAL STATE ==="));
			UE_LOG(LogTemp, Warning, TEXT("  Mesh skeleton: '%s' (%d bones)"),
				FinalMeshSkel ? *FinalMeshSkel->GetName() : TEXT("NULL"),
				FinalMeshSkel ? FinalMeshSkel->GetReferenceSkeleton().GetNum() : 0);
			UE_LOG(LogTemp, Warning, TEXT("  AnimSkel: '%s' (%d bones)"),
				*AnimSkel->GetName(), AnimSkel->GetReferenceSkeleton().GetNum());
			if (AnimBP)
			{
				USkeleton* FinalABPSkel = AnimBP->TargetSkeleton.Get();
				UE_LOG(LogTemp, Warning, TEXT("  AnimBP TargetSkeleton: '%s'"),
					FinalABPSkel ? *FinalABPSkel->GetName() : TEXT("NULL"));
			}
			if (BS)
			{
				USkeleton* FinalBSSkel = BS->GetSkeleton();
				UE_LOG(LogTemp, Warning, TEXT("  BlendSpace skeleton: '%s'"),
					FinalBSSkel ? *FinalBSSkel->GetName() : TEXT("NULL"));
			}

			UE_LOG(LogTemp, Warning, TEXT("=== BELL SKEL MERGE: COMPLETE — Ctrl+S to persist, then delete SK_Bell_New_Skeleton ==="));
			return false; // One-shot
		}), 5.0f);
	}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGameplayHelpersModule, GameplayHelpers)
