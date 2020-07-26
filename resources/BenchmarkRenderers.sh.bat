#!/bin/bash
: <<'END BATCH'

rem This script can be run on Windows, Mac, and Linux systems without modification.

@echo off
cls
if not exist BenchmarkRenderers.lua (
	echo "Error: Run this script in the same directory as it's stored in."
	pause
	exit 1
)
set EXEPATH=C:\Program Files\CraftOS-PC\CraftOS-PC.exe
echo This script will compare the software and hardware renderers to see which is better for your system. It is recommended you close all applications before running this (especially programs that use large amounts of CPU or GPU time).
:getpath
echo The current path to CraftOS-PC is %EXEPATH%. Is this correct?
set /P "response=(Y/N) "
if /I NOT %response%==Y (
	set /P "EXEPATH=Type the path to the CraftOS-PC executable: "
	goto getpath
)
echo Running software tests...
start /wait "" "%EXEPATH%" --sdl --addBenchmarkFunction --script BenchmarkRenderers.lua --args software
echo Running hardware tests...
start /wait "" "%EXEPATH%" --hardware-sdl --addBenchmarkFunction --script BenchmarkRenderers.lua --args hardware
goto:eof

END BATCH

if [ ! -e BenchmarkRenderers.lua ]; then
	echo "Error: Run this script in the same directory as it's stored in."
	read -e
	exit 1
fi
if [ $(uname -s) == "Darwin" ]; then EXEPATH=/Applications/CraftOS-PC.app/Contents/MacOS/craftos
elif [ $(uname -s) == "Linux" ]; then 
	if [ -e /usr/local/bin/craftos ]; then EXEPATH=/usr/local/bin/craftos
	else EXEPATH=/usr/bin/craftos; fi
else echo "Unknown platform $(uname -s)"; exit 1; fi
echo This script will compare the software and hardware renderers to see which is better for your system. It is recommended you close all applications before running this (especially programs that use large amounts of CPU or GPU time).
response=n
while [ $response != "Y" -a $response != "y" ]; do
	echo The current path to CraftOS-PC is $EXEPATH. Is this correct?
	read -p "(Y/N) " response
	if [ $response != "Y" -a $response != "y" ]; then set -p "Type the path to the CraftOS-PC executable: " EXEPATH; fi
done
echo Running software tests...
"$EXEPATH" --sdl --addBenchmarkFunction --script BenchmarkRenderers.lua --args software
echo Running hardware tests...
"$EXEPATH" --hardware-sdl --addBenchmarkFunction --script BenchmarkRenderers.lua --args hardware