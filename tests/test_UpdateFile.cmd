
:: Prepare test 
set testName=%~n0
set testDir=tmp\%testName%
IF EXIST %testDir%.A rmdir %testDir%.A /S /Q
IF EXIST %testDir%.B rmdir %testDir%.B /S /Q
IF EXIST %testDir%.txt del %testDir%.txt

echo:Start> %testDir%.txt
IF NOT EXIST tmp md tmp
md %testDir%.A
md %testDir%.B

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

echo:Uno> %testDir%.A\Uno.txt
echo:Dos> %testDir%.A\Dos.txt
..\filesync.exe sync %testDir%.A %testDir%.B >> %testDir%.txt

ping 127.0.0.1 -n 6 > nul

echo:Updated>> %testDir%.A\Uno.txt
..\filesync.exe sync %testDir%.A %testDir%.B >> %testDir%.txt

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

..\filesync.exe read %testDir%.A/nodesFile.fs >> %testDir%.txt
..\filesync.exe read %testDir%.B/nodesFile.fs >> %testDir%.txt
 
:: Check test results

IF NOT EXIST %testDir%.A\Uno.txt goto error
IF NOT EXIST %testDir%.A\Dos.txt goto error

IF NOT EXIST %testDir%.B\Uno.txt goto error
IF NOT EXIST %testDir%.B\Dos.txt goto error

FC %testDir%.A\Uno.txt %testDir%.B\Uno.txt >NUL || goto error

:: Display test result
goto end
:error
echo:Test %testName% Failed...>> %testDir%.txt
echo:Test %testName% Failed...
goto eof
:end
echo:Test %testName% OK!>> %testDir%.txt
echo:Test %testName% OK!
:eof
