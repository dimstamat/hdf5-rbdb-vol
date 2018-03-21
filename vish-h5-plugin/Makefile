VISH=VishLogger5

#LIBS = ${SOCKET_LIBS} $(hdf5ref_LIB) -lm 
LIBS = -lvscript $(hdf5ref_LIB) -lm 
LIBS+=${OCEAN}

all.dtor=visible

include $(VPATH)../GNUmakefile.rules
-include ${MAKE_ARCH_INCLUDES}/hdf5.cfg

LIBS+=${hdf5_LIB}
SYSCFLAGS+=${hdf5_FLAGS}

INCLUDES+=-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_POSIX_SOURCE -D_BSD_SOURCE

Wold_style_cast=


