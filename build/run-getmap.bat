@setlocal
@set TMPURL=http://wms.jpl.nasa.gov/cgi-bin/wms.cgi?LAYERS=modis,global_mosaic^&styles=default,visual^&


@set TMPEXE=Release\GetMap.exe
@if NOT EXIST %TMPEXE% (
@echo Can NOT find %TMPEXE%! *** FIX ME ***
@exit /b 1
)

@set TMP3RD=D:\Projects\3rdParty.x64\bin

@if NOT EXIST %TMP3RD%\nul (
@echo Can NOT find %TMP3RD%! *** FIX ME ***
@exit /b 1
)
@set PATH=%TMP3RD%;%PATH%


%TMPEXE% "--base-url=%TMPURL%" %*

