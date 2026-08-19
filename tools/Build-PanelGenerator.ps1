param(
    [string]$PythonPath = '',
    [switch]$SkipInstall
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$venvRoot = Join-Path $root 'build\panel-generator-venv'
$venvPython = Join-Path $venvRoot 'Scripts\python.exe'
$entryPoint = Join-Path $root 'tools\panel_generator\Panel_Generator.py'
$requirements = Join-Path $root 'tools\panel_generator\requirements-build.txt'
$workPath = Join-Path $root 'build\panel-generator-pyinstaller'
$specPath = Join-Path $root 'build\panel-generator-spec'
$outputPath = Join-Path $root 'dist'
$tclStage = Join-Path $root 'build\panel-generator-tcl'

function Remove-GeneratorBuildDirectory {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $buildRoot = (Resolve-Path -LiteralPath (Join-Path $root 'build')).Path
    if (-not $resolved.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a path outside the repository build directory: $resolved"
    }
    Get-ChildItem -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Attributes = [IO.FileAttributes]::Normal }
    (Get-Item -LiteralPath $resolved -Force).Attributes = [IO.FileAttributes]::Normal
    Remove-Item -LiteralPath $resolved -Recurse -Force
}

if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    if ([string]::IsNullOrWhiteSpace($PythonPath)) {
        throw 'Project Panel Generator venv is missing. Supply -PythonPath with the bundled workspace Python.'
    }
    & $PythonPath -m venv $venvRoot
    if ($LASTEXITCODE -ne 0) { throw 'Failed to create the Panel Generator build venv.' }
}

if (-not $SkipInstall) {
    & $venvPython -m pip install --disable-pip-version-check -r $requirements
    if ($LASTEXITCODE -ne 0) { throw 'Failed to install pinned Panel Generator build dependencies.' }
}

$pyvenv = Get-Content -LiteralPath (Join-Path $venvRoot 'pyvenv.cfg')
$homeLine = $pyvenv | Where-Object { $_ -match '^home\s*=' } | Select-Object -First 1
if ($null -eq $homeLine) { throw 'Unable to locate the base Python home from pyvenv.cfg.' }
$basePython = ($homeLine -split '=', 2)[1].Trim()
$tclSource = Join-Path $basePython 'tcl\tcl8.6'
$tkSource = Join-Path $basePython 'tcl\tk8.6'
if (-not (Test-Path -LiteralPath (Join-Path $tclSource 'init.tcl') -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $tkSource 'tk.tcl') -PathType Leaf)) {
    throw 'The selected Python runtime does not contain a complete tkinter Tcl/Tk library.'
}
if (Test-Path -LiteralPath $tclStage) { Remove-Item -LiteralPath $tclStage -Recurse -Force }
New-Item -ItemType Directory -Path $tclStage -Force | Out-Null
Copy-Item -LiteralPath $tclSource -Destination $tclStage -Recurse -Force
Copy-Item -LiteralPath $tkSource -Destination $tclStage -Recurse -Force
$previousTclLibrary = $env:TCL_LIBRARY
$previousTkLibrary = $env:TK_LIBRARY
$env:TCL_LIBRARY = Join-Path $tclStage 'tcl8.6'
$env:TK_LIBRARY = Join-Path $tclStage 'tk8.6'
Remove-GeneratorBuildDirectory $workPath
Remove-GeneratorBuildDirectory $specPath

try {
    & $venvPython -m PyInstaller --noconfirm --clean --onefile `
        --name Panel_Generator `
        --paths (Join-Path $root 'tools') `
        --workpath $workPath `
        --specpath $specPath `
        --distpath $outputPath `
        $entryPoint
    if ($LASTEXITCODE -ne 0) { throw 'Panel Generator one-file build failed.' }
}
finally {
    $env:TCL_LIBRARY = $previousTclLibrary
    $env:TK_LIBRARY = $previousTkLibrary
}

$exe = Join-Path $outputPath 'Panel_Generator.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Panel Generator executable is missing: $exe"
}
$smokeTemp = Join-Path $root 'build\panel-generator-runtime-smoke'
if (-not (Test-Path -LiteralPath $smokeTemp)) {
    New-Item -ItemType Directory -Path $smokeTemp -Force | Out-Null
}
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
try {
    $env:TEMP = $smokeTemp
    $env:TMP = $smokeTemp
    & $exe --tk-smoke
    if ($LASTEXITCODE -ne 0) { throw 'Panel Generator tkinter smoke test failed.' }
}
finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
}
Write-Output $exe
