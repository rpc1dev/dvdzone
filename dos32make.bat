@echo off
set PATH=e:\djgpp\bin;%PATH%
set DJGPP=e:\djgpp\djgpp.env
gpp -o dvdzone.exe -Wno-deprecated *.c*
pause
