@echo off
setlocal enabledelayedexpansion

if "%~1"=="" goto :printHelp
if "%~2"=="" goto :printHelp

set "INFILE=%~1"

set "OUTFILE=%~2"

set "VARNAME=%~n1"

:: Check if input file exists
if not exist "%INFILE%" (
  echo Error: File "%INFILE%" does not exist
  exit /b 1
)

echo Input file: "%INFILE%"
echo Output file: "%OUTFILE%"

:: Create output file and add header content
(
echo static const char *%VARNAME% = R"(
) > "%OUTFILE%"

:: Append content of the text file
type "%INFILE%" >> "%OUTFILE%"

echo(>> "%OUTFILE%"
:: Add trailing content
echo ^)^";>> "%OUTFILE%"

echo Conversion completed: "%OUTFILE%"

exit /b 0

:printHelp

echo Usage: %0 input_file output_dir

endlocal
