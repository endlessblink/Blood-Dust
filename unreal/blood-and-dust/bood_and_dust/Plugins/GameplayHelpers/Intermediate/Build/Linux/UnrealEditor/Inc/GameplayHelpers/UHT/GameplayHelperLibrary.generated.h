// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "GameplayHelperLibrary.h"

#ifdef GAMEPLAYHELPERS_GameplayHelperLibrary_generated_h
#error "GameplayHelperLibrary.generated.h already included, missing '#pragma once' in GameplayHelperLibrary.h"
#endif
#define GAMEPLAYHELPERS_GameplayHelperLibrary_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class ACharacter;
class UAnimSequence;

// ********** Begin Class UGameplayHelperLibrary ***************************************************
#define FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h_19_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execPlayAnimationOneShot); \
	DECLARE_FUNCTION(execSetCharacterWalkSpeed);


struct Z_Construct_UClass_UGameplayHelperLibrary_Statics;
GAMEPLAYHELPERS_API UClass* Z_Construct_UClass_UGameplayHelperLibrary_NoRegister();

#define FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h_19_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUGameplayHelperLibrary(); \
	friend struct ::Z_Construct_UClass_UGameplayHelperLibrary_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend GAMEPLAYHELPERS_API UClass* ::Z_Construct_UClass_UGameplayHelperLibrary_NoRegister(); \
public: \
	DECLARE_CLASS2(UGameplayHelperLibrary, UBlueprintFunctionLibrary, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/GameplayHelpers"), Z_Construct_UClass_UGameplayHelperLibrary_NoRegister) \
	DECLARE_SERIALIZER(UGameplayHelperLibrary)


#define FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h_19_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGameplayHelperLibrary(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	UGameplayHelperLibrary(UGameplayHelperLibrary&&) = delete; \
	UGameplayHelperLibrary(const UGameplayHelperLibrary&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGameplayHelperLibrary); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGameplayHelperLibrary); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGameplayHelperLibrary) \
	NO_API virtual ~UGameplayHelperLibrary();


#define FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h_16_PROLOG
#define FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h_19_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h_19_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h_19_INCLASS_NO_PURE_DECLS \
	FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h_19_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UGameplayHelperLibrary;

// ********** End Class UGameplayHelperLibrary *****************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
