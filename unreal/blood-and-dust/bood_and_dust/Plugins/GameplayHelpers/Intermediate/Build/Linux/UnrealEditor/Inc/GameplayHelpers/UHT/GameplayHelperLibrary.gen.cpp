// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "GameplayHelperLibrary.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeGameplayHelperLibrary() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_ACharacter_NoRegister();
ENGINE_API UClass* Z_Construct_UClass_UAnimSequence_NoRegister();
ENGINE_API UClass* Z_Construct_UClass_UBlueprintFunctionLibrary();
GAMEPLAYHELPERS_API UClass* Z_Construct_UClass_UGameplayHelperLibrary();
GAMEPLAYHELPERS_API UClass* Z_Construct_UClass_UGameplayHelperLibrary_NoRegister();
UPackage* Z_Construct_UPackage__Script_GameplayHelpers();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UGameplayHelperLibrary Function PlayAnimationOneShot *********************
struct Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics
{
	struct GameplayHelperLibrary_eventPlayAnimationOneShot_Parms
	{
		ACharacter* Character;
		UAnimSequence* AnimSequence;
		float PlayRate;
		float BlendIn;
		float BlendOut;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Gameplay|Animation" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * Play an AnimSequence as a one-shot dynamic montage on the character.\n\x09 * Blends in/out smoothly and returns to AnimBP state machine when done.\n\x09 * Uses the DefaultSlot so multiple calls interrupt each other (no stacking).\n\x09 */" },
#endif
		{ "CPP_Default_BlendIn", "0.250000" },
		{ "CPP_Default_BlendOut", "0.250000" },
		{ "CPP_Default_PlayRate", "1.000000" },
		{ "DefaultToSelf", "Character" },
		{ "ModuleRelativePath", "Public/GameplayHelperLibrary.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Play an AnimSequence as a one-shot dynamic montage on the character.\nBlends in/out smoothly and returns to AnimBP state machine when done.\nUses the DefaultSlot so multiple calls interrupt each other (no stacking)." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function PlayAnimationOneShot constinit property declarations ******************
	static const UECodeGen_Private::FObjectPropertyParams NewProp_Character;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_AnimSequence;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_PlayRate;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_BlendIn;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_BlendOut;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function PlayAnimationOneShot constinit property declarations ********************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function PlayAnimationOneShot Property Definitions *****************************
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_Character = { "Character", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayHelperLibrary_eventPlayAnimationOneShot_Parms, Character), Z_Construct_UClass_ACharacter_NoRegister, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_AnimSequence = { "AnimSequence", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayHelperLibrary_eventPlayAnimationOneShot_Parms, AnimSequence), Z_Construct_UClass_UAnimSequence_NoRegister, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_PlayRate = { "PlayRate", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayHelperLibrary_eventPlayAnimationOneShot_Parms, PlayRate), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_BlendIn = { "BlendIn", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayHelperLibrary_eventPlayAnimationOneShot_Parms, BlendIn), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_BlendOut = { "BlendOut", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayHelperLibrary_eventPlayAnimationOneShot_Parms, BlendOut), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_Character,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_AnimSequence,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_PlayRate,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_BlendIn,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::NewProp_BlendOut,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::PropPointers) < 2048);
// ********** End Function PlayAnimationOneShot Property Definitions *******************************
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_UGameplayHelperLibrary, nullptr, "PlayAnimationOneShot", 	Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::PropPointers, 
	UE_ARRAY_COUNT(Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::PropPointers), 
sizeof(Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::GameplayHelperLibrary_eventPlayAnimationOneShot_Parms),
RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::GameplayHelperLibrary_eventPlayAnimationOneShot_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameplayHelperLibrary::execPlayAnimationOneShot)
{
	P_GET_OBJECT(ACharacter,Z_Param_Character);
	P_GET_OBJECT(UAnimSequence,Z_Param_AnimSequence);
	P_GET_PROPERTY(FFloatProperty,Z_Param_PlayRate);
	P_GET_PROPERTY(FFloatProperty,Z_Param_BlendIn);
	P_GET_PROPERTY(FFloatProperty,Z_Param_BlendOut);
	P_FINISH;
	P_NATIVE_BEGIN;
	UGameplayHelperLibrary::PlayAnimationOneShot(Z_Param_Character,Z_Param_AnimSequence,Z_Param_PlayRate,Z_Param_BlendIn,Z_Param_BlendOut);
	P_NATIVE_END;
}
// ********** End Class UGameplayHelperLibrary Function PlayAnimationOneShot ***********************

// ********** Begin Class UGameplayHelperLibrary Function SetCharacterWalkSpeed ********************
struct Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics
{
	struct GameplayHelperLibrary_eventSetCharacterWalkSpeed_Parms
	{
		ACharacter* Character;
		float NewSpeed;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Gameplay|Character" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * Set the character's max walk speed at runtime.\n\x09 * Wraps CharacterMovementComponent->MaxWalkSpeed = NewSpeed.\n\x09 */" },
#endif
		{ "DefaultToSelf", "Character" },
		{ "ModuleRelativePath", "Public/GameplayHelperLibrary.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Set the character's max walk speed at runtime.\nWraps CharacterMovementComponent->MaxWalkSpeed = NewSpeed." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function SetCharacterWalkSpeed constinit property declarations *****************
	static const UECodeGen_Private::FObjectPropertyParams NewProp_Character;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_NewSpeed;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function SetCharacterWalkSpeed constinit property declarations *******************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function SetCharacterWalkSpeed Property Definitions ****************************
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::NewProp_Character = { "Character", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayHelperLibrary_eventSetCharacterWalkSpeed_Parms, Character), Z_Construct_UClass_ACharacter_NoRegister, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::NewProp_NewSpeed = { "NewSpeed", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayHelperLibrary_eventSetCharacterWalkSpeed_Parms, NewSpeed), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::NewProp_Character,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::NewProp_NewSpeed,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::PropPointers) < 2048);
// ********** End Function SetCharacterWalkSpeed Property Definitions ******************************
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_UGameplayHelperLibrary, nullptr, "SetCharacterWalkSpeed", 	Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::PropPointers, 
	UE_ARRAY_COUNT(Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::PropPointers), 
sizeof(Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::GameplayHelperLibrary_eventSetCharacterWalkSpeed_Parms),
RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::Function_MetaDataParams), Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::GameplayHelperLibrary_eventSetCharacterWalkSpeed_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UGameplayHelperLibrary::execSetCharacterWalkSpeed)
{
	P_GET_OBJECT(ACharacter,Z_Param_Character);
	P_GET_PROPERTY(FFloatProperty,Z_Param_NewSpeed);
	P_FINISH;
	P_NATIVE_BEGIN;
	UGameplayHelperLibrary::SetCharacterWalkSpeed(Z_Param_Character,Z_Param_NewSpeed);
	P_NATIVE_END;
}
// ********** End Class UGameplayHelperLibrary Function SetCharacterWalkSpeed **********************

// ********** Begin Class UGameplayHelperLibrary ***************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UGameplayHelperLibrary;
UClass* UGameplayHelperLibrary::GetPrivateStaticClass()
{
	using TClass = UGameplayHelperLibrary;
	if (!Z_Registration_Info_UClass_UGameplayHelperLibrary.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("GameplayHelperLibrary"),
			Z_Registration_Info_UClass_UGameplayHelperLibrary.InnerSingleton,
			StaticRegisterNativesUGameplayHelperLibrary,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_UGameplayHelperLibrary.InnerSingleton;
}
UClass* Z_Construct_UClass_UGameplayHelperLibrary_NoRegister()
{
	return UGameplayHelperLibrary::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UGameplayHelperLibrary_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * Static helper functions for common gameplay operations.\n * These are RUNTIME functions (not editor-only) so they work in packaged builds.\n */" },
#endif
		{ "IncludePath", "GameplayHelperLibrary.h" },
		{ "ModuleRelativePath", "Public/GameplayHelperLibrary.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Static helper functions for common gameplay operations.\nThese are RUNTIME functions (not editor-only) so they work in packaged builds." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGameplayHelperLibrary constinit property declarations *******************
// ********** End Class UGameplayHelperLibrary constinit property declarations *********************
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("PlayAnimationOneShot"), .Pointer = &UGameplayHelperLibrary::execPlayAnimationOneShot },
		{ .NameUTF8 = UTF8TEXT("SetCharacterWalkSpeed"), .Pointer = &UGameplayHelperLibrary::execSetCharacterWalkSpeed },
	};
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UGameplayHelperLibrary_PlayAnimationOneShot, "PlayAnimationOneShot" }, // 2093966774
		{ &Z_Construct_UFunction_UGameplayHelperLibrary_SetCharacterWalkSpeed, "SetCharacterWalkSpeed" }, // 472177278
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGameplayHelperLibrary>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UGameplayHelperLibrary_Statics
UObject* (*const Z_Construct_UClass_UGameplayHelperLibrary_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UBlueprintFunctionLibrary,
	(UObject* (*)())Z_Construct_UPackage__Script_GameplayHelpers,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UGameplayHelperLibrary_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UGameplayHelperLibrary_Statics::ClassParams = {
	&UGameplayHelperLibrary::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	0,
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UGameplayHelperLibrary_Statics::Class_MetaDataParams), Z_Construct_UClass_UGameplayHelperLibrary_Statics::Class_MetaDataParams)
};
void UGameplayHelperLibrary::StaticRegisterNativesUGameplayHelperLibrary()
{
	UClass* Class = UGameplayHelperLibrary::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, MakeConstArrayView(Z_Construct_UClass_UGameplayHelperLibrary_Statics::Funcs));
}
UClass* Z_Construct_UClass_UGameplayHelperLibrary()
{
	if (!Z_Registration_Info_UClass_UGameplayHelperLibrary.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGameplayHelperLibrary.OuterSingleton, Z_Construct_UClass_UGameplayHelperLibrary_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UGameplayHelperLibrary.OuterSingleton;
}
UGameplayHelperLibrary::UGameplayHelperLibrary(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGameplayHelperLibrary);
UGameplayHelperLibrary::~UGameplayHelperLibrary() {}
// ********** End Class UGameplayHelperLibrary *****************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h__Script_GameplayHelpers_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGameplayHelperLibrary, UGameplayHelperLibrary::StaticClass, TEXT("UGameplayHelperLibrary"), &Z_Registration_Info_UClass_UGameplayHelperLibrary, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGameplayHelperLibrary), 4166263262U) },
	};
}; // Z_CompiledInDeferFile_FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h__Script_GameplayHelpers_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h__Script_GameplayHelpers_250233691{
	TEXT("/Script/GameplayHelpers"),
	Z_CompiledInDeferFile_FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h__Script_GameplayHelpers_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_blood_and_dust_bood_and_dust_Plugins_GameplayHelpers_Source_GameplayHelpers_Public_GameplayHelperLibrary_h__Script_GameplayHelpers_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
