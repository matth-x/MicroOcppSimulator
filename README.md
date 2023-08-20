# <img src="https://user-images.githubusercontent.com/63792403/133922028-fefc8abb-fde9-460b-826f-09a458502d17.png" alt="Icon" height="24"> &nbsp; MicroOcppSimulator

Tester / Demo App for the [MicroOcpp](https://github.com/matth-x/MicroOcpp) Client, running on native Ubuntu or the WSL.

![Screenshot](https://github.com/agruenb/arduino-ocpp-dashboard/blob/master/docs/img/status_page.png)

The Simulator has two purposes:
- As a development tool, it allows to run MicroOcpp directly on the host computer and simplifies the development (no flashing of the microcontroller required)
- As a demonstration tool, it allows backend operators to test and use MicroOcpp without the need to set up an actual microcontroller or to buy an actual charger with MicroOcpp.

That means that the Simulator runs on your computer and connects to an OCPP server using the same software like a microcontroller. It provides a Graphical User Interface to show the connection status and to trigger simulated charging sessions (and further simulated actions).

## Installation

On Windows, get the Windows Subsystem for Linux (WSL): [https://ubuntu.com/wsl](https://ubuntu.com/wsl)

Then follow the same steps like for Ubuntu.

On Ubuntu (other distros probably work as well, tested on Ubuntu 20.04 and 22.04), install cmake, OpenSSL and the C++ compiler:

```shell
sudo apt install cmake libssl-dev build-essential
```

Navigate to the preferred installation directory or just to the home folder. Clone the Simulator and all submodules:

```shell
git clone --recurse-submodules https://github.com/matth-x/MicroOcppSimulator
```

Navigate to the copy of the Simulator, prepare some necessary local folders and build:

```shell
cd MicroOcppSimulator
mkdir build
mkdir mo_store
cmake -S . -B ./build
cmake --build ./build -j 16 --target mo_simulator
```

The installation is complete! To run the Simulator, type:

```shell
./build/mo_simulator
```

This will open [localhost:8000](http://localhost:8000). You can access the Graphical User Interface by entering that address into a browser running on the same computer. Make sure that the firewall settings allow the Simulator to connect and to be reached.

The Simulator should be up and running now!

## Building the Webapp (Developers)

The webapp is registered as a git submodule in *webapp-src*.

Before you can build the webapp, you have to create a *.env.production* file in the *webapp-src* folder. If you just want to try out the build process, you can simply duplicate the *.env.development* file and rename it.

After that, to build it for deployment, all you have to do is run `./build-webapp/build_webapp.ps1` (Windows) from the root directory.
For this to work NodeJS, npm and git have to be installed on your machine. The called script automatically performs the following tasks: 

 - pull the newest version of the the [arduino-ocpp-dashboard](https://github.com/agruenb/arduino-ocpp-dashboard)
 - check if you have added a *.env.production* file
 - install webapp dependencies
 - build the webapp
 - compress the webapp
 - move the g-zipped bundle file into the public folder

During the process there might be some warnings displayed. Als long as the script exits without an error everything worked fine. An up-to-date version of the webapp should be placed in the *public* folder.

## License

This project is licensed under the GPL as it uses the [Mongoose Embedded Networking Library](https://github.com/cesanta/mongoose). If you have a proprietary license of Mongoose, then the [MIT License](https://github.com/matth-x/MicroOcpp/blob/master/LICENSE) applies.
