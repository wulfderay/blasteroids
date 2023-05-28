# ----------------------------
# PlayStation 1 Psy-Q MAKEFILE
# ----------------------------
all:
	#del mem.map
	#del main.sym
	#del main.exe
	#del main.cpe
	#cls
	asmpsx /l zapotec.asm,zapotec.obj
	ccpsx -O3 -Xo$80010000 zapotec.obj dlist.c game.c main.c -omain.cpe,main.sym,mem.map
	cpe2x /ca main.cpe
