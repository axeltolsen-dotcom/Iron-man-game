// Copyright notice: sample project, free to modify.
using UnrealBuildTool;

public class IronManArena : ModuleRules
{
	public IronManArena(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"MeshDescription",
			"StaticMeshDescription",
			"ProceduralMeshComponent",
			"GeometryFramework",
			"InputCore",
			"EnhancedInput",   // UInputAction / UInputMappingContext created at runtime
			"UMG",             // UUserWidget, UProgressBar, UTextBlock, UButton ...
			"Slate",
			"SlateCore",       // FSlateFontInfo, FSlateColor, FCoreStyle
			"CinematicCamera", // ACineCameraActor for the suit-up cinematics
			"Json",            // FJsonObject: Content/Story/story.json + morgan.json
			"JsonUtilities",   // FJsonObjectConverter: contrib lighting presets -> component properties
			"RenderCore"       // IsRayTracingAllowed() for the ray tracing toggle
		});

		// Niagara is intentionally NOT used: all effects are built from engine basic
		// shapes + point lights so the project runs with zero external assets.
		// If you add Niagara effects later, add "Niagara" here and enable the plugin.
	}
}
