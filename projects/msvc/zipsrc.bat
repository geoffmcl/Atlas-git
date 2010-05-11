@REM 2010-05-08 - changed to version 8
@set TEMPV=08
@set TEMPA=Atlas-0.4.8
@REM 2010-04-24 - changed to version 7
@REM set TEMPV=07
@REM set TEMPA=Atlas-0.4.3

@REM ==========================================
@cls
@dir /ad /od
@REM All other should follow, with no change...
@set TEMPZS=atlas-%TEMPV%-src.zip
@set TEMPZM=atlas-%TEMPV%-sln.zip
@set TEMPZA=atlas-%TEMPV%-all.zip
@set TEMPP=atlas-diff%TEMPV%.patch
@set TEMPZE=atlas-%TEMPV%-exe.zip
@set TEMP3RD=atlas-%TEMPV%-3rd.zip
@set TEMPR=%TEMPA%\projects\msvc
@REM Assumed to be in Atlas-???/projects/msvc...
@if NOT EXIST ..\..\..\%TEMPR%\zipsrc.bat goto ERR1
@set TEMPU=-a
@if NOT EXIST ..\..\..\%TEMPZS% goto DNZ1
@echo This is an update of an existing %TEMPZS%
@set TEMPU=-u
:DNZ1
@if NOT EXIST ..\..\..\%TEMPP% goto NOPATCH
@echo Found %TEMPP%, and will be included...
:CONT1
@echo ZIP Version = %TEMPV%, so using %TEMPZS% %TEMPZM%
@echo Zip this source to %TEMPZA% file...
@echo Also creating %TEMPZE% file, using bin folder... and %TEMP3RD%
@echo NOTE: Must commence with a ***CLEAN UP***, including Debug and Release folders...
@echo BUT HAVE ALL OTHER TEMPORARY AND TEST FOLDERS BEEN REMOVED?
@xdelete -? >nul
@if ERRORLEVEL 1 goto NOCLEAN
:DNCLN
@if NOT EXIST cleanup.bat goto ERR2
@echo --**-- This batch file uses WinZIP command line zip, wcline, through a zip8.bat batch file.
@echo --**-- If you do NOT have these, then this batch is of no use to you.
@echo.
@echo ====== READ ALL THE ABOVE CAREFULLY ======
@echo *** CONTINUE? *** Ctrl+C to abort
@pause


@echo Doing cleanup first...
@call cleanup

@echo Doing Zipping...
@cd ..\..
@set TEMPU=%TEMPU% -o -P
@call zip8 %TEMPU% -x*.sln ..\%TEMPZS% *.* src\*.*

@set TEMPU=-a
@if NOT EXIST ..\%TEMPZM% goto DNZ2
@echo This is an update of an existing %TEMPZS%
@set TEMPU=-u
:DNZ2
@set TEMPU=%TEMPU% -o -P
@call zip8 %TEMPU% ..\%TEMPZM% projects\msvc\*.* projects\msvc\winsrc\*.*

cd ..
@set TEMPI=%TEMPZM% %TEMPZS%
@if NOT EXIST %TEMPP% goto DNZ3
@set TEMPI=%TEMPP% %TEMPZM% %TEMPZS%
:DNZ3
@set TEMPU=-a
@if NOT EXIST %TEMPZA% goto DNZ4
@echo This is an update of an existing %TEMPZS%
@set TEMPU=-u
:DNZ4
@set TEMPU=%TEMPU% -o
@call zip8 %TEMPU% %TEMPZA% %TEMPI%

@set TEMPU=-a
@if NOT EXIST %TEMPZE% goto DNZ5
@echo This is an update of an existing %TEMPES%
@set TEMPU=-u
:DNZ5
@set TEMPU=%TEMPU% -o
@call zip8 %TEMPU% -x*.ilk %TEMPZE% %TEMPR%\bin\*.*

@set TEMPU=-a
@if NOT EXIST %TEMP3RD% goto DN3RD
@echo This is an update of an existing %TEMP3RD%
@set TEMPU=-u
:DN3RD
@set TEMPU=%TEMPU% -o -P -r
@cd %TEMPR%
@cd ..
@call zip8 %TEMPU% ..\..\%TEMP3RD% 3rdparty\*.*
@cd ..\..
@echo All done...
@goto END

:NOPATCH
@echo.
@echo WARNING: Can NOT locate the patch file ..\..\..\%TEMPP%
@echo So the PATCH file will NOT be included in the ALL zip
@echo *** REALLY CONSIDER IF THIS IS WHAT YOU WANT ***
@echo Maybe the makediff.bat needs to be run first...
@echo.
@goto CONT1

:NOCLEAN
@echo.
@echo PROBLEM: Can NOT locate the 'xdelete' tool in the PATH
@echo This just means the object files can not be deleted...
@echo so will be put in the zip, which is BAD...
@set TEMPDEL=
@if NOT EXIST Release\. goto DNREL1
@echo Have FOUND a Release folder..
@set TEMPDEL=Release
:DNREL1
@if NOT EXIST Debug\. goto DNDBG1
@echo Have FOUND a Debug folder...
@set TEMPDEL=Debug
:DNDBG1
@if "%TEMPDEL%x" == "x" goto NODEL
@echo Like del /S %TEMPDEL%; then each sub-folder...; then rd %TEMPDEL%...
@echo Aborting so you can manually delete it/them... Release and Debug...
@goto END

:NODEL
@echo But Debug and Release folders NOT found, so ok...
@goto DNCLN

:ERR1
@echo.
@echo ERROR: BIG PROBLEM! DO not seem to be in the correct folder to start...
@echo Current folder is [%CD%], and can NOT locate self as
@echo ..\..\..\%TEMPA%\projects\msvc\zipsrc.bat
@goto END

:ERR2
@echo.
@echo ERROR: Can NOT locate cleanup.bat batch file...
@goto END

:END
