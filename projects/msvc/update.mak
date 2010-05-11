# update.mak - 2010-05-07

# This uses my person folder arrangement, and source files,
# so may not function without change...
mf	=	update.mak

prj1	=	jpeg32
dsrc1	=	C:\Projects\jpeg-8a\msvc
ddst1	=	..\3rdparty\lib

jsrc1	=	$(dsrc1)\$(prj1).lib
jsrc2	=	$(dsrc1)\$(prj1)d.lib
jsrc3	=	$(dsrc1)\$(prj1).idb
jsrc4	=	$(dsrc1)\$(prj1)d.idb
jsrc5	=	$(dsrc1)\$(prj1).pdb
jsrc6	=	$(dsrc1)\$(prj1)d.pdb

jdst1	=	$(ddst1)\$(prj1).lib
jdst2	=	$(ddst1)\$(prj1)d.lib
jdst3	=	$(ddst1)\$(prj1).idb
jdst4	=	$(ddst1)\$(prj1)d.idb
jdst5	=	$(ddst1)\$(prj1).pdb
jdst6	=	$(ddst1)\$(prj1)d.pdb

prj2	=	SimGear
dsrc2	=	C:\FG\33\SimGear\lib
ddst2	=	$(ddst1)

ssrc1	=	$(dsrc2)\$(prj2).lib
ssrc2	=	$(dsrc2)\$(prj2)d.lib
ssrc3	=	$(dsrc2)\$(prj2).idb
ssrc4	=	$(dsrc2)\$(prj2)d.idb
ssrc5	=	$(dsrc2)\$(prj2).pdb
ssrc6	=	$(dsrc2)\$(prj2)d.pdb

sdst1	=	$(ddst2)\$(prj2).lib
sdst2	=	$(ddst2)\$(prj2)d.lib
sdst3	=	$(ddst2)\$(prj2).idb
sdst4	=	$(ddst2)\$(prj2)d.idb
sdst5	=	$(ddst2)\$(prj2).pdb
sdst6	=	$(ddst2)\$(prj2)d.pdb

all:	$(jdst1) $(jdst2) $(jdst3) $(jdst4) $(jdst5) $(jdst6) \
	$(sdst1) $(sdst2) $(sdst3) $(sdst4) $(sdst5) $(sdst6)

$(jdst1):	$(jsrc1) $(mf)
	copy $(jsrc1) $(jdst1) >nul

$(jdst2):	$(jsrc2) $(mf)
	copy $(jsrc2) $(jdst2) >nul

$(jdst3):	$(jsrc3) $(mf)
	copy $(jsrc3) $(jdst3) >nul

$(jdst4):	$(jsrc4) $(mf)
	copy $(jsrc4) $(jdst4) >nul

$(jdst5):	$(jsrc5) $(mf)
	copy $(jsrc5) $(jdst5) >nul

$(jdst6):	$(jsrc6) $(mf)
	copy $(jsrc6) $(jdst6) >nul

$(sdst1):	$(ssrc1) $(mf)
	copy $(ssrc1) $(sdst1) >nul

$(sdst2):	$(ssrc2) $(mf)
	copy $(ssrc2) $(sdst2) >nul

$(sdst3):	$(ssrc3) $(mf)
	copy $(ssrc3) $(sdst3) >nul

$(sdst4):	$(ssrc4) $(mf)
	copy $(ssrc4) $(sdst4) >nul

$(sdst5):	$(ssrc5) $(mf)
	copy $(ssrc5) $(sdst5) >nul

$(sdst6):	$(ssrc6) $(mf)
	copy $(ssrc6) $(sdst6) >nul

# eof - update.mak
