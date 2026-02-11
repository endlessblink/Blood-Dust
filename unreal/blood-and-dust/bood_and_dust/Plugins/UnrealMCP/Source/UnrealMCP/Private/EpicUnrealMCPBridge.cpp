#include "EpicUnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPMaterialGraphCommands.h"
#include "Commands/EpicUnrealMCPLandscapeCommands.h"
#include "Commands/EpicUnrealMCPGameplayCommands.h"
#include "Commands/EpicUnrealMCPWidgetCommands.h"
#include "Commands/EpicUnrealMCPAICommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UEpicUnrealMCPBridge::UEpicUnrealMCPBridge()
{
    EditorCommands = MakeShared<FEpicUnrealMCPEditorCommands>();
    BlueprintCommands = MakeShared<FEpicUnrealMCPBlueprintCommands>();
    BlueprintGraphCommands = MakeShared<FEpicUnrealMCPBlueprintGraphCommands>();
    MaterialGraphCommands = MakeShared<FEpicUnrealMCPMaterialGraphCommands>();
    LandscapeCommands = MakeShared<FEpicUnrealMCPLandscapeCommands>();
    GameplayCommands = MakeShared<FEpicUnrealMCPGameplayCommands>();
    WidgetCommands = MakeShared<FEpicUnrealMCPWidgetCommands>();
    AICommands = MakeShared<FEpicUnrealMCPAICommands>();
}

UEpicUnrealMCPBridge::~UEpicUnrealMCPBridge()
{
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintGraphCommands.Reset();
    MaterialGraphCommands.Reset();
    GameplayCommands.Reset();
    WidgetCommands.Reset();
    AICommands.Reset();
}

// Initialize subsystem
void UEpicUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UEpicUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Shutting down"));
    StopServer();
}

// Start the MCP server
void UEpicUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UEpicUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server stopped"));
}

// Execute a command received from a client
FString UEpicUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Executing command: %s"), *CommandType);
    
    // Use TSharedPtr for the promise so the lambda is copyable (required by FTickerDelegate)
    TSharedPtr<TPromise<FString>> Promise = MakeShared<TPromise<FString>>();
    TFuture<FString> Future = Promise->GetFuture();

    // Schedule execution during the next engine tick via FTSTicker.
    // This runs on the game thread during the normal tick loop, NOT inside the task graph's
    // ProcessTasksUntilIdle. This is critical for heavy commands like import_mesh that
    // internally use the task graph (Nanite building, etc.) - running them inside
    // AsyncTask(GameThread) causes a TaskGraph RecursionGuard assertion crash.
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this, CommandType, Params, Promise](float DeltaTime) -> bool
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
        
        try
        {
            TSharedPtr<FJsonObject> ResultJson;
            
            if (CommandType == TEXT("ping"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
            }
            // Editor Commands (including actor manipulation)
            else if (CommandType == TEXT("get_actors_in_level") ||
                     CommandType == TEXT("find_actors_by_name") ||
                     CommandType == TEXT("spawn_actor") ||
                     CommandType == TEXT("delete_actor") ||
                     CommandType == TEXT("set_actor_transform") ||
                     CommandType == TEXT("spawn_blueprint_actor") ||
                     CommandType == TEXT("set_actor_property") ||
                     CommandType == TEXT("get_actor_properties") ||
                     CommandType == TEXT("create_material") ||
                     CommandType == TEXT("create_material_instance") ||
                     CommandType == TEXT("set_material_instance_parameter") ||
                     CommandType == TEXT("import_texture") ||
                     CommandType == TEXT("set_texture_properties") ||
                     CommandType == TEXT("create_pbr_material") ||
                     CommandType == TEXT("create_landscape_material") ||
                     CommandType == TEXT("import_mesh") ||
                     CommandType == TEXT("list_assets") ||
                     CommandType == TEXT("does_asset_exist") ||
                     CommandType == TEXT("get_asset_info") ||
                     CommandType == TEXT("get_height_at_location") ||
                     CommandType == TEXT("snap_actor_to_ground") ||
                     CommandType == TEXT("scatter_meshes_on_landscape") ||
                     CommandType == TEXT("take_screenshot") ||
                     CommandType == TEXT("get_material_info") ||
                     CommandType == TEXT("focus_viewport_on_actor") ||
                     CommandType == TEXT("get_texture_info") ||
                     CommandType == TEXT("delete_actors_by_pattern") ||
                     CommandType == TEXT("import_skeletal_mesh") ||
                     CommandType == TEXT("import_animation") ||
                     CommandType == TEXT("delete_asset") ||
                     CommandType == TEXT("set_nanite_enabled") ||
                     CommandType == TEXT("scatter_foliage") ||
                     CommandType == TEXT("import_sound"))
            {
                ResultJson = EditorCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Commands
            else if (CommandType == TEXT("create_blueprint") ||
                     CommandType == TEXT("add_component_to_blueprint") ||
                     CommandType == TEXT("set_physics_properties") ||
                     CommandType == TEXT("compile_blueprint") ||
                     CommandType == TEXT("set_static_mesh_properties") ||
                     CommandType == TEXT("set_mesh_material_color") ||
                     CommandType == TEXT("get_available_materials") ||
                     CommandType == TEXT("apply_material_to_actor") ||
                     CommandType == TEXT("set_mesh_asset_material") ||
                     CommandType == TEXT("apply_material_to_blueprint") ||
                     CommandType == TEXT("get_actor_material_info") ||
                     CommandType == TEXT("get_blueprint_material_info") ||
                     CommandType == TEXT("read_blueprint_content") ||
                     CommandType == TEXT("analyze_blueprint_graph") ||
                     CommandType == TEXT("get_blueprint_variable_details") ||
                     CommandType == TEXT("get_blueprint_function_details") ||
                     CommandType == TEXT("create_character_blueprint") ||
                     CommandType == TEXT("create_anim_blueprint") ||
                     CommandType == TEXT("setup_locomotion_state_machine") ||
                     CommandType == TEXT("set_character_properties"))
            {
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Graph Commands
            else if (CommandType == TEXT("add_blueprint_node") ||
                     CommandType == TEXT("connect_nodes") ||
                     CommandType == TEXT("create_variable") ||
                     CommandType == TEXT("set_blueprint_variable_properties") ||
                     CommandType == TEXT("add_event_node") ||
                     CommandType == TEXT("delete_node") ||
                     CommandType == TEXT("set_node_property") ||
                     CommandType == TEXT("create_function") ||
                     CommandType == TEXT("add_function_input") ||
                     CommandType == TEXT("add_function_output") ||
                     CommandType == TEXT("delete_function") ||
                     CommandType == TEXT("rename_function") ||
                     CommandType == TEXT("add_enhanced_input_action_event") ||
                     CommandType == TEXT("create_input_action") ||
                     CommandType == TEXT("add_input_mapping"))
            {
                ResultJson = BlueprintGraphCommands->HandleCommand(CommandType, Params);
            }
            // Material Graph Commands
            else if (CommandType == TEXT("create_material_asset") ||
                     CommandType == TEXT("get_material_graph") ||
                     CommandType == TEXT("add_material_expression") ||
                     CommandType == TEXT("connect_material_expressions") ||
                     CommandType == TEXT("connect_to_material_output") ||
                     CommandType == TEXT("set_material_expression_property") ||
                     CommandType == TEXT("delete_material_expression") ||
                     CommandType == TEXT("recompile_material") ||
                     CommandType == TEXT("configure_landscape_layer_blend"))
            {
                ResultJson = MaterialGraphCommands->HandleCommand(CommandType, Params);
            }
            // Landscape Commands
            else if (CommandType == TEXT("get_landscape_info") ||
                     CommandType == TEXT("sculpt_landscape") ||
                     CommandType == TEXT("smooth_landscape") ||
                     CommandType == TEXT("flatten_landscape") ||
                     CommandType == TEXT("paint_landscape_layer") ||
                     CommandType == TEXT("get_landscape_layers") ||
                     CommandType == TEXT("set_landscape_material") ||
                     CommandType == TEXT("create_landscape_layer") ||
                     CommandType == TEXT("add_layer_to_landscape"))
            {
                ResultJson = LandscapeCommands->HandleCommand(CommandType, Params);
            }
            // Gameplay Commands (game mode, montage, impulse, post-process effects, niagara)
            else if (CommandType == TEXT("set_game_mode_default_pawn") ||
                     CommandType == TEXT("create_anim_montage") ||
                     CommandType == TEXT("play_montage_on_actor") ||
                     CommandType == TEXT("apply_impulse") ||
                     CommandType == TEXT("trigger_post_process_effect") ||
                     CommandType == TEXT("spawn_niagara_system"))
            {
                ResultJson = GameplayCommands->HandleCommand(CommandType, Params);
            }
            // Widget Commands (UMG widget blueprints, viewport display, properties)
            else if (CommandType == TEXT("create_widget_blueprint") ||
                     CommandType == TEXT("add_widget_to_viewport") ||
                     CommandType == TEXT("set_widget_property"))
            {
                ResultJson = WidgetCommands->HandleCommand(CommandType, Params);
            }
            // AI Commands (behavior trees, blackboards, tasks, decorators)
            else if (CommandType == TEXT("create_behavior_tree") ||
                     CommandType == TEXT("create_blackboard") ||
                     CommandType == TEXT("add_bt_task") ||
                     CommandType == TEXT("add_bt_decorator") ||
                     CommandType == TEXT("assign_behavior_tree"))
            {
                ResultJson = AICommands->HandleCommand(CommandType, Params);
            }
            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
                
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise->SetValue(ResultString);
                return false;
            }
            
            // Check if the result contains an error
            bool bSuccess = true;
            FString ErrorMessage;
            
            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess && ResultJson->HasField(TEXT("error")))
                {
                    ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                }
            }
            
            if (bSuccess)
            {
                // Set success status and include the result
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                // Set error status and include the error message
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise->SetValue(ResultString);
        return false; // Execute once, don't repeat
    }));

    return Future.Get();
}