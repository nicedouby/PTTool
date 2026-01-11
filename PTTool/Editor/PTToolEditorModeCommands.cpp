// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolEditorModeCommands.h"
#include "PTToolEditorMode.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "PTToolEditorModeCommands"

FPTToolEditorModeCommands::FPTToolEditorModeCommands()
	: TCommands<FPTToolEditorModeCommands>("PTToolEditorMode",
		NSLOCTEXT("PTToolEditorMode", "PTToolEditorModeCommands", "PTTool Editor Mode"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FPTToolEditorModeCommands::RegisterCommands()
{
	TArray <TSharedPtr<FUICommandInfo>>& ToolCommands = Commands.FindOrAdd(NAME_Default);

	UI_COMMAND(SimpleTool, "Show Actor Info", "Opens message box with info about a clicked actor", EUserInterfaceActionType::Button, FInputChord());
	ToolCommands.Add(SimpleTool);

	UI_COMMAND(InteractiveTool, "Measure Distance", "Measures distance between 2 points (click to set origin, shift-click to set end point)", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(InteractiveTool);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FPTToolEditorModeCommands::GetCommands()
{
	return FPTToolEditorModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
