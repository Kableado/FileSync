
:: Prepare test 
set testName=%~n0
set testDir=tmp\%testName%
IF EXIST %testDir%.A rmdir %testDir%.A /S /Q
IF EXIST %testDir%.B rmdir %testDir%.B /S /Q
IF EXIST %testDir%.txt del %testDir%.txt

echo:Test %testName% started> %testDir%.txt
echo [7mTest %testName% started[0m
IF NOT EXIST tmp md tmp
md %testDir%.A
md %testDir%.B

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

echo:Uno> %testDir%.A\Uno.txt
echo:Dos> %testDir%.A\Dos.txt
..\filesync.exe -sync -dir %testDir%.A -dir %testDir%.B >> %testDir%.txt

del %testDir%.A\Uno.txt
..\filesync.exe -sync -dir %testDir%.A -dir %testDir%.B >> %testDir%.txt

ping 127.0.0.1 -n 2 > nul

echo:UnoRepuesto> %testDir%.A\Uno.txt
..\filesync.exe -sync -dir %testDir%.A -dir %testDir%.B >> %testDir%.txt

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

..\filesync.exe -read %testDir%.A/nodesFile.fs >> %testDir%.txt
..\filesync.exe -read %testDir%.B/nodesFile.fs >> %testDir%.txt
 
:: Check test results

IF NOT EXIST %testDir%.A\Uno.txt goto error
IF NOT EXIST %testDir%.A\Dos.txt goto error

IF NOT EXIST %testDir%.B\Uno.txt goto error
IF NOT EXIST %testDir%.B\Dos.txt goto error


:: Display test result
goto end
:error
echo:Test %testName% Failed...>> %testDir%.txt
echo [41mTest %testName% Failed...[0m
goto eof
:end
echo:Test %testName% OK!>> %testDir%.txt
echo [42mTest %testName% OK![0m
:eof
echo:>> %testDir%.txt
echo:
echo:>> %testDir%.txt
echo:
