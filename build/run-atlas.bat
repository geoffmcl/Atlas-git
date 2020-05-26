@setlocal
@set TMPEXE=Release\atlas.exe
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

%TMPEXE% %*

