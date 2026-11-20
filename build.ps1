[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Ue4ssSource,

    [Parameter(Mandatory = $true)]
    [string] $Ue4ssDll
)

$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $Ue4ssSource).Path
$loader = (Resolve-Path -LiteralPath $Ue4ssDll).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio with the MSVC x64 tools is required.' }
$env:Path = "$(Split-Path -Parent $vswhere);$env:Path"
$vcvars = Join-Path $visualStudio 'VC/Auxiliary/Build/vcvars64.bat'
$environment = & cmd.exe /d /c "call `"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the MSVC x64 tools.' }
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}

$build = Join-Path $PSScriptRoot 'build'
$package = Join-Path $PSScriptRoot 'dist/EasyGenderAndNameChange'
$dlls = Join-Path $package 'dlls'
New-Item -ItemType Directory -Force $build, $dlls | Out-Null

# Generate a small import library from the installed loader so the build can
# link against the same UE4SS DLL that will load the mod.
$exports = & dumpbin.exe /nologo /exports $loader
if ($LASTEXITCODE -ne 0) { throw 'Could not read UE4SS DLL exports.' }
$symbols = foreach ($line in $exports) {
    if ($line -match '^\s+\d+\s+[\da-fA-F]+\s+[\da-fA-F]+\s+(\?\S*CppUserModBase\S*)') { $Matches[1] }
}
if (-not ($symbols -like '??0CppUserModBase@*')) { throw 'The UE4SS C++ mod constructor export is missing.' }
$definition = Join-Path $build 'UE4SS.def'
$importLibrary = Join-Path $build 'UE4SS.lib'
@('LIBRARY UE4SS.dll', 'EXPORTS') + $symbols | Set-Content -LiteralPath $definition -Encoding Ascii
& lib.exe /nologo /machine:x64 "/def:$definition" "/out:$importLibrary"
if ($LASTEXITCODE -ne 0) { throw 'Could not build the UE4SS import library.' }

function Get-Dependency($Name, $Repository, $Tag, $Header) {
    $checkout = Join-Path $build $Name
    if (-not (Test-Path -LiteralPath (Join-Path $checkout $Header))) {
        & git.exe -c advice.detachedHead=false clone --quiet --depth 1 --branch $Tag $Repository $checkout | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "Could not obtain the UE4SS dependency $Name." }
    }
    return $checkout
}

# These are the header-only build inputs pinned by UE4SS 17e30d5c.
$imgui = Get-Dependency imgui 'https://github.com/ocornut/imgui.git' 'v1.92.1' 'imgui.h'
$textEdit = Get-Dependency textedit 'https://github.com/UE4SS-RE/ImGuiColorTextEdit.git' 'v1.2.0' 'TextEditor.h'
$fmt = Get-Dependency fmt 'https://github.com/fmtlib/fmt.git' '11.2.0' 'include/fmt/format.h'
$includes = @((Join-Path $source 'UE4SS/include'), $imgui, $textEdit, (Join-Path $fmt 'include'))
$includes += 'include/Unreal', 'include/Unreal/Core', 'generated_include' |
    ForEach-Object { Join-Path $source "deps/first/Unreal/$_" }
$includes += Get-ChildItem -LiteralPath (Join-Path $source 'deps/first') -Directory |
    ForEach-Object { Join-Path $_.FullName 'include' } | Where-Object { Test-Path -LiteralPath $_ }
$includeFlags = $includes | ForEach-Object { "/I$_" }
& cl.exe /nologo /LD /MD /EHsc /std:c++latest /O2 /utf-8 /DUNICODE /D_UNICODE /DUE_BUILD_SHIPPING=1 @includeFlags `
    /DPLATFORM_WINDOWS /DPLATFORM_MICROSOFT /DOVERRIDE_PLATFORM_HEADER_NAME=Windows /DUBT_COMPILED_PLATFORM=Win64 `
    "/Fo:$build/main.obj" "/Fe:$dlls/main.dll" (Join-Path $PSScriptRoot 'main.cpp') `
    /link $importLibrary "/IMPLIB:$build/EasyGenderAndNameChange.lib"
if ($LASTEXITCODE -ne 0) { throw 'Could not build Easy Gender and Name Change.' }

$enabled = Join-Path $package 'enabled.txt'
if (-not (Test-Path -LiteralPath $enabled)) { New-Item -ItemType File -Path $enabled | Out-Null }
Write-Output $package
