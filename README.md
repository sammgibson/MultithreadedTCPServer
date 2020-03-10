 # CSE 130 Assignment #2: HTTP Server

## Overview
The goal of this assignment is to create a multithreaded  HTTP server that can respond to basic requests such as GET and PUT. This program responds to curls and can log all requests.
This repo contains five files.
- **Makefile:** Creates the httpserver executable
- **httpserver.cpp:** Program file, creates and runs a basic HTTP server.
- **WRITEUP.pdf:** Describes the testing done.
- **DESIGN.pdf:** Short summary describing the outline of the program.
- **README.md:** Description of program.
 
## Program Execution
To run the program simply run the **make** command in the linux command line. The **make** command should create the object file **httpserver.o** and the executable binary file **httpserver**. In order to run the executable file use the "**./**" command immediately in front of the executable **(example:  ./httpserver)** . The program will run until the person running the executable decides to end it. The server can constantly recieve different curls and netcats and should only finish running when the user is done.

There are two ways to run the executable. You can just pass in the server address you would like it to run on, with which it will provide the default port for it to run on which is port 80, **(ex: ./httpserver localhost)** . Otherwise you can provide two commandline arguments. The first argument would then be the server address, and the second would be the port. In order to test the program, you will need to run curl commands. PUTs will follow the format of  **"curl -s -T {file} http://{serveraddress}:{port#}  --request-target 27-character-filename"** . and GET curls will follow the format of **"curl -s  http://{serveraddress}:{port#}  --request-target 27-character-filename"**. In order to have a log file to log all interactions the server has you will need to add the "-l" flag to the command line executable BEFORE passing the host and port, (ex: ./httpserver -l logfile localhost 5050). In order to change the number of threads from the default of 4 threads you can use the "-N" followed by a number. For example (./httpserver -N 6 localhost 5050) would create 6 threads. It needs to be put in the command line arguments BEFORE the hsot and port.
