#!/usr/bin/env pwsh
#Requires -Version 5.1
Set-StrictMode -Version Latest

<#
.SYNOPSIS
    Shared IL patching utilities for CameraUnlock mods using Mono.Cecil.
.DESCRIPTION
    Provides common assembly patching operations:
    - Screen center raycast patching (for aim decoupling)
    - Method injection
    - Patch marker management
    - Safe assembly modification with backup/restore

    IMPORTANT: Mono.Cecil must be loaded before using these functions.
    Call Initialize-AssemblyPatching first with the path to Mono.Cecil.dll.
#>

# Track if Mono.Cecil is loaded
$Script:CecilLoaded = $false
$Script:CecilPath = $null

<#
.SYNOPSIS
    Initializes the assembly patching module by loading Mono.Cecil.
.PARAMETER CecilPath
    Path to Mono.Cecil.dll.
#>
function Initialize-AssemblyPatching {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$CecilPath
    )

    if (-not (Test-Path $CecilPath)) {
        throw "Mono.Cecil.dll not found at: $CecilPath"
    }

    Add-Type -Path $CecilPath
    $Script:CecilLoaded = $true
    $Script:CecilPath = $CecilPath
    Write-Host "Loaded Mono.Cecil from: $CecilPath" -ForegroundColor Gray
}

<#
.SYNOPSIS
    Gets the inline C# patcher code for screen center raycast patching.
.DESCRIPTION
    Returns the C# code that can be compiled and executed to patch assemblies.
    This code patches `new Vector3(Screen.width/2, Screen.height/2, 0)` patterns
    to call a custom method instead.
.PARAMETER PatchMarker
    Name of the marker type to add (default: "HeadTracking_Patched_v2").
.OUTPUTS
    String containing the C# patcher code.
#>
function Get-ScreenCenterPatcherCode {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$PatchMarker = "HeadTracking_Patched_v2"
    )

    return @"
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Mono.Cecil;
using Mono.Cecil.Cil;

public static class ScreenCenterPatcher
{
    private const string PatchMarker = "$PatchMarker";

    /// <summary>
    /// Checks if an assembly is already patched.
    /// </summary>
    public static bool IsPatched(AssemblyDefinition assembly)
    {
        return assembly.MainModule.Types.Any(t => t.Name == PatchMarker);
    }

    /// <summary>
    /// Adds a patch marker type to the assembly.
    /// </summary>
    public static void AddPatchMarker(AssemblyDefinition assembly)
    {
        var markerType = new TypeDefinition(
            "HeadTracking",
            PatchMarker,
            TypeAttributes.NotPublic | TypeAttributes.Class,
            assembly.MainModule.TypeSystem.Object);
        assembly.MainModule.Types.Add(markerType);
    }

    /// <summary>
    /// Injects a static method call at the end of a method (before ret).
    /// </summary>
    public static bool InjectMethodCall(MethodDefinition targetMethod, MethodReference methodToCall)
    {
        if (targetMethod == null || !targetMethod.HasBody)
            return false;

        var il = targetMethod.Body.GetILProcessor();
        var retInstruction = targetMethod.Body.Instructions.Last();

        if (retInstruction.OpCode != OpCodes.Ret)
        {
            // Find the last ret instruction
            retInstruction = targetMethod.Body.Instructions.LastOrDefault(i => i.OpCode == OpCodes.Ret);
            if (retInstruction == null)
                return false;
        }

        il.InsertBefore(retInstruction, il.Create(OpCodes.Call, methodToCall));
        return true;
    }

    /// <summary>
    /// Patches screen center raycast patterns in a method.
    /// Replaces: new Vector3(Screen.width / 2, Screen.height / 2, 0f)
    /// With: call to the specified replacement method
    /// </summary>
    public static int PatchScreenCenterRaycasts(MethodDefinition method, MethodReference replacementMethod)
    {
        if (method == null || !method.HasBody)
            return 0;

        var instructions = method.Body.Instructions;
        var il = method.Body.GetILProcessor();
        int patchCount = 0;

        // Find all newobj Vector3 instructions followed by ScreenPointToRay
        for (int i = 0; i < instructions.Count; i++)
        {
            var instr = instructions[i];

            if (instr.OpCode != OpCodes.Newobj)
                continue;

            var methodRef = instr.Operand as MethodReference;
            if (methodRef == null ||
                methodRef.DeclaringType.Name != "Vector3" ||
                methodRef.Parameters.Count != 3)
                continue;

            // Check if next instruction is ScreenPointToRay
            if (i + 1 >= instructions.Count)
                continue;

            var nextInstr = instructions[i + 1];
            if (nextInstr.OpCode != OpCodes.Callvirt && nextInstr.OpCode != OpCodes.Call)
                continue;

            var nextMethodRef = nextInstr.Operand as MethodReference;
            if (nextMethodRef == null || nextMethodRef.Name != "ScreenPointToRay")
                continue;

            // Found the pattern - trace back to find Screen.get_width
            int startIdx = -1;
            for (int j = i - 1; j >= 0 && j >= i - 15; j--)
            {
                if (instructions[j].OpCode == OpCodes.Call)
                {
                    var callRef = instructions[j].Operand as MethodReference;
                    if (callRef != null &&
                        callRef.Name == "get_width" &&
                        callRef.DeclaringType.Name == "Screen")
                    {
                        startIdx = j;
                        break;
                    }
                }
            }

            if (startIdx < 0)
                continue;

            // Remove instructions from startIdx to i (inclusive of newobj)
            // and replace with call to replacement method
            var instructionsToRemove = new List<Instruction>();
            for (int k = startIdx; k <= i; k++)
            {
                instructionsToRemove.Add(instructions[k]);
            }

            // Insert the call before removing to preserve instruction references
            var newCall = il.Create(OpCodes.Call, replacementMethod);
            il.InsertBefore(instructions[startIdx], newCall);

            // Remove the old instructions
            foreach (var toRemove in instructionsToRemove)
            {
                il.Remove(toRemove);
            }

            patchCount++;
            // Adjust i since we modified the instruction list
            i = startIdx;
        }

        return patchCount;
    }

    /// <summary>
    /// Creates a MethodDefinition for a new method (if needed).
    /// </summary>
    public static MethodDefinition CreateVoidMethod(AssemblyDefinition assembly, string name, MethodAttributes attributes)
    {
        var method = new MethodDefinition(
            name,
            attributes,
            assembly.MainModule.TypeSystem.Void);
        method.Body.Instructions.Add(Instruction.Create(OpCodes.Ret));
        return method;
    }
}
"@
}

<#
.SYNOPSIS
    Compiles and loads the screen center patcher code.
.DESCRIPTION
    Compiles the C# patcher code into an in-memory assembly that can be used
    to patch game assemblies.
.PARAMETER CecilPath
    Path to Mono.Cecil.dll (required for compilation).
.OUTPUTS
    The compiled patcher type.
#>
function New-ScreenCenterPatcher {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$CecilPath = $Script:CecilPath,

        [Parameter(Mandatory=$false)]
        [string]$PatchMarker = "HeadTracking_Patched_v2"
    )

    if (-not $CecilPath) {
        throw "Mono.Cecil path not set. Call Initialize-AssemblyPatching first."
    }

    $code = Get-ScreenCenterPatcherCode -PatchMarker $PatchMarker

    $compilerParams = New-Object System.CodeDom.Compiler.CompilerParameters
    [void]$compilerParams.ReferencedAssemblies.Add($CecilPath)
    [void]$compilerParams.ReferencedAssemblies.Add("System.dll")
    [void]$compilerParams.ReferencedAssemblies.Add("System.Core.dll")
    $compilerParams.CompilerOptions = "/nowarn:1668 /warn:0"
    $compilerParams.TreatWarningsAsErrors = $false

    # Check if type already exists to avoid Add-Type output issues
    $existingType = [AppDomain]::CurrentDomain.GetAssemblies() |
        ForEach-Object { $_.GetTypes() } |
        Where-Object { $_.Name -eq 'ScreenCenterPatcher' } |
        Select-Object -First 1

    if (-not $existingType) {
        Add-Type -TypeDefinition $code -CompilerParameters $compilerParams
    }
    return [ScreenCenterPatcher]
}

<#
.SYNOPSIS
    Patches an Assembly-CSharp.dll for head tracking.
.DESCRIPTION
    Performs common patching operations for head tracking mods:
    1. Injects StaticTracker.ApplyTracking() call into a controller method
    2. Patches screen center raycasts to use StaticTracker.GetAimScreenPosition()
    3. Adds patch marker to prevent double-patching
.PARAMETER AssemblyPath
    Path to the Assembly-CSharp.dll to patch.
.PARAMETER ModDllPath
    Path to the mod DLL containing StaticTracker.
.PARAMETER ControllerTypeName
    Name of the controller type to patch (e.g., "FirstPersonPlayerController").
.PARAMETER ControllerMethodName
    Name of the method to inject into (e.g., "LateUpdate").
.PARAMETER RaycastTypeNames
    Array of type names to patch raycast calls in.
.PARAMETER CecilPath
    Path to Mono.Cecil.dll (optional, uses cached path if not provided).
.OUTPUTS
    Hashtable with patching results.
#>
function Invoke-HeadTrackingPatch {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$AssemblyPath,

        [Parameter(Mandatory=$true)]
        [string]$ModDllPath,

        [Parameter(Mandatory=$true)]
        [string]$ControllerTypeName,

        [Parameter(Mandatory=$false)]
        [string]$ControllerMethodName = "LateUpdate",

        [Parameter(Mandatory=$false)]
        [string[]]$RaycastTypeNames = @(),

        [Parameter(Mandatory=$false)]
        [string]$CecilPath = $Script:CecilPath,

        [Parameter(Mandatory=$false)]
        [string]$PatchMarker = "HeadTracking_Patched_v2"
    )

    $results = @{
        Success = $false
        AlreadyPatched = $false
        InjectedCall = $false
        RaycastPatches = 0
        Errors = @()
    }

    if (-not $CecilPath) {
        $results.Errors += "Mono.Cecil path not set. Call Initialize-AssemblyPatching first."
        return $results
    }

    # Compile patcher
    try {
        $patcher = New-ScreenCenterPatcher -CecilPath $CecilPath -PatchMarker $PatchMarker
    } catch {
        $results.Errors += "Failed to compile patcher: $_"
        return $results
    }

    $managedDir = Split-Path -Parent $AssemblyPath

    $resolver = New-Object Mono.Cecil.DefaultAssemblyResolver
    $resolver.AddSearchDirectory($managedDir)

    $readerParams = New-Object Mono.Cecil.ReaderParameters
    $readerParams.AssemblyResolver = $resolver
    $readerParams.ReadWrite = $false
    $readerParams.InMemory = $true

    try {
        # Read assembly into memory
        $assemblyBytes = [System.IO.File]::ReadAllBytes($AssemblyPath)
        $memStream = New-Object System.IO.MemoryStream(,$assemblyBytes)
        $assembly = [Mono.Cecil.AssemblyDefinition]::ReadAssembly($memStream, $readerParams)

        # Check if already patched
        if ($patcher::IsPatched($assembly)) {
            $results.AlreadyPatched = $true
            $results.Success = $true
            Write-Host "  Assembly already patched - skipping" -ForegroundColor Gray
            $assembly.Dispose()
            $memStream.Dispose()
            return $results
        }

        # Read mod assembly to get method references
        $modBytes = [System.IO.File]::ReadAllBytes($ModDllPath)
        $modStream = New-Object System.IO.MemoryStream(,$modBytes)
        $modAssembly = [Mono.Cecil.AssemblyDefinition]::ReadAssembly($modStream, $readerParams)

        $staticTrackerType = $modAssembly.MainModule.Types | Where-Object { $_.Name -eq "StaticTracker" } | Select-Object -First 1
        if (-not $staticTrackerType) {
            $results.Errors += "StaticTracker type not found in mod DLL"
            $modAssembly.Dispose()
            $modStream.Dispose()
            $assembly.Dispose()
            $memStream.Dispose()
            return $results
        }

        $applyMethod = $staticTrackerType.Methods | Where-Object { $_.Name -eq "ApplyTracking" -and $_.IsStatic } | Select-Object -First 1
        if (-not $applyMethod) {
            $results.Errors += "StaticTracker.ApplyTracking() not found"
            $modAssembly.Dispose()
            $modStream.Dispose()
            $assembly.Dispose()
            $memStream.Dispose()
            return $results
        }
        $applyTrackingRef = $assembly.MainModule.ImportReference($applyMethod)

        $getAimMethod = $staticTrackerType.Methods | Where-Object { $_.Name -eq "GetAimScreenPosition" -and $_.IsStatic } | Select-Object -First 1
        $getAimScreenPositionRef = $null
        if ($getAimMethod) {
            $getAimScreenPositionRef = $assembly.MainModule.ImportReference($getAimMethod)
        }

        $modAssembly.Dispose()
        $modStream.Dispose()

        # Find controller and inject call
        $controllerType = $assembly.MainModule.Types | Where-Object { $_.Name -eq $ControllerTypeName } | Select-Object -First 1
        if ($controllerType) {
            $targetMethod = $controllerType.Methods | Where-Object { $_.Name -eq $ControllerMethodName -and -not $_.IsStatic -and $_.HasBody } | Select-Object -First 1

            if (-not $targetMethod) {
                # Try Update as fallback
                $targetMethod = $controllerType.Methods | Where-Object { $_.Name -eq "Update" -and -not $_.IsStatic -and $_.HasBody } | Select-Object -First 1
            }

            if ($targetMethod) {
                if ($patcher::InjectMethodCall($targetMethod, $applyTrackingRef)) {
                    $results.InjectedCall = $true
                    Write-Host "  Injected StaticTracker.ApplyTracking() into ${ControllerTypeName}.$($targetMethod.Name)" -ForegroundColor Green
                }
            } else {
                Write-Host "  Warning: Could not find suitable method in $ControllerTypeName" -ForegroundColor Yellow
            }
        } else {
            Write-Host "  Warning: $ControllerTypeName not found" -ForegroundColor Yellow
        }

        # Patch raycast calls
        if ($getAimScreenPositionRef -and $RaycastTypeNames.Count -gt 0) {
            foreach ($typeName in $RaycastTypeNames) {
                $type = $assembly.MainModule.Types | Where-Object { $_.Name -eq $typeName } | Select-Object -First 1
                if ($type) {
                    $updateMethod = $type.Methods | Where-Object { $_.Name -eq "Update" -and $_.HasBody } | Select-Object -First 1
                    if ($updateMethod) {
                        $patches = $patcher::PatchScreenCenterRaycasts($updateMethod, $getAimScreenPositionRef)
                        $results.RaycastPatches += $patches
                        Write-Host "  Patched $patches raycast(s) in ${typeName}.Update" -ForegroundColor Green
                    }
                }
            }
        }

        # Add patch marker
        $patcher::AddPatchMarker($assembly)

        # Write patched assembly
        $assembly.Write($AssemblyPath)
        Write-Host "  Successfully patched $(Split-Path -Leaf $AssemblyPath)" -ForegroundColor Green

        $results.Success = $true
        $assembly.Dispose()
        $memStream.Dispose()

    } catch {
        $results.Errors += "Patching error: $_"
        Write-Host "  Error: $_" -ForegroundColor Red
    }

    return $results
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-AssemblyPatching',
    'Get-ScreenCenterPatcherCode',
    'New-ScreenCenterPatcher',
    'Invoke-HeadTrackingPatch'
)
