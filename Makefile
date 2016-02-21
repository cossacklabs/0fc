#
# Copyright (c) 2015 Cossack Labs Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

NACL_SDK_ROOT ?= /home/andrey/progs/libs/nacl_sdk/pepper_47/
PNACL_ROOT ?= /home/andrey/progs/libs/nacl_sdk/pepper_47/
#PNACL_ROOT=/home/andrey/progs/libs/nacl_sdk/pepper_44
#PNACL_TOOLCHAIN=/toolchain/linux_pnacl

# Project Build flags
WARNINGS := -Wno-long-long -Wall -Wswitch-enum -pedantic -Werror
CXXFLAGS := -pthread -std=gnu++98 $(WARNINGS)

#
# Compute tool paths
#
GETOS := python $(NACL_SDK_ROOT)/tools/getos.py
OSHELPERS = python $(NACL_SDK_ROOT)/tools/oshelpers.py
OSNAME := $(shell $(GETOS))
RM := $(OSHELPERS) rm

PNACL_TC_PATH := $(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_pnacl)
PNACL_CXX := $(PNACL_TC_PATH)/bin/pnacl-clang++
PNACL_FINALIZE := $(PNACL_TC_PATH)/bin/pnacl-finalize
CXXFLAGS := -g -I$(NACL_SDK_ROOT)/include -I$(NACL_SDK_ROOT)/include/pnacl -Iwebthemis/themis/src -Iwebthemis/themis/src/wrappers/themis webthemis/getentropy_pnacl.cc
LDFLAGS := -L$(NACL_SDK_ROOT)/lib/pnacl/Release -lppapi_cpp -lppapi -lnacl_io -ljsoncpp -Lwebthemis/build -lthemis -lsoter -lcrypto -lnacl_io --pnacl-exceptions=sjlj

#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN


all: secure_chat.pexe

clean:
	$(RM) static/*.pexe *.bc

secure_chat.bc: secure_chat.cc
	$(PNACL_CXX)  -std=gnu++11 -o $@ -O2 secure_chat.cc $(CXXFLAGS) $(LDFLAGS)

secure_chat.pexe: secure_chat.bc
	$(PNACL_FINALIZE) -o static/$@ $<


#
# Makefile target to run the SDK's simple HTTP server and serve this example.
#
HTTPD_PY := python $(NACL_SDK_ROOT)/tools/httpd.py --no-dir-check

.PHONY: serve
serve: all
	$(HTTPD_PY) -C "$(CURDIR)"
