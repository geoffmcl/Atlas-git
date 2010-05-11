@REM Set the EXES
@set TEMPXA=bin\Atlas.exe
@if "%TEMPDBG%x" == "x" goto DNAEXE
@set TEMPXA=bin\AtlasD.exe
@echo Using DEBUG version of Atlas [%TEMPXA%]
:DNAEXE
@set TEMPXM=bin\Map.exe
@if "%TEMPDBG%x" == "x" goto DNMEXE
@set TEMPXM=bin\MapD.exe
@echo Using DEBUG version of Map [%TEMPXM%]
:DNMEXE

@REM Old versions - but maps are NOT compatible
@REM set TEMPXM=C:\MDOS\Atlas\Map.exe
@REM set TEMPXA=C:\MDOS\Atlas\Atlas.exe

@REM == Set ROOT ==
@if "%1x" == "x" goto SETDEF
@set TEMPROOT=%1
@goto CHKVER
:SETDEF
@set TEMPROOT=C:\FG\33\data
:CHKVER
@if NOT EXIST %TEMPROOT%\version goto ERRNR

@set TEMPS=..\..\src\data

@set TEMPFS=%TEMPS%\Fonts
@set TEMPF=%TEMPAT%\Fonts

@set TEMPF1=%TEMPF%\Helvetica-Bold.100.txf
@set TEMPSF1=%TEMPFS%\Helvetica-Bold.100.txf

@set TEMPF2=%TEMPF%\Helvetica.100.txf
@set TEMPSF2=%TEMPFS%\Helvetica.100.txf

@set TEMPSAP=%TEMPS%\Palettes
@set TEMPAP=%TEMPAT%\Palettes
@set TEMPAPD=%TEMPAP%\default.ap
@set TEMPSAPD=%TEMPSAP%\default.ap


@set TEMPBG=%TEMPAT%\background.jpg
@set TEMPSBG=%TEMPS%\background.jpg

@REM set the command
@REM set TEMPMCMD=--verbose --fg-root=%TEMPROOT% --fg-scenery=%TEMPROOT%\Scenery --atlas=%TEMPAT% --palette=%TEMPAPD% --render-to-window --png
@REM set TEMPMCMD=--verbose --fg-root=%TEMPROOT% --fg-scenery=%TEMPROOT%\Scenery --atlas=%TEMPAT% --palette=%TEMPAPD% --render-to-window
@set TEMPMCMD=--verbose --fg-root=%TEMPROOT% --fg-scenery=%TEMPROOT%\Scenery --atlas=%TEMPAT% --palette=%TEMPAPD%
@REM echo Checking TEMPPNG - set to render png images
@if "%TEMPPNG%x" == "x" goto DNPNG
@set TEMPMCMD=%TEMPMCMD% --png
:DNPNG
@REM echo Checking TEMPRTW - set to render to window
@if "%TEMPRTW%x" == "x" goto DNRTW
@set TEMPMCMD=%TEMPMCMD% --render-to-window
@if "%TEMPRTB%x" == "x" goto DNRTW
@goto ERRRTWB
:DNRTW
@REM echo Checking TEMPRTB - set to render to buffer
@if "%TEMPRTB%x" == "x" goto DNRTB
@set TEMPMCMD=%TEMPMCMD% --render-offscreen
@if "%TEMPRTW%x" == "x" goto DNRTB
@goto ERRRTWB
:DNRTB
@REM echo Checking TEMPLIT - set to render to buffer
@if "%TEMPLIT%x" == "x" goto DNLIT
@if /i "%TEMPLIT%x" == "onx" goto SETLIT
@if /i "%TEMPLIT%x" == "offx" goto RESETLIT
@echo.
@echo ERROR: Environment TEMPLIT can ONLY be 'on', or 'off', NOT %TEMPLIT%!
@echo Fix environment, and re-run...
@set TEMPROOT=
@goto WAIT

:SETLIT
@set TEMPMCMD=%TEMPMCMD% --lighting
@goto DNLIT
:RESETLIT
@set TEMPMCMD=%TEMPMCMD% --no-lighting
@goto DNLIT

:DNLIT
@set TEMPACMD=--fg-root=%TEMPROOT% --fg-scenery=%TEMPROOT%\Scenery --atlas=%TEMPAT% --palette=%TEMPAPD%

@goto END

:ERRRTWB
@echo.
@echo ERROR: Conflicting items TEMPRTW and TEMPRTB!
@echo Remove one or other from environment... Can NOT render to Windows and Buffer at same time!
@set TEMPROOT=
@goto WAIT

:ERRNR
@echo ERROR: Can NOT locate FG ROOT directory [%TEMPROOT%]...
@echo Amend this batch file, atsetup.bat, to point to the location of FG base data
@echo Or give the data loaction as a second parameter to runmap and runatlas
@set TEMPROOT=
@goto WAIT

:WAIT
@pause
@goto END

:END
