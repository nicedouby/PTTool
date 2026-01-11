// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PTTool : ModuleRules
{
	public PTTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				"PTTool/Tools",
				"PTTool.Core"
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				
				// ... add other private include paths required here ...
				"PTTool/Tools",
				"PTTool.Core"
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorFramework",
				"EditorSubsystem",
				"EditorStyle",
				"UnrealEd",
				"LevelEditor",
				"InteractiveToolsFramework",
				"EditorInteractiveToolsFramework",
				"AssetRegistry",
				"PropertyEditor",
				"RHI",          // 声明 & 类型
				"RenderCore",
				"ApplicationCore", // <-- 添加：提供平台剪贴板等功能
				"ImageWrapper", // <-- 添加：用于保存 PNG
				"UMG", // <-- 添加：用于 FWidgetRenderer
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd", "EditorStyle"
		});
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
