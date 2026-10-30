// Copyright notice: sample project, free to modify.
using UnrealBuildTool;
using System.Collections.Generic;

public class IronManArenaTarget : TargetRules
{
	public IronManArenaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("IronManArena");
	}
}
