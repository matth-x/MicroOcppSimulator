#build, compress the web-app and move the bundle file to the public folder

#exit script on error
$ErrorActionPreference = "Stop"
#check relevant installs
echo "[info] NodeJS and npm need to be installed to build the web-app"

cd ./webapp-src

#fetch most recent version
git pull

echo "Building web-app..."

#check .env.production file exists
$file = ".env.production"

#If the file does not exist, create it.
if (Test-Path -Path $file -PathType Leaf) {
   echo "production environment found"
}else{
   echo "no .env.production file found"
   cd ..
   exit 1
}
   

#install dependencies
npm install
#build the webapp
npm run build
#compress the project
npm run compress

#move the compressed file into the public folder
Move-Item ./dist/bundle.html.gz ../public/ -Force

echo "[success] Up-to-date version of the web-app bundle was placed in the /public folder!"

cd ..