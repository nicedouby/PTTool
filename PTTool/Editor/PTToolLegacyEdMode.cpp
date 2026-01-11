// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolLegacyEdMode.h"
#include "PTToolLegacyModeToolkit.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "PTToolLegacyEdMode"

const FEditorModeID FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId = TEXT("EM_PTToolLegacyEdMode");

FPTToolLegacyEdMode::FPTToolLegacyEdMode()
{
	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyEdMode: Constructor called"));
}

FPTToolLegacyEdMode::~FPTToolLegacyEdMode()
{
	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyEdMode: Destructor called"));
}

void FPTToolLegacyEdMode::Enter()
{
	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyEdMode::Enter() called"));
	
	FEdMode::Enter();

	// Create and initialize the toolkit wrapper
	if (!Toolkit.IsValid())
	{
		LegacyToolkit = MakeShareable(new FPTToolLegacyModeToolkit);
		Toolkit = LegacyToolkit;
		Toolkit->Init(Owner->GetToolkitHost());
	}

	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyEdMode::Enter() completed, Toolkit initialized"));
}

void FPTToolLegacyEdMode::Exit()
{
	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyEdMode::Exit() called"));
	
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}
	LegacyToolkit.Reset();

	FEdMode::Exit();
	
	UE_LOG(LogTemp, Log, TEXT("FPTToolLegacyEdMode::Exit() completed"));
}

bool FPTToolLegacyEdMode::UsesTransformWidget() const
{
	UE_LOG(LogTemp, Verbose, TEXT("FPTToolLegacyEdMode::UsesTransformWidget() returning true"));
	return true;
}

bool FPTToolLegacyEdMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	UE_LOG(LogTemp, Verbose, TEXT("FPTToolLegacyEdMode::UsesTransformWidget(CheckMode) returning true"));
	return true;
}

#undef LOCTEXT_NAMESPACE
