@REM setlocal
@echo Zip all the source... the zip file is created 3 directories up from here...
@if "%1x" == "x" goto HELP
@set TEMPZIP=..\..\..\%1
@set TEMPXX=%TEMP%\tempatzx.txt
@if NOT EXIST %TEMPXX% goto GENEXCL
:DNXX
@if NOT EXIST ..\..\projects\msvc\zipall.bat goto ERR2
@set TEMPE=%~x1
@set TEMPDIF=%~n1
@set TEMPSLN=%TEMPDIF%
@set TEMPDIF=%TEMPDIF%.patch
@set TEMPSLN=%TEMPSLN%-sln.zip
@if /i "%TEMPE%x" NEQ ".zipx" goto ERR3
@echo This batch file requires a batch file or exe called zip8 to do the zipping...
@echo If you do not have this, then adjust this batch file to suit your environment...
@set TEMPO=-a
@if NOT EXIST ..\..\..\. goto ERR4
@if NOT EXIST %TEMPZIP% goto DNZ1
@echo %TEMPZIP% file already exists... will only do an UPDATE...
@set TEMPO=-u
:DNZ1
@set TEMPCMD=zip8 %TEMPO% -r -p -x@%TEMPXX% %TEMPZIP% ..\..\*.*
@echo Command will be [%TEMPCMD%]
@echo Continue to make zip [%1]? Ctrl + C to abort...
@pause

@call %TEMPCMD%
@if NOT EXIST %TEMPZIP% goto NOZIP
@goto END

:NOZIP
@echo.
@echo ERROR: Zip file %TEMPZIP% was NOT created!
@goto END

:HELP
@echo.
@echo HELP
@echo Just enter the name of the zip file... and the complete
@echo source will be zipped to it, excluding some generated MSVC files...
@echo The ZIP file will be create 3 folders up, in ..\..\..\., so do NOT
@echo add a path name.
@goto END

:GENEXCL
@if EXIST %TEMPXX% Del %TEMPXX% > nul
@echo *.old > %TEMPXX%
@if NOT EXIST %TEMPXX% goto Err1
@echo *.bak >> %TEMPXX%
@echo *.obj >> %TEMPXX%
@echo *.err >> %TEMPXX%
@echo *.pdb >> %TEMPXX%
@echo *.lst >> %TEMPXX%
@echo *.pch >> %TEMPXX%
@echo *.ilk >> %TEMPXX%
@echo *.NCB >> %TEMPXX%
@echo *.plg >> %TEMPXX%
@echo *.OPT >> %TEMPXX%
@echo *.idb >> %TEMPXX%
@echo *.aps >> %TEMPXX%
@echo *.sbr >> %TEMPXX%
@echo *.suo >> %TEMPXX%
@echo *.bsc >> %TEMPXX%
@echo *.manifest >> %TEMPXX%
@echo *.user >> %TEMPXX%
@echo *.res >> %TEMPXX%
@goto DNXX

:Err1
@echo.
@echo ERROR: Appear UNABLE to create [%TEMPXX%] file ...
@echo Do NOT know the reason? Perhasp permissions?? Unknown??? Can only exit, doing nothing ;=((
@pause
@goto End

:ERR2
@echo.
@echo ERROR: Do not appear to be in projects\msvc folder???
@echo Can NOT continue...
@goto END

:ERR3
@echo.
@echo ERROR: File name given does NOT have a zip extension??? Got [%TEMPE%]
@echo Can NOT continue...
@goto END

:ERR4
@echo.
@echo ERROR: Expect a folder depth greater that 3 to do this...
@echo The ZIP is created as ..\..\..\%1

:END
