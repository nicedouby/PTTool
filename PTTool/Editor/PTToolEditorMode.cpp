// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolEditorMode.h"
#include "PTToolEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "PTToolEditorModeCommands.h"
#include "Modules/ModuleManager.h"


//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
// AddYourTool Step 1 - include the header file for your Tools here
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
#include "PTToolSimpleTool.h"
#include "PTToolInteractiveTool.h"

// step 2: register a ToolBuilder in FPTToolEditorMode::Enter() below


#define LOCTEXT_NAMESPACE "PTToolEditorMode"

const FEditorModeID UPTToolEditorMode::EM_PTToolEditorModeId = TEXT("EM_PTToolEditorMode");

FString UPTToolEditorMode::SimpleToolName = TEXT("PTTool_ActorInfoTool");
FString UPTToolEditorMode::InteractiveToolName = TEXT("PTTool_MeasureDistanceTool");


UPTToolEditorMode::UPTToolEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	// appearance and icon in the editing mode ribbon can be customized here
	Info = FEditorModeInfo(UPTToolEditorMode::EM_PTToolEditorModeId,
		LOCTEXT("ModeName", "PTTool"),
		FSlateIcon(),
		true);
}


UPTToolEditorMode::~UPTToolEditorMode()
{
}


void UPTToolEditorMode::ActorSelectionChangeNotify()
{
}

void UPTToolEditorMode::Enter()
{
	UEdMode::Enter();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// AddYourTool Step 2 - register the ToolBuilders for your Tools here.
	// The string name you pass to the ToolManager is used to select/activate your ToolBuilder later.
	//////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////// 
	const FPTToolEditorModeCommands& SampleToolCommands = FPTToolEditorModeCommands::Get();

	RegisterTool(SampleToolCommands.SimpleTool, SimpleToolName, NewObject<UPTToolSimpleToolBuilder>(this));
	RegisterTool(SampleToolCommands.InteractiveTool, InteractiveToolName, NewObject<UPTToolInteractiveToolBuilder>(this));

	// Do NOT auto-select/auto-activate any tools here.
	// Selecting an InteractiveTool type on Enter can capture viewport input and block normal editor dragging.
}


void UPTToolEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FPTToolEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UPTToolEditorMode::GetModeCommands() const
{
	return FPTToolEditorModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
