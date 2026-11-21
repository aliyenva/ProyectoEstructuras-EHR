# compilar.ps1  â€“  Script de compilacion para MinGW (MSYS2 ucrt64)
# Uso: .\compilar.ps1

$GPP    = "C:\msys64\ucrt64\bin\g++.exe"
$SRC    = "src\main.cpp"
$OUT    = "build\ehr_sistema.exe"
$INC    = "-Iinclude"
$LIBS   = "-lwinhttp -lws2_32"
$FLAGS  = "-std=c++17 -Wall -O2"

Write-Host "`n  [Build] Compilando EHR Sistema..." -ForegroundColor Cyan

# Crear carpeta build si no existe
if (-not (Test-Path "build")) { New-Item -ItemType Directory -Path "build" | Out-Null }

# Copiar datos al build
if (-not (Test-Path "build\data")) {
    Copy-Item -Recurse "data" "build\data"
    Write-Host "  [Build] Datos copiados a build\data" -ForegroundColor Gray
}

$cmd = "& `"$GPP`" $FLAGS $INC $SRC -o $OUT $LIBS"
Write-Host "  [Build] Comando: $cmd" -ForegroundColor Gray

Invoke-Expression $cmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n  [Build] EXITO -> $OUT`n" -ForegroundColor Green
    Write-Host "  Para ejecutar: .\build\ehr_sistema.exe" -ForegroundColor Yellow
} else {
    Write-Host "`n  [Build] ERROR de compilacion (codigo $LASTEXITCODE)" -ForegroundColor Red
}

