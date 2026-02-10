#include "Modules/ModuleManager.h"

class FGameplayHelpersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGameplayHelpersModule, GameplayHelpers)
