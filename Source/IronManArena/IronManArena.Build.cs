// Copyright notice: sample project, free to modify.
using System.IO;
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
			"RenderCore",      // IsRayTracingAllowed() for the ray tracing toggle
			"Niagara"          // UBossVFXComponent (Ultron boss pack). Soft refs only: no Niagara content is required
		});

		// Loose JSON that C++ reads at runtime with FFileHelper from folders that also hold uassets (or are not
		// in DefaultGame.ini DirectoriesToAlwaysStageAsNonUFS). Without this the packaged game silently loses
		// all SFX (ArenaAudio), the NPC manifest and the CityKit manifest. Story/, UI/, City/ are staged by ini.
		string[] RuntimeJson = new string[]
		{
			"Content/Audio/sfx_manifest.json",
			"Content/NPC/npc_manifest.json",
			"Content/NPC/Data/npc_manifest.json",
			"Content/CityKit/citykit_manifest.json",
		};
		foreach (string Rel in RuntimeJson)
		{
			string Abs = Path.Combine(ModuleDirectory, "..", "..", Rel);
			if (File.Exists(Abs))
			{
				RuntimeDependencies.Add("$(ProjectDir)/" + Rel, StagedFileType.NonUFS);
			}
		}

		// Base effects are still built from engine basic shapes + point lights so the project runs with
		// zero external assets. Niagara systems under /Game/FX/ are optional upgrades (missing = fallback).
	}
}
