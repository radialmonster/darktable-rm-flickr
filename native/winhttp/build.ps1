param(
  [string]$LuaRoot = "C:\Users\Owner\AppData\Local\Programs\Lua",
  [string]$LuaInclude,
  [string]$LuaLib,
  [string]$OutDir = "C:\Users\Owner\AppData\Local\darktable\lua\dtrmflickr",
  [string]$OutName = "dtrmflickr_winhttp.dll"
)

$ErrorActionPreference = "Stop"

if (!$LuaInclude) { $LuaInclude = Join-Path $LuaRoot "include" }
if (!$LuaLib) { $LuaLib = Join-Path $LuaRoot "lib\lua54.lib" }
$out = Join-Path $OutDir $OutName
$source = Join-Path $PSScriptRoot "dtrmflickr_winhttp.c"

if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
  throw "cl.exe was not found. Install Visual Studio Build Tools with the C++ workload, then run this from a Developer PowerShell."
}
if (!(Test-Path $LuaInclude)) {
  throw "Lua include directory not found: $LuaInclude"
}
if (!(Test-Path $LuaLib)) {
  throw "Lua import library not found: $LuaLib"
}
if (!(Test-Path $source)) {
  throw "Source file not found: $source"
}
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

cl.exe /nologo /LD /O2 /MT /I"$LuaInclude" "$source" /link /OUT:"$out" "$LuaLib" winhttp.lib
Write-Host "Built $out"
