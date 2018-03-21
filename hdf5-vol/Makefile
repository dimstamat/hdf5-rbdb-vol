# H5_DEFAULT_PLUGINDIR=...

H5_DEFAULT_PLUGINDIR=@0/HDF5Plugins
# Dim: link for linking Berkeley DB libraries, copy for copying
BDB_LIBS=link
VISH_LIB_PATH=/home/dimos/projects/hdf5/vish/lib/arch-Linuxx86_64-Debug/
VISH_BIN_PATH=/home/dimos/projects/hdf5/vish/bin/arch-Linuxx86_64-Debug/


TARGET=hdf5
TARGET_DIRS=toolslib tools

#MYDIR=$(shell sh -c "pwd")
MYDIR=$(abspath .)

SRC_C=${OBJ_PATH}/H5Tinit.c $(filter-out H5detect.c  H5make_libsettings.c, $(subst ${VPATH},,$(wildcard ${VPATH}*.c)))
SRC_C+=${OBJ_PATH}/H5lib_settings.c

LIBS_Linuxx86_64=-lpthread
LIBS_Linuxi686=-lpthread
LIBS =  $(LIBS_${MAKE_UNAME}) -lm ${DLLIB}

all.dtor=visible 

include $(VPATH)../GNUmakefile.rules

make-toolslib: all visible
make-tools: make-toolslib

SYSCFLAGS=-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64\
	-D_POSIX_SOURCE -D_BSD_SOURCE			\
	-UH5_DEBUG_API -D_HDF5DLL_			\
	-Wno-variadic-macros				\
	-Wno-long-long					\
	-DH5_BUILT_AS_DYNAMIC_LIB			\
	-DH5_DEFAULT_PLUGINDIR="\"$(H5_DEFAULT_PLUGINDIR)\""

SYSCFLAGS+=-msse4.2

INCLUDES+=-I$(OBJ_PATH)

# Dim: support for Berkeley DB include
BDB_INSTALL = /home/dimos/local
BDB_HOME = /home/dimos/Dropbox/phd/projects/hdf5/hdf5-rbdb-vol/retro-bdb
#BDB_ORIGINAL_HOME = /home/dimos/projects/hdf5/bdb-original/bdb-original-home
INCLUDES+=-I$(BDB_INSTALL)/include -I$(BDB_HOME)/src -I$(BDB_HOME)/build_unix

#
# Support for dmalloc library
#
LIBS += $(dmalloc_LIB)
SYSCFLAGS+=${dmalloc_FLAGS}

# Dim: support for Berkeley DB library
#LIBS += -L/home/dimos/local/lib -ldb

ifdef	zlib123_FLAGS
SYSCFLAGS+=-DH5_HAVE_ZLIB_H  -DH5_HAVE_FILTER_DEFLATE=1 ${zlib123_FLAGS}
LIBS+=${zlib123_LIB}
endif

ifdef	zlib1_FLAGS
SYSCFLAGS+=-DH5_HAVE_ZLIB_H  -DH5_HAVE_FILTER_DEFLATE=1 ${zlib1_FLAGS}
LIBS+=${zlib1_LIB}
endif

LIBS+=${aec_LIB}

# Dim: support for Berkeley DB library
TARGET_FLAGS+=-I$(MYDIR) ${zlib123_FLAGS} ${zlib1_FLAGS} -DWITH_BDB_VOL
TARGET_LIB+=$(LIBS_${MAKE_UNAME}) ${zlib123_LIB} ${zlib1_LIB} $${DLLIB} -L$(BDB_INSTALL)/lib -ldb ${aec_LIB}

show:
	echo MYDIR=$(MYDIR)
	echo $(TARGET_FLAGS)

#me=$(shell cd ${VPATH}; pwd)
me=$(abspath ${VPATH})

${OBJ_PATH}/H5Tinit.$O: ${OBJ_PATH}/H5Tinit.c
	$(CC) -I${VPATH}. -I${me} $(CFLAGS) -c $(SOURCEFILE_PARAMETER) $(OUTPUT_OBJECT)$@
	$(MAKEDEP)


${OBJ_PATH}/H5Tinit.c: ${OBJ_PATH}/H5detect.exe
	$< > $@

${OBJ_PATH}/H5detect.exe: ${VPATH}H5detect.c
	$(CC) $< -o $@


settings: ${OBJ_PATH}/H5lib_settings.c
	cat $<

${OBJ_PATH}/H5lib_settings.c: ${OBJ_PATH}/H5make_libsettings.exe
	$< > $@

${OBJ_PATH}/H5make_libsettings.exe: ${VPATH}H5make_libsettings.c
	$(CC) $< -o $@

# Dim: copy or make a link to the Berkeley DB libraries
# Do not link/copy the static libraries! We get the following error if we do so:
#/usr/bin/ld: /home/dimos/projects/hdf5/vish/lib/arch-Linuxx86_64-Debug/libdb.a(retro.o): relocation R_X86_64_32 against `.rodata' can not be used when making a shared object; recompile with -fPIC
#/home/dimos/projects/hdf5/vish/lib/arch-Linuxx86_64-Debug/libdb.a: error adding symbols: Bad value

ifeq ($(BDB_LIBS), link)
	@echo linking BDB libs...
	@ln -s $(BDB_INSTALL)/lib/*.la $(VISH_LIB_PATH)
	#@ln -s $(BDB_INSTALL)/lib/*.a $(VISH_LIB_PATH)
	@ln -s $(BDB_INSTALL)/lib/*.so $(VISH_BIN_PATH)
else ifeq ($(BDB_LIBS), copy)
	@echo copying BDB libs...
	@cp  $(BDB_INSTALL)/lib/*.la $(VISH_LIB_PATH)
	#@cp  $(BDB_INSTALL)/lib/*.a $(VISH_LIB_PATH)
	@cp $(BDB_INSTALL)/lib/*.so $(VISH_BIN_PATH)
endif


clean::	clean-libsettings

clean-libsettings:
	-${RM} ${OBJ_PATH}/H5make_libsettings.exe ${OBJ_PATH}/H5lib_settings.c


#HDF5_VERSION=hdf5-1.8.14
#HDF5_VERSION=hdf5-vol

#${VPATH}%.h: $(HDF5_VERSION)/src/%.h

#${VPATH}%.h: $(HDF5_VERSION)/src/%.h
#	cp $< $@

#${VPATH}%.c: $(HDF5_VERSION)/src/%.c
#	cp $< $@

