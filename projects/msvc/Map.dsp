# Microsoft Developer Studio Project File - Name="Map" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Map - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Map.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Map.mak" CFG="Map - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Map - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Map - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Map - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\Map"
# PROP Intermediate_Dir "Release\Map"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir "."
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /GR /GX /O2 /I "." /I ".\winsrc" /I "..\3rdparty\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_DEPRECATE" /D "HAVE_CONFIG_H" /D "NOMINMAX" /D "ATLAS_JPEG" /D "FREEGLUT_STATIC" /D "GLEW_STATIC" /D "ATLAS_MAP" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib ws2_32.lib freeglut_static.lib sg.lib fnt.lib ul.lib SimGear.lib jpeg32.lib libpng.lib zlib.lib /libpath:"..\3rdparty\lib" /nologo /subsystem:console /machine:I386 /out:.\bin\Map.exe

!ELSEIF  "$(CFG)" == "Map - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\Map"
# PROP Intermediate_Dir "Debug\Map"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir "."
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /GR /GX /ZI /Od /I "." /I ".\winsrc" /I "..\3rdparty\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_DEPRECATE" /D "HAVE_CONFIG_H" /D "NOMINMAX" /D "ATLAS_JPEG" /D "FREEGLUT_STATIC" /D "GLEW_STATIC" /D "ATLAS_MAP" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib ws2_32.lib freeglut_staticd.lib sg_d.lib fnt_d.lib ul_d.lib SimGeard.lib jpeg32d.lib libpngd.lib zlibd.lib /libpath:"..\3rdparty\lib" /nologo /subsystem:console /debug /machine:I386 /out:.\bin\MapD.exe

!ENDIF 

# Begin Target

# Name - "Map - Win32 Release"
# Name - "Map - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;cc"
# Begin Source File

SOURCE=..\..\src\Bucket.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\Image.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\Map.cxx
# End Source File
# Begin Source File

SOURCE=.\winsrc\MapEXT.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\misc.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\Palette.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\Subbucket.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\TileMapper.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\Tiles.cxx
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\src\Bucket.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\Image.hxx
# End Source File
# Begin Source File

SOURCE=.\winsrc\MapEXT.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\misc.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\Palette.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\Subbucket.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\TileMapper.hxx
# End Source File
# End Group
# Begin Source File

SOURCE=.\README.map.txt
# End Source File
# End Target
# End Project
