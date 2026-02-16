#include "Modules/ModuleManager.h"

// BUILD_ID: bump this every time you change plugin code and rebuild.
// Search for this exact string in the editor log to confirm the new binary is loaded.
#define GAMEPLAY_HELPERS_BUILD_ID TEXT("GameplayHelpers BUILD_ID=2026-02-16-v22")

class FGameplayHelpersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogTemp, Warning, TEXT("=== %s LOADED ==="), GAMEPLAY_HELPERS_BUILD_ID);
	}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGameplayHelpersModule, GameplayHelpers)
