// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/*
 * Deprecated (removed):
 *
 * This project previously had a UCLASS-based UEdMode (UPTToolEditorMode).
 * The plugin has migrated to the legacy FEdMode implementation (see PTToolLegacyEdMode.*).
 *
 * IMPORTANT:
 * Keeping an old UCLASS/GENERATED_BODY() editor mode header under Source/ can make UHT/UBT
 * emit a corresponding *.gen.cpp and unity build include even if you try to ifdef it out,
 * which results in missing generated file errors and/or duplicate editor modes.
 *
 * If you need the old implementation for reference, retrieve it from version control history.
 */
