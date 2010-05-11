@echo Testing GetMap ...
@echo Info: http://onearth.jpl.nasa.gov/tiled.html
@REM NOTE THE ESCAPE CHAR, '^', BEFORE EACH '&', TO ENSURE IT REMAINS!!!
@REM ===================================================================
@set TEMPBASE=http://wms.jpl.nasa.gov/cgi-bin/wms.cgi?LAYERS=modis,global_mosaic^&styles=default,visual^&
@set TEMPEXE=bin\GetMap.exe
@if NOT EXIST %TEMPEXE% goto ERR1
@if "%1x" == "x" goto HELP
@if NOT EXIST %1\. goto ERR2
@set TEMPHTM=%1\tempmaps.htm
@REM KSFO = --lat=37.621 --lon=-122.381
@REM %TEMPEXE% --help
@REM GetMap - FlightGear mapping retrieval utility
@REM Usage:
@REM   --size=pixels           Create map of size pixels*pixels (default 256)
@REM   --base-url=url          Beginning of the url of the WMS server
@REM   --atlas=path            Create maps of all scenery, and store them in path
@REM   --min-lat               Minimum latitude
@REM   --max-lat               Maximum latitude
@REM   --min-lon               Minimum longitude
@REM   --max-lon               Maximum longitude
@REM   --verbose               Display information during processing
@set TEMPCMD=--base-url="%TEMPBASE%"
@set TEMPCMD=%TEMPCMD% --size=512
@set TEMPCMD=%TEMPCMD% --atlas=%1
@set TEMPCMD=%TEMPCMD% --min-lat=36
@set TEMPCMD=%TEMPCMD% --max-lat=39
@set TEMPCMD=%TEMPCMD% --min-lon=-123
@set TEMPCMD=%TEMPCMD% --max-lon=-121
@set TEMPCMD=%TEMPCMD% --verbose
@echo Run %TEMPEXE% with commands -
@echo %TEMPCMD%
@echo This should write 6 images... Ctrl+C to abort...
@pause
%TEMPEXE% %TEMPCMD%
@echo ^<html^>^<head^>^<title^>GetMap Maps^</title^> >%TEMPHTM%
@if NOT EXIST %TEMPHTM% goto NOHTM
@echo ^<style type="text/css"^> >>%TEMPHTM%
@echo .nob { margin:0; padding:0; border:0; } >>%TEMPHTM%
@echo ^</style^> >>%TEMPHTM%
@echo ^<body^> >>%TEMPHTM%
@echo ^<table border="0" cellpadding="0" cellspacing="0" class="nob" align="center" summary="image holder"^> >>%TEMPHTM%
@echo ^<tr^> >>%TEMPHTM%
@echo ^<td class="nob"^>^<img class="nob" src="w123n38.jpg" alt="IMG"^>^</td^> >>%TEMPHTM%
@echo ^<td class="nob"^>^<img class="nob" src="w122n38.jpg" alt="IMG"^>^</td^> >>%TEMPHTM%
@echo ^</tr^> >>%TEMPHTM%
@echo ^<tr^> >>%TEMPHTM%
@echo ^<td class="nob"^>^<img class="nob" src="w123n37.jpg" alt="IMG"^>^</td^> >>%TEMPHTM%
@echo ^<td class="nob"^>^<img class="nob" src="w122n37.jpg" alt="IMG"^>^</td^> >>%TEMPHTM%
@echo ^</tr^> >>%TEMPHTM%
@echo ^<tr^> >>%TEMPHTM%
@echo ^<td class="nob"^>^<img class="nob" src="w123n36.jpg" alt="IMG"^>^</td^> >>%TEMPHTM%
@echo ^<td class="nob"^>^<img class="nob" src="w122n36.jpg" alt="IMG"^>^</td^> >>%TEMPHTM%
@echo ^</tr^> >>%TEMPHTM%
@echo ^</table^> >>%TEMPHTM%
@echo ^</body^> >>%TEMPHTM%
@echo ^</html^> >>%TEMPHTM%
@start  %TEMPHTM%
@goto END

:NOHTM
@echo WARNING: Can NOT write %TEMPHTM% file, to view maps...
@goto END

:ERR1
@echo ERROR: Can NOT locate %TEMPEXE% ... check name, location ...
@goto END

:ERR2
@echo ERROR: Can NOT locate %1 path ... check name, or create first ...
@goto END

:HELP
@echo GetMap will fetch maps from the above server, but you
@echo must give an existing OUTPUT directory,
@echo where to store the maps retrieved ...
@goto END

:END
