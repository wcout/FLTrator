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
all clean install res:
	@tput setaf 1
	@echo "cross compiler for $(HOST) not installed."
	@tput sgr0
else
WINDRES=$(shell sh -c 'which $(HOST)-windres')

APPLICATION=fltrator
LANDSCAPE=fltrator-landscape

FLTK_CONFIG=$(FLTK_DIR)fltk-config

# change path as required...
INSTALL_PATH=$(HOME)/Downloads/$(APPLICATION)
RSC_PATH=$(INSTALL_PATH)
TARGET_ROOT=$(HOME)

SRC=$(ROOT)/src

OBJ1=\
	$(APPLICATION).o

OBJ2=\
	$(LANDSCAPE).o

OBJ3=\
	playsound.o

INCLUDE=-I$(ROOT)/include -I.

LDFLAGS=
LDLIBS=`$(FLTK_CONFIG) --use-images --ldstaticflags` -static -lgcc -static -lstdc++ -static -lwinmm
LDLIBSCPP=-static -lgcc -static -lstdc++ -dynamic -static -lwinmm

FLTKCXXFLAGS = `$(FLTK_CONFIG) --cxxflags`
CXXDEFS+=-DUSE_FLTK_RUN=$(USE_FLTK_RUN)
CXXDEFS+=-DHAVE_SLOW_CPU=$(HAVE_SLOW_CPU)
CXXFLAGS+=$(CXXDEFS) -g -Wall -pedantic $(INCLUDE) $(FLTKCXXFLAGS)
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

export TARGET_NAME=$(APPLICATION)-win32
export TARGET_PATH=$(TARGET_ROOT)/$(TARGET_NAME)

.PHONY: clean all target

all:: $(TARGET1) $(TARGET2) $(TARGET3)

$(TARGET1): depend $(OBJ1) $(APPLICATION).res
	@echo Linking $@...
	$(CXX) -o $@ $(LDFLAGS) $(OBJ1) fltrator.res $(LDLIBS)

$(TARGET2): depend $(OBJ2)
	@echo Linking $@...
	$(CXX) -o $@ $(LDFLAGS) $(OBJ2) $(LDLIBS)

$(TARGET3): depend $(OBJ3)
	@echo Linking $@...
	$(CXX) -o $@ $(LDFLAGS) $(OBJ3) $(LDLIBSCPP) -mwindows

%.o: $(SRC)/%.cxx
	@echo Compiling $@...
	$(CXX) -c -o $@ $< $(OPT) $(CXXFLAGS)

%.res: %.rc
	@echo Compiling $@...
	@echo Make resource $@..
	$(WINDRES) $< -O coff -o $@

install: all
	@echo installing..
	mkdir -p $(INSTALL_PATH)
	mkdir -p $(RSC_PATH)/images
	mkdir -p $(RSC_PATH)/wav
	mkdir -p $(RSC_PATH)/levels
	mkdir -p $(RSC_PATH)/bgsound
	mkdir -p $(RSC_PATH)/demo
	$(CP) -f $(TARGET1) $(INSTALL_PATH)
	$(CP) -f $(TARGET2) $(INSTALL_PATH)
	$(CP) -f $(TARGET3) $(INSTALL_PATH)
	cd ..; find images -name "*.gif" | while read file; do \
	dest=$(RSC_PATH)/"$$file"; \
	destd=`dirname "$$dest"`; \
	mkdir -p "$$destd"; \
	$(CP) -a "$$file" "$$dest"; \
	done
	cd ..; find wav -name "*.wav" | while read file; do \
	dest=$(RSC_PATH)/"$$file"; \
	destd=`dirname "$$dest"`; \
	mkdir -p "$$destd"; \
	$(CP) -a "$$file" "$$dest"; \
	done
	cd ..; find bgsound -name "*.wav" | while read file; do \
	dest=$(RSC_PATH)/"$$file"; \
	destd=`dirname "$$dest"`; \
	mkdir -p "$$destd"; \
	$(CP) -a "$$file" "$$dest"; \
	done
	$(CP) -a ../bgsound/README $(RSC_PATH)/bgsound/.
	$(CP) -a ../demo/*.txt $(RSC_PATH)/demo/.
	$(CP) -a ../ATTRIBUTION $(RSC_PATH)/.
	$(CP) -a ../README.md $(RSC_PATH)/.
	$(CP) -a ../levels/L_*.txt $(RSC_PATH)/levels/.

target: all
	@echo TARGET_ROOT $(TARGET_ROOT)
	@echo make target $(TARGET_PATH)
	@if [ "$(TARGET_PATH)" = "" ]; then echo "TARGET_PATH not set" && exit 1; fi
	@if [ "$(TARGET_PATH)" = "/" ]; then echo "Invalid TARGET_PATH" && exit 1; fi
	mkdir -p $(TARGET_PATH) &&	cd $(TARGET_PATH) && rm -rf *
	mkdir -p $(TARGET_PATH)/images
	mkdir -p $(TARGET_PATH)/wav
	mkdir -p $(TARGET_PATH)/levels
	mkdir -p $(TARGET_PATH)/bgsound
	$(CP) -f $(TARGET1) $(TARGET_PATH)
	$(CP) -f $(TARGET2) $(TARGET_PATH)
	$(CP) -f $(TARGET3) $(TARGET_PATH)
	strip $(TARGET_PATH)/$(TARGET1)
	strip $(TARGET_PATH)/$(TARGET2)
	strip $(TARGET_PATH)/$(TARGET3)
	cd ..; find images -name "*.gif" | while read file; do \
	dest=$(TARGET_PATH)/"$$file"; \
	destd=`dirname "$$dest"`; \
	mkdir -p "$$destd"; \
	$(CP) -a "$$file" "$$dest"; \
	done
	cd ..; find wav -name "*.wav" | while read file; do \
	dest=$(TARGET_PATH)/"$$file"; \
	destd=`dirname "$$dest"`; \
	mkdir -p "$$destd"; \
	$(CP) -a "$$file" "$$dest"; \
	done
	cd ..; find bgsound -name "*.wav" | while read file; do \
	dest=$(TARGET_PATH)/"$$file"; \
	destd=`dirname "$$dest"`; \
	mkdir -p "$$destd"; \
	$(CP) -a "$$file" "$$dest"; \
	done
	cd ..; find demo -name "*.txt" | while read file; do \
	dest=$(TARGET_PATH)/"$$file"; \
	destd=`dirname "$$dest"`; \
	mkdir -p "$$destd"; \
	$(CP) -a "$$file" "$$dest"; \
	done
	$(CP) -a $(ROOT)/ATTRIBUTION $(TARGET_PATH)/.
	$(CP) -a $(ROOT)/README.md $(TARGET_PATH)/.
	$(CP) -a $(ROOT)/levels/L_*.txt $(TARGET_PATH)/levels/.
	$(CP) -a $(ROOT)/rsc $(TARGET_PATH)
	$(CP) -a *.ico $(TARGET_PATH)
	cd $(TARGET_ROOT) && zip -r $(TARGET_PATH).zip $(TARGET_NAME)
	cd $(TARGET_ROOT) && sha1sum $(TARGET_NAME).zip > sha1sum.txt && md5sum $(TARGET_NAME).zip > md5sum.txt

clean::
	$(RM) -f $(ALL) *.o *.res core makedepend depend *~
	$(RM) -f $(TARGET1)
	$(RM) -f $(TARGET2)
	$(RM) -f $(TARGET3)

res: *.rc
	@echo Make resources..
	$(WINDRES) $(APPLICATION).rc -O coff -o $(APPLICATION).res

depend: $(SRC)/*.cxx
	@echo Make dependencies..
	$(CXX) $(CXXFLAGS) -M $(SRC)/*.cxx >makedepend
	# only a dummy
	touch depend

-include makedepend

endif
