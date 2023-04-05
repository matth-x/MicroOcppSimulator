#build, compress the web-app and move the bundle file to the public folder

#exit script on error
set -e
#check relevant installs
echo "[info] NodeJS and npm need to be installed to build the web-app"

cd ./webapp-src

echo "Building web-app..."

#check .env.production file exists
if [ -f ".env.production" ]
then
echo "production environment found"
else
   echo "no .env.production file found"
   exit 1
fi

#install dependencies
npm install
#build the webapp
npm run build
#compress the project
npm run compress

#move the compressed file into the public folder
mv -f ./dist/bundle.html.gz ../public/ 

echo "Up-to-date version of the web-app bundle was placed in the /public folder!"