# build_webapp.ps1
# Build and compress frontend, do not handle API_ROOT

$ErrorActionPreference = 'Stop'
Write-Host "[info] Installing dependencies and building frontend..."

Set-Location ./webapp-src
git pull

npm install
npm run build
npm run compress

# Ensure bundle.html.gz exists
if (-not (Test-Path "./dist/bundle.html.gz")) {
  Write-Error "[error] Missing dist/bundle.html.gz, build failed"
  exit 1
}

# Move compressed file to public
if (-not (Test-Path "../public")) { 
    New-Item -ItemType Directory -Path "../public" | Out-Null 
}
Move-Item "./dist/bundle.html.gz" "../public/" -Force

Write-Host "[success] Frontend resources packaged and placed in public/ directory"
Set-Location ..