# specify path to FLTK 1.3-x (mingw compiled)
FLTK_DIR=../../fltk-1.3-mingw32/
USE_FLTK_RUN=0
HAVE_SLOW_CPU=0

export ROOT?=$(PWD)/..

HOST=i686-w64-mingw32
CXX=$(shell sh -c 'which $(HOST)-g++')

ifeq ($(CXX),)
HOST=i586-pc-mingw32
CXX=$(shell sh -c 'which $(HOST)-g++')
endif

ifeq ($(CXX),)
HOST=i586-mingw32msvc
CXX=$(shell sh -c 'which $(HOST)-g++')
endif

ifeq ($(CXX),)
all clean install:
	@tput setaf 1
	@echo "cross compiler for $(HOST) not installed."
	@tput sgr0
else

APPLICATION=fltrator
LANDSCAPE=fltrator-landscape

FLTK_CONFIG=$(FLTK_DIR)fltk-config

# change path as required...
INSTALL_PATH=$(HOME)/Downloads/$(APPLICATION)
RSC_PATH=$(INSTALL_PATH)

SRC=$(ROOT)/src

OBJ1=\
	$(APPLICATION).o

OBJ2=\
	$(LANDSCAPE).o

OBJ3=\
	playsound.o

INCLUDE=-I$(ROOT)/include -I.

LDFLAGS=
LDLIBS=`$(FLTK_CONFIG) --use-images --ldstaticflags` -static-libgcc -static-libstdc++ -static -lwinmm
LDLIBSCPP=-static-libgcc -static-libstdc++ -static -lwinmm

FLTKCXXFLAGS = `$(FLTK_CONFIG) --cxxflags`
CXXDEFS+=-DUSE_FLTK_RUN=$(USE_FLTK_RUN)
CXXDEFS+=-DHAVE_SLOW_CPU=$(HAVE_SLOW_CPU)
CXXFLAGS+=$(CXXDEFS) -g -Wall --pedantic $(INCLUDE) $(FLTKCXXFLAGS)
#OPT=
OPT=-O3 -DNDEBUG

CP=cp
MV=mv
RM=rm
MKDIR=mkdir
PATCH=patch
TAR=tar

TARGET1=$(APPLICATION).exe

TARGET2=$(LANDSCAPE).exe

TARGET3=playsound.exe

$(TARGET1): depend $(OBJ1)
	@echo Linking $@...
	$(CXX) -o $@ $(LDFLAGS) $(OBJ1) $(LDLIBS)

$(TARGET2): depend $(OBJ2)
	@echo Linking $@...
	$(CXX) -o $@ $(LDFLAGS) $(OBJ2) $(LDLIBS)

$(TARGET3): depend $(OBJ3)
	@echo Linking $@...
	$(CXX) -o $@ $(LDFLAGS) $(OBJ3) $(LDLIBSCPP) -mwindows


.PHONY: clean all

all:: $(TARGET1) $(TARGET2) $(TARGET3)

%.o: $(SRC)/%.cxx
	@echo Compiling $@...
	$(CXX) -c -o $@ $< $(OPT) $(CXXFLAGS)

install: all
	@echo installing..
	mkdir -p $(INSTALL_PATH)
	mkdir -p $(RSC_PATH)/images
	mkdir -p $(RSC_PATH)/wav
	mkdir -p $(RSC_PATH)/levels
	$(CP) -f $(TARGET1) $(INSTALL_PATH)
	$(CP) -f $(TARGET2) $(INSTALL_PATH)
	$(CP) -f $(TARGET3) $(INSTALL_PATH)
	$(CP) -a ../images/*.gif $(RSC_PATH)/images/.
	$(CP) -a ../wav/*.wav $(RSC_PATH)/wav/.
	$(CP) -a ../ATTRIBUTION $(RSC_PATH)/.
	$(CP) -a ../README $(RSC_PATH)/.
	$(CP) -a ../levels/L_*.txt $(RSC_PATH)/levels/.

clean::
	$(RM) -f $(ALL) *.o core makedepend depend *~
	$(RM) -f $(TARGET1)
	$(RM) -f $(TARGET2)
	$(RM) -f $(TARGET3)

depend: $(SRC)/*.cxx
	@echo Make dependencies..
	$(CXX) $(CXXFLAGS) -M $(SRC)/*.cxx >makedepend
	# only a dummy
	touch depend

-include makedepend

endif
