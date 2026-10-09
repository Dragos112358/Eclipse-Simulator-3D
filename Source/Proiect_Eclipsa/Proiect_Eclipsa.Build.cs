// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class Proiect_Eclipsa : ModuleRules
{
    public Proiect_Eclipsa(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableUndefinedIdentifierWarnings = false;
        PublicDefinitions.Add("NOMINMAX");

        // 1. MODULE CRITICE LA RUNTIME (esentiale pentru logica jocului, retea, HUD si Cesium)
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "CesiumRuntime",
            "Sockets",
            "Networking",
            "RenderCore",
            "RHI",
            "ProceduralMeshComponent",
            "UMG",
            "Niagara",
            "AVIWriter"
        });

        // 2. MODULE DE UI (puse corect în Private pentru a nu expune antetele în tot proiectul)
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore"
        });

        // 3. SEPARARE EDITOR VS STANDALONE / SHIPPING (Salvare masiva de RAM)
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] {
                "LevelSequence",
                "MovieScene",
                "MovieRenderPipelineCore",
                "MovieRenderPipelineRenderPasses"
            });
        }

        // 4. OPTIMIZARI DE COMPILARE SI MEMORIE
        bUseUnity = true;
        MinFilesUsingPrecompiledHeaderOverride = 1;

        // --- NOU: paths de include stricte, compilare mai rapida ---
        bLegacyPublicIncludePaths = false;

        // --- NOU: dezactivam RTTI clasic C++ daca nu folosesti dynamic_cast<> manual ---
        bUseRTTI = false;

        // --- NOU: standard C++ explicit ---
        CppStandard = CppStandardVersion.Cpp20;
    }
}