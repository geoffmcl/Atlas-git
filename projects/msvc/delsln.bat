@echo Delete the MSVC7/8/9/... solution files ...
@echo *****************************************************
@echo ARE YOU SURE YOU WANT TO DO THIS? Ctrl+C to ABORT ...
@echo *****************************************************
@pause
@echo Checking the SOLUTION files ...
@set TEMPC=0
@if NOT EXIST *.sln goto DNSLN
@echo Will delete the following...
@dir *.sln
@set /A TEMPC+=1
:DNSLN
@if NOT EXIST *.vcproj goto DNVC
@echo Will delete the following vcproj files
@dir *.vcproj
@set /A TEMPC+=1
:DNVC
@if "%TEMPC%x" == "0x" goto NONE

@echo *** REALLY CONTINUE? *** Last chance ;=)) Ctrl+C to abort...
@pause

@echo Deleting SOLUTION files ...
@if NOT EXIST *.sln goto DNSLN2
@del *.sln > nul

:DNSLN2
@if NOT EXIST *.vcproj goto DNVC2
@if EXIST *.vcproj del *.vcproj > nul

:DNVC2
@echo All done ...
@goto END

:NONE
@echo.
@echo Appears NOTHING to delete...
@goto END

:END
