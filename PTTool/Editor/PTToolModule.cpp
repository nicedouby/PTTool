// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolModule.h"
#include "PTToolEditorModeCommands.h"
#include "PTToolLegacyEdMode.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Components/SplineComponent.h"
#include "EditorModeRegistry.h"
#include "EditorModeManager.h"
#include "Containers/Ticker.h"

#define LOCTEXT_NAMESPACE "PTToolModule"

void FPTToolModule::RegisterVisualizer(double X)
{
	if (GUnrealEd)
	{
		// Example: Delayed activation of legacy mode to avoid reentrancy issues
		// In production, you would call this from your desired activation point
		
		// Use a ticker to delay activation slightly to avoid tool context conflicts
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float DeltaTime) -> bool
		{
			if (GLevelEditorModeTools())
			{
				UE_LOG(LogTemp, Log, TEXT("PTToolModule: Attempting to activate legacy mode..."));
				
				// Check if mode is already active
				if (!GLevelEditorModeTools()->IsModeActive(FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId))
				{
					GLevelEditorModeTools()->ActivateMode(FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId);
					
					UE_LOG(LogTemp, Log, TEXT("PTToolModule: Legacy mode activated. GetShowWidget = %s"), 
						GLevelEditorModeTools()->GetShowWidget() ? TEXT("true") : TEXT("false"));
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("PTToolModule: Legacy mode was already active"));
				}
			}
			
			// Return false to run only once
			return false;
		}), 1.0f); // Delay by 1 second
	}
}

void FPTToolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FPTToolEditorModeCommands::Register();

	// Register the legacy FEdMode for transform gizmo support
	FEditorModeRegistry::Get().RegisterMode<FPTToolLegacyEdMode>(
		FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId,
		LOCTEXT("PTToolLegacyEdModeName", "PTTool Legacy"),
		FSlateIcon(),
		true);

	UE_LOG(LogTemp, Log, TEXT("PTToolModule: Legacy FEdMode registered with ID: %s"), 
		*FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId.ToString());
	
	//FEditorDelegates::PostEngineInit.AddRaw(this, &FPTToolModule::RegisterVisualizer);
	FEditorDelegates::OnEditorInitialized.AddRaw(this, &FPTToolModule::RegisterVisualizer);
	
}

void FPTToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// Unregister the legacy FEdMode
	FEditorModeRegistry::Get().UnregisterMode(FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId);
	
	UE_LOG(LogTemp, Log, TEXT("PTToolModule: Legacy FEdMode unregistered"));

	FPTToolEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPTToolModule, PTTool)