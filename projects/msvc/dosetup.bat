@REM atsetup.bat HAS to be run before this...
@REM ========================================
@if "%TEMPROOT%x" == "x" goto NOSU5
@if "%TEMPAT%x" == "x" goto NOSU0
@if "%TEMPS%x" == "x" goto NOSU1
@if "%TEMPFS%x" == "x" goto NOSU2
@if "%TEMPSAP%x" == "x" goto NOSU3
@if "%TEMPAP%x" == "x" goto NOSU4
@if "%TEMPSBG%x" == "x" goto NOSU6
@if "%TEMPBG%x" == "x" goto NOSU7
@if "%TEMPF%x" == "x" goto NOSU8
@if "%TEMPF1%x" == "x" goto NOSU9
@if "%TEMPSF1%x" == "x" goto NOSU10
@if "%TEMPSF2%x" == "x" goto NOSU11
@if "%TEMPSAPD%x" == "x" goto NOSU12
@if "%TEMPAPD%x" == "x" goto NOSU13

@if NOT EXIST %TEMPROOT%\version goto ERR2

@REM check and advise on DEBUG version status
@if "%TEMPDBG%x" == "x" goto NODBG
@echo Found TEMPDBG, so using the DEBUG versions...
@goto DNDBG
:NODBG
@echo Set TEMPDBG=1 in the environment to use the DEBUG versions...
:DNDBG
@REM check and advise on PNG generation status
@if "%TEMPPNG%x" == "x" goto NOPNG
@echo Found TEMPPNG, so will ouptut PNG files, instead of def jpg...
@goto DNPNG
:NOPNG
@echo Set TEMPPNG=1 in the environment to ouptut PNG files, instead of def jpg...
:DNPNG

@echo Setup/check a '%TEMPAT%' directory... with data...
@if EXIST %TEMPAT%\. goto DNAT
@echo Creating %TEMPAT% directory...
@echo This is a NEW setup! *** CONTINUE? *** Ctrl+C to abort...
@pause
@md %TEMPAT%
@if NOT EXIST %TEMPAT%\. goto ERR4
:DNAT

@if EXIST %TEMPBG% goto DNBG
@if NOT EXIST %TEMPSBG% goto ERR9
@echo Copy %TEMPSBG% to %TEMPBG%
@copy %TEMPSBG% %TEMPBG% > nul
@if NOT EXIST %TEMPSBG% goto ERR9B
:DNBG
@echo Got file %TEMPBG%...

@if NOT EXIST %TEMPF% @md %TEMPF%
@if NOT EXIST %TEMPF%\. goto ERR5

@if EXIST %TEMPF1% goto DNF1
@if NOT EXIST %TEMPFS%\. goto ERR6
@if NOT EXIST %TEMPSF1%\. goto ERR7
@echo Copy %TEMPSF1% to %TEMPF1%
@Copy %TEMPSF1% %TEMPF1% > nul
@if NOT EXIST %TEMPF1%\. goto ERR6B
:DNF1
@echo Got file %TEMPF1%...

@if EXIST %TEMPF2% goto DNF2
@if NOT EXIST %TEMPFS%\. goto ERR6
@if NOT EXIST %TEMPSF2%\. goto ERR8
@echo Copy %TEMPSF2% to %TEMPF2%
@Copy %TEMPSF2% %TEMPF2% > nul
@if NOT EXIST %TEMPF2%\. goto ERR8B
:DNF2
@echo Got file %TEMPF2%...

@if EXIST %TEMPAPD% goto DNAP
@if NOT EXIST %TEMPSAPD% goto ERR10
@if NOT EXIST %TEMPAP%\. @md %TEMPAP%
@if NOT EXIST %TEMPAP%\. goto ERR10A
@echo Copy %TEMPSAPD% to %TEMPAPD%
@Copy %TEMPSAPD% %TEMPAPD% > nul
@if NOT EXIST %TEMPAPD% goto ERR10B
:DNAP
@echo Got file %TEMPAPD%...

@REM END Local setup
@goto END

:NOSU0
@echo ERROR TEMPAT not set...
@goto NOSETUP
:NOSU1
@echo ERROR TEMPS not set...
@goto NOSETUP
:NOSU2
@echo ERROR TEMPFS not set...
@goto NOSETUP
:NOSU3
@echo ERROR: TEMPSAP not set...
@goto NOSETUP
:NOSU4
@echo ERROR: TEMPAP not set...
@goto NOSETUP
:NOSU5
@echo ERROR: TEMPROOT not set...
@goto NOSETUP
:NOSU6
@echo ERROR: TEMPSBG not set...
@goto NOSETUP
:NOSU7
@echo ERROR: TEMPBG not set...
@goto NOSETUP
:NOSU8
@echo ERROR: TEMPF not set...
@goto NOSETUP
:NOSU9
@echo ERROR: TEMPF1 not set...
@goto NOSETUP
:NOSU10
@echo ERROR: TEMPSF1 not set...
@goto NOSETUP
:NOSU11
@echo ERROR: TEMPSF2 not set...
@goto NOSETUP
:NOSU12
@echo ERROR: TEMPSAPD not set...
@goto NOSETUP
:NOSU13
@echo ERROR: TEMPAPD not set...
@goto NOSETUP

:NOSETUP
@echo This is set in atsetup.bat, which MUST be run first...
@echo This is called in runmap.bat and runatlas.bat... Use one of these...
@pause
@goto END

:ERR1
@echo.
@echo ERROR: Can NOT locate the executable %TEMPX%! Check name, location, and adjust...
@goto END

:ERR2
@echo.
@echo ERROR: Can NOT locate the FG data version file %TEMPROOT%\version! Check name, location, and adjust...
@goto END

:ERR3
@echo.
@echo ERROR: Can NOT locate the palette file %TEMPP%! Check name, location, and adjust...
@goto END

:ERR4
@echo.
@echo ERROR: FAILED to create %TEMPAT% directory! Check why??? Perhaps another files called %TEMPAT%???
@goto END

:ERR5
@echo.
@echo ERROR: FAILED to create %TEMPF% directory! Check why??? Perhaps another files called %TEMPF%???
@goto END

:ERR6
@echo.
@echo ERROR: FAILED to locate %TEMPFS% directory! Can not copy FONTS??? Check name. location...
@goto END

:ERR6B
@echo.
@echo ERROR: FAILED to copy %TEMPFS1% to %TEMPF1%! Can not copy FONTS??? Check name. location...
@goto END

:ERR7
@echo.
@echo ERROR: FAILED to locate %TEMPF1% file! Can not copy FONTS??? Check name. location...
@goto END

:ERR8
@echo.
@echo ERROR: FAILED to locate %TEMPF2% file! Can not copy FONTS??? Check name. location...
@goto END

:ERR8B
@echo.
@echo ERROR: FAILED to copy %TEMPF2% file to %TEMPF2%! Can not copy FONTS??? Check name. location...
@goto END

:ERR9
@echo.
@echo ERROR: FAILED to locate %TEMPSBG% file! Can not copy BACKGROUND world??? Check name. location...
@goto END

:ERR9B
@echo.
@echo ERROR: FAILED to copy %TEMPSBG% file! Can not copy BACKGROUND world??? Check name. location...
@goto END

:ERR10
@echo.
@echo ERROR: FAILED to copy %TEMPSAPD% file! Can not copy default PALETTE??? Check name. location...
@goto END

:ERR10A
@echo.
@echo ERROR: FAILED to create %TEMPAP% directory! Can not copy default PALETTE??? Check name. location...
@goto END

:ERR10B
@echo.
@echo ERROR: FAILED to copy %TEMPSAPD% to %TEMPAPD%! Can not copy default PALETTE??? Check name. location...
@goto END

:END
