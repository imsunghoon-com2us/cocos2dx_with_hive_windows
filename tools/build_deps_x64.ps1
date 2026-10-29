# Builds the x64 third-party libraries that cocos2d-x 3.17.2 only ships as x86 prebuilt files.
#   tools\build_deps_x64.ps1 [-Only zlib,libpng] [-Force] [-EngineRoot <dir>] [-WorkRoot <dir>]
# Sources are downloaded to %TEMP%\cocos2dx_deps_x64\dl, built under -WorkRoot (outside the repo),
# and the results are installed into <EngineRoot>\external\...\win64 using the engine's own file names.
# Toolchain: CMake 3.31 + Visual Studio 17 2022 generator, MSVC v141 toolset, x64, Release, /MD.
# Libraries without a usable upstream CMake project use the helper projects in tools\deps-x64\<lib>.
param(
    [string[]]$Only,
    [switch]$Force,
    [string]$EngineRoot = (Join-Path (Split-Path $PSScriptRoot -Parent | Split-Path -Parent) 'cocos2d-x-3.17.2'),
    [string]$WorkRoot = 'C:\Project\cocos2d-deps-x64'
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$E       = Join-Path $EngineRoot 'external'
$Helpers = Join-Path $PSScriptRoot 'deps-x64'
$DlDir   = Join-Path $env:TEMP 'cocos2dx_deps_x64\dl'
$SrcDir  = Join-Path $WorkRoot 'src'
$BldDir  = Join-Path $WorkRoot 'build'
$LogDir  = Join-Path $WorkRoot 'logs'
$Stage   = Join-Path $WorkRoot 'stage'
$TarExe  = Join-Path $env:SystemRoot 'System32\tar.exe'

if (-not (Test-Path (Join-Path $E 'win32-specific'))) { throw "engine external dir not found: $E (run tools\setup_engine.ps1)" }

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $cmake) { $cmake = Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe' }
if (-not (Test-Path $cmake)) { throw 'cmake not found. Run tools\check_env.ps1.' }

# The 64-bit MSBuild reads a wrong Windows SDK root from the 64-bit registry view (LNK1104 ucrtd.lib).
# Point UCRTContentRoot at the 32-bit view KitsRoot10 (same workaround as tools\build.ps1).
$kits32 = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32').OpenSubKey('SOFTWARE\Microsoft\Windows Kits\Installed Roots')
if ($kits32 -and $kits32.GetValue('KitsRoot10')) { $env:UCRTContentRoot = $kits32.GetValue('KitsRoot10') }

# VC tools (dumpbin / lib.exe), x64 host, v141 toolset
$vcRoot = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC'
$vcBin = Get-ChildItem $vcRoot -Directory -Filter '14.16.*' -ErrorAction SilentlyContinue | Select-Object -First 1 |
    ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x64' }
if (-not $vcBin -or -not (Test-Path (Join-Path $vcBin 'lib.exe'))) { throw "MSVC v141 x64 tools not found under $vcRoot" }
$env:PATH = "$vcBin;$env:PATH"
$DumpBin = Join-Path $vcBin 'dumpbin.exe'
$LibExe  = Join-Path $vcBin 'lib.exe'

foreach ($d in $DlDir, $SrcDir, $BldDir, $LogDir, $Stage) { New-Item -ItemType Directory -Force $d | Out-Null }

# ---------------------------------------------------------------- helpers
function Invoke-Native([string]$Exe, [string[]]$ArgList, [string]$Log) {
    $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        "> $Exe $($ArgList -join ' ')" | Out-File -FilePath $Log -Append -Encoding ascii
        & $Exe @ArgList *>> $Log
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $old }
    if ($code -ne 0) {
        $tail = (Get-Content $Log -Tail 25 -ErrorAction SilentlyContinue) -join "`n"
        throw "$(Split-Path $Exe -Leaf) failed ($code), log: $Log`n$tail"
    }
}

function Remove-Tree([string]$Path) { if (Test-Path $Path) { [IO.Directory]::Delete($Path, $true) } }

# download (cached) + extract (cached); returns the source root
function Get-Source([string]$Name, [string]$Url) {
    $zip = Join-Path $DlDir "$Name.zip"
    $dst = Join-Path $SrcDir $Name
    if (-not (Test-Path $zip)) {
        Write-Host "  download $Url"
        Invoke-WebRequest -Uri $Url -OutFile "$zip.part" -UseBasicParsing
        Move-Item "$zip.part" $zip
    }
    if (-not (Test-Path (Join-Path $dst '.extracted'))) {
        Remove-Tree $dst
        New-Item -ItemType Directory -Force $dst | Out-Null
        & $TarExe -xf $zip -C $dst
        if ($LASTEXITCODE -ne 0) { throw "tar failed for $zip" }
        New-Item -ItemType File (Join-Path $dst '.extracted') | Out-Null
    }
    $items = @(Get-ChildItem $dst -Exclude '.extracted')
    if ($items.Count -eq 1 -and $items[0].PSIsContainer) { return $items[0].FullName }
    return $dst
}

# configure + build with the VS generator; returns the build dir
function Build-CMake([string]$Name, [string]$Src, [string[]]$CMakeArgs, [string[]]$Configs = @('Release'), [string[]]$Targets = @(), [string]$BuildName = $Name) {
    $CMakeArgs = @($CMakeArgs | ForEach-Object { $_ -replace '\\', '/' })   # CMake parses backslashes as escapes
    $bd  = Join-Path $BldDir $BuildName
    $log = Join-Path $LogDir "$BuildName.log"
    if ($Force) { Remove-Tree $bd }
    if (-not (Test-Path (Join-Path $bd 'ALL_BUILD.vcxproj'))) {
        Remove-Tree $bd
        if (Test-Path $log) { Remove-Item $log }
        Invoke-Native $cmake (@('-S', $Src, '-B', $bd, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-T', 'v141') + $CMakeArgs) $log
    }
    foreach ($cfg in $Configs) {
        $a = @('--build', $bd, '--config', $cfg)
        foreach ($t in $Targets) { $a += @('--target', $t) }
        $a += @('--', '/m', '/v:minimal')
        Invoke-Native $cmake $a $log
    }
    return $bd
}

function Find-Built([string]$Dir, [string]$Leaf, [string]$Cfg = 'Release') {
    $hit = Get-ChildItem $Dir -Recurse -Filter $Leaf -File | Where-Object { $_.FullName -match "\\$Cfg\\" } | Select-Object -First 1
    if (-not $hit) { throw "built file not found: $Leaf ($Cfg) under $Dir" }
    return $hit.FullName
}

function Install-File([string]$From, [string]$To) {
    New-Item -ItemType Directory -Force (Split-Path $To -Parent) | Out-Null
    Copy-Item $From $To -Force
}

# machine type check: every installed dll/lib must be x64 (8664)
function Test-X64([string]$File) {
    $out = & $DumpBin /headers $File
    if (-not ($out | Select-String '8664 machine \(x64\)' -Quiet)) { throw "not an x64 binary: $File" }
}

function Get-Stage([string]$Name) { Join-Path $Stage $Name }

# ---------------------------------------------------------------- recipes
function Build-Zlib($src, $lib) {
    $inc = Join-Path $Helpers 'rename_outputs.cmake'
    $bd = Build-CMake zlib $src @('-DZLIB_BUILD_EXAMPLES=OFF', "-DCMAKE_PROJECT_INCLUDE=$inc", '-DRENAME_TARGETS=zlib', '-DRENAME_PREFIX=', '-DRENAME_SUFFIX=1') -Targets @('zlib')
    # zlib 1.3.1's CMake emits zlib.dll; rename_outputs.cmake makes it zlib1.dll + zlib1.lib (import lib)
    $out = Join-Path $E 'win32-specific\zlib\prebuilt\win64'
    Install-File (Find-Built $bd 'zlib1.dll') (Join-Path $out 'zlib1.dll')
    Install-File (Find-Built $bd 'zlib1.lib') (Join-Path $out 'libzlib.lib')
    # staging for libpng's find_package(ZLIB)
    $st = Get-Stage 'zlib'; New-Item -ItemType Directory -Force "$st\include", "$st\lib" | Out-Null
    Copy-Item (Join-Path $src 'zlib.h') "$st\include" -Force
    Copy-Item (Join-Path $bd 'zconf.h') "$st\include" -Force
    Copy-Item (Find-Built $bd 'zlib1.lib') "$st\lib\zlib.lib" -Force
}

function Build-Libpng($src, $lib) {
    $st = Get-Stage 'zlib'
    if (-not (Test-Path "$st\lib\zlib.lib")) { Write-Host '  (staging zlib first)'; Build-Zlib (Get-Source 'zlib' $script:Libs['zlib'].Url) $null }
    $bd = Build-CMake libpng $src @('-DPNG_SHARED=OFF', '-DPNG_STATIC=ON', '-DPNG_TESTS=OFF', "-DZLIB_INCLUDE_DIR=$st\include", "-DZLIB_LIBRARY=$st\lib\zlib.lib")
    $built = Get-ChildItem $bd -Recurse -Filter '*.lib' -File | Where-Object { $_.FullName -match '\\Release\\' -and $_.Name -match 'png' } | Select-Object -First 1
    if (-not $built) { throw 'libpng static lib not found' }
    Install-File $built.FullName (Join-Path $E 'png\prebuilt\win64\libpng.lib')
}

function Build-Jpeg($src, $lib) {
    $bd = Build-CMake jpeg (Join-Path $Helpers 'jpeg') @("-DJPEG_SRC=$src", "-DENGINE_JPEG_INC=$E\jpeg\include\win32")
    Install-File (Find-Built $bd 'libjpeg.lib') (Join-Path $E 'jpeg\prebuilt\win64\libjpeg.lib')
}

function Build-Tiff($src, $lib) {
    $bd = Build-CMake tiff (Join-Path $Helpers 'tiff') @("-DTIFF_SRC=$src", "-DENGINE_TIFF_INC=$E\tiff\include\win32")
    Install-File (Find-Built $bd 'libtiff.lib') (Join-Path $E 'tiff\prebuilt\win64\libtiff.lib')
}

function Build-Webp($src, $lib) {
    $bd = Build-CMake webp (Join-Path $Helpers 'webp') @("-DWEBP_SRC=$src")
    Install-File (Find-Built $bd 'libwebp.lib') (Join-Path $E 'webp\prebuilt\win64\libwebp.lib')
}

function Build-Freetype($src, $lib) {
    $bd = Build-CMake freetype (Join-Path $Helpers 'freetype') @("-DFT_SRC=$src", "-DENGINE_FT_INC=$E\freetype2\include\win32\freetype2")
    Install-File (Find-Built $bd 'freetype.lib') (Join-Path $E 'freetype2\prebuilt\win64\freetype.lib')
}

function Build-Glfw3($src, $lib) {
    $bd = Build-CMake glfw3 $src @('-DGLFW_BUILD_EXAMPLES=OFF', '-DGLFW_BUILD_TESTS=OFF', '-DGLFW_BUILD_DOCS=OFF', '-DUSE_MSVC_RUNTIME_LIBRARY_DLL=ON', '-DBUILD_SHARED_LIBS=OFF') -Targets @('glfw')
    Install-File (Find-Built $bd 'glfw3.lib') (Join-Path $E 'glfw3\prebuilt\win64\glfw3.lib')
}

function Build-Glew($src, $lib) {
    $bd = Build-CMake glew (Join-Path $src 'build\cmake') @('-DBUILD_UTILS=OFF') -Targets @('glew')
    $out = Join-Path $E 'win32-specific\gles\prebuilt\win64'
    Install-File (Find-Built $bd 'glew32.dll') (Join-Path $out 'glew32.dll')
    Install-File (Find-Built $bd 'glew32.lib') (Join-Path $out 'glew32.lib')
}

function Build-Sqlite3($src, $lib) {
    $bd = Build-CMake sqlite3 (Join-Path $Helpers 'sqlite3') @("-DSQLITE_SRC=$src")
    $out = Join-Path $E 'sqlite3\libraries\win64'
    Install-File (Find-Built $bd 'sqlite3.dll') (Join-Path $out 'sqlite3.dll')
    Install-File (Find-Built $bd 'sqlite3.lib') (Join-Path $out 'sqlite3.lib')
}

function Build-Chipmunk($src, $lib) {
    $bd = Build-CMake chipmunk (Join-Path $Helpers 'chipmunk') @("-DCHIPMUNK_SRC=$src") -Configs @('Debug', 'Release')
    $out = Join-Path $E 'chipmunk\prebuilt\win64'
    Install-File (Find-Built $bd 'libchipmunk.lib' 'Debug')   (Join-Path $out 'debug-lib\libchipmunk.lib')
    Install-File (Find-Built $bd 'libchipmunk.lib' 'Release') (Join-Path $out 'release-lib\libchipmunk.lib')
}

function Build-OpenAL($src, $lib) {
    $bd = Build-CMake openal $src @('-DLIBTYPE=SHARED', '-DALSOFT_UTILS=OFF', '-DALSOFT_EXAMPLES=OFF', '-DALSOFT_TESTS=OFF', '-DALSOFT_INSTALL=OFF', '-DALSOFT_NO_CONFIG_UTIL=ON') -Targets @('OpenAL')
    $out = Join-Path $E 'win32-specific\OpenalSoft\prebuilt\win64'
    Install-File (Find-Built $bd 'OpenAL32.dll') (Join-Path $out 'OpenAL32.dll')
    Install-File (Find-Built $bd 'OpenAL32.lib') (Join-Path $out 'OpenAL32.lib')
}

# ogg + vorbis: shared, outputs renamed to libogg / libvorbis / libvorbisfile (dll and import lib)
function Build-Ogg($src, $lib) {
    $ren = Join-Path $Helpers 'rename_outputs.cmake'
    $bd = Build-CMake ogg $src @('-DBUILD_SHARED_LIBS=ON', '-DBUILD_TESTING=OFF', '-DINSTALL_DOCS=OFF', "-DCMAKE_PROJECT_INCLUDE=$ren", '-DRENAME_TARGETS=ogg', '-DRENAME_PREFIX=lib') -Targets @('ogg')
    $st = Get-Stage 'ogg'; New-Item -ItemType Directory -Force "$st\include\ogg", "$st\lib" | Out-Null
    Copy-Item (Join-Path $src 'include\ogg\*.h') "$st\include\ogg" -Force
    Copy-Item (Get-ChildItem $bd -Recurse -Filter 'config_types.h' | Select-Object -First 1).FullName "$st\include\ogg" -Force
    Copy-Item (Find-Built $bd 'libogg.lib') "$st\lib\libogg.lib" -Force
    $out = Join-Path $E 'win32-specific\OggDecoder\prebuilt\win64'
    Install-File (Find-Built $bd 'libogg.dll') (Join-Path $out 'libogg.dll')
    Install-File (Find-Built $bd 'libogg.lib') (Join-Path $out 'libogg.lib')
}

function Build-Vorbis($src, $lib) {
    $st = Get-Stage 'ogg'
    if (-not (Test-Path "$st\lib\libogg.lib")) { Write-Host '  (staging ogg first)'; Build-Ogg (Get-Source 'ogg' $script:Libs['ogg'].Url) $null }
    $ren = Join-Path $Helpers 'rename_outputs.cmake'
    $bd = Build-CMake vorbis $src @('-DBUILD_SHARED_LIBS=ON', '-DBUILD_TESTING=OFF', "-DOGG_INCLUDE_DIR=$st\include", "-DOGG_LIBRARY=$st\lib\libogg.lib", "-DCMAKE_PROJECT_INCLUDE=$ren", '-DRENAME_TARGETS=vorbis;vorbisfile', '-DRENAME_PREFIX=lib') -Targets @('vorbis', 'vorbisfile')
    $out = Join-Path $E 'win32-specific\OggDecoder\prebuilt\win64'
    foreach ($n in 'libvorbis', 'libvorbisfile') {
        Install-File (Find-Built $bd "$n.dll") (Join-Path $out "$n.dll")
        Install-File (Find-Built $bd "$n.lib") (Join-Path $out "$n.lib")
    }
}

# mpg123: official prebuilt x64 DLL; import lib generated from its exports with LIBRARY libmpg123.dll
function Build-Mpg123($src, $lib) {
    $out = Join-Path $E 'win32-specific\MP3Decoder\prebuilt\win64'
    New-Item -ItemType Directory -Force $out | Out-Null
    Copy-Item (Join-Path $src 'libmpg123-0.dll') (Join-Path $out 'libmpg123.dll') -Force
    $names = & $DumpBin /exports (Join-Path $out 'libmpg123.dll') |
        ForEach-Object { if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)') { $Matches[1] } }
    if (-not ($names -contains 'mpg123_init')) { throw 'mpg123_init not exported' }
    $def = Join-Path $WorkRoot 'build\libmpg123.def'
    New-Item -ItemType Directory -Force (Split-Path $def -Parent) | Out-Null
    (@('LIBRARY libmpg123.dll', 'EXPORTS') + $names) | Set-Content $def -Encoding ascii
    Invoke-Native $LibExe @("/DEF:$def", '/MACHINE:X64', "/OUT:$(Join-Path $out 'libmpg123.lib')") (Join-Path $LogDir 'mpg123.log')
}

# win-iconv: iconv.def exports both iconv_* and libiconv_* names (the engine's iconv.h maps to libiconv_*)
function Build-Iconv($src, $lib) {
    $bd = Build-CMake iconv $src @('-DBUILD_STATIC=OFF', '-DBUILD_SHARED=ON', '-DBUILD_EXECUTABLE=OFF', '-DBUILD_TEST=OFF') -Targets @('iconv')
    $out = Join-Path $E 'win32-specific\icon\prebuilt\win64'
    Install-File (Find-Built $bd 'iconv.dll') (Join-Path $out 'iconv.dll')
    Install-File (Find-Built $bd 'iconv.lib') (Join-Path $out 'libiconv.lib')
}

# ---------------------------------------------------------------- table
$script:Libs = [ordered]@{
    'zlib'     = @{ Version = '1.3.1';    Url = 'https://github.com/madler/zlib/archive/refs/tags/v1.3.1.zip';                      Recipe = ${function:Build-Zlib};
                    Outputs = 'win32-specific\zlib\prebuilt\win64\zlib1.dll', 'win32-specific\zlib\prebuilt\win64\libzlib.lib' }
    'libpng'   = @{ Version = '1.6.16';   Url = 'https://github.com/glennrp/libpng/archive/refs/tags/v1.6.16.zip';                 Recipe = ${function:Build-Libpng};
                    Outputs = 'png\prebuilt\win64\libpng.lib' }
    'jpeg'     = @{ Version = 'IJG 9';    Url = 'https://www.ijg.org/files/jpegsr9.zip';                                            Recipe = ${function:Build-Jpeg};
                    Outputs = 'jpeg\prebuilt\win64\libjpeg.lib' }
    'tiff'     = @{ Version = '4.0.3';    Url = 'https://download.osgeo.org/libtiff/old/tiff-4.0.3.zip';                            Recipe = ${function:Build-Tiff};
                    Outputs = 'tiff\prebuilt\win64\libtiff.lib' }
    'webp'     = @{ Version = '0.4.4';    Url = 'https://github.com/webmproject/libwebp/archive/refs/tags/v0.4.4.zip';              Recipe = ${function:Build-Webp};
                    Outputs = 'webp\prebuilt\win64\libwebp.lib' }
    'freetype' = @{ Version = '2.5.5';    Url = 'https://github.com/freetype/freetype/archive/refs/tags/VER-2-5-5.zip';             Recipe = ${function:Build-Freetype};
                    Outputs = 'freetype2\prebuilt\win64\freetype.lib' }
    'glfw3'    = @{ Version = '3.2.1';    Url = 'https://github.com/glfw/glfw/archive/refs/tags/3.2.1.zip';                         Recipe = ${function:Build-Glfw3};
                    Outputs = 'glfw3\prebuilt\win64\glfw3.lib' }
    'glew'     = @{ Version = '2.2.0';    Url = 'https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip';   Recipe = ${function:Build-Glew};
                    Outputs = 'win32-specific\gles\prebuilt\win64\glew32.dll', 'win32-specific\gles\prebuilt\win64\glew32.lib' }
    'sqlite3'  = @{ Version = '3.45.3';   Url = 'https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip';                      Recipe = ${function:Build-Sqlite3};
                    Outputs = 'sqlite3\libraries\win64\sqlite3.dll', 'sqlite3\libraries\win64\sqlite3.lib' }
    'chipmunk' = @{ Version = '7.0.1';    Url = 'https://github.com/slembcke/Chipmunk2D/archive/refs/tags/Chipmunk-7.0.1.zip';      Recipe = ${function:Build-Chipmunk};
                    Outputs = 'chipmunk\prebuilt\win64\debug-lib\libchipmunk.lib', 'chipmunk\prebuilt\win64\release-lib\libchipmunk.lib' }
    'openal'   = @{ Version = '1.19.1';   Url = 'https://github.com/kcat/openal-soft/archive/refs/tags/openal-soft-1.19.1.zip';     Recipe = ${function:Build-OpenAL};
                    Outputs = 'win32-specific\OpenalSoft\prebuilt\win64\OpenAL32.dll', 'win32-specific\OpenalSoft\prebuilt\win64\OpenAL32.lib' }
    'ogg'      = @{ Version = '1.3.5';    Url = 'https://github.com/xiph/ogg/archive/refs/tags/v1.3.5.zip';                         Recipe = ${function:Build-Ogg};
                    Outputs = 'win32-specific\OggDecoder\prebuilt\win64\libogg.dll', 'win32-specific\OggDecoder\prebuilt\win64\libogg.lib' }
    'vorbis'   = @{ Version = '1.3.7';    Url = 'https://github.com/xiph/vorbis/archive/refs/tags/v1.3.7.zip';                      Recipe = ${function:Build-Vorbis};
                    Outputs = 'win32-specific\OggDecoder\prebuilt\win64\libvorbis.dll', 'win32-specific\OggDecoder\prebuilt\win64\libvorbis.lib',
                              'win32-specific\OggDecoder\prebuilt\win64\libvorbisfile.dll', 'win32-specific\OggDecoder\prebuilt\win64\libvorbisfile.lib' }
    'mpg123'   = @{ Version = '1.32.10';  Url = 'https://www.mpg123.de/download/win64/1.32.10/mpg123-1.32.10-x86-64.zip';            Recipe = ${function:Build-Mpg123};
                    Outputs = 'win32-specific\MP3Decoder\prebuilt\win64\libmpg123.dll', 'win32-specific\MP3Decoder\prebuilt\win64\libmpg123.lib' }
    'iconv'    = @{ Version = 'win-iconv 0.0.8'; Url = 'https://github.com/win-iconv/win-iconv/archive/refs/tags/v0.0.8.zip';       Recipe = ${function:Build-Iconv};
                    Outputs = 'win32-specific\icon\prebuilt\win64\iconv.dll', 'win32-specific\icon\prebuilt\win64\libiconv.lib' }
}

# ---------------------------------------------------------------- run
$names = @($script:Libs.Keys)
if ($Only) {
    $req = @($Only | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    $bad = @($req | Where-Object { $names -notcontains $_ })
    if ($bad) { throw "unknown lib(s): $($bad -join ', '). Known: $($names -join ', ')" }
    $names = @($names | Where-Object { $req -contains $_ })
}

$results = [ordered]@{}
foreach ($n in $names) {
    $lib = $script:Libs[$n]
    $outs = @($lib.Outputs | ForEach-Object { Join-Path $E $_ })
    Write-Host "[$n $($lib.Version)]"
    if (-not $Force -and -not ($outs | Where-Object { -not (Test-Path $_) })) {
        Write-Host '  skip (outputs exist)'; $results[$n] = 'SKIP'; continue
    }
    try {
        $src = Get-Source $n $lib.Url
        & $lib.Recipe $src $lib
        foreach ($o in $outs) {
            if (-not (Test-Path $o)) { throw "missing output: $o" }
            Test-X64 $o
        }
        $results[$n] = 'OK'; Write-Host '  OK'
    } catch {
        $results[$n] = 'FAIL'
        Write-Host "  FAIL: $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host ''
Write-Host 'Summary'
$results.GetEnumerator() | ForEach-Object { '{0,-10} {1}' -f $_.Key, $_.Value } | Write-Host
if ($results.Values -contains 'FAIL') { exit 1 }
