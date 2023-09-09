# execute after building cmake Simulator WebAssembly target:
# move created WebAssembly files into webapp folder and patch webapp

echo "installing WebAssembly files"

cp ./build/mo_simulator_wasm.mjs ./webapp-src/src/
cp ./build/mo_simulator_wasm.wasm ./webapp-src/public/

if [ -e ./webapp-src/src/DataService_wasm.js.template ]
then
    echo "patch DataService"
    mv ./webapp-src/src/DataService.js ./webapp-src/src/DataService_http.js.template
    mv ./webapp-src/src/DataService_wasm.js.template ./webapp-src/src/DataService.js
fi
