pushd %~dp0

@echo off

call cmake -S . -B ./build-win

pause
popd