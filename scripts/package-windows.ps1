param(
    [string]$BuildDirectory = 'build/windows-x64',
    [string]$NSIS = 'makensis',
    [string]$OutputFile = ''
)
$ErrorActionPreference = 'Stop'
if ($NSIS -eq 'makensis') {
    $nsisCommand = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if ($nsisCommand) {
        $NSIS = $nsisCommand.Source
    } else {
        $nsisCandidates = @("${env:ProgramFiles(x86)}/NSIS/makensis.exe", "$env:ProgramFiles/NSIS/makensis.exe")
        $NSIS = $nsisCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
        if (-not $NSIS) { throw 'NSIS 3.10+ not found. Install NSIS or pass -NSIS <path to makensis.exe>.' }
    }
}
$nsisVersion = (& $NSIS /VERSION | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $nsisVersion -notmatch '^v?(\d+\.\d+(?:\.\d+)?)$') {
    throw 'Unable to determine NSIS version.'
}
$parsedVersion = [version]$Matches[1]
if ($parsedVersion.Major -ne 3 -or $parsedVersion -lt [version]'3.10') {
    throw "NSIS 3.10+ is required; found $nsisVersion."
}
Write-Output "Using NSIS $nsisVersion at $NSIS"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = (Resolve-Path -LiteralPath $BuildDirectory).Path
if (-not $OutputFile) { $OutputFile = Join-Path $buildPath 'dLab.Series-Windows-x64-Setup.exe' }
$outputPath = [System.IO.Path]::GetFullPath($OutputFile)
& $NSIS /V2 "/DUI_FILE=$buildPath/Installer/Release/dLabInstallerUI.dll" "/DPACKAGE_DIR=$buildPath/Package/dLab.Series" "/DOUTPUT_FILE=$outputPath" "$repoRoot/resources/windows/installer/dLab.Series.nsi"
if ($LASTEXITCODE -ne 0) { throw "NSIS exited with $LASTEXITCODE" }
Write-Output "Built $outputPath. Installation was not run."
