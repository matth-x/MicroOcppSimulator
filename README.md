# ArduinoOcpp Simulator

Description WIP

## Building the Webapp

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

License: GPL-v3
