// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PixelInspectorModule : ModuleRules
{
    public PixelInspectorModule(TargetInfo Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
				"RHI",
				"RenderCore",
				"Slate",
                "EditorStyle",
                "Engine",
                "UnrealEd",
                "PropertyEditor",
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
             new string[] {
					"Engine",
                    "UnrealEd"
				}
         );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"LevelEditor"
			}
		);

		if (UEBuildConfiguration.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "UnrealEd",
    				"SlateCore",
    				"Slate",
                }
            );

            CircularlyReferencedDependentModules.AddRange(
                new string[] {
                    "UnrealEd"
                }
            );
        }
	}
}
