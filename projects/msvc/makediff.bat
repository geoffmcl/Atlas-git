@REM 2010-05-07 - changed to version 8
@REM 2010-04-24 - changed to version 7
@set TEMPV=08
@set TEMPA=Atlas-0.4.8
@set TEMPS=C:\FGCVS\Atlas
@REM ======================================
@set TEMPP=atlas-diff%TEMPV%.patch
@set TEMPZE=atlas-%TEMPV%-exe.zip
@set TEMPR=%TEMPA%\projects\msvc
@REM Assumed to be in Atlas-???/projects/msvc...
@if NOT EXIST ..\..\..\%TEMPR%\makediff.bat goto ERR1
@if NOT EXIST ..\..\..\%TEMPP% goto NOPATCH
@if NOT EXIST %TERMPS%\. goto NOCVS
@echo.
@dir ..\..\..\%TEMPP%
@echo ERROR: ..\..\..\%TEMPP% ALREADY EXISTS!
@echo Delete, rename or move before running this again...
@echo Maybe you forgot to change the TEMPV value of %TEMPV%
@goto END

:NOPATCH
@echo Use diff to get a current patch file %TEMPP%...
@cd ..\..\..
diff -ur %TEMPS% %TEMPA% > %TEMPP%
@cd %TEMPR%
@if NOT EXIST ..\..\..\%TEMPP% goto FAILED
@dir ..\..\..\%TEMPP%
@echo Done ..\..\..\%TEMPP%... Review...
@call np ..\..\..\%TEMPP%
@goto END

:FAILED
@echo.
@echo ERROR: FAILED TO CREATE ..\..\..\%TEMPP%!
@goto END

:NOCVS
@echo.
@echo ERROR: Can NOT locate the Atlas CVS source on [%TEMPS%]
@echo Modify this batch file, if the source is elsewhere..
@goto END

:ERR1
@echo.
@echo ERROR: Can NOT locate self on ..\..\..\%TEMPR%\makediff.bat!
@echo NOT IN CORRECT LOCATION, of root [%TEMPA%] has changed...
@goto END


:END
