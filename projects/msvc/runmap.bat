@call clrsetup
@set TEMPAT=%1
@if "%1x" == "x" goto HELP
@if /i "%1x" == "-hx" goto HELP
@if /i "%1x" == "-?x" goto HELP
@if /i "%1x" == "--helpx" goto HELP
@call atsetup %2
@if "%TEMPROOT%x" == "x" goto MISSING
@if "%TEMPAT%x" == "x" goto MISSING
@if "%TEMPS%x" == "x" goto MISSING
@if "%TEMPFS%x" == "x" goto MISSING
@if "%TEMPSAP%x" == "x" goto MISSING
@if "%TEMPAP%x" == "x" goto MISSING
@if "%TEMPSBG%x" == "x" goto MISSING
@if "%TEMPBG%x" == "x" goto MISSING
@if "%TEMPF%x" == "x" goto MISSING
@if "%TEMPF1%x" == "x" goto MISSING
@if "%TEMPSF1%x" == "x" goto MISSING
@if "%TEMPSF2%x" == "x" goto MISSING
@if "%TEMPSAPD%x" == "x" goto MISSING
@if "%TEMPAPD%x" == "x" goto MISSING

@call dosetup
@if NOT EXIST %TEMPXM% goto ERR1
@if NOT EXIST %TEMPAT%\. goto ERR4
@if NOT EXIST %TEMPSBG% goto ERR9
@if NOT EXIST %TEMPF%\. goto ERR5
@if NOT EXIST %TEMPFS%\. goto ERR6
@if NOT EXIST %TEMPSF1%\. goto ERR7
@if NOT EXIST %TEMPFS%\. goto ERR6
@if NOT EXIST %TEMPSF2%\. goto ERR8
@if NOT EXIST %TEMPSAPD% goto ERR10
@if NOT EXIST %TEMPAP%\. goto ERR10A
@set TEMPHTM=%TEMPAT%\temphtm.htm
@set TEMPTXT=%TEMPAT%\runtxt.txt
@REM END Local setup

@type %TEMPROOT%\version
@echo Above is version of FG data...

%TEMPXM% --version >%TEMPTXT%

@REM Run map EXE...
%TEMPXM% %TEMPMCMD%

@set TEMPI1=w123n37.jpg
@set TEMPI2=w122n37.jpg
@if "%TEMPPNG%x" == "x" goto DNIPNG
@set TEMPI1=w123n37.png
@set TEMPI2=w122n37.png
:DNIPNG

@echo Run Date: %DATE% %TIME% >>%TEMPTXT%
@echo Run CMD : %TEMPXM% %TEMPMCMD% >>%TEMPTXT%
@type %TEMPTXT%

@if NOT EXIST %TEMPAT%\10\%TEMPI1% goto NOVIEW

@REM OUTPUT A HTML VIEWING FILE
@echo ^<html^>^<head^>^<title^>GetMap Maps^</title^> >%TEMPHTM%
@if NOT EXIST %TEMPHTM% goto NOHTM
@echo ^<style type="text/css"^> >>%TEMPHTM%
@echo .nob { margin:0; padding:0; border:0; } >>%TEMPHTM%
@echo .nobt { margin:0; padding:0; border:0; text-decoration:none; } >>%TEMPHTM%
@echo ^</style^> >>%TEMPHTM%
@echo ^<body^> >>%TEMPHTM%
@echo ^<table border="0" cellpadding="0" cellspacing="0" class="nob" align="center" summary="image holder"^> >>%TEMPHTM%
@echo ^<tr^> >>%TEMPHTM%
@echo ^<td class="nob"^>^<a class="nobt" href="10/%TEMPI1%"^>^<img class="nob" src="10/%TEMPI1%" width="256" height="256" alt="IMG %TEMPI1%"^>^</a^>^</td^> >>%TEMPHTM%
@echo ^<td class="nob"^>^<a class="nobt" href="10/%TEMPI2%"^>^<img class="nob" src="10/%TEMPI2%" width="256" height="256" alt="IMG %TEMPI2%"^>^</a^>^</td^> >>%TEMPHTM%
@echo ^</tr^> >>%TEMPHTM%
@echo ^</table^> >>%TEMPHTM%
@echo ^<p^>Display of files [10/%TEMPI1%] and [10/%TEMPI2%] at 256x256 pixels. Click image to see full 1024x1024 size.^</p^> >>%TEMPHTM%
@echo ^<pre^> >>%TEMPHTM%
@type %TEMPTXT% >>%TEMPHTM%
@echo ^</pre^> >>%TEMPHTM%
@echo ^<p^>Run Date: %DATE% %TIME% ^<br^> >>%TEMPHTM%
@echo Command Run: %TEMPXM% %TEMPMCMD%^</p^> >>%TEMPHTM%
@echo ^</body^> >>%TEMPHTM%
@echo ^</html^> >>%TEMPHTM%
@start  %TEMPHTM%
@goto DNVIEW

:NOVIEW
@echo ERROR: Can NOT locate file %TEMPAT%\10\%TEMPI1%?
@echo Was it created, or did %TEMPXM% fail?
@goto DNVIEW

:DNVIEW
@REM More stuff ?? ;=))

@goto END

:NOHTM
@echo WARNING: Failed to write %TEMPHTM% file...
@goto END

:ERRNB
@echo.
@echo ERROR: Can NOT locate the setup batch, dosetup.bat! Check name, location, and adjust...
@goto END

:ERR1
@echo.
@echo ERROR: Can NOT locate the executable %TEMPXM%! Check name, location, and adjust...
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
@echo This was supposed ot be done in dosetup.bat, called at the beginning...
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

:HELP
@echo.
@set TEMPAT=temp
@call atsetup
@echo HELP
@echo Minimum: Enter an OUTPUT directory to use...
@echo An attempt to creat this directory will be made,
@echo and the necessary map work data files will be copied there...
@echo Then [%TEMPXM%] will be run, with a command like...
@echo %TEMPMCMD%
@echo An optional second parameter, if present, must be the directory of FG base data
@echo This is required if your data is NOT at %TEMPROOT%
@if EXIST %TEMPROOT%\version goto DNROOT
@echo And it looks like you will need this 2nd parameter.
:DNROOT
@call clrsetup
@goto END

:MISSING
@echo ERROR: setup failed! Aborting...
@goto END

:END
