# Microsoft Developer Studio Project File - Name="win_ulib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=win_ulib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "win_ulib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "win_ulib.mak" CFG="win_ulib - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "win_ulib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "win_ulib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "win_ulib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\win_ulib"
# PROP Intermediate_Dir "Release\win_ulib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GR /GX /O2 /MD /I "." /I ".\winsrc" /I "..\3rdparty\include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WIN32" /D "NDEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "FREEGLUT_STATIC" /D "ATLAS_JPEG" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:.\lib\win_ulib.lib

!ELSEIF  "$(CFG)" == "win_ulib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\win_ulib"
# PROP Intermediate_Dir "Debug\win_ulib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GR /GX /ZI /Od /MDd /I "." /I ".\winsrc" /I "..\3rdparty\include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "WIN32" /D "_DEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "FREEGLUT_STATIC" /D "ATLAS_JPEG" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:.\lib\win_ulibD.lib

!ENDIF 

# Begin Target

# Name - "win_ulib - Win32 Release"
# Name - "win_ulib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cc;cxx;def;odl;idl;hpj;bat;asm;asmx"
# Begin Source File

SOURCE=.\winsrc\asprintf.cxx
# End Source File
# Begin Source File

SOURCE=.\winsrc\getopt.cxx
# End Source File
# Begin Source File

SOURCE=.\winsrc\strsep.cxx
# End Source File
# Begin Source File

SOURCE=.\winsrc\sg_ext_funcs.cxx
# End Source File
# Begin Source File

SOURCE=.\winsrc\win_utils.cxx
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;inc;xsd"
# Begin Source File

SOURCE=.\winsrc\asprintf.h
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\winsrc\getopt.h
# End Source File
# Begin Source File

SOURCE=.\winsrc\strsep.h
# End Source File
# Begin Source File

SOURCE=.\winsrc\sg_ext_funcs.hxx
# End Source File
# Begin Source File

SOURCE=.\winsrc\win_utils.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\README.win_ulib.txt
# End Source File
# End Target
# End Project
