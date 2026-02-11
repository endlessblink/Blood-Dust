#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Engine.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
// Character Blueprint includes
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimBlueprint.h"
#include "UObject/SavePackage.h"
// Animation Blueprint includes
#include "Factories/AnimBlueprintFactory.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstance.h"
// AnimGraph State Machine includes
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateEntryNode.h"
#include "AnimationStateGraph.h"
#include "AnimationTransitionGraph.h"
#include "Animation/AnimSequence.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "Kismet/KismetMathLibrary.h"

FEpicUnrealMCPBlueprintCommands::FEpicUnrealMCPBlueprintCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))
    {
        return HandleCreateBlueprint(Params);
    }
    else if (CommandType == TEXT("add_component_to_blueprint"))
    {
        return HandleAddComponentToBlueprint(Params);
    }
    else if (CommandType == TEXT("set_physics_properties"))
    {
        return HandleSetPhysicsProperties(Params);
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_properties"))
    {
        return HandleSetStaticMeshProperties(Params);
    }
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    else if (CommandType == TEXT("set_mesh_material_color"))
    {
        return HandleSetMeshMaterialColor(Params);
    }
    // Material management commands
    else if (CommandType == TEXT("get_available_materials"))
    {
        return HandleGetAvailableMaterials(Params);
    }
    else if (CommandType == TEXT("apply_material_to_actor"))
    {
        return HandleApplyMaterialToActor(Params);
    }
    else if (CommandType == TEXT("apply_material_to_blueprint"))
    {
        return HandleApplyMaterialToBlueprint(Params);
    }
    else if (CommandType == TEXT("get_actor_material_info"))
    {
        return HandleGetActorMaterialInfo(Params);
    }
    else if (CommandType == TEXT("set_mesh_asset_material"))
    {
        return HandleSetMeshAssetMaterial(Params);
    }
    else if (CommandType == TEXT("get_blueprint_material_info"))
    {
        return HandleGetBlueprintMaterialInfo(Params);
    }
    // Blueprint analysis commands
    else if (CommandType == TEXT("read_blueprint_content"))
    {
        return HandleReadBlueprintContent(Params);
    }
    else if (CommandType == TEXT("analyze_blueprint_graph"))
    {
        return HandleAnalyzeBlueprintGraph(Params);
    }
    else if (CommandType == TEXT("get_blueprint_variable_details"))
    {
        return HandleGetBlueprintVariableDetails(Params);
    }
    else if (CommandType == TEXT("get_blueprint_function_details"))
    {
        return HandleGetBlueprintFunctionDetails(Params);
    }
    else if (CommandType == TEXT("create_character_blueprint"))
    {
        return HandleCreateCharacterBlueprint(Params);
    }
    else if (CommandType == TEXT("create_anim_blueprint"))
    {
        return HandleCreateAnimBlueprint(Params);
    }
    else if (CommandType == TEXT("setup_locomotion_state_machine"))
    {
        return HandleSetupLocomotionStateMachine(Params);
    }
    else if (CommandType == TEXT("set_character_properties"))
    {
        return HandleSetCharacterProperties(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Check if blueprint already exists
    FString PackagePath = TEXT("/Game/Blueprints/");
    FString AssetName = BlueprintName;
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
    }

    // Create the blueprint factory
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    
    // Handle parent class
    FString ParentClass;
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);
    
    // Default to Actor if no parent class specified
    UClass* SelectedParentClass = AActor::StaticClass();
    
    // Try to find the specified parent class
    if (!ParentClass.IsEmpty())
    {
        FString ClassName = ParentClass;
        if (!ClassName.StartsWith(TEXT("A")))
        {
            ClassName = TEXT("A") + ClassName;
        }
        
        // First try direct StaticClass lookup for common classes
        UClass* FoundClass = nullptr;
        if (ClassName == TEXT("APawn"))
        {
            FoundClass = APawn::StaticClass();
        }
        else if (ClassName == TEXT("AActor"))
        {
            FoundClass = AActor::StaticClass();
        }
        else
        {
            // Try loading the class using LoadClass which is more reliable than FindObject
            const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
            FoundClass = LoadClass<AActor>(nullptr, *ClassPath);
            
            if (!FoundClass)
            {
                // Try alternate paths if not found
                const FString GameClassPath = FString::Printf(TEXT("/Script/Game.%s"), *ClassName);
                FoundClass = LoadClass<AActor>(nullptr, *GameClassPath);
            }
        }

        if (FoundClass)
        {
            SelectedParentClass = FoundClass;
            UE_LOG(LogTemp, Log, TEXT("Successfully set parent class to '%s'"), *ClassName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find specified parent class '%s' at paths: /Script/Engine.%s or /Script/Game.%s, defaulting to AActor"), 
                *ClassName, *ClassName, *ClassName);
        }
    }
    
    Factory->ParentClass = SelectedParentClass;

    // Create the blueprint
    UPackage* Package = CreatePackage(*(PackagePath + AssetName));
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (NewBlueprint)
    {
        // Notify the asset registry
        FAssetRegistryModule::AssetCreated(NewBlueprint);

        // Mark the package dirty
        Package->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), AssetName);
        ResultObj->SetStringField(TEXT("path"), PackagePath + AssetName);
        return ResultObj;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentType;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Create the component - dynamically find the component class by name
    UClass* ComponentClass = nullptr;

    // Try to find the class with exact name first
    ComponentClass = FindObject<UClass>(nullptr, *ComponentType);
    
    // If not found, try with "Component" suffix
    if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
    {
        FString ComponentTypeWithSuffix = ComponentType + TEXT("Component");
        ComponentClass = FindObject<UClass>(nullptr, *ComponentTypeWithSuffix);
    }
    
    // If still not found, try with "U" prefix
    if (!ComponentClass && !ComponentType.StartsWith(TEXT("U")))
    {
        FString ComponentTypeWithPrefix = TEXT("U") + ComponentType;
        ComponentClass = FindObject<UClass>(nullptr, *ComponentTypeWithPrefix);
        
        // Try with both prefix and suffix
        if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
        {
            FString ComponentTypeWithBoth = TEXT("U") + ComponentType + TEXT("Component");
            ComponentClass = FindObject<UClass>(nullptr, *ComponentTypeWithBoth);
        }
    }
    
    // Verify that the class is a valid component type
    if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
    }

    // Add the component to the blueprint
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
    if (NewNode)
    {
        // Set transform if provided
        USceneComponent* SceneComponent = Cast<USceneComponent>(NewNode->ComponentTemplate);
        if (SceneComponent)
        {
            if (Params->HasField(TEXT("location")))
            {
                SceneComponent->SetRelativeLocation(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
            }
            if (Params->HasField(TEXT("rotation")))
            {
                SceneComponent->SetRelativeRotation(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
            }
            if (Params->HasField(TEXT("scale")))
            {
                SceneComponent->SetRelativeScale3D(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
            }
        }

        // Add to root if no parent specified
        Blueprint->SimpleConstructionScript->AddNode(NewNode);

        // Compile the blueprint
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("component_name"), ComponentName);
        ResultObj->SetStringField(TEXT("component_type"), ComponentType);
        return ResultObj;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    // Set physics properties
    if (Params->HasField(TEXT("simulate_physics")))
    {
        PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
    }

    if (Params->HasField(TEXT("mass")))
    {
        float Mass = Params->GetNumberField(TEXT("mass"));
        // In UE5.5, use proper overrideMass instead of just scaling
        PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
        UE_LOG(LogTemp, Display, TEXT("Set mass for component %s to %f kg"), *ComponentName, Mass);
    }

    if (Params->HasField(TEXT("linear_damping")))
    {
        PrimComponent->SetLinearDamping(Params->GetNumberField(TEXT("linear_damping")));
    }

    if (Params->HasField(TEXT("angular_damping")))
    {
        PrimComponent->SetAngularDamping(Params->GetNumberField(TEXT("angular_damping")));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Compile the blueprint
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), BlueprintName);
    ResultObj->SetBoolField(TEXT("compiled"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Starting blueprint actor spawn"));
    
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Missing blueprint_name parameter"));
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Missing actor_name parameter"));
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Looking for blueprint '%s'"), *BlueprintName);

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Blueprint not found: %s"), *BlueprintName);
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Blueprint found, getting transform parameters"));

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Location set to (%f, %f, %f)"), Location.X, Location.Y, Location.Z);
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
        UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Rotation set to (%f, %f, %f)"), Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
    }

    UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Getting editor world"));

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Failed to get editor world"));
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Creating spawn transform"));

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));

    // Add a small delay to allow the engine to process the newly compiled class
    FPlatformProcess::Sleep(0.2f);

    UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: About to spawn actor from blueprint '%s' with GeneratedClass: %s"), 
           *BlueprintName, Blueprint->GeneratedClass ? *Blueprint->GeneratedClass->GetName() : TEXT("NULL"));

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);
    
    UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: SpawnActor completed, NewActor: %s"), 
           NewActor ? *NewActor->GetName() : TEXT("NULL"));
    
    if (NewActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Setting actor label to '%s'"), *ActorName);
        NewActor->SetActorLabel(*ActorName);
        
        UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: About to convert actor to JSON"));
        TSharedPtr<FJsonObject> Result = FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
        
        UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: JSON conversion completed, returning result"));
        return Result;
    }

    UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Failed to spawn blueprint actor"));
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    // Set static mesh properties
    if (Params->HasField(TEXT("static_mesh")))
    {
        FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
        UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
        if (Mesh)
        {
            MeshComponent->SetStaticMesh(Mesh);
        }
    }

    if (Params->HasField(TEXT("material")))
    {
        FString MaterialPath = Params->GetStringField(TEXT("material"));
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            MeshComponent->SetMaterial(0, Material);
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetMeshMaterialColor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    // Try to cast to StaticMeshComponent or PrimitiveComponent
    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    // Get color parameter
    TArray<float> ColorArray;
    const TArray<TSharedPtr<FJsonValue>>* ColorJsonArray;
    if (!Params->TryGetArrayField(TEXT("color"), ColorJsonArray) || ColorJsonArray->Num() != 4)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'color' must be an array of 4 float values [R, G, B, A]"));
    }

    for (const TSharedPtr<FJsonValue>& Value : *ColorJsonArray)
    {
        ColorArray.Add(FMath::Clamp(Value->AsNumber(), 0.0f, 1.0f));
    }

    FLinearColor Color(ColorArray[0], ColorArray[1], ColorArray[2], ColorArray[3]);

    // Get material slot index
    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    // Get parameter name
    FString ParameterName = TEXT("BaseColor");
    Params->TryGetStringField(TEXT("parameter_name"), ParameterName);

    // Get or create material
    UMaterialInterface* Material = nullptr;
    
    // Check if a specific material path was provided
    FString MaterialPath;
    if (Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (!Material)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
        }
    }
    else
    {
        // Use existing material on the component
        Material = PrimComponent->GetMaterial(MaterialSlot);
        if (!Material)
        {
            // Try to use a default material
            Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
            if (!Material)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No material found on component and failed to load default material"));
            }
        }
    }

    // Create a dynamic material instance
    UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(Material, PrimComponent);
    if (!DynMaterial)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance"));
    }

    // Set the color parameter
    DynMaterial->SetVectorParameterValue(*ParameterName, Color);

    // Apply the material to the component
    PrimComponent->SetMaterial(MaterialSlot, DynMaterial);

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    // Log success
    UE_LOG(LogTemp, Log, TEXT("Successfully set material color on component %s: R=%f, G=%f, B=%f, A=%f"), 
        *ComponentName, Color.R, Color.G, Color.B, Color.A);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    
    TArray<TSharedPtr<FJsonValue>> ColorResultArray;
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.R));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.G));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.B));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.A));
    ResultObj->SetArrayField(TEXT("color"), ColorResultArray);
    
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetAvailableMaterials(const TSharedPtr<FJsonObject>& Params)
{
    // Get parameters - make search path completely dynamic
    FString SearchPath;
    if (!Params->TryGetStringField(TEXT("search_path"), SearchPath))
    {
        // Default to empty string to search everywhere
        SearchPath = TEXT("");
    }
    
    bool bIncludeEngineMaterials = true;
    if (Params->HasField(TEXT("include_engine_materials")))
    {
        bIncludeEngineMaterials = Params->GetBoolField(TEXT("include_engine_materials"));
    }

    // Get asset registry module
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // Create filter for materials
    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterialInstanceDynamic::StaticClass()->GetClassPathName());
    
    // Add search paths dynamically
    if (!SearchPath.IsEmpty())
    {
        // Ensure the path starts with /
        if (!SearchPath.StartsWith(TEXT("/")))
        {
            SearchPath = TEXT("/") + SearchPath;
        }
        // Ensure the path ends with / for proper directory search
        if (!SearchPath.EndsWith(TEXT("/")))
        {
            SearchPath += TEXT("/");
        }
        Filter.PackagePaths.Add(*SearchPath);
        UE_LOG(LogTemp, Log, TEXT("Searching for materials in: %s"), *SearchPath);
    }
    else
    {
        // Search in common game content locations
        Filter.PackagePaths.Add(TEXT("/Game/"));
        UE_LOG(LogTemp, Log, TEXT("Searching for materials in all game content"));
    }
    
    if (bIncludeEngineMaterials)
    {
        Filter.PackagePaths.Add(TEXT("/Engine/"));
        UE_LOG(LogTemp, Log, TEXT("Including Engine materials in search"));
    }
    
    Filter.bRecursivePaths = true;

    // Get assets from registry
    TArray<FAssetData> AssetDataArray;
    AssetRegistry.GetAssets(Filter, AssetDataArray);
    
    UE_LOG(LogTemp, Log, TEXT("Asset registry found %d materials"), AssetDataArray.Num());

    // Also try manual search using EditorAssetLibrary for more comprehensive results
    TArray<FString> AllAssetPaths;
    if (!SearchPath.IsEmpty())
    {
        AllAssetPaths = UEditorAssetLibrary::ListAssets(SearchPath, true, false);
    }
    else
    {
        AllAssetPaths = UEditorAssetLibrary::ListAssets(TEXT("/Game/"), true, false);
    }
    
    // Filter for materials from the manual search
    for (const FString& AssetPath : AllAssetPaths)
    {
        if (AssetPath.Contains(TEXT("Material")) && !AssetPath.Contains(TEXT(".uasset")))
        {
            UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
            if (Asset && Asset->IsA<UMaterialInterface>())
            {
                // Check if we already have this asset from registry search
                bool bAlreadyFound = false;
                for (const FAssetData& ExistingData : AssetDataArray)
                {
                    if (ExistingData.GetObjectPathString() == AssetPath)
                    {
                        bAlreadyFound = true;
                        break;
                    }
                }
                
                if (!bAlreadyFound)
                {
                    // Create FAssetData manually for this asset
                    FAssetData ManualAssetData(Asset);
                    AssetDataArray.Add(ManualAssetData);
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Total materials found after manual search: %d"), AssetDataArray.Num());

    // Convert to JSON
    TArray<TSharedPtr<FJsonValue>> MaterialArray;
    for (const FAssetData& AssetData : AssetDataArray)
    {
        TSharedPtr<FJsonObject> MaterialObj = MakeShared<FJsonObject>();
        MaterialObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        MaterialObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        MaterialObj->SetStringField(TEXT("package"), AssetData.PackageName.ToString());
        MaterialObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
        
        MaterialArray.Add(MakeShared<FJsonValueObject>(MaterialObj));
        
        UE_LOG(LogTemp, Verbose, TEXT("Found material: %s at %s"), *AssetData.AssetName.ToString(), *AssetData.GetObjectPathString());
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("materials"), MaterialArray);
    ResultObj->SetNumberField(TEXT("count"), MaterialArray.Num());
    ResultObj->SetStringField(TEXT("search_path_used"), SearchPath.IsEmpty() ? TEXT("/Game/") : SearchPath);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    // Find the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* TargetActor = FEpicUnrealMCPCommonUtils::FindActorByName(World, ActorName);
    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Load the material
    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }

    // Mark actor for undo and OFPA package dirtying BEFORE mutation
    TargetActor->Modify();

    bool bAppliedToAny = false;
    TSharedPtr<FJsonObject> AppliedMeshes = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> MeshArray;

    // Apply to ALL SkeletalMeshComponents (priority for characters)
    TArray<USkeletalMeshComponent*> SkelMeshComponents;
    TargetActor->GetComponents<USkeletalMeshComponent>(SkelMeshComponents);

    for (USkeletalMeshComponent* SkelComp : SkelMeshComponents)
    {
        if (SkelComp)
        {
            SkelComp->Modify();
            FString CompMeshName;
            if (USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMeshAsset())
            {
                CompMeshName = SkelMesh->GetPathName();
            }
            if (MaterialSlot < 0)
            {
                int32 NumMaterials = SkelComp->GetNumMaterials();
                for (int32 i = 0; i < NumMaterials; i++)
                {
                    SkelComp->SetMaterial(i, Material);
                }
            }
            else
            {
                SkelComp->SetMaterial(MaterialSlot, Material);
            }
            SkelComp->MarkRenderStateDirty();
            bAppliedToAny = true;

            TSharedPtr<FJsonObject> MeshInfo = MakeShared<FJsonObject>();
            MeshInfo->SetStringField(TEXT("mesh"), CompMeshName);
            MeshInfo->SetStringField(TEXT("type"), TEXT("SkeletalMesh"));
            MeshArray.Add(MakeShared<FJsonValueObject>(MeshInfo));
        }
    }

    // Also apply to StaticMeshComponents
    TArray<UStaticMeshComponent*> StaticMeshComponents;
    TargetActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);

    for (UStaticMeshComponent* MeshComp : StaticMeshComponents)
    {
        if (MeshComp)
        {
            MeshComp->Modify();
            FString CompMeshName;
            if (UStaticMesh* Mesh = MeshComp->GetStaticMesh())
            {
                CompMeshName = Mesh->GetPathName();
            }
            if (MaterialSlot < 0)
            {
                int32 NumMaterials = MeshComp->GetNumMaterials();
                for (int32 i = 0; i < NumMaterials; i++)
                {
                    MeshComp->SetMaterial(i, Material);
                }
            }
            else
            {
                MeshComp->SetMaterial(MaterialSlot, Material);
            }
            MeshComp->MarkRenderStateDirty();
            bAppliedToAny = true;

            TSharedPtr<FJsonObject> MeshInfo = MakeShared<FJsonObject>();
            MeshInfo->SetStringField(TEXT("mesh"), CompMeshName);
            MeshInfo->SetStringField(TEXT("type"), TEXT("StaticMesh"));
            MeshArray.Add(MakeShared<FJsonValueObject>(MeshInfo));
        }
    }

    if (!bAppliedToAny)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No mesh components found on actor (checked StaticMesh and SkeletalMesh)"));
    }

    // Mark the actor's package dirty so OFPA serializes the material override
    TargetActor->MarkPackageDirty();

    // Also dirty the OFPA external package for proper persistence
    if (UPackage* ExternalPackage = TargetActor->GetExternalPackage())
    {
        ExternalPackage->SetDirtyFlag(true);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetArrayField(TEXT("applied_to"), MeshArray);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetMeshAssetMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MeshPath;
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    // Load the material
    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }

    // Try loading as StaticMesh first, then SkeletalMesh
    UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(MeshPath);
    if (!LoadedAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *MeshPath));
    }

    int32 TotalSlots = 0;
    FString MeshType;

    if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(LoadedAsset))
    {
        MeshType = TEXT("StaticMesh");
        TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
        TotalSlots = StaticMaterials.Num();

        if (MaterialSlot < 0)
        {
            StaticMesh->Modify();
            for (int32 i = 0; i < StaticMaterials.Num(); i++)
            {
                StaticMaterials[i].MaterialInterface = Material;
            }
        }
        else
        {
            if (MaterialSlot >= StaticMaterials.Num())
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material slot %d out of range (mesh has %d slots)"), MaterialSlot, StaticMaterials.Num()));
            }
            StaticMesh->Modify();
            StaticMaterials[MaterialSlot].MaterialInterface = Material;
        }

        StaticMesh->PostEditChange();
        StaticMesh->MarkPackageDirty();
        UEditorAssetLibrary::SaveLoadedAsset(StaticMesh);
    }
    else if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(LoadedAsset))
    {
        MeshType = TEXT("SkeletalMesh");
        TArray<FSkeletalMaterial>& SkelMaterials = SkelMesh->GetMaterials();
        TotalSlots = SkelMaterials.Num();

        if (MaterialSlot < 0)
        {
            SkelMesh->Modify();
            for (int32 i = 0; i < SkelMaterials.Num(); i++)
            {
                SkelMaterials[i].MaterialInterface = Material;
            }
        }
        else
        {
            if (MaterialSlot >= SkelMaterials.Num())
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material slot %d out of range (mesh has %d slots)"), MaterialSlot, SkelMaterials.Num()));
            }
            SkelMesh->Modify();
            SkelMaterials[MaterialSlot].MaterialInterface = Material;
        }

        SkelMesh->PostEditChange();
        SkelMesh->MarkPackageDirty();
        UEditorAssetLibrary::SaveLoadedAsset(SkelMesh);
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset is not a StaticMesh or SkeletalMesh: %s"), *MeshPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("mesh_path"), MeshPath);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("mesh_type"), MeshType);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetNumberField(TEXT("total_slots"), TotalSlots);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleApplyMaterialToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component - try SCS first, then CDO for inherited components
    UPrimitiveComponent* PrimComponent = nullptr;

    // Pass 1: Search SCS nodes (user-added components)
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->GetVariableName().ToString() == ComponentName)
            {
                PrimComponent = Cast<UPrimitiveComponent>(Node->ComponentTemplate);
                break;
            }
        }
    }

    // Pass 2: Search CDO components (inherited from C++ parent, e.g. ACharacter::Mesh)
    if (!PrimComponent && Blueprint->GeneratedClass)
    {
        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            TArray<UActorComponent*> AllComps;
            CDO->GetComponents(AllComps);
            for (UActorComponent* Comp : AllComps)
            {
                UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(Comp);
                if (PC && PC->GetName() == ComponentName)
                {
                    PrimComponent = PC;
                    break;
                }
            }
        }
    }

    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Component '%s' not found in SCS or CDO of blueprint '%s'"), *ComponentName, *BlueprintName));
    }

    // Load the material
    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }

    // Apply the material
    PrimComponent->Modify();
    if (MaterialSlot < 0)
    {
        int32 NumMaterials = PrimComponent->GetNumMaterials();
        for (int32 i = 0; i < NumMaterials; i++)
        {
            PrimComponent->SetMaterial(i, Material);
        }
    }
    else
    {
        if (MaterialSlot >= PrimComponent->GetNumMaterials())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("Material slot %d out of range (component has %d slots)"), MaterialSlot, PrimComponent->GetNumMaterials()));
        }
        PrimComponent->SetMaterial(MaterialSlot, Material);
    }
    PrimComponent->MarkRenderStateDirty();

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("component_name"), ComponentName);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetBoolField(TEXT("success"), true);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetActorMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* TargetActor = FEpicUnrealMCPCommonUtils::FindActorByName(World, ActorName);
    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get mesh components and their materials
    TArray<UStaticMeshComponent*> MeshComponents;
    TargetActor->GetComponents<UStaticMeshComponent>(MeshComponents);

    TArray<TSharedPtr<FJsonValue>> MaterialSlots;

    for (UStaticMeshComponent* MeshComp : MeshComponents)
    {
        if (MeshComp)
        {
            for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
            {
                TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
                SlotInfo->SetNumberField(TEXT("slot"), i);
                SlotInfo->SetStringField(TEXT("component"), MeshComp->GetName());
                SlotInfo->SetStringField(TEXT("component_type"), TEXT("StaticMesh"));

                UMaterialInterface* Material = MeshComp->GetMaterial(i);
                if (Material)
                {
                    SlotInfo->SetStringField(TEXT("material_name"), Material->GetName());
                    SlotInfo->SetStringField(TEXT("material_path"), Material->GetPathName());
                    SlotInfo->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());
                }
                else
                {
                    SlotInfo->SetStringField(TEXT("material_name"), TEXT("None"));
                    SlotInfo->SetStringField(TEXT("material_path"), TEXT(""));
                    SlotInfo->SetStringField(TEXT("material_class"), TEXT(""));
                }

                MaterialSlots.Add(MakeShared<FJsonValueObject>(SlotInfo));
            }
        }
    }

    // Also check SkeletalMeshComponents
    TArray<USkeletalMeshComponent*> SkelMeshComponents;
    TargetActor->GetComponents<USkeletalMeshComponent>(SkelMeshComponents);

    for (USkeletalMeshComponent* SkelComp : SkelMeshComponents)
    {
        if (SkelComp)
        {
            for (int32 i = 0; i < SkelComp->GetNumMaterials(); i++)
            {
                TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
                SlotInfo->SetNumberField(TEXT("slot"), i);
                SlotInfo->SetStringField(TEXT("component"), SkelComp->GetName());
                SlotInfo->SetStringField(TEXT("component_type"), TEXT("SkeletalMesh"));

                UMaterialInterface* Material = SkelComp->GetMaterial(i);
                if (Material)
                {
                    SlotInfo->SetStringField(TEXT("material_name"), Material->GetName());
                    SlotInfo->SetStringField(TEXT("material_path"), Material->GetPathName());
                    SlotInfo->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());
                }
                else
                {
                    SlotInfo->SetStringField(TEXT("material_name"), TEXT("None"));
                    SlotInfo->SetStringField(TEXT("material_path"), TEXT(""));
                    SlotInfo->SetStringField(TEXT("material_class"), TEXT(""));
                }

                MaterialSlots.Add(MakeShared<FJsonValueObject>(SlotInfo));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetArrayField(TEXT("material_slots"), MaterialSlots);
    ResultObj->SetNumberField(TEXT("total_slots"), MaterialSlots.Num());
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    // Get material slot information
    TArray<TSharedPtr<FJsonValue>> MaterialSlots;
    int32 NumMaterials = 0;
    
    // Check if we have a static mesh assigned
    UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
    if (StaticMesh)
    {
        NumMaterials = StaticMesh->GetNumSections(0); // Get number of material slots for LOD 0
        
        for (int32 i = 0; i < NumMaterials; i++)
        {
            TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
            SlotInfo->SetNumberField(TEXT("slot"), i);
            SlotInfo->SetStringField(TEXT("component"), ComponentName);
            
            UMaterialInterface* Material = MeshComponent->GetMaterial(i);
            if (Material)
            {
                SlotInfo->SetStringField(TEXT("material_name"), Material->GetName());
                SlotInfo->SetStringField(TEXT("material_path"), Material->GetPathName());
                SlotInfo->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());
            }
            else
            {
                SlotInfo->SetStringField(TEXT("material_name"), TEXT("None"));
                SlotInfo->SetStringField(TEXT("material_path"), TEXT(""));
                SlotInfo->SetStringField(TEXT("material_class"), TEXT(""));
            }
            
            MaterialSlots.Add(MakeShared<FJsonValueObject>(SlotInfo));
        }
    }
    else
    {
        // If no static mesh is assigned, we can't determine material slots
        UE_LOG(LogTemp, Warning, TEXT("No static mesh assigned to component %s in blueprint %s"), *ComponentName, *BlueprintName);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("component_name"), ComponentName);
    ResultObj->SetArrayField(TEXT("material_slots"), MaterialSlots);
    ResultObj->SetNumberField(TEXT("total_slots"), MaterialSlots.Num());
    ResultObj->SetBoolField(TEXT("has_static_mesh"), StaticMesh != nullptr);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadBlueprintContent(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    // Get optional parameters
    bool bIncludeEventGraph = true;
    bool bIncludeFunctions = true;
    bool bIncludeVariables = true;
    bool bIncludeComponents = true;
    bool bIncludeInterfaces = true;

    Params->TryGetBoolField(TEXT("include_event_graph"), bIncludeEventGraph);
    Params->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
    Params->TryGetBoolField(TEXT("include_variables"), bIncludeVariables);
    Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);
    Params->TryGetBoolField(TEXT("include_interfaces"), bIncludeInterfaces);

    // Load the blueprint
    UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    ResultObj->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
    ResultObj->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

    // Include variables if requested
    if (bIncludeVariables)
    {
        TArray<TSharedPtr<FJsonValue>> VariableArray;
        for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
        {
            TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
            VarObj->SetStringField(TEXT("name"), Variable.VarName.ToString());
            VarObj->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
            VarObj->SetStringField(TEXT("default_value"), Variable.DefaultValue);
            VarObj->SetBoolField(TEXT("is_editable"), (Variable.PropertyFlags & CPF_Edit) != 0);
            VariableArray.Add(MakeShared<FJsonValueObject>(VarObj));
        }
        ResultObj->SetArrayField(TEXT("variables"), VariableArray);
    }

    // Include functions if requested
    if (bIncludeFunctions)
    {
        TArray<TSharedPtr<FJsonValue>> FunctionArray;
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph)
            {
                TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
                FuncObj->SetStringField(TEXT("name"), Graph->GetName());
                FuncObj->SetStringField(TEXT("graph_type"), TEXT("Function"));
                
                // Count nodes in function
                int32 NodeCount = Graph->Nodes.Num();
                FuncObj->SetNumberField(TEXT("node_count"), NodeCount);
                
                FunctionArray.Add(MakeShared<FJsonValueObject>(FuncObj));
            }
        }
        ResultObj->SetArrayField(TEXT("functions"), FunctionArray);
    }

    // Include event graph if requested
    if (bIncludeEventGraph)
    {
        TSharedPtr<FJsonObject> EventGraphObj = MakeShared<FJsonObject>();
        
        // Find the main event graph
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph && Graph->GetName() == TEXT("EventGraph"))
            {
                EventGraphObj->SetStringField(TEXT("name"), Graph->GetName());
                EventGraphObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
                
                // Get basic node information
                TArray<TSharedPtr<FJsonValue>> NodeArray;
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (Node)
                    {
                        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
                        NodeObj->SetStringField(TEXT("name"), Node->GetName());
                        NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
                        NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                        NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
                    }
                }
                EventGraphObj->SetArrayField(TEXT("nodes"), NodeArray);
                break;
            }
        }
        
        ResultObj->SetObjectField(TEXT("event_graph"), EventGraphObj);
    }

    // Include components if requested
    if (bIncludeComponents)
    {
        TArray<TSharedPtr<FJsonValue>> ComponentArray;
        if (Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate)
                {
                    TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
                    CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
                    CompObj->SetStringField(TEXT("class"), Node->ComponentTemplate->GetClass()->GetName());
                    CompObj->SetBoolField(TEXT("is_root"), Node == Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode());
                    ComponentArray.Add(MakeShared<FJsonValueObject>(CompObj));
                }
            }
        }
        ResultObj->SetArrayField(TEXT("components"), ComponentArray);
    }

    // Include interfaces if requested
    if (bIncludeInterfaces)
    {
        TArray<TSharedPtr<FJsonValue>> InterfaceArray;
        for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
        {
            TSharedPtr<FJsonObject> InterfaceObj = MakeShared<FJsonObject>();
            InterfaceObj->SetStringField(TEXT("name"), Interface.Interface ? Interface.Interface->GetName() : TEXT("Unknown"));
            InterfaceArray.Add(MakeShared<FJsonValueObject>(InterfaceObj));
        }
        ResultObj->SetArrayField(TEXT("interfaces"), InterfaceArray);
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAnalyzeBlueprintGraph(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    // Get optional parameters
    bool bIncludeNodeDetails = true;
    bool bIncludePinConnections = true;
    bool bTraceExecutionFlow = true;

    Params->TryGetBoolField(TEXT("include_node_details"), bIncludeNodeDetails);
    Params->TryGetBoolField(TEXT("include_pin_connections"), bIncludePinConnections);
    Params->TryGetBoolField(TEXT("trace_execution_flow"), bTraceExecutionFlow);

    // Load the blueprint
    UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
    }

    // Find the specified graph
    UEdGraph* TargetGraph = nullptr;
    
    // Check event graphs first
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetName() == GraphName)
        {
            TargetGraph = Graph;
            break;
        }
    }
    
    // Check function graphs if not found
    if (!TargetGraph)
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
    }

    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }

    TSharedPtr<FJsonObject> GraphData = MakeShared<FJsonObject>();
    GraphData->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
    GraphData->SetStringField(TEXT("graph_type"), TargetGraph->GetClass()->GetName());

    // Analyze nodes
    TArray<TSharedPtr<FJsonValue>> NodeArray;
    TArray<TSharedPtr<FJsonValue>> ConnectionArray;

    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if (Node)
        {
            TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
            NodeObj->SetStringField(TEXT("name"), Node->GetName());
            NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
            NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

            if (bIncludeNodeDetails)
            {
                NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
                NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);
                NodeObj->SetBoolField(TEXT("can_rename"), Node->bCanRenameNode);
            }

            // Include pin information if requested
            if (bIncludePinConnections)
            {
                TArray<TSharedPtr<FJsonValue>> PinArray;
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin)
                    {
                        TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                        PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                        PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                        PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
                        PinObj->SetNumberField(TEXT("connections"), Pin->LinkedTo.Num());
                        
                        // Record connections for this pin
                        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                        {
                            if (LinkedPin && LinkedPin->GetOwningNode())
                            {
                                TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                                ConnObj->SetStringField(TEXT("from_node"), Pin->GetOwningNode()->GetName());
                                ConnObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
                                ConnObj->SetStringField(TEXT("to_node"), LinkedPin->GetOwningNode()->GetName());
                                ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
                                ConnectionArray.Add(MakeShared<FJsonValueObject>(ConnObj));
                            }
                        }
                        
                        PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
                    }
                }
                NodeObj->SetArrayField(TEXT("pins"), PinArray);
            }

            NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
    }

    GraphData->SetArrayField(TEXT("nodes"), NodeArray);
    GraphData->SetArrayField(TEXT("connections"), ConnectionArray);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    ResultObj->SetObjectField(TEXT("graph_data"), GraphData);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintVariableDetails(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString VariableName;
    bool bSpecificVariable = Params->TryGetStringField(TEXT("variable_name"), VariableName);

    // Load the blueprint
    UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
    }

    TArray<TSharedPtr<FJsonValue>> VariableArray;

    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        // If looking for specific variable, skip others
        if (bSpecificVariable && Variable.VarName.ToString() != VariableName)
        {
            continue;
        }

        TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
        VarObj->SetStringField(TEXT("name"), Variable.VarName.ToString());
        VarObj->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
        VarObj->SetStringField(TEXT("sub_category"), Variable.VarType.PinSubCategory.ToString());
        VarObj->SetStringField(TEXT("default_value"), Variable.DefaultValue);
        VarObj->SetStringField(TEXT("friendly_name"), Variable.FriendlyName.IsEmpty() ? Variable.VarName.ToString() : Variable.FriendlyName);
        
        // Get tooltip from metadata (VarTooltip doesn't exist in UE 5.5)
        FString TooltipValue;
        if (Variable.HasMetaData(FBlueprintMetadata::MD_Tooltip))
        {
            TooltipValue = Variable.GetMetaData(FBlueprintMetadata::MD_Tooltip);
        }
        VarObj->SetStringField(TEXT("tooltip"), TooltipValue);
        
        VarObj->SetStringField(TEXT("category"), Variable.Category.ToString());

        // Property flags
        VarObj->SetBoolField(TEXT("is_editable"), (Variable.PropertyFlags & CPF_Edit) != 0);
        VarObj->SetBoolField(TEXT("is_blueprint_visible"), (Variable.PropertyFlags & CPF_BlueprintVisible) != 0);
        VarObj->SetBoolField(TEXT("is_editable_in_instance"), (Variable.PropertyFlags & CPF_DisableEditOnInstance) == 0);
        VarObj->SetBoolField(TEXT("is_config"), (Variable.PropertyFlags & CPF_Config) != 0);

        // Replication
        VarObj->SetNumberField(TEXT("replication"), (int32)Variable.ReplicationCondition);

        VariableArray.Add(MakeShared<FJsonValueObject>(VarObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    
    if (bSpecificVariable)
    {
        ResultObj->SetStringField(TEXT("variable_name"), VariableName);
        if (VariableArray.Num() > 0)
        {
            ResultObj->SetObjectField(TEXT("variable"), VariableArray[0]->AsObject());
        }
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Variable not found: %s"), *VariableName));
        }
    }
    else
    {
        ResultObj->SetArrayField(TEXT("variables"), VariableArray);
        ResultObj->SetNumberField(TEXT("variable_count"), VariableArray.Num());
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintFunctionDetails(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString FunctionName;
    bool bSpecificFunction = Params->TryGetStringField(TEXT("function_name"), FunctionName);

    bool bIncludeGraph = true;
    Params->TryGetBoolField(TEXT("include_graph"), bIncludeGraph);

    // Load the blueprint
    UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
    }

    TArray<TSharedPtr<FJsonValue>> FunctionArray;

    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph) continue;

        // If looking for specific function, skip others
        if (bSpecificFunction && Graph->GetName() != FunctionName)
        {
            continue;
        }

        TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
        FuncObj->SetStringField(TEXT("name"), Graph->GetName());
        FuncObj->SetStringField(TEXT("graph_type"), TEXT("Function"));

        // Get function signature from graph
        TArray<TSharedPtr<FJsonValue>> InputPins;
        TArray<TSharedPtr<FJsonValue>> OutputPins;

        // Find function entry and result nodes
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node)
            {
                if (Node->GetClass()->GetName().Contains(TEXT("FunctionEntry")))
                {
                    // Process input parameters
                    for (UEdGraphPin* Pin : Node->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Output && Pin->PinName != TEXT("then"))
                        {
                            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                            PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                            InputPins.Add(MakeShared<FJsonValueObject>(PinObj));
                        }
                    }
                }
                else if (Node->GetClass()->GetName().Contains(TEXT("FunctionResult")))
                {
                    // Process output parameters
                    for (UEdGraphPin* Pin : Node->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != TEXT("exec"))
                        {
                            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                            PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                            OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
                        }
                    }
                }
            }
        }

        FuncObj->SetArrayField(TEXT("input_parameters"), InputPins);
        FuncObj->SetArrayField(TEXT("output_parameters"), OutputPins);
        FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

        // Include graph details if requested
        if (bIncludeGraph)
        {
            TArray<TSharedPtr<FJsonValue>> NodeArray;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node)
                {
                    TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
                    NodeObj->SetStringField(TEXT("name"), Node->GetName());
                    NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
                    NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                    NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
                }
            }
            FuncObj->SetArrayField(TEXT("graph_nodes"), NodeArray);
        }

        FunctionArray.Add(MakeShared<FJsonValueObject>(FuncObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    
    if (bSpecificFunction)
    {
        ResultObj->SetStringField(TEXT("function_name"), FunctionName);
        if (FunctionArray.Num() > 0)
        {
            ResultObj->SetObjectField(TEXT("function"), FunctionArray[0]->AsObject());
        }
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
        }
    }
    else
    {
        ResultObj->SetArrayField(TEXT("functions"), FunctionArray);
        ResultObj->SetNumberField(TEXT("function_count"), FunctionArray.Num());
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateCharacterBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString BlueprintPath = TEXT("/Game/Characters/");
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);
    if (!BlueprintPath.EndsWith(TEXT("/")))
    {
        BlueprintPath += TEXT("/");
    }

    FString FullPath = BlueprintPath + BlueprintName;

    // Check if already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint already exists: %s"), *FullPath));
    }

    // Get optional parameters
    FString SkeletalMeshPath;
    Params->TryGetStringField(TEXT("skeletal_mesh_path"), SkeletalMeshPath);

    FString AnimBlueprintPath;
    Params->TryGetStringField(TEXT("anim_blueprint_path"), AnimBlueprintPath);

    double CapsuleRadius = 40.0;
    Params->TryGetNumberField(TEXT("capsule_radius"), CapsuleRadius);

    double CapsuleHalfHeight = 90.0;
    Params->TryGetNumberField(TEXT("capsule_half_height"), CapsuleHalfHeight);

    double MaxWalkSpeed = 500.0;
    Params->TryGetNumberField(TEXT("max_walk_speed"), MaxWalkSpeed);

    double MaxSprintSpeed = 800.0;
    Params->TryGetNumberField(TEXT("max_sprint_speed"), MaxSprintSpeed);

    double JumpZVelocity = 420.0;
    Params->TryGetNumberField(TEXT("jump_z_velocity"), JumpZVelocity);

    double CameraBoomLength = 250.0;
    Params->TryGetNumberField(TEXT("camera_boom_length"), CameraBoomLength);

    double CameraBoomOffsetZ = 150.0;
    Params->TryGetNumberField(TEXT("camera_boom_socket_offset_z"), CameraBoomOffsetZ);

    // ---- Phase 1: Create Blueprint with ACharacter parent ----
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ACharacter::StaticClass();

    UPackage* Package = CreatePackage(*FullPath);
    UBlueprint* NewBP = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *BlueprintName,
            RF_Standalone | RF_Public, nullptr, GWarn));

    if (!NewBP)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Character Blueprint"));
    }

    FAssetRegistryModule::AssetCreated(NewBP);
    Package->MarkPackageDirty();

    // ---- Phase 2: First compile to create CDO ----
    FKismetEditorUtilities::CompileBlueprint(NewBP);

    // ---- Phase 3: Configure CDO components (inherited from ACharacter) ----
    ACharacter* CDO = NewBP->GeneratedClass->GetDefaultObject<ACharacter>();
    if (!CDO)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get Character CDO after compile"));
    }

    // Configure capsule
    if (UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
    {
        Capsule->SetCapsuleRadius(CapsuleRadius);
        Capsule->SetCapsuleHalfHeight(CapsuleHalfHeight);
    }

    // Configure character movement
    if (UCharacterMovementComponent* Movement = CDO->GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = MaxWalkSpeed;
        Movement->JumpZVelocity = JumpZVelocity;
        Movement->bOrientRotationToMovement = true;
        Movement->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
        Movement->AirControl = 0.35f;
        Movement->BrakingDecelerationWalking = 2000.0f;
    }

    // Don't use controller rotation for character mesh
    CDO->bUseControllerRotationPitch = false;
    CDO->bUseControllerRotationYaw = false;
    CDO->bUseControllerRotationRoll = false;

    // Configure skeletal mesh if provided
    if (!SkeletalMeshPath.IsEmpty())
    {
        USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
        if (SkelMesh)
        {
            if (USkeletalMeshComponent* MeshComp = CDO->GetMesh())
            {
                MeshComp->SetSkeletalMesh(SkelMesh);
                MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -CapsuleHalfHeight));
                MeshComp->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f)); // Face forward

                // Set animation blueprint if provided
                if (!AnimBlueprintPath.IsEmpty())
                {
                    UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBlueprintPath);
                    if (AnimBP && AnimBP->GeneratedClass)
                    {
                        MeshComp->SetAnimInstanceClass(AnimBP->GeneratedClass);
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("create_character_blueprint: Could not load skeletal mesh at '%s'"), *SkeletalMeshPath);
        }
    }

    // ---- Phase 4: Add SpringArm + Camera via SCS (not inherited from ACharacter) ----
    USimpleConstructionScript* SCS = NewBP->SimpleConstructionScript;
    if (SCS)
    {
        // Create SpringArm (CameraBoom)
        USCS_Node* SpringArmNode = SCS->CreateNode(USpringArmComponent::StaticClass(), TEXT("CameraBoom"));
        if (SpringArmNode)
        {
            USpringArmComponent* SpringArmTemplate = Cast<USpringArmComponent>(SpringArmNode->ComponentTemplate);
            if (SpringArmTemplate)
            {
                SpringArmTemplate->TargetArmLength = CameraBoomLength;
                SpringArmTemplate->SocketOffset = FVector(0.0f, 0.0f, CameraBoomOffsetZ);
                SpringArmTemplate->bUsePawnControlRotation = true;
                SpringArmTemplate->bEnableCameraLag = true;
                SpringArmTemplate->CameraLagSpeed = 10.0f;
            }

            // Attach to root (capsule)
            SCS->AddNode(SpringArmNode);

            // Create Camera attached to SpringArm
            USCS_Node* CameraNode = SCS->CreateNode(UCameraComponent::StaticClass(), TEXT("FollowCamera"));
            if (CameraNode)
            {
                SpringArmNode->AddChildNode(CameraNode);
            }
        }
    }

    // ---- Phase 5: Final compile and save ----
    FKismetEditorUtilities::CompileBlueprint(NewBP);
    Package->MarkPackageDirty();

    // Save the blueprint package
    FString PackageFilename = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, NewBP, *PackageFilename, SaveArgs);

    // ---- Build response ----
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), FullPath);
    Result->SetStringField(TEXT("parent_class"), TEXT("Character"));

    if (NewBP->GeneratedClass)
    {
        Result->SetStringField(TEXT("generated_class"), NewBP->GeneratedClass->GetPathName());
    }

    // List components
    TArray<TSharedPtr<FJsonValue>> ComponentList;

    auto AddComponent = [&ComponentList](const FString& Name, const FString& Type) {
        TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
        CompObj->SetStringField(TEXT("name"), Name);
        CompObj->SetStringField(TEXT("type"), Type);
        ComponentList.Add(MakeShared<FJsonValueObject>(CompObj));
    };

    AddComponent(TEXT("CapsuleComponent"), TEXT("UCapsuleComponent"));
    AddComponent(TEXT("Mesh"), TEXT("USkeletalMeshComponent"));
    AddComponent(TEXT("CharacterMovement"), TEXT("UCharacterMovementComponent"));
    AddComponent(TEXT("CameraBoom"), TEXT("USpringArmComponent"));
    AddComponent(TEXT("FollowCamera"), TEXT("UCameraComponent"));

    Result->SetArrayField(TEXT("components"), ComponentList);

    // Settings summary
    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    Settings->SetNumberField(TEXT("capsule_radius"), CapsuleRadius);
    Settings->SetNumberField(TEXT("capsule_half_height"), CapsuleHalfHeight);
    Settings->SetNumberField(TEXT("max_walk_speed"), MaxWalkSpeed);
    Settings->SetNumberField(TEXT("jump_z_velocity"), JumpZVelocity);
    Settings->SetNumberField(TEXT("camera_boom_length"), CameraBoomLength);
    Settings->SetNumberField(TEXT("camera_boom_socket_offset_z"), CameraBoomOffsetZ);
    Result->SetObjectField(TEXT("settings"), Settings);

    Result->SetStringField(TEXT("message"), TEXT("Character Blueprint created successfully"));
    return Result;
}
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Skeleton is REQUIRED for AnimBlueprint (crash without it)
    FString SkeletonPath;
    if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'skeleton_path' parameter. AnimBlueprint requires a target skeleton."));
    }

    USkeleton* TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
    if (!TargetSkeleton)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load skeleton at: %s"), *SkeletonPath));
    }

    FString BlueprintPath = TEXT("/Game/Characters/");
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);
    if (!BlueprintPath.EndsWith(TEXT("/")))
    {
        BlueprintPath += TEXT("/");
    }

    FString FullPath = BlueprintPath + BlueprintName;

    // Check if already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("AnimBlueprint already exists: %s"), *FullPath));
    }

    // Optional: preview mesh for the anim editor
    FString PreviewMeshPath;
    Params->TryGetStringField(TEXT("preview_mesh_path"), PreviewMeshPath);

    // Create the AnimBlueprint using UAnimBlueprintFactory
    UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
    Factory->TargetSkeleton = TargetSkeleton;
    Factory->ParentClass = UAnimInstance::StaticClass();

    UPackage* Package = CreatePackage(*FullPath);
    UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(
        Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package, *BlueprintName,
            RF_Standalone | RF_Public, nullptr, GWarn));

    if (!AnimBP)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create AnimBlueprint"));
    }

    // Set preview mesh if provided
    if (!PreviewMeshPath.IsEmpty())
    {
        USkeletalMesh* PreviewMesh = LoadObject<USkeletalMesh>(nullptr, *PreviewMeshPath);
        if (PreviewMesh)
        {
            AnimBP->SetPreviewMesh(PreviewMesh);
        }
    }

    // Register and save
    FAssetRegistryModule::AssetCreated(AnimBP);
    Package->MarkPackageDirty();

    // Compile the AnimBlueprint
    FKismetEditorUtilities::CompileBlueprint(AnimBP);

    // Save package
    FString PackageFilename = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, AnimBP, *PackageFilename, SaveArgs);

    // Build response
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), FullPath);
    Result->SetStringField(TEXT("skeleton_path"), TargetSkeleton->GetPathName());
    Result->SetStringField(TEXT("parent_class"), TEXT("AnimInstance"));

    if (AnimBP->GeneratedClass)
    {
        Result->SetStringField(TEXT("generated_class"), AnimBP->GeneratedClass->GetPathName());
    }

    Result->SetStringField(TEXT("message"), TEXT("AnimBlueprint created successfully"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetupLocomotionStateMachine(const TSharedPtr<FJsonObject>& Params)
{
	// === Part 0: Parse parameters ===
	FString AnimBPPath;
	if (!Params->TryGetStringField(TEXT("anim_blueprint_path"), AnimBPPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'anim_blueprint_path'"));
	}

	FString IdleAnimPath, WalkAnimPath, RunAnimPath;
	if (!Params->TryGetStringField(TEXT("idle_animation"), IdleAnimPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'idle_animation'"));
	}
	if (!Params->TryGetStringField(TEXT("walk_animation"), WalkAnimPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'walk_animation'"));
	}
	Params->TryGetStringField(TEXT("run_animation"), RunAnimPath);

	FString JumpAnimPath;
	Params->TryGetStringField(TEXT("jump_animation"), JumpAnimPath);
	bool bHasJump = !JumpAnimPath.IsEmpty();

	double WalkThreshold = 5.0, RunThreshold = 300.0;
	double CrossfadeDuration = 0.2;
	Params->TryGetNumberField(TEXT("walk_speed_threshold"), WalkThreshold);
	Params->TryGetNumberField(TEXT("run_speed_threshold"), RunThreshold);
	Params->TryGetNumberField(TEXT("crossfade_duration"), CrossfadeDuration);

	bool bHasRun = !RunAnimPath.IsEmpty();

	// === Part 1: Load assets ===
	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBPPath);
	if (!AnimBP)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load AnimBlueprint: %s"), *AnimBPPath));
	}

	UAnimSequence* IdleAnim = LoadObject<UAnimSequence>(nullptr, *IdleAnimPath);
	UAnimSequence* WalkAnim = LoadObject<UAnimSequence>(nullptr, *WalkAnimPath);
	UAnimSequence* RunAnim = bHasRun ? LoadObject<UAnimSequence>(nullptr, *RunAnimPath) : nullptr;

	if (!IdleAnim)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load idle animation: %s"), *IdleAnimPath));
	}
	if (!WalkAnim)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load walk animation: %s"), *WalkAnimPath));
	}
	if (bHasRun && !RunAnim)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load run animation: %s"), *RunAnimPath));
	}

	UAnimSequence* JumpAnim = bHasJump ? LoadObject<UAnimSequence>(nullptr, *JumpAnimPath) : nullptr;
	if (bHasJump && !JumpAnim)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load jump animation: %s"), *JumpAnimPath));
	}

	// === Part 2: Find AnimGraph ===
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (Graph->GetFName() == FName("AnimGraph"))
		{
			AnimGraph = Graph;
			break;
		}
	}
	if (!AnimGraph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AnimGraph not found in AnimBlueprint"));
	}

	// Find Root (output) node
	UAnimGraphNode_Root* RootNode = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node))
		{
			RootNode = Root;
			break;
		}
	}
	if (!RootNode)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AnimGraph Root node not found"));
	}

	// === Part 3: Create State Machine ===
	UAnimGraphNode_StateMachine* SMNode = NewObject<UAnimGraphNode_StateMachine>(AnimGraph);
	SMNode->NodePosX = RootNode->NodePosX - 400;
	SMNode->NodePosY = RootNode->NodePosY;
	AnimGraph->AddNode(SMNode, true, false);
	SMNode->CreateNewGuid();
	SMNode->PostPlacedNewNode(); // Creates EditorStateMachineGraph
	SMNode->AllocateDefaultPins();

	// Connect SM output → Slot (DefaultSlot) → Root input
	// The Slot node enables montage/PlaySlotAnimationAsDynamicMontage playback
	UEdGraphPin* SMOutputPin = nullptr;
	for (UEdGraphPin* Pin : SMNode->Pins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			SMOutputPin = Pin;
			break;
		}
	}
	UEdGraphPin* RootInputPin = nullptr;
	for (UEdGraphPin* Pin : RootNode->Pins)
	{
		if (Pin->Direction == EGPD_Input)
		{
			RootInputPin = Pin;
			break;
		}
	}

	// Create Slot node between SM and Root for montage playback
	UAnimGraphNode_Slot* SlotNode = NewObject<UAnimGraphNode_Slot>(AnimGraph);
	SlotNode->Node.SlotName = FName("DefaultSlot");
	SlotNode->NodePosX = RootNode->NodePosX - 200;
	SlotNode->NodePosY = RootNode->NodePosY;
	AnimGraph->AddNode(SlotNode, true, false);
	SlotNode->CreateNewGuid();
	SlotNode->AllocateDefaultPins();

	UEdGraphPin* SlotInputPin = nullptr;
	UEdGraphPin* SlotOutputPin = nullptr;
	for (UEdGraphPin* Pin : SlotNode->Pins)
	{
		if (Pin->Direction == EGPD_Input) SlotInputPin = Pin;
		if (Pin->Direction == EGPD_Output) SlotOutputPin = Pin;
	}

	// Wire: SM → Slot → Root
	if (SMOutputPin && SlotInputPin)
	{
		SMOutputPin->MakeLinkTo(SlotInputPin);
	}
	if (SlotOutputPin && RootInputPin)
	{
		SlotOutputPin->MakeLinkTo(RootInputPin);
	}

	// === Part 4: Create States ===
	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
	if (!SMGraph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("State machine graph was not created"));
	}

	// Helper: create a state with an animation sequence player
	auto CreateState = [&](const FString& StateName, UAnimSequence* Anim, int32 PosX, int32 PosY) -> UAnimStateNode*
	{
		UAnimStateNode* StateNode = NewObject<UAnimStateNode>(SMGraph);
		StateNode->NodePosX = PosX;
		StateNode->NodePosY = PosY;
		SMGraph->AddNode(StateNode, true, false);
		StateNode->CreateNewGuid();
		StateNode->PostPlacedNewNode(); // Creates BoundGraph with StateResult
		StateNode->AllocateDefaultPins();

		// Rename the state's bound graph to our desired name
		if (StateNode->BoundGraph)
		{
			StateNode->BoundGraph->Rename(*StateName, nullptr);
		}

		// Add SequencePlayer inside the state's graph
		UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(StateNode->BoundGraph);
		if (StateGraph && Anim)
		{
			UAnimGraphNode_SequencePlayer* SeqPlayer = NewObject<UAnimGraphNode_SequencePlayer>(StateGraph);
			SeqPlayer->SetAnimationAsset(Anim);
			SeqPlayer->NodePosX = -200;
			SeqPlayer->NodePosY = 0;
			StateGraph->AddNode(SeqPlayer, true, false);
			SeqPlayer->CreateNewGuid();
			SeqPlayer->AllocateDefaultPins();

			// Connect SequencePlayer pose output → StateResult pose input
			if (StateGraph->MyResultNode)
			{
				UEdGraphPin* SeqOut = nullptr;
				for (UEdGraphPin* Pin : SeqPlayer->Pins)
				{
					if (Pin->Direction == EGPD_Output)
					{
						SeqOut = Pin;
						break;
					}
				}
				UEdGraphPin* ResultIn = nullptr;
				for (UEdGraphPin* Pin : StateGraph->MyResultNode->Pins)
				{
					if (Pin->Direction == EGPD_Input)
					{
						ResultIn = Pin;
						break;
					}
				}
				if (SeqOut && ResultIn)
				{
					SeqOut->MakeLinkTo(ResultIn);
				}
			}
		}

		return StateNode;
	};

	UAnimStateNode* IdleState = CreateState(TEXT("Idle"), IdleAnim, 200, 0);
	UAnimStateNode* WalkState = CreateState(TEXT("Walk"), WalkAnim, 500, -150);
	UAnimStateNode* RunState = bHasRun ? CreateState(TEXT("Run"), RunAnim, 800, 0) : nullptr;
	UAnimStateNode* JumpState = bHasJump ? CreateState(TEXT("Jump"), JumpAnim, 500, 200) : nullptr;

	// === Part 5: Connect Entry to Idle ===
	if (SMGraph->EntryNode && IdleState)
	{
		UEdGraphPin* EntryOutput = nullptr;
		for (UEdGraphPin* Pin : SMGraph->EntryNode->Pins)
		{
			if (Pin->Direction == EGPD_Output)
			{
				EntryOutput = Pin;
				break;
			}
		}
		UEdGraphPin* IdleInput = IdleState->GetInputPin();
		if (EntryOutput && IdleInput)
		{
			EntryOutput->MakeLinkTo(IdleInput);
		}
	}

	// === Part 6: Create Transitions ===
	auto CreateTransition = [&](UAnimStateNode* Source, UAnimStateNode* Target) -> UAnimStateTransitionNode*
	{
		UAnimStateTransitionNode* TransNode = NewObject<UAnimStateTransitionNode>(SMGraph);
		SMGraph->AddNode(TransNode, true, false);
		TransNode->CreateNewGuid();
		TransNode->PostPlacedNewNode(); // Creates BoundGraph for transition rule
		TransNode->AllocateDefaultPins(); // Must happen BEFORE CreateConnections (needs Pins array)
		TransNode->CreateConnections(Source, Target);
		TransNode->CrossfadeDuration = CrossfadeDuration;
		return TransNode;
	};

	UAnimStateTransitionNode* IdleToWalk = CreateTransition(IdleState, WalkState);
	UAnimStateTransitionNode* WalkToIdle = CreateTransition(WalkState, IdleState);
	UAnimStateTransitionNode* WalkToRun = bHasRun ? CreateTransition(WalkState, RunState) : nullptr;
	UAnimStateTransitionNode* RunToWalk = bHasRun ? CreateTransition(RunState, WalkState) : nullptr;

	// Jump transitions: any locomotion state -> Jump (when IsFalling), Jump -> Idle (when not falling)
	UAnimStateTransitionNode* IdleToJump = (bHasJump && JumpState) ? CreateTransition(IdleState, JumpState) : nullptr;
	UAnimStateTransitionNode* WalkToJump = (bHasJump && JumpState) ? CreateTransition(WalkState, JumpState) : nullptr;
	UAnimStateTransitionNode* RunToJump = (bHasJump && bHasRun && JumpState && RunState) ? CreateTransition(RunState, JumpState) : nullptr;
	UAnimStateTransitionNode* JumpToIdle = (bHasJump && JumpState) ? CreateTransition(JumpState, IdleState) : nullptr;

	// === Part 7: Add Speed variable to AnimBP ===
	int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(AnimBP, FName("Speed"));
	if (VarIndex == INDEX_NONE)
	{
		FBPVariableDescription SpeedVar;
		SpeedVar.VarName = FName("Speed");
		SpeedVar.VarGuid = FGuid::NewGuid();
		SpeedVar.VarType.PinCategory = UEdGraphSchema_K2::PC_Real;
		SpeedVar.VarType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		SpeedVar.DefaultValue = TEXT("0.0");
		SpeedVar.PropertyFlags |= CPF_BlueprintVisible;
		AnimBP->NewVariables.Add(SpeedVar);
	}

	if (bHasJump)
	{
		int32 FallingVarIndex = FBlueprintEditorUtils::FindNewVariableIndex(AnimBP, FName("IsFalling"));
		if (FallingVarIndex == INDEX_NONE)
		{
			FBPVariableDescription IsFallingVar;
			IsFallingVar.VarName = FName("IsFalling");
			IsFallingVar.VarGuid = FGuid::NewGuid();
			IsFallingVar.VarType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
			IsFallingVar.DefaultValue = TEXT("false");
			IsFallingVar.PropertyFlags |= CPF_BlueprintVisible;
			AnimBP->NewVariables.Add(IsFallingVar);
		}
	}

	// Part 7.5: Compile so Speed (and IsFalling) variables are baked into generated class
	// This is required before K2Node_VariableGet/Set can allocate pins for Speed/IsFalling
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	FKismetEditorUtilities::CompileBlueprint(AnimBP);

	// === Part 8: Setup EventBlueprintUpdateAnimation for speed calculation ===
	UEdGraph* EventGraph = nullptr;
	if (AnimBP->UbergraphPages.Num() > 0)
	{
		EventGraph = AnimBP->UbergraphPages[0];
	}

	if (EventGraph)
	{
		// Find or create BlueprintUpdateAnimation event
		UK2Node_Event* UpdateAnimEvent = nullptr;
		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			UK2Node_Event* EvNode = Cast<UK2Node_Event>(Node);
			if (EvNode && EvNode->EventReference.GetMemberName() == FName("BlueprintUpdateAnimation"))
			{
				UpdateAnimEvent = EvNode;
				break;
			}
		}

		if (!UpdateAnimEvent)
		{
			UpdateAnimEvent = NewObject<UK2Node_Event>(EventGraph);
			UpdateAnimEvent->EventReference.SetExternalMember(
				FName("BlueprintUpdateAnimation"), UAnimInstance::StaticClass());
			UpdateAnimEvent->NodePosX = 0;
			UpdateAnimEvent->NodePosY = 400;
			EventGraph->AddNode(UpdateAnimEvent, true, false);
			UpdateAnimEvent->CreateNewGuid();
			UpdateAnimEvent->AllocateDefaultPins();
		}

		// TryGetPawnOwner (UAnimInstance member, called on self)
		UFunction* TryGetPawnOwnerFunc = UAnimInstance::StaticClass()->FindFunctionByName(FName("TryGetPawnOwner"));
		UK2Node_CallFunction* GetPawnNode = nullptr;
		if (TryGetPawnOwnerFunc)
		{
			GetPawnNode = NewObject<UK2Node_CallFunction>(EventGraph);
			GetPawnNode->SetFromFunction(TryGetPawnOwnerFunc);
			GetPawnNode->NodePosX = 300;
			GetPawnNode->NodePosY = 400;
			EventGraph->AddNode(GetPawnNode, true, false);
			GetPawnNode->CreateNewGuid();
			GetPawnNode->AllocateDefaultPins();
		}

		// GetVelocity (AActor member, called on pawn)
		UFunction* GetVelocityFunc = AActor::StaticClass()->FindFunctionByName(FName("GetVelocity"));
		UK2Node_CallFunction* GetVelNode = nullptr;
		if (GetVelocityFunc)
		{
			GetVelNode = NewObject<UK2Node_CallFunction>(EventGraph);
			GetVelNode->SetFromFunction(GetVelocityFunc);
			GetVelNode->NodePosX = 600;
			GetVelNode->NodePosY = 400;
			EventGraph->AddNode(GetVelNode, true, false);
			GetVelNode->CreateNewGuid();
			GetVelNode->AllocateDefaultPins();
		}

		// VSize (KismetMathLibrary, pure - no exec pins)
		UFunction* VSizeFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName("VSize"));
		UK2Node_CallFunction* VSizeNode = nullptr;
		if (VSizeFunc)
		{
			VSizeNode = NewObject<UK2Node_CallFunction>(EventGraph);
			VSizeNode->SetFromFunction(VSizeFunc);
			VSizeNode->NodePosX = 900;
			VSizeNode->NodePosY = 400;
			EventGraph->AddNode(VSizeNode, true, false);
			VSizeNode->CreateNewGuid();
			VSizeNode->AllocateDefaultPins();
		}

		// Set Speed variable
		UK2Node_VariableSet* SetSpeedNode = NewObject<UK2Node_VariableSet>(EventGraph);
		SetSpeedNode->VariableReference.SetSelfMember(FName("Speed"));
		SetSpeedNode->NodePosX = 1200;
		SetSpeedNode->NodePosY = 400;
		EventGraph->AddNode(SetSpeedNode, true, false);
		SetSpeedNode->CreateNewGuid();
		SetSpeedNode->AllocateDefaultPins();

		// Wire execution: Event → SetSpeed (pure functions have no exec pins)
		UEdGraphPin* EventThen = UpdateAnimEvent->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* SetSpeedExec = SetSpeedNode->FindPin(UEdGraphSchema_K2::PN_Execute);
		if (EventThen && SetSpeedExec) EventThen->MakeLinkTo(SetSpeedExec);

		// Wire data chain (all pure nodes, evaluated lazily by Blueprint VM):
		// TryGetPawnOwner.ReturnValue → GetVelocity.self
		if (GetPawnNode && GetVelNode)
		{
			UEdGraphPin* PawnReturn = GetPawnNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
			UEdGraphPin* VelSelf = GetVelNode->FindPin(UEdGraphSchema_K2::PN_Self);
			if (PawnReturn && VelSelf) PawnReturn->MakeLinkTo(VelSelf);
		}

		// GetVelocity.ReturnValue → VSize.A
		if (GetVelNode && VSizeNode)
		{
			UEdGraphPin* VelReturn = GetVelNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
			UEdGraphPin* VSizeA = VSizeNode->FindPin(TEXT("A"));
			if (VelReturn && VSizeA) VelReturn->MakeLinkTo(VSizeA);
		}

		// VSize.ReturnValue → SetSpeed.Speed
		if (VSizeNode)
		{
			UEdGraphPin* VSizeReturn = VSizeNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
			UEdGraphPin* SpeedInput = SetSpeedNode->FindPin(FName("Speed"));
			if (!SpeedInput)
			{
				// Fallback: search all input pins for one matching Speed variable
				for (UEdGraphPin* Pin : SetSpeedNode->Pins)
				{
					if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
					{
						SpeedInput = Pin;
						break;
					}
				}
			}
			if (VSizeReturn && SpeedInput)
			{
				VSizeReturn->MakeLinkTo(SpeedInput);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: Failed to connect VSize→Speed (VSizeReturn=%d, SpeedInput=%d)"),
					VSizeReturn != nullptr, SpeedInput != nullptr);
			}
		}

		// === IsFalling chain (only when jump is enabled) ===
		// Uses pure functions only: TryGetPawnOwner → GetMovementComponent → IsFalling → SetIsFalling
		// GetMovementComponent is a UFUNCTION on APawn (returns UPawnMovementComponent*)
		// IsFalling is a UFUNCTION on UNavMovementComponent (parent of UPawnMovementComponent)
		// Both are const/pure = no exec pins needed, simple data chain
		if (bHasJump)
		{
			// GetMovementComponent (APawn UFUNCTION, pure - no exec pins)
			UFunction* GetMCFunc = APawn::StaticClass()->FindFunctionByName(FName("GetMovementComponent"));
			UK2Node_CallFunction* GetMCNode = nullptr;
			if (GetMCFunc)
			{
				GetMCNode = NewObject<UK2Node_CallFunction>(EventGraph);
				GetMCNode->SetFromFunction(GetMCFunc);
				GetMCNode->NodePosX = 300;
				GetMCNode->NodePosY = 600;
				EventGraph->AddNode(GetMCNode, true, false);
				GetMCNode->CreateNewGuid();
				GetMCNode->AllocateDefaultPins();

				// Wire TryGetPawnOwner.ReturnValue → GetMovementComponent.self
				if (GetPawnNode)
				{
					UEdGraphPin* PawnReturn = GetPawnNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
					UEdGraphPin* MCSelf = GetMCNode->FindPin(UEdGraphSchema_K2::PN_Self);
					if (PawnReturn && MCSelf) PawnReturn->MakeLinkTo(MCSelf);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: GetMovementComponent not found on APawn!"));
			}

			// IsFalling (UNavMovementComponent UFUNCTION, pure - no exec pins)
			UFunction* IsFallingFunc = UNavMovementComponent::StaticClass()->FindFunctionByName(FName("IsFalling"));
			UK2Node_CallFunction* IsFallingNode = nullptr;
			if (IsFallingFunc)
			{
				IsFallingNode = NewObject<UK2Node_CallFunction>(EventGraph);
				IsFallingNode->SetFromFunction(IsFallingFunc);
				IsFallingNode->NodePosX = 600;
				IsFallingNode->NodePosY = 600;
				EventGraph->AddNode(IsFallingNode, true, false);
				IsFallingNode->CreateNewGuid();
				IsFallingNode->AllocateDefaultPins();

				// Wire GetMovementComponent.ReturnValue → IsFalling.self
				if (GetMCNode)
				{
					UEdGraphPin* MCReturn = GetMCNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
					UEdGraphPin* IsFallingSelf = IsFallingNode->FindPin(UEdGraphSchema_K2::PN_Self);
					if (MCReturn && IsFallingSelf) MCReturn->MakeLinkTo(IsFallingSelf);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: IsFalling not found on UNavMovementComponent!"));
			}

			// Set IsFalling variable
			UK2Node_VariableSet* SetIsFallingNode = NewObject<UK2Node_VariableSet>(EventGraph);
			SetIsFallingNode->VariableReference.SetSelfMember(FName("IsFalling"));
			SetIsFallingNode->NodePosX = 900;
			SetIsFallingNode->NodePosY = 600;
			EventGraph->AddNode(SetIsFallingNode, true, false);
			SetIsFallingNode->CreateNewGuid();
			SetIsFallingNode->AllocateDefaultPins();

			// Wire IsFalling.ReturnValue → SetIsFalling input
			if (IsFallingNode)
			{
				UEdGraphPin* IsFallingReturn = IsFallingNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
				UEdGraphPin* SetFallingInput = SetIsFallingNode->FindPin(FName("IsFalling"));
				if (!SetFallingInput)
				{
					// Fallback: find boolean input pin
					for (UEdGraphPin* Pin : SetIsFallingNode->Pins)
					{
						if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
						{
							SetFallingInput = Pin;
							break;
						}
					}
				}
				if (IsFallingReturn && SetFallingInput) IsFallingReturn->MakeLinkTo(SetFallingInput);
			}

			// Wire exec: SetSpeed.Then → SetIsFalling.Execute
			// (GetMovementComponent and IsFalling are pure functions with no exec pins)
			{
				UEdGraphPin* SetSpeedThen = SetSpeedNode->FindPin(UEdGraphSchema_K2::PN_Then);
				UEdGraphPin* SetFallingExec = SetIsFallingNode->FindPin(UEdGraphSchema_K2::PN_Execute);
				if (SetSpeedThen && SetFallingExec) SetSpeedThen->MakeLinkTo(SetFallingExec);
			}
		}
	}

	// === Part 9: Setup Transition Rules (speed comparison) ===
	// Find comparison function once (try multiple UE5 naming conventions)
	UFunction* GreaterFunc = nullptr;
	UFunction* LessFunc = nullptr;
	{
		TArray<FName> GreaterNames = {FName("Greater_DoubleDouble"), FName("Greater_FloatFloat")};
		TArray<FName> LessNames = {FName("Less_DoubleDouble"), FName("Less_FloatFloat")};
		for (const FName& Name : GreaterNames)
		{
			GreaterFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(Name);
			if (GreaterFunc)
			{
				UE_LOG(LogTemp, Display, TEXT("setup_locomotion: Found comparison func: %s"), *Name.ToString());
				break;
			}
		}
		for (const FName& Name : LessNames)
		{
			LessFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(Name);
			if (LessFunc) break;
		}
		if (!GreaterFunc || !LessFunc)
		{
			UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: Comparison functions not found! Greater=%d, Less=%d"), GreaterFunc != nullptr, LessFunc != nullptr);
		}
	}

	auto SetupTransitionRule = [&](UAnimStateTransitionNode* TransNode, bool bGreaterThan, double Threshold)
	{
		if (!TransNode)
		{
			UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: TransNode is null"));
			return;
		}
		if (!TransNode->BoundGraph)
		{
			UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: TransNode->BoundGraph is null"));
			return;
		}

		UAnimationTransitionGraph* TransGraph = Cast<UAnimationTransitionGraph>(TransNode->BoundGraph);
		if (!TransGraph || !TransGraph->MyResultNode)
		{
			UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: TransGraph or MyResultNode is null"));
			return;
		}

		UFunction* CompFunc = bGreaterThan ? GreaterFunc : LessFunc;
		if (!CompFunc)
		{
			UE_LOG(LogTemp, Error, TEXT("setup_locomotion: No comparison function available for transition rule"));
			return;
		}

		// VariableGet for Speed
		UK2Node_VariableGet* SpeedGet = NewObject<UK2Node_VariableGet>(TransGraph);
		SpeedGet->VariableReference.SetSelfMember(FName("Speed"));
		SpeedGet->NodePosX = -300;
		SpeedGet->NodePosY = 0;
		TransGraph->AddNode(SpeedGet, true, false);
		SpeedGet->CreateNewGuid();
		SpeedGet->AllocateDefaultPins();

		UK2Node_CallFunction* CompNode = NewObject<UK2Node_CallFunction>(TransGraph);
		CompNode->SetFromFunction(CompFunc);
		CompNode->NodePosX = -100;
		CompNode->NodePosY = 0;
		TransGraph->AddNode(CompNode, true, false);
		CompNode->CreateNewGuid();
		CompNode->AllocateDefaultPins();

		// Set threshold as default value on B pin
		UEdGraphPin* CompB = CompNode->FindPin(TEXT("B"));
		if (CompB)
		{
			CompB->DefaultValue = FString::SanitizeFloat(Threshold);
		}

		// Wire: SpeedGet output → Comp.A
		UEdGraphPin* SpeedOut = SpeedGet->GetValuePin();
		UEdGraphPin* CompA = CompNode->FindPin(TEXT("A"));
		if (SpeedOut && CompA)
		{
			SpeedOut->MakeLinkTo(CompA);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: Failed to connect Speed→Comp.A (SpeedOut=%d, CompA=%d)"), SpeedOut != nullptr, CompA != nullptr);
		}

		// Wire: Comp.ReturnValue → TransitionResult input
		UEdGraphPin* CompReturn = CompNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
		// Find the boolean input pin on the transition result node
		UEdGraphPin* ResultPin = nullptr;
		for (UEdGraphPin* Pin : TransGraph->MyResultNode->Pins)
		{
			if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				ResultPin = Pin;
				break;
			}
		}
		if (!ResultPin)
		{
			ResultPin = TransGraph->MyResultNode->FindPin(TEXT("bCanEnterTransition"));
		}
		if (CompReturn && ResultPin)
		{
			CompReturn->MakeLinkTo(ResultPin);
			UE_LOG(LogTemp, Display, TEXT("setup_locomotion: Transition rule wired (threshold=%.1f, greater=%d)"), Threshold, bGreaterThan);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("setup_locomotion: Failed to wire transition result (CompReturn=%d, ResultPin=%d)"),
				CompReturn != nullptr, ResultPin != nullptr);
			// Log all pins on result node for debugging
			for (UEdGraphPin* Pin : TransGraph->MyResultNode->Pins)
			{
				UE_LOG(LogTemp, Warning, TEXT("  ResultNode pin: %s, category=%s, dir=%d"),
					*Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString(), (int)Pin->Direction);
			}
		}
	};

	SetupTransitionRule(IdleToWalk, true, WalkThreshold);   // Speed > WalkThreshold
	SetupTransitionRule(WalkToIdle, false, WalkThreshold);  // Speed < WalkThreshold
	if (WalkToRun) SetupTransitionRule(WalkToRun, true, RunThreshold);
	if (RunToWalk) SetupTransitionRule(RunToWalk, false, RunThreshold);

	// Jump transition rules (IsFalling-based)
	if (bHasJump)
	{
		// Find Not_PreBool function for inverting IsFalling
		UFunction* NotBoolFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName("Not_PreBool"));

		auto SetupBoolTransitionRule = [&](UAnimStateTransitionNode* TransNode, bool bInvert)
		{
			if (!TransNode || !TransNode->BoundGraph) return;

			UAnimationTransitionGraph* TransGraph = Cast<UAnimationTransitionGraph>(TransNode->BoundGraph);
			if (!TransGraph || !TransGraph->MyResultNode) return;

			// VariableGet for IsFalling
			UK2Node_VariableGet* FallingGet = NewObject<UK2Node_VariableGet>(TransGraph);
			FallingGet->VariableReference.SetSelfMember(FName("IsFalling"));
			FallingGet->NodePosX = -300;
			FallingGet->NodePosY = 0;
			TransGraph->AddNode(FallingGet, true, false);
			FallingGet->CreateNewGuid();
			FallingGet->AllocateDefaultPins();

			// Find transition result bool input
			UEdGraphPin* ResultPin = nullptr;
			for (UEdGraphPin* Pin : TransGraph->MyResultNode->Pins)
			{
				if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
				{
					ResultPin = Pin;
					break;
				}
			}
			if (!ResultPin)
			{
				ResultPin = TransGraph->MyResultNode->FindPin(TEXT("bCanEnterTransition"));
			}

			if (bInvert && NotBoolFunc)
			{
				// IsFalling -> NOT -> Result (for exit-jump transitions)
				UK2Node_CallFunction* NotNode = NewObject<UK2Node_CallFunction>(TransGraph);
				NotNode->SetFromFunction(NotBoolFunc);
				NotNode->NodePosX = -100;
				NotNode->NodePosY = 0;
				TransGraph->AddNode(NotNode, true, false);
				NotNode->CreateNewGuid();
				NotNode->AllocateDefaultPins();

				UEdGraphPin* FallingOut = FallingGet->GetValuePin();
				UEdGraphPin* NotA = NotNode->FindPin(TEXT("A"));
				if (FallingOut && NotA) FallingOut->MakeLinkTo(NotA);

				UEdGraphPin* NotReturn = NotNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
				if (NotReturn && ResultPin) NotReturn->MakeLinkTo(ResultPin);
			}
			else
			{
				// IsFalling directly -> Result (for enter-jump transitions)
				UEdGraphPin* FallingOut = FallingGet->GetValuePin();
				if (FallingOut && ResultPin) FallingOut->MakeLinkTo(ResultPin);
			}
		};

		// Enter jump: IsFalling = true
		SetupBoolTransitionRule(IdleToJump, false);
		SetupBoolTransitionRule(WalkToJump, false);
		if (RunToJump) SetupBoolTransitionRule(RunToJump, false);

		// Exit jump: IsFalling = false (inverted)
		SetupBoolTransitionRule(JumpToIdle, true);
	}

	// === Part 10: Compile ===
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	FKismetEditorUtilities::CompileBlueprint(AnimBP);

	// Check compile status
	bool bCompileSucceeded = (AnimBP->Status != EBlueprintStatus::BS_Error);
	UE_LOG(LogTemp, Display, TEXT("setup_locomotion: Final compile status=%d (0=UpToDate, 1=Dirty, 2=Error, 3=BeingCreated)"), (int)AnimBP->Status);

	// Build result
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), AnimBP->Status != EBlueprintStatus::BS_Error);
	ResultObj->SetNumberField(TEXT("compile_status"), (int)AnimBP->Status);
	ResultObj->SetStringField(TEXT("anim_blueprint"), AnimBPPath);
	if (AnimBP->Status == EBlueprintStatus::BS_Error)
	{
		UE_LOG(LogTemp, Error, TEXT("setup_locomotion: AnimBP compile FAILED (status=BS_Error). Open ABP in editor for details."));
		ResultObj->SetStringField(TEXT("error"), TEXT("AnimBP compilation failed - open ABP in editor to see errors"));
	}
	int32 JumpTransitionCount = bHasJump ? (3 + (bHasRun ? 1 : 0)) : 0;
	ResultObj->SetNumberField(TEXT("state_count"), 2 + (bHasRun ? 1 : 0) + (bHasJump ? 1 : 0));
	ResultObj->SetNumberField(TEXT("transition_count"), 2 + (bHasRun ? 2 : 0) + JumpTransitionCount);
	ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Locomotion state machine created with speed-based transitions%s%s"),
		bHasRun ? TEXT(" + run state") : TEXT(""),
		bHasJump ? TEXT(" + jump state (IsFalling-based)") : TEXT("")));
	return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetCharacterProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
	}

	// Load the Blueprint
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Blueprint: %s"), *BlueprintPath));
	}

	if (!BP->GeneratedClass)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no GeneratedClass - compile it first"));
	}

	ACharacter* CDO = Cast<ACharacter>(BP->GeneratedClass->GetDefaultObject());
	if (!CDO)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint is not a Character Blueprint"));
	}

	USkeletalMeshComponent* MeshComp = CDO->GetMesh();
	if (!MeshComp)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Character has no SkeletalMeshComponent"));
	}

	TArray<FString> ChangesApplied;

	// Set AnimBlueprint if provided
	FString AnimBPPath;
	if (Params->TryGetStringField(TEXT("anim_blueprint_path"), AnimBPPath) && !AnimBPPath.IsEmpty())
	{
		UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBPPath);
		if (AnimBP && AnimBP->GeneratedClass)
		{
			MeshComp->SetAnimInstanceClass(AnimBP->GeneratedClass);
			ChangesApplied.Add(FString::Printf(TEXT("AnimBP set to %s"), *AnimBPPath));
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load AnimBlueprint: %s"), *AnimBPPath));
		}
	}

	// Set SkeletalMesh if provided
	FString MeshPath;
	if (Params->TryGetStringField(TEXT("skeletal_mesh_path"), MeshPath) && !MeshPath.IsEmpty())
	{
		USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
		if (SkelMesh)
		{
			MeshComp->SetSkeletalMesh(SkelMesh);
			ChangesApplied.Add(FString::Printf(TEXT("SkeletalMesh set to %s"), *MeshPath));
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load SkeletalMesh: %s"), *MeshPath));
		}
	}

	// Set mesh relative transform if provided
	double MeshOffsetZ = 0;
	if (Params->TryGetNumberField(TEXT("mesh_offset_z"), MeshOffsetZ))
	{
		FVector Loc = MeshComp->GetRelativeLocation();
		Loc.Z = MeshOffsetZ;
		MeshComp->SetRelativeLocation(Loc);
		ChangesApplied.Add(FString::Printf(TEXT("Mesh Z offset set to %.1f"), MeshOffsetZ));
	}

	if (ChangesApplied.Num() == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No properties provided to change (use anim_blueprint_path, skeletal_mesh_path, or mesh_offset_z)"));
	}

	// Compile and mark dirty
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	BP->GetPackage()->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("blueprint"), BlueprintPath);

	TArray<TSharedPtr<FJsonValue>> ChangesArray;
	for (const FString& Change : ChangesApplied)
	{
		ChangesArray.Add(MakeShared<FJsonValueString>(Change));
	}
	Result->SetArrayField(TEXT("changes"), ChangesArray);
	return Result;
}
