Param(
    [Parameter(Mandatory)]
    [string]$Workspace,

    [Parameter(Mandatory)]
    [string]$BuildDir,

    [Parameter(Mandatory)]
    [string]$ValidationTools
)

$global:ProgressPreference = 'SilentlyContinue'
$global:ErrorActionPreference = 'Stop'

$FILES=@(
    'msvc\x64\__bin\Release\libmfxhw64.dll',
    'msvc\x64\__bin\Release\mfx_player.exe',
    'msvc\x64\__bin\Release\mfx_transcoder.exe',
    'icc\x64\_studio\mfx_lib\plugin\mfxplugin_hw64.dll',
    'VPL_build\x64\__bin\Release\libmfx64-gen.dll'
)

$package_dir="$Workspace\output\LucasPackage"
$package_name="LucasPackage.zip"

New-Item -ItemType "directory" -Path $package_dir\to_archive\imports\mediasdk
Set-Location -Path $BuildDir; Copy-Item $FILES -Destination $package_dir\to_archive\imports\mediasdk

Copy-Item $ValidationTools\mediasdkDir.exe -Destination $package_dir\to_archive
Copy-Item $Workspace\workspace_snapshot.json -Destination $package_dir\to_archive

Compress-Archive -Path $package_dir\to_archive\* -DestinationPath $package_dir\$package_name