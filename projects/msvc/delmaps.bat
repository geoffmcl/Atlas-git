@echo Delete the maps in 'test', if any...
@if NOT EXIST test\. goto NONE
@echo *** CONTINUE? ***
@pause
@echo Arue you sure? Last chance ;=)) Ctrl+C to abort..
@pause
cd test
@for %%i in (10 9 8 6 4) do @(call :DELIT %%i)
cd ..
@goto END

:DELIT
@if "%1x" == "x" goto :EOF
@if NOT EXIST %1\. goto :EOF
@cd %1
@if EXIST *.jpg del *.jpg
@if EXIST *.png del *.png
@cd ..
@rd %1
@goto :EOF

:NONE
@echo Directory 'test' does NOT exist!
@goto END

:END
