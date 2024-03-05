### JSON key-value store

MicroOcpp has a local storage for the persistency of the OCPP configurations, transactions and more. As MicroOcpp is initialized for the first time, this folder is populated with all stored objects. The storage format is JSON with mostly human-readable keys. Feel free to open the stored objects with your favorite JSON viewer and inspect them to learn more about MicroOcpp.

To change the local storage folder in a productive environment, see the build flag `MO_FILENAME_PREFIX` in the CMakeLists.txt. The folder must already exist, MicroOcpp won't create a new folder at the specified location.
