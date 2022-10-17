# This needs cl compiler installed.
# The cl compiler comes with visual studio and can be executed from the "Developer PowerShell" only
#
# !!! Important !!!
# The generated "server_win.exe" cannot be executed from the /build directory. There it will not find
# the /public directory. The "server_win.exe" file needs to be moved to the same directory as /public

cl ../main.c ../mongoose/mongoose.c /Fe: server_win.exe