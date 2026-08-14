[CmdletBinding()]
param([string]$RepositoryRoot = '')

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
}

$generator = Join-Path $RepositoryRoot 'tools\Generate-M485EContracts.ps1'
$errors = Join-Path $RepositoryRoot 'spec\ErrorCodes.csv'
$constants = Join-Path $RepositoryRoot 'spec\PublicConstants.csv'
$cpp = Join-Path $RepositoryRoot 'generated\cpp\M485E_Contracts.generated.h'
$capl = Join-Path $RepositoryRoot 'generated\capl\M485E_Contracts.generated.cin'

function Invoke-Generator {
    param(
        [string]$Mode,
        [string]$ErrorPath,
        [string]$ConstantPath,
        [string]$CppPath,
        [string]$CaplPath,
        [bool]$ShouldSucceed
    )

    $failure = $null
    try {
        & $generator -Mode $Mode -RepositoryRoot $RepositoryRoot `
            -ErrorCodesPath $ErrorPath -PublicConstantsPath $ConstantPath `
            -CppOutputPath $CppPath -CaplOutputPath $CaplPath *> $null
    }
    catch {
        $failure = $_
    }

    if ($ShouldSucceed -and $null -ne $failure) {
        throw $failure
    }
    if (-not $ShouldSucceed -and $null -eq $failure) {
        throw "Generator unexpectedly accepted an invalid contract input."
    }
}

Invoke-Generator 'Check' $errors $constants $cpp $capl $true

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("M485E.ContractTests." + [Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $tempRoot)
try {
    $errorRows = @(Import-Csv -LiteralPath $errors -Encoding UTF8)
    $constantRows = @(Import-Csv -LiteralPath $constants -Encoding UTF8)
    $generatedCppText = [IO.File]::ReadAllText($cpp)
    $generatedCaplText = [IO.File]::ReadAllText($capl)

    if ($generatedCaplText -match "(?<!`r)`n" -or $generatedCaplText -notmatch "`r`n") {
        throw "Generated CAPL contract must use CRLF line endings."
    }
    if ($generatedCaplText.Contains("`t")) {
        throw "Generated CAPL contract must not contain tab characters."
    }

    foreach ($constantRow in $constantRows) {
        if (-not $generatedCppText.Contains("$($constantRow.Symbol) = $($constantRow.Value);")) {
            throw "C++ generated contract is missing $($constantRow.Symbol)."
        }
        if (-not $generatedCaplText.Contains("$($constantRow.Symbol) = $($constantRow.Value);")) {
            throw "CAPL generated contract is missing $($constantRow.Symbol)."
        }
    }
    foreach ($errorRow in $errorRows) {
        if (-not $generatedCppText.Contains("$($errorRow.Symbol) = $($errorRow.Code);")) {
            throw "C++ generated contract is missing $($errorRow.Symbol)."
        }
        if (-not $generatedCaplText.Contains(
            "$($errorRow.Symbol) = $($errorRow.Code); // $($errorRow.Message_Key)"
        )) {
            throw "CAPL generated contract is missing $($errorRow.Symbol) or its Message Key."
        }
        $messageSymbol = $errorRow.Symbol.Replace('M485E_ERR_', 'M485E_MESSAGE_KEY_')
        $messageLength = $errorRow.Message_Key.Length + 1
        if (-not $generatedCaplText.Contains(
            "char $messageSymbol`[$messageLength`] = `"$($errorRow.Message_Key)`";"
        )) {
            throw "CAPL generated contract is missing the usable Message Key for $($errorRow.Symbol)."
        }
    }

    if ($errorRows.Count -ne 91) {
        throw "The v0.4 error contract must contain exactly 91 definitions."
    }
    $baselineText = (($errorRows | ForEach-Object {
        '{0}|{1}|{2}|{3}|{4}' -f $_.Code, $_.Symbol, $_.Severity, $_.Message_Key, $_.Meaning
    }) -join "`n")
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $baselineHash = ([BitConverter]::ToString(
            $sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($baselineText))
        )).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
    if ($baselineHash -cne '44982a3b07f94690ae3f2e19477f025a88085247cff3ed5c029fbcc7f0e97b50') {
        throw "A released v0.4 error definition was changed or reordered."
    }

    if ($constantRows.Count -ne 189) {
        throw "The v0.4 public constant contract must contain exactly 189 definitions."
    }
    $constantBaselineText = (($constantRows | ForEach-Object {
        '{0}|{1}|{2}|{3}' -f $_.Group, $_.Symbol, $_.Value, $_.Meaning
    }) -join "`n")
    $constantSha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $constantBaselineHash = ([BitConverter]::ToString(
            $constantSha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($constantBaselineText))
        )).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $constantSha256.Dispose()
    }
    if ($constantBaselineHash -cne '01813a8644b651c09a5a0620a35bcd032b3ca88111e656ee07a155d15c37d508') {
        throw "A v0.4 public constant definition was changed or reordered."
    }

    $extendedErrors = Join-Path $tempRoot 'ExtendedErrors.csv'
    $extension = [PSCustomObject]@{
        Code = '-110'
        Symbol = 'M485E_ERR_TEST_EXTENSION'
        Severity = 'Error'
        Message_Key = 'ERR_TEST_EXTENSION'
        Meaning = 'Generator extension test only'
    }
    @($errorRows + $extension) | Export-Csv -LiteralPath $extendedErrors -Encoding UTF8 -NoTypeInformation
    $extendedCpp = Join-Path $tempRoot 'Extended.h'
    Invoke-Generator 'Generate' $extendedErrors $constants $extendedCpp `
        (Join-Path $tempRoot 'Extended.cin') $true
    $expectedExtendedCount = $errorRows.Count + 1
    if (-not ([IO.File]::ReadAllText($extendedCpp).Contains(
        "M485E_ERROR_CODE_COUNT = $expectedExtendedCount"
    ))) {
        throw "A valid appended error code was not generated."
    }

    $badError = Join-Path $tempRoot 'DuplicateError.csv'
    @($errorRows + $errorRows[0]) | Export-Csv -LiteralPath $badError -Encoding UTF8 -NoTypeInformation
    Invoke-Generator 'Generate' $badError $constants (Join-Path $tempRoot 'a.h') (Join-Path $tempRoot 'a.cin') $false

    $badSeverity = Join-Path $tempRoot 'BadSeverity.csv'
    $errorRows[0].Severity = 'Invalid'
    $errorRows | Export-Csv -LiteralPath $badSeverity -Encoding UTF8 -NoTypeInformation
    Invoke-Generator 'Generate' $badSeverity $constants (Join-Path $tempRoot 'b.h') (Join-Path $tempRoot 'b.cin') $false

    $badConstant = Join-Path $tempRoot 'DuplicateConstant.csv'
    @($constantRows + $constantRows[0]) | Export-Csv -LiteralPath $badConstant -Encoding UTF8 -NoTypeInformation
    Invoke-Generator 'Generate' $errors $badConstant (Join-Path $tempRoot 'c.h') (Join-Path $tempRoot 'c.cin') $false

    $badNumber = Join-Path $tempRoot 'BadNumber.csv'
    $constantRows[0].Value = 'not-a-number'
    $constantRows | Export-Csv -LiteralPath $badNumber -Encoding UTF8 -NoTypeInformation
    Invoke-Generator 'Generate' $errors $badNumber (Join-Path $tempRoot 'd.h') (Join-Path $tempRoot 'd.cin') $false

    $staleCpp = Join-Path $tempRoot 'Stale.h'
    $validCapl = Join-Path $tempRoot 'Valid.cin'
    Invoke-Generator 'Generate' $errors $constants $staleCpp $validCapl $true
    Add-Content -LiteralPath $staleCpp -Value '// stale'
    Invoke-Generator 'Check' $errors $constants $staleCpp $validCapl $false
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

Write-Host 'M485E contract generator tests PASS.'
