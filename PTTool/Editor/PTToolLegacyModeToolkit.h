// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"
#include "PTToolEditorModeToolkit.h"

/**
 * Legacy mode toolkit wrapper that reuses the existing FPTToolEditorModeToolkit.
 * This allows the legacy FEdMode to display the same UI while supporting transform gizmos.
 */
class FPTToolLegacyModeToolkit : public FModeToolkit
{
public:
	FPTToolLegacyModeToolkit();
	virtual ~FPTToolLegacyModeToolkit();

	// FModeToolkit interface
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost) override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

private:
	// The inner toolkit that we're wrapping/reusing
	TSharedPtr<FPTToolEditorModeToolkit> InnerToolkit;
	
	// Cached widget from the inner toolkit
	TSharedPtr<SWidget> ToolkitWidget;
};
