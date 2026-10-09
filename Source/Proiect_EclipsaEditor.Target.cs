// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Proiect_EclipsaEditorTarget : TargetRules
{
	public Proiect_EclipsaEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("Proiect_Eclipsa");

        //Alte chestii adaugate
        //bUseUnityBuild = true;
        //bUseIncrementalLinking = true;
    }
}
