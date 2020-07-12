PLATFORM ?= $(shell uname -s)
OPTIMIZE ?= 2
LTO ?= 0

VERSION ?= unknown
TARGET ?= darwin12
OSX_VERSION_MIN ?= 10.5
LINKER_VERSION ?= 134.9
LIBLTO_PATH ?=
ADDITIONAL_CXXFLAGS ?=

override CXXFLAGS=-std=c++0x -Wall -Wextra
override CXXFLAGS+=-Wno-missing-field-initializers
override CXXFLAGS+=-I. -O$(OPTIMIZE)

ifeq ($(LTO), 1)
  override CXXFLAGS+=-flto
endif

ifeq ($(OCDEBUG), 1)
  override CXXFLAGS+=-g
endif

override CXXFLAGS+=-DOSXCROSS_VERSION="\"$(VERSION)\""
override CXXFLAGS+=-DOSXCROSS_TARGET="\"$(TARGET)\""
override CXXFLAGS+=-DOSXCROSS_OSX_VERSION_MIN="\"$(OSX_VERSION_MIN)\""
override CXXFLAGS+=-DOSXCROSS_LINKER_VERSION="\"$(LINKER_VERSION)\""
override CXXFLAGS+=-DOSXCROSS_LIBLTO_PATH="\"$(LIBLTO_PATH)\""
override CXXFLAGS+=-DOSXCROSS_BUILD_DIR="\"$(BUILD_DIR)\""

override CXXFLAGS+= $(ADDITIONAL_CXXFLAGS)

ifneq (,$(findstring FreeBSD, $(PLATFORM)))
  override LDFLAGS+=-lutil
else ifneq (,$(findstring Darwin, $(PLATFORM)))
  override CXXFLAGS+=-Wno-deprecated
  override LDFLAGS+=-framework CoreServices -Wno-deprecated
else ifneq (,$(findstring CYGWIN, $(PLATFORM)))
  # -std=gnu++0x doesn't work, so work around...
  override CXXFLAGS+=-U__STRICT_ANSI__
else

ifeq (,$(findstring Windows, $(PLATFORM)))
ifeq (,$(findstring OpenBSD, $(PLATFORM)))
  override LDFLAGS+=-lrt
endif
endif

endif

BIN=wrapper

SRCS= \
 main.cpp \
 tools.cpp \
 target.cpp \
 programs/osxcross-version.cpp \
 programs/osxcross-env.cpp \
 programs/osxcross-conf.cpp \
 programs/osxcross-cmp.cpp \
 programs/osxcross-man.cpp \
 programs/sw_vers.cpp \
 programs/pkg-config.cpp \
 programs/xcrun.cpp \
 programs/xcodebuild.cpp \
 programs/dsymutil.cpp

OBJS=$(subst .cpp,.o,$(SRCS))

all: wrapper

wrapper: $(OBJS)
	$(CXX) $(CXXFLAGS) -o wrapper $(OBJS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f $(BIN) $(OBJS)

-include $(SRCS:cpp=d)
