PLATFORM ?= $(shell uname -s)
OPTIMIZE ?= 2
LTO ?= 0

VERSION ?= unknown
TARGET ?= darwin12
# OSX_VERSION_MIN is the old name of DEFAULT_DEPLOYMENT_TARGET, still
# accepted for direct Makefile builds.
ifneq ($(strip $(OSX_VERSION_MIN)),)
DEFAULT_DEPLOYMENT_TARGET ?= $(OSX_VERSION_MIN)
endif
DEFAULT_DEPLOYMENT_TARGET ?= 10.5
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
override CXXFLAGS+=-DOSXCROSS_DEFAULT_DEPLOYMENT_TARGET="\"$(DEFAULT_DEPLOYMENT_TARGET)\""
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

SRCS= \
 main.cpp \
 upstream-compiler-option-tables/options.cpp \
 tools/tools.cpp \
 tools/json.cpp \
 tools/plist.cpp \
 tools/process.cpp \
 tools/response-file.cpp \
 target.cpp \
 programs/programs.cpp \
 programs/argument-parser.cpp \
 programs/osxcross/osxcross-version.cpp \
 programs/osxcross/osxcross-env.cpp \
 programs/osxcross/osxcross-conf.cpp \
 programs/host/osxcross-man.cpp \
 programs/host/pkg-config.cpp \
 programs/macos/sw_vers.cpp \
 programs/xcode/xcrun.cpp \
 programs/xcode/xcodebuild.cpp \
 programs/xcode/sdk.cpp \
 programs/llvm/lipo.cpp \
 programs/llvm/ld.cpp \
 programs/llvm/as.cpp

# The object files, their dependency files and the wrapper binary go to
# obj/, in the same subdirectories as their sources, so that the source tree
# holds only sources.
OBJ_DIR=obj
OBJS=$(addprefix $(OBJ_DIR)/,$(SRCS:.cpp=.o))
DEPS=$(OBJS:.o=.d)
BIN=$(OBJ_DIR)/wrapper

.PHONY: all wrapper clean

all: wrapper

# "make wrapper" builds obj/wrapper.
wrapper: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR)

-include $(DEPS)
