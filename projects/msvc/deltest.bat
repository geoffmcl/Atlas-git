@echo Delete the 'test' folder...
@if NOT EXIST test\. goto DONE
@echo *** ARE YOU SURE? ***
@pause
@echo Last chance? Ctrl+C to abort...
@pause

rmdir /S test

@goto END

:DONE
@echo Appears already deleted...
@goto END

:END
