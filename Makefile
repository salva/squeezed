


#---------- Project settings

#SRCS=$(wildcard *.cpp) 
SRCS=configParser.cpp fileInfo.cpp util.cpp\
	musicDB.cpp serverShoutCast.cpp slimProto.cpp slimIPC.cpp slimDisplay.cpp\
	main.cpp \
	TCPserverPosix.cpp 
HDRS =$(wildcard *.hpp)
HDRS+=$(wildcard *.h)


BIN_OUT=SqueezeD


#------------ cross-compilation from 64 to 32 bit


#---------- OS specific settings
HOST=$(shell uname)

#	this requires the following package under ubuntu
# 	libc6-dev-i386 g++-multilib
ifeq ($(TARGET),32bit)
	CFLAGS=-m32 
	#-I/usr/include/i486-linux-gnu/ 
	LFLAGS=-m32 -lstdc++
	HOST=linux32
endif


CC=g++
OBJDIR= obj/$(HOST)
BINDIR = bin/$(HOST)

CFLAGS+=-Os -g
LFLAGS+=-Os -lpthread
OBJ_EXT=o

ifeq ($(HOST),Cygwin)
	CFLAGS+=-D__CYGWIN__ -I/usr/include 
	#--enable-pthreads
endif



#---------- scripting

OBJS=$(SRCS:%.cpp=$(OBJDIR)/%.$(OBJ_EXT))


#---------- default compilation rules
$(OBJDIR)/%.o : %.c $(HDRS)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : %.cpp $(HDRS)
	$(CC) -c $(CFLAGS) -o $@ $<
	


all: squeezed


paths:
	@if [ ! -d $(OBJDIR) ]; then mkdir -p $(OBJDIR); fi
	@if [ ! -d $(BINDIR) ]; then mkdir -p $(BINDIR); fi

_32bit:
	make TARGET=32bit

squeezed: paths $(OBJS)
#	echo $(HDRS)
#	ar rs $(BINDIR)/libNM.lib $(OBJS_NM)
	$(CC) $(LFLAGS) -o $(BINDIR)/$(BIN_OUT) $(OBJS) 
	strip $(BINDIR)/$(BIN_OUT)

clean:
	-rm -r $(OBJDIR)/*.$(OBJ_EXT)
	-rm -r $(BINDIR)/*
