// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

class FPTToolLegacyModeToolkit;

/**
 * Legacy FEdMode implementation for PTTool.
 * This mode enables transform gizmo functionality while reusing the existing toolkit UI.
 */
class FPTToolLegacyEdMode : public FEdMode
{
public:
	const static FEditorModeID EM_PTToolLegacyEdModeId;

	FPTToolLegacyEdMode();
	virtual ~FPTToolLegacyEdMode();

	// FEdMode interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool UsesTransformWidget() const override;
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override;

private:
	TSharedPtr<FPTToolLegacyModeToolkit> LegacyToolkit;
};
