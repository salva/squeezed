

#------------------ Project settings -----------------------------------------

#to support many file formats:
CONFIG=-DUSE_TAGLIB 
#for compressed screen updates:
#CONFIG+=-D_USE_LZF


#SRCS=$(wildcard *.cpp) 
SRCS=configParser.cpp fileInfo.cpp util.cpp\
	musicDB.cpp serverShoutCast.cpp slimProto.cpp slimIPC.cpp slimDisplay.cpp\
	main.cpp \
	TCPserverPosix.cpp \

#	lzf_c.c


HDRS =$(wildcard *.hpp)
HDRS+=$(wildcard *.h)


BIN_OUT=squeezed



#------------------ Default settings --------------------------

STRIP=strip
CXX=g++

CXXFLAGS=-Os -g -I/usr/include/taglib $(CONFIG)
#LDFLAGS+=
LIBS=-lpthread -ltag
OBJ_EXT=o



#------------------ OS specific settings -------------------------
HOST=$(shell uname)

ifeq ($(HOST),Linux)
	PLATFORM=$(shell uname -m)
	HOST=linux-$(PLATFORM)
endif


#------------------ Cross-compilation from 64 to 32 bit
#	this requires the following package under ubuntu
# 	libc6-dev-i386 g++-multilib
ifeq ($(TARGET),32bit)
	CXXFLAGS+=-m32 
	#-I/usr/include/i486-linux-gnu/ 
	LDFLAGS+=-m32 
	LIBS+=-lstdc++
	HOST=linux-x86
endif

ifeq ($(HOST),Cygwin)
	CXXFLAGS+=-D__CYGWIN__ 
	#--enable-pthreads
endif



#------------------ Scripting ------------------------------------------------

OBJDIR=obj/$(HOST)
BINDIR=bin/$(HOST)

OBJS=$(SRCS:%.cpp=$(OBJDIR)/%.$(OBJ_EXT))
## OBJS=$(OBJT:%.c=$(OBJDIR)/%.$(OBJ_EXT))

#------------------ Default compilation rules --------------------------------
$(OBJDIR)/%.o : %.c $(HDRS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/%.o : %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	

#------------------ Make targets ---------------------------------------------
all: squeezed 
#openWRTtests


paths:
	@if [ ! -d $(OBJDIR) ]; then mkdir -p $(OBJDIR); fi
	@if [ ! -d $(BINDIR) ]; then mkdir -p $(BINDIR); fi


#requires libc6-dev-i386 g++-multilib
_32bit:
	make TARGET=32bit


#requires mingw32
# NOT WORKING (yet). mingw doesn't (yet) have a dirent.d_type 
_win32:
	make CXX=i586-mingw32msvc-c++ HOST=win32 STRIP=echo


#Some tests to determine what causes libstdc++ to be linked in:
# _zlib doesn't link to libstdc++, _tag does...
openWRTtests: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -luClibc++ main_hello.cpp -o $(BINDIR)/$(BIN_OUT)_hello 
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -luClibc++ -lz main_zlib.cpp -o $(BINDIR)/$(BIN_OUT)_zlib
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -luClibc++ -lz -ltag main_tag.cpp -o $(BINDIR)/$(BIN_OUT)_tag


lzf: paths
	(cd lzf; $(CXX) $(CXXFLAGS) -c -o ../$(OBJDIR)/lzf.o lzf_c.c)
	#echo $(OBJS)
	OBJS=$(OBJS) lzf.o
	#echo $(OBJS)

squeezed: paths $(OBJS)
#	echo $(HDRS)
#	ar rs $(BINDIR)/libSqueezed.lib $(OBJS)
	$(CXX) $(LDFLAGS) $(LIBS) $(OBJS) -o $(BINDIR)/$(BIN_OUT)
	$(STRIP) $(BINDIR)/$(BIN_OUT)

clean:
	-rm -r $(OBJDIR)/*.$(OBJ_EXT)
	-rm -r $(BINDIR)/*
