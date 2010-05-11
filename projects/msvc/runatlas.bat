@call clrsetup
@set TEMPAT=%1
@if "%1x" == "x" goto HELP
@call atsetup
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
@if NOT EXIST %TEMPXA% goto ERR1
@if NOT EXIST %TEMPAT%\. goto ERR4
@echo *** STILL TO BE COMPLETED *** but trying...
@echo Running: %TEMPXA% %TEMPACMD%
%TEMPXA% %TEMPACMD%
@goto END

:HELP
@echo.
@set TEMPAT=temp
@call atsetup
@echo HELP
@echo Enter an INPUT directory to use... where the 'maps' are.
@echo This should be where you created the Maps, with runmap.bat
@echo and the necessary Map/Atlas work data files has been copied there...
@echo Then [%TEMPXA%] will be run, with a command like...
@echo %TEMPACMD%
@goto END

:MISSING
@echo ERROR: setup failed! Aborting...
@goto END

:ERR1
@echo.
@echo ERROR: Can NOT locate executable %TEMPXA%! Check atsetup.bat, name, location...
@goto END

:ERR4


:END
