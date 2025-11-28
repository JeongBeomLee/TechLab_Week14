# ThirdParty 라이브러리 자동 다운로드
# Google Drive에서 lib.zip을 다운로드하고 ThirdParty/lib에 압축 해제

param(
    [string]$ProjectRoot = (Split-Path $PSScriptRoot -Parent | Split-Path -Parent)
)

$ErrorActionPreference = "Stop"

$LibDir = Join-Path $ProjectRoot "ThirdParty\lib"
$PythonDir = Join-Path $ProjectRoot "ThirdParty\Python"
$PythonExe = Join-Path $PythonDir "python.exe"

# Google Drive 파일 ID (lib.zip)
$LibFileId = "1KntcGe_4ONi1wb1dOnqjbUYr-G-mIC8h"

# lib 폴더가 이미 존재하고 내용이 있으면 스킵
if (Test-Path $LibDir) {
    $libContents = Get-ChildItem -Path $LibDir -Directory
    if ($libContents.Count -gt 0) {
        Write-Host "[OK] Libraries already exist: $LibDir"
        exit 0
    }
}

Write-Host "[Setup] Downloading ThirdParty libraries..."

# Python 환경 확인
if (-not (Test-Path $PythonExe)) {
    Write-Host "[Setup] Python not found, running EnsurePython.ps1..."
    & "$PSScriptRoot\EnsurePython.ps1"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[ERROR] Failed to setup Python"
        exit 1
    }
}

# gdown 설치 확인
Write-Host "[Check] Ensuring gdown is installed..."
& $PythonExe -m pip install gdown --quiet --no-warn-script-location 2>&1 | Out-Null

try {
    # 임시 zip 파일 경로
    $ZipFile = Join-Path $env:TEMP "mundi_lib.zip"

    # lib 디렉토리 생성
    New-Item -ItemType Directory -Force -Path $LibDir | Out-Null

    # gdown으로 다운로드
    Write-Host "[Download] Downloading lib.zip from Google Drive..."
    $gdownScript = @"
import gdown
import sys

file_id = '$LibFileId'
output = r'$ZipFile'
url = f'https://drive.google.com/uc?id={file_id}'

try:
    gdown.download(url, output, quiet=False)
    print('[OK] Download complete')
except Exception as e:
    print(f'[ERROR] {e}', file=sys.stderr)
    sys.exit(1)
"@

    $gdownScript | & $PythonExe -
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[ERROR] Failed to download lib.zip"
        exit 1
    }

    # 압축 해제
    Write-Host "[Extract] Extracting to $LibDir"
    Expand-Archive -Path $ZipFile -DestinationPath $LibDir -Force

    # lib.zip 내부에 lib 폴더가 있는 경우 처리
    $innerLibDir = Join-Path $LibDir "lib"
    if (Test-Path $innerLibDir) {
        Write-Host "[Organize] Moving contents from inner lib folder..."
        Get-ChildItem -Path $innerLibDir | Move-Item -Destination $LibDir -Force
        Remove-Item $innerLibDir -Force -Recurse
    }

    # 임시 파일 정리
    Remove-Item $ZipFile -Force -ErrorAction SilentlyContinue

    Write-Host "[OK] Libraries setup complete: $LibDir"
    exit 0
}
catch {
    Write-Error "[ERROR] Failed to setup libraries: $_"
    exit 1
}
