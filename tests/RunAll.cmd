@echo off

for /r %%v in (test_*.cmd) do call "%%v"

pause
