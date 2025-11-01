param(
  [switch]$Tests = $false,
  [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')]
  [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'

$buildDir = Join-Path -Path $PSScriptRoot -ChildPath '..' | Resolve-Path | ForEach-Object { Join-Path $_ 'build' }
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

pushd $buildDir
try {
  cmake -S .. -B . -DENABLE_TESTS:BOOL=$($Tests.IsPresent) | Write-Host
  cmake --build . --config $Config -j | Write-Host
}
finally {
  popd
}

Write-Host "Build completed. Binaries in: $buildDir/bin or $buildDir/$Config"
