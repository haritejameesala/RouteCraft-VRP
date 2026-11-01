CXX ?= g++
JAVA_HOME ?= $(shell powershell -NoProfile -Command "(Get-Command javac).Source | Split-Path -Parent | Split-Path -Parent" 2>NUL)

ifeq ($(OS),Windows_NT)
ifeq ($(origin CXX),default)
ifneq ($(wildcard C:/msys64/ucrt64/bin/g++.exe),)
CXX := C:/msys64/ucrt64/bin/g++
export PATH := C:/msys64/ucrt64/bin;C:/msys64/usr/bin;$(PATH)
endif
endif
endif

CPPFLAGS += -Iinclude/DSA -Iinclude/JNI
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -O2
JNI_CPPFLAGS := -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/win32"

SOURCE_DIR := Source
DSA_DIR := $(SOURCE_DIR)/DSA
JAVA_DIR := $(SOURCE_DIR)/Java
JNI_DIR := $(SOURCE_DIR)/JNI
OBJECT_DIR := object

CORE_SOURCES := \
    $(DSA_DIR)/RouteObjects.cpp \
    $(DSA_DIR)/RouteConstructionAlgorithms.cpp \
    $(DSA_DIR)/RouteImprovementAlgorithms.cpp \
    $(DSA_DIR)/RoadNetworkAlgorithms.cpp
JNI_SOURCE := $(JNI_DIR)/RouteOptimizerJni.cpp

CORE_OBJECTS := $(patsubst $(DSA_DIR)/%.cpp,$(OBJECT_DIR)/%.o,$(CORE_SOURCES))
JNI_OBJECT := $(OBJECT_DIR)/RouteOptimizerJni.o

DLL := build/RouteCraftDsa.dll
APP_OBJECT := $(OBJECT_DIR)/RouteCraftDsaTests.o
JAVA_SOURCES := \
    $(JAVA_DIR)/RouteCraftVRP.java \
    $(JAVA_DIR)/RouteOptimizerBridge.java \
    $(JAVA_DIR)/RouteOptimizerBridgeTest.java
JAVA_STAMP := build/.java-classes

.PHONY: all java test clean

all: a.out $(DLL) java

$(OBJECT_DIR):
	mkdir -p $@

build:
	mkdir -p $@

$(OBJECT_DIR)/%.o: $(DSA_DIR)/%.cpp | $(OBJECT_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(JNI_CPPFLAGS) -c $< -o $@

$(JNI_OBJECT): $(JNI_SOURCE) | $(OBJECT_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(JNI_CPPFLAGS) -c $< -o $@

a.out: $(APP_OBJECT) $(CORE_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(DLL): $(CORE_OBJECTS) $(JNI_OBJECT) | build
	$(CXX) -shared $^ -o $@

java: $(JAVA_STAMP)

$(JAVA_STAMP): $(JAVA_SOURCES) | build
	javac -encoding UTF-8 -d build $(JAVA_SOURCES)
	touch $@

test: all
	./a.out
	java '-Djava.library.path=build' -cp build RouteOptimizerBridgeTest

clean:
	rm -rf $(OBJECT_DIR) build a.out a.out.exe
