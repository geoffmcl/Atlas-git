@echo Run a series of Map tests...
@if "%1x" == "x" goto DODEL
@if /i "%1x" == "Nx" goto NODEL
@if /i "%1x" == "Dx" goto DELONLY
@echo.
@echo ERROR: Only command allowed is -
@echo N - No delete of previous, if any...
@echo D - Only delete previous, if any...
@echo.
@goto END

:DODEL
@echo Note, any previous 'temp' folders will be delete... Use N to skip deletion, but map will not regenerate existing maps...
@goto DNDEL
:NODEL
@echo Got [%1] so NO deletions...
@goto DNDEL

:DELONLY
@echo.
@echo DELETE all the 'temp' folders...
@goto ASK

:DNDEL
@echo Because map uses exit() to exit, you MAY get windows dialogs saying 'Map exploded' or something equally stupid...
@echo Just OK them, or [Close Program] if it gets to that...
@echo runmap pauses before each test, so you can abort at any time...
:ASK
@echo *** CONTINUE? *** Ctrl+C to abort...
@pause

@if /i "%1x" == "Nx" goto NODEL2
@if NOT EXIST temp1\. goto DND1
@del /S /Q temp1
@call :REMDIRS temp1
:DND1
@if NOT EXIST temp2\. goto DND2
@del /S /Q temp2
@call :REMDIRS temp2
:DND2
@if NOT EXIST temp3\. goto DND3
@del /S /Q temp3
@call :REMDIRS temp3
:DND3
@if NOT EXIST temp4\. goto DND4
@del /S /Q temp4
@call :REMDIRS temp4
:DND4
@if NOT EXIST tempb1\. goto DNB1
@del /S /Q tempb1
@call :REMDIRS tempb1
:DNB1
@if NOT EXIST tempb2\. goto DNB2
@del /S /Q tempb2
@call :REMDIRS tempb2
:DNB2
@if NOT EXIST tempb3\. goto DNB3
@del /S /Q tempb3
@call :REMDIRS tempb3
:DNB3
@if NOT EXIST tempb4\. goto DNB4
@del /S /Q tempb4
@call :REMDIRS tempb4
:DNB4
@if /i "%1x" == "Dx" goto END
@goto NODEL2

:REMDIRS
@if EXIST %1\10\. rd %1\10
@if EXIST %1\9\. rd %1\9
@if EXIST %1\8\. rd %1\8
@if EXIST %1\6\. rd %1\6
@if EXIST %1\4\. rd %1\4
@if EXIST %1\Fonts\. rd %1\Fonts
@if EXIST %1\Palettes\. rd %1\Palettes
@if EXIST %1\. rd %1
@goto :EOF

:NODEL2

@echo Default settings...
@call setdefault

call runmap temp1
@set TEMPLIT=off
call runmap temp2
@set TEMPDBG=1
call runmap temp3
@set TEMPLIT=on
call runmap temp4

@set TEMPDBG=
@set TEMPLIT=
@set TEMPRTB=on
call runmap tempb1
@set TEMPLIT=off
call runmap tempb2
@set TEMPDBG=1
call runmap tempb3
@set TEMPLIT=on
call runmap tempb4

@set TEMP1=C:\MDOS\zip8.bat
@if NOT EXIST %TEMP1% goto DNZIPS
@if EXIST temp1.zip del temp1.zip
@call zip8 -a -P -r -o temp1.zip temp1\10\*.* temp2\10\*.* temp3\10\*.* temp4\10\*.* tempb1\10\*.* tempb2\10\*.* tempb3\10\*.* tempb4\10\*.*
@call zip8 -a -P -r -o temp1.zip temp1\*.htm temp2\*.htm temp3\*.htm temp4\*.htm tempb1\*.htm tempb2\*.htm tempb3\*.htm tempb4\*.htm
@call unzip8 -vb temp1.zip
:DNZIPS

:END
