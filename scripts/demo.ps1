param(
  [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')]
  [string]$Config = 'Release',
  [switch]$VerboseOutput
)

$ErrorActionPreference = 'Stop'

function Resolve-BinPath {
  param(
    [Parameter(Mandatory)] [string]$BuildDir,
    [Parameter(Mandatory)] [string]$Name
  )
  $bd = [string]$BuildDir
  $candidates = @(
    [System.IO.Path]::Combine($bd, 'bin', "${Name}.exe"),
    [System.IO.Path]::Combine($bd, $Config, "${Name}.exe"),
    [System.IO.Path]::Combine($bd, 'bin', $Name),
    [System.IO.Path]::Combine($bd, $Config, $Name)
  )
  foreach ($p in $candidates) { if (Test-Path $p) { return $p } }
  return $null
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDir = (Join-Path -Path $root -ChildPath 'build')
$testDir = (Join-Path -Path $root -ChildPath 'build/tests')

Write-Host "==> Building (with tests)"
& (Join-Path $PSScriptRoot 'build.ps1') -Config $Config -Tests | Out-Null

Write-Host "==> Running tests"
pushd $buildDir
try {
    $runTests = Resolve-BinPath -BuildDir $testDir -Name 'runTests'
    if (-not $runTests) { throw "runTests not found and ctest unavailable" }
    & $runTests
}
finally { popd }

$gwatch = Resolve-BinPath -BuildDir $buildDir -Name 'gwatch'
if (-not $gwatch) { throw "gwatch binary not found in $buildDir" }
$debugee = Resolve-BinPath -BuildDir $testDir -Name 'gwatch_debuggee_app'
if (-not $debugee) { throw "sample debugee not found in $testDir" }
$stress = Resolve-BinPath -BuildDir $testDir -Name 'gwatch_debuggee_stress'
if (-not $stress) { throw "stress test debugee not found in $testDir" }

Write-Host "==> Showing sample debugee source (tests/debugee/app.cpp)"
Get-Content (Join-Path -Path $root -ChildPath 'tests/debugee/app.cpp') | Write-Host

Write-Host "==> Quick demo of gwatch"
& $gwatch --var g_counter --exec $debugee

Write-Host "==> Stress timing: bare (20000 iterations)"
$tBare = Measure-Command { & $stress | Out-Null }
Write-Host ("   bare:   {0:N3} s" -f $tBare.TotalSeconds)

Write-Host "==> Stress timing: gwatch + stress test (20000 iterations)"
$tWatch = Measure-Command { & $gwatch --var g_counter --exec $stress | Out-Null }
Write-Host ("   gwatch: {0:N3} s" -f $tWatch.TotalSeconds)

$ratio = if ($tBare.TotalSeconds -gt 0) { $tWatch.TotalSeconds / $tBare.TotalSeconds } else { [double]::NaN }
Write-Host ("   ratio:  {0:N2}x" -f $ratio)

if ($VerboseOutput) {
  Write-Host "==> Env"; Write-Host ($PSVersionTable | Out-String)
}
