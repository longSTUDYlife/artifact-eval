@echo off
rem: Note %~dp0 get path of this batch file
rem: Need to change drive if My Documents is on a drive other than C:
set driverLetter=%~dp0
set driverLetter=%driverLetter:~0,2%
%driverLetter%
cd %~dp0
"%localappdata%/Arduino15/packages/arduino/tools/dfu-util/0.9.0-arduino1/dfu-suffix" -a %4 -v 0483 -p df11

"%localappdata%/Arduino15/packages/arduino/tools/dfu-util/0.9.0-arduino1/dfu-util" -t 1024 -s 0x08000000:leave -d %3 -a %2 -D %4
rem: java -jar maple_loader.jar %1 %2 %3 %4 %5 %6 %7 %8 %9

