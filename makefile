# Makefile for Multi-REMUXer Native Windows Application
# Requires Visual Studio Build Tools or MinGW-w64

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -municode
LDFLAGS = -static-libgcc -static-libstdc++
LIBS = -lcomctl32 -lshell32 -lole32 -luuid -lshlwapi -lgdi32

# Directories
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = $(SRCDIR)/main.cpp $(SRCDIR)/bdmv_parser.cpp $(SRCDIR)/ffmpeg_wrapper.cpp
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/MultiREMUXer.exe

# Resource file
RESOURCE_RC = $(SRCDIR)/resources.rc
RESOURCE_OBJ = $(OBJDIR)/resources.o

# Default target
all: $(TARGET)

# Create directories
$(OBJDIR):
	mkdir $(OBJDIR)

$(BINDIR):
	mkdir $(BINDIR)

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile resource file
$(RESOURCE_OBJ): $(RESOURCE_RC) | $(OBJDIR)
	windres $(RESOURCE_RC) -o $(RESOURCE_OBJ)

# Link executable
$(TARGET): $(OBJECTS) $(RESOURCE_OBJ) | $(BINDIR)
	$(CXX) $(OBJECTS) $(RESOURCE_OBJ) -o $(TARGET) $(LDFLAGS) $(LIBS)

# Clean build files
clean:
	if exist $(OBJDIR) rmdir /s /q $(OBJDIR)
	if exist $(BINDIR) rmdir /s /q $(BINDIR)

# Install (copy to system folder)
install: $(TARGET)
	copy $(TARGET) "C:\Program Files\MultiREMUXer\"

# Package for distribution
package: $(TARGET)
	mkdir dist
	copy $(TARGET) dist\
	copy ffmpeg.exe dist\
	copy README.txt dist\
	copy LICENSE.txt dist\
	"C:\Program Files\7-Zip\7z.exe" a -tzip MultiREMUXer_v1.0.zip dist\*

.PHONY: all clean install package

# Build configuration for Visual Studio
vs_build:
	cl /std:c++17 /O2 /EHsc $(SRCDIR)\main.cpp /Fe$(TARGET) /link comctl32.lib shell32.lib ole32.lib

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Release build with optimizations
release: CXXFLAGS += -O3 -DNDEBUG -s
release: $(TARGET)