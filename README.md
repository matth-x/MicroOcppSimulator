# <img src="https://github.com/matth-x/MicroOcpp/assets/63792403/1c49d1ad-7afc-48d3-a54e-9aef2d4886db" alt="Icon" height="24"> &nbsp; MicroOcppSimulator

[![Build (Ubuntu)](https://github.com/matth-x/MicroOcppSimulator/workflows/Ubuntu/badge.svg)]((https://github.com/matth-x/MicroOcppSimulator/actions))
[![Build (Docker)](https://github.com/matth-x/MicroOcppSimulator/workflows/Docker/badge.svg)]((https://github.com/matth-x/MicroOcppSimulator/actions))
[![Build (WebAssembly)](https://github.com/matth-x/MicroOcppSimulator/workflows/WebAssembly/badge.svg)]((https://github.com/matth-x/MicroOcppSimulator/actions))

Tester / Demo App for the [MicroOCPP](https://github.com/matth-x/MicroOcpp) Client, running on native Ubuntu, WSL, WebAssembly or MSYS2. Online demo: [Try it](https://demo.micro-ocpp.com/)

[![Screenshot](https://github.com/agruenb/arduino-ocpp-dashboard/blob/master/docs/img/status_page.png)](https://demo.micro-ocpp.com/)

The Simulator has two purposes:
- As a development tool, it allows to run MicroOCPP directly on the host computer and simplifies the development (no flashing of the microcontroller required)
- As a demonstration tool, it allows backend operators to test and use MicroOCPP without the need to set up an actual microcontroller or to buy an actual charger with MicroOCPP.

That means that the Simulator runs on your computer and connects to an OCPP server using the same software like a
microcontroller. It provides a Graphical User Interface to show the connection status and to trigger simulated charging
sessions (and further simulated actions).

## Running on Docker

The Simulator can be run on Docker. This is the easiest way to get it up and running. The Docker image is based on
Ubuntu 20.04 and contains all necessary dependencies.

Firstly, build the image:

```shell
docker build -t matthx/microocppsimulator:latest .
```

Then run the image:

```shell
docker run -p 8000:8000 matthx/microocppsimulator:latest
```

The Simulator should be up and running now on [localhost:8000](http://localhost:8000).

## Installation (Ubuntu or WSL)

On Windows, get the Windows Subsystem for Linux (WSL): [https://ubuntu.com/wsl](https://ubuntu.com/wsl) or [MSYS2](https://www.msys2.org/).

Then follow the same steps like for Ubuntu.

On Ubuntu (other distros probably work as well, tested on Ubuntu 20.04 and 22.04), install cmake, OpenSSL and the C++
compiler:

```shell
sudo apt install cmake libssl-dev build-essential
```

Navigate to the preferred installation directory or just to the home folder. Clone the Simulator and all submodules:

```shell
git clone --recurse-submodules https://github.com/matth-x/MicroOcppSimulator
```

Navigate to the copy of the Simulator and build:

```shell
cd MicroOcppSimulator
cmake -S . -B ./build
cmake --build ./build -j 16 --target mo_simulator
```

The installation is complete! To run the Simulator, type:

```shell
./build/mo_simulator
```

This will open [localhost:8000](http://localhost:8000). You can access the Graphical User Interface by entering that
address into a browser running on the same computer. Make sure that the firewall settings allow the Simulator to connect
and to be reached.

The Simulator should be up and running now!

## Building the Webapp (Developers)

The webapp is registered as a git submodule in *webapp-src*.

Before you can build the webapp, you have to create a *.env.production* file in the *webapp-src* folder. If you just
want to try out the build process, you can simply duplicate the *.env.development* file and rename it.

After that, to build it for deployment, all you have to do is run `./build-webapp/build_webapp.ps1` (Windows) from the
root directory.
For this to work NodeJS, npm and git have to be installed on your machine. The called script automatically performs the
following tasks:

- pull the newest version of the the [arduino-ocpp-dashboard](https://github.com/agruenb/arduino-ocpp-dashboard)
- check if you have added a *.env.production* file
- install webapp dependencies
- build the webapp
- compress the webapp
- move the g-zipped bundle file into the public folder

During the process there might be some warnings displayed. Als long as the script exits without an error everything worked fine. An up-to-date version of the webapp should be placed in the *public* folder.

## Porting to WebAssembly (Developers)

If you want to run the Simulator in the browser instead of a Linux host, you can port the code for WebAssembly.

Make sure that emscripten is installed and on the path (see [https://emscripten.org/docs/getting_started/downloads.html#installation-instructions-using-the-emsdk-recommended](https://emscripten.org/docs/getting_started/downloads.html#installation-instructions-using-the-emsdk-recommended)).

Then, create the CMake build files with the corresponding emscripten tool and change the target:

```shell
emcmake cmake -S . -B ./build
cmake --build ./build -j 16 --target mo_simulator_wasm
```

The compiler toolchain should emit the WebAssembly binary and a JavaScript wrapper into the build folder. They need to be built into the preact webapp. Instead of making XHR requests to the server, the webapp will call the API of the WebAssembly JS wrapper then. The `install_webassembly.sh` script patches the webapp sources with the WASM binary and JS wrapper:

```shell
./build-webapp/install_webassembly.sh
```

Now, the GUI can be developed or built as described in the [webapp repository](https://github.com/agruenb/arduino-ocpp-dashboard).

After building the GUI, the emited files contain the full Simulator functionality. To run the Simualtor, start an HTTP file server in the dist folder and access it with your browser.

## License

This project is licensed under the GPL as it uses the [Mongoose Embedded Networking Library](https://github.com/cesanta/mongoose). If you have a proprietary license of Mongoose, then the [MIT License](https://github.com/matth-x/MicroOcpp/blob/master/LICENSE) applies.
