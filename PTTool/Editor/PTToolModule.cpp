// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolModule.h"
#include "PTToolEditorModeCommands.h"
#include "PTToolLegacyEdMode.h"
#include "EditorModeRegistry.h"
#include "PTToolEditorModeToolkit.h"


#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "PTToolModule"

const FName FPTToolModule::PTToolLegacyTabId = TEXT("PTToolLegacyTab");

void FPTToolModule::TryInvokeLegacyTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PTToolLegacyTabId);
}

void FPTToolModule::RegisterVisualizer(double X)
{
	// Legacy mode activation is intentionally not forced anymore.
	// Enter the "PTTool Legacy" mode from the left Modes panel when needed.
}

void FPTToolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FPTToolEditorModeCommands::Register();

	// Register the legacy FEdMode for transform gizmo support
	FEditorModeRegistry::Get().RegisterMode<FPTToolLegacyEdMode>(
		FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId,
		LOCTEXT("PTToolLegacyEdModeName", "PTTool Legacy Legacy Legacy"),
		FSlateIcon(),
		true);

	// Register a dockable tab (Nomad tab) that hosts the PTTool UI.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PTToolLegacyTabId,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args)
		{
			TSharedRef<SDockTab> Tab = SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				.Label(LOCTEXT("PTToolLegacyTabLabel", "PTTool 666"));

			// Keep the toolkit alive for as long as the tab lives.
			TSharedPtr<FPTToolEditorModeToolkit> Toolkit = MakeShared<FPTToolEditorModeToolkit>();
			Tab->SetContent(Toolkit->BuildStandaloneWidget());

			Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([Toolkit](TSharedRef<SDockTab>) mutable
			{
				// Explicitly release the strong reference on close to encourage timely cleanup.
				Toolkit.Reset();
			}));
			return Tab;
		}))
		.SetDisplayName(LOCTEXT("PTToolLegacyTabDisplayName", "PTTool"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UE_LOG(LogTemp, Log, TEXT("PTToolModule: Legacy FEdMode registered with ID: %s"),
		*FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId.ToString());
	
	//FEditorDelegates::PostEngineInit.AddRaw(this, &FPTToolModule::RegisterVisualizer);
	//FEditorDelegates::OnEditorInitialized.AddRaw(this, &FPTToolModule::RegisterVisualizer);
	
}

void FPTToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// Unregister the dockable tab
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PTToolLegacyTabId);

	// Unregister the legacy FEdMode
	FEditorModeRegistry::Get().UnregisterMode(FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId);
	
	UE_LOG(LogTemp, Log, TEXT("PTToolModule: Legacy FEdMode unregistered"));

	FPTToolEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPTToolModule, PTTool)