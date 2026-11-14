PLATFORM ?= $(shell uname -s)
OPTIMIZE ?= 2
LTO ?= 0

VERSION ?= unknown
TARGET ?= darwin12
OSX_VERSION_MIN ?= 10.5
LINKER_VERSION ?= 134.9
BUILD_FLAVOR ?= stable
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

ifeq ($(strip $(SUPPORTED_ARCHS)),)
  $(error SUPPORTED_ARCHS is not set. Rebuild from scratch.)
endif

override CXXFLAGS+=-DOSXCROSS_VERSION="\"$(VERSION)\""
override CXXFLAGS+=-DOSXCROSS_BUILD_FLAVOR="\"$(BUILD_FLAVOR)\""
override CXXFLAGS+=-DOSXCROSS_TARGET="\"$(TARGET)\""
override CXXFLAGS+=-DOSXCROSS_OSX_VERSION_MIN="\"$(OSX_VERSION_MIN)\""
override CXXFLAGS+=-DOSXCROSS_LINKER_VERSION="\"$(LINKER_VERSION)\""
override CXXFLAGS+=-DOSXCROSS_LIBLTO_PATH="\"$(LIBLTO_PATH)\""
override CXXFLAGS+=-DOSXCROSS_BUILD_DIR="\"$(BUILD_DIR)\""
override CXXFLAGS+=-DOSXCROSS_SUPPORTED_ARCHS="\"$(SUPPORTED_ARCHS)\""
override CXXFLAGS+=-DOSXCROSS_GCC_TARGET_ARCHS="\"$(GCC_TARGET_ARCHS)\""

override CXXFLAGS+= $(ADDITIONAL_CXXFLAGS)

# Write a .d file with the headers of each object, so that a changed header
# rebuilds the objects that include it. -MP keeps the build working after a
# header has been removed.
override CPPFLAGS+=-MMD -MP

ifneq (,$(findstring FreeBSD, $(PLATFORM)))
  override LDFLAGS+=-lutil
else ifneq (,$(findstring Darwin, $(PLATFORM)))
  override CXXFLAGS+=-Wno-deprecated
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
 tools/tools.cpp \
 tools/json.cpp \
 target.cpp \
 progs.cpp \
 programs/osxcross-version.cpp \
 programs/osxcross-env.cpp \
 programs/osxcross-conf.cpp \
 programs/osxcross-man.cpp \
 programs/sw_vers.cpp \
 programs/pkg-config.cpp \
 programs/xcrun.cpp \
 programs/xcodebuild.cpp \
 programs/lipo.cpp \
 programs/ld.cpp \
 programs/as.cpp

OBJS=$(subst .cpp,.o,$(SRCS))
DEPS=$(OBJS:.o=.d)

all: wrapper

wrapper: $(OBJS)
	$(CXX) $(CXXFLAGS) -o wrapper $(OBJS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f $(BIN) $(OBJS) $(DEPS)

-include $(DEPS)
