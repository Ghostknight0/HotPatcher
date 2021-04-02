// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KDLC : ModuleRules
{
	public KDLC(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
				new string[] { "MobilePatchingUtils/Private" }
            );
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core", "CoreUObject", "Engine", "InputCore" , "PakFile", "Json" 
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "Http", "MobilePatchingUtils", "BuildPatchServices"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				 "AssetRegistry"
				// ... add any modules that your module loads dynamically here ...
			}
			);
        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
