// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolModule.h"
#include "PTToolEditorModeCommands.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Components/SplineComponent.h"




#define LOCTEXT_NAMESPACE "PTToolModule"

void FPTToolModule::RegisterVisualizer(double X)
{
	if (GUnrealEd)
	{
	
		
	}
}

void FPTToolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FPTToolEditorModeCommands::Register();

	
	//FEditorDelegates::PostEngineInit.AddRaw(this, &FPTToolModule::RegisterVisualizer);
	FEditorDelegates::OnEditorInitialized.AddRaw(this, &FPTToolModule::RegisterVisualizer);
	
}

void FPTToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FPTToolEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPTToolModule, PTTool)