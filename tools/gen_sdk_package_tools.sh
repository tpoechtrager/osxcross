#!/usr/bin/env bash
#
# Package the macOS SDKs into a tar file to be used by `build.sh`.
# As opposed to gen_sdk_package.sh, which is used for extraction of SDKs
# from full Xcode version, gen_sdk_tools.sh extracts SDKs from Xcode
# Command Line Tools.
#
# Tested with XCode Command Line Tools 12.x
#

XCODE_TOOLS=1 ./tools/gen_sdk_package.sh
