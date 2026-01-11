// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolLegacyModeToolkit.h"
#include "PTToolEditorModeToolkit.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "PTToolLegacyModeToolkit"

FPTToolLegacyModeToolkit::FPTToolLegacyModeToolkit()
{
	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyModeToolkit: Constructor called"));
}

FPTToolLegacyModeToolkit::~FPTToolLegacyModeToolkit()
{
	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyModeToolkit: Destructor called"));
}

void FPTToolLegacyModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyModeToolkit::Init() called"));

	// Create the inner toolkit that contains the actual UI logic
	InnerToolkit = MakeShareable(new FPTToolEditorModeToolkit);
	
	// Build the widget using the inner toolkit's standalone widget builder
	ToolkitWidget = InnerToolkit->BuildStandaloneWidget();
	
	// Call the base class Init
	FModeToolkit::Init(InitToolkitHost);

	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyModeToolkit::Init() completed"));
}

void FPTToolLegacyModeToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	// We don't register any tabs since we're using GetInlineContent
	// The widget will be displayed inline in the mode panel
}

void FPTToolLegacyModeToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	// Nothing to unregister
}

FName FPTToolLegacyModeToolkit::GetToolkitFName() const
{
	return FName("PTToolLegacyMode");
}

FText FPTToolLegacyModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "PTTool Legacy Mode");
}

FText FPTToolLegacyModeToolkit::GetToolkitName() const
{
	return GetBaseToolkitName();
}

FText FPTToolLegacyModeToolkit::GetToolkitToolTipText() const
{
	return LOCTEXT("ToolTip", "PTTool Legacy Editor Mode with Transform Gizmo Support");
}

FLinearColor FPTToolLegacyModeToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f);
}

FString FPTToolLegacyModeToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "PTTool ").ToString();
}

TSharedPtr<SWidget> FPTToolLegacyModeToolkit::GetInlineContent() const
{
	return ToolkitWidget;
}

#undef LOCTEXT_NAMESPACE
