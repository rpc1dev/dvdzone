@echo off
Rem *** This batch file can be to compile with the **free** version of VC++ toolkit
Rem *** from Microsoft. See: http://msdn.microsoft.com/visualc/vctoolkit2003/

if "%1"=="" goto :default
goto :%1

:default
cl /o dvdzone.exe /nologo *.c*
goto :exit


:clean

del dvdzone.exe

del dos32aspi.obj
del dosaspi.obj
del dvdzone.obj
del scsi.obj
del scsistub.obj
del sgio.obj
del sptx.obj
del stuc.obj
del winaspi.obj


:exit
