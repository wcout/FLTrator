# specify path to FLTK 1.3-x (if FLTK is not installed)
#FLTK_DIR=../fltk-1.3/
USE_FLTK_RUN=1
HAVE_SLOW_CPU=0

export ROOT?=$(PWD)
APPLICATION=fltrator
LANDSCAPE=fltrator-landscape

FLTK_CONFIG=$(FLTK_DIR)fltk-config

RSC_PATH=$(HOME)/.$(APPLICATION)
INSTALL_PATH=$(HOME)/bin

SRC=$(ROOT)/.

OBJ1=\
	$(APPLICATION).o

OBJ2=\
	$(LANDSCAPE).o

INCLUDE=-I$(ROOT)/include -I.

LDFLAGS=
LDLIBS=`$(FLTK_CONFIG) --use-images --ldstaticflags`

FLTKCXXFLAGS = `$(FLTK_CONFIG) --cxxflags`
CXXDEFS+=-DUSE_FLTK_RUN=$(USE_FLTK_RUN)
CXXDEFS+=-DHAVE_SLOW_CPU=$(HAVE_SLOW_CPU)
CXXFLAGS+=$(CXXDEFS) -g -Wall --pedantic $(INCLUDE) $(FLTKCXXFLAGS)
#OPT=
OPT=-O3

CXX=g++
CP=cp
MV=mv
RM=rm
MKDIR=mkdir
PATCH=patch
TAR=tar

TARGET1=$(APPLICATION)

TARGET2=$(LANDSCAPE)

$(TARGET1): depend $(OBJ1)
	@echo Linking $@...
	$(CXX) -o $@ $(LDFLAGS) $(OBJ1) $(LDLIBS) -lrt $(OPT)

$(TARGET2): depend $(OBJ2)
	@echo Linking $@...
	$(CXX) -o $@ $(LDFLAGS) $(OBJ2) $(LDLIBS) $(OPT)


.PHONY: clean all

all:: $(TARGET1) $(TARGET2)

%.o: $(SRC)/%.cxx
	@echo Compiling $@...
	$(CXX) -c -o $@ $< $(CXXFLAGS)

install: all
	@echo installing..
	mkdir -p $(INSTALL_PATH)
	mkdir -p $(RSC_PATH)/images
	mkdir -p $(RSC_PATH)/wav
	mkdir -p $(RSC_PATH)/levels
	$(CP) -f $(ROOT)/$(TARGET1) $(INSTALL_PATH)
	$(CP) -f $(ROOT)/$(TARGET2) $(INSTALL_PATH)
	$(CP) -a $(ROOT)/images/*.gif $(RSC_PATH)/images/.
	$(CP) -a $(ROOT)/wav/*.wav $(RSC_PATH)/wav/.
	$(CP) -a $(ROOT)/ATTRIBUTION $(RSC_PATH)/.
	$(CP) -a $(ROOT)/levels/L_*.txt $(RSC_PATH)/levels/.

uninstall:
	@echo uninstalling..
	rm $(INSTALL_PATH)/$(TARGET1)
	rm $(INSTALL_PATH)/$(TARGET2)
	rm -rf $(RSC_PATH)

clean::
	$(RM) -f $(ALL) *.o core makedepend depend *~
	$(RM) -f $(TARGET1)
	$(RM) -f $(TARGET2)

depend: $(SRC)/*.cxx
	@echo Make dependencies..
	$(CXX) $(CXXFLAGS) -M $(SRC)/*.cxx >makedepend
	# only a dummy
	touch depend

-include makedepend
