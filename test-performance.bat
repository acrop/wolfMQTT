set QOS=%1
if "%QOS%" EQU "" (set QOS=0)
echo QOS is %QOS%
 .\build\i686-pc-windows-msvc-14-Release\bin\multithread.exe -h 127.0.0.1 -C 2000 -q %QOS%
pause
