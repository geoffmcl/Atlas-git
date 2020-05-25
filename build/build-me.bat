@setlocal
@set DOINST=0
@set TMPPRJ=atlas
@set TMPSRC=..
@set TMPLOG=bldlog-1.txt
@REM set TMPGEN=Visual Studio 16 2019
@set TMP3RD=D:\Projects\3rdParty.x64
@set BOOST_ROOT=C:\Projects\boost_1_72_0

@set TMPOPTS=
@REM set TMPOPTS=%TMPOPTS% -G "%TMPGEN%" -A x64
@set TMPOPTS=%TMPOPTS% -DCMAKE_INSTALL_PREFIX:PATH=%TMP3RD%
@set TMPOPTS=%TMPOPTS% -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON

@echo Being build %TMPPRJ% %DATE% %TIME% >%TMPLOG%

cmake -S %TMPSRC% %TMPOPTS% >>%TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR1

cmake --build . --config Debug >>%TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR2

cmake --build . --config Release >>%TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR3
@REM :DONEREL

@echo Appears successful...
@echo.
@if NOT %DOINST% EQU 1 (
@echo Install NOT configured! Set DOINST=1
@goto END
)

@echo Continue with install, to %TMP3RD%?
@ask Only 'y' continues. All else aborts...
@if ERRORLEVEL 2 goto NOASK
@if ERRORLEVEL 1 goto INSTALL
@echo Skipping install at this time...
@goto END

:NOASK
@echo Ask utility NOT found...
:INSTALL
@echo Continue with install? Only Ctrl+C aborts
@pause

cmake --build . --config Release --target INSTALL >>%TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR4
@if EXIST install_manifest.txt (
@copy install_manifest.txt install_manifest_release.txt
)
@echo.
fa4 " -- " %TMPLOG%
@echo.
@echo All done...

@goto END

:ERR1
@echo cmake config, gen error
@goto ISERR

:Err2
@echo build debug error
@goto ISERR

:ERR3
@echo build release error
@goto ISERR


:ERR4
@echo install error
@goto ISERR

:ISERR
@echo See %TMPLOG% for details...
@endlocal
@exit /b 1

:END
@endlocal
@exit /b 0

@REM eof
