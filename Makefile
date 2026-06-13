# Makefile for pdf2xml - Windows/MinGW build
# Requires: g++ (with MinGW), mingw32-make

SRCDIR = pdf2xml
XPDFDIR = $(SRCDIR)/xpdf
GOODIR = $(XPDFDIR)/goo
XPDFPDF = $(XPDFDIR)/xpdf
FOFIDIR = $(XPDFDIR)/fofi
PNGDIR = $(SRCDIR)/image/png
ZLIBDIR = $(SRCDIR)/image/zlib

CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -DWIN32 -O2
CFLAGS = -std=c11 -DWIN32 -O2
INCLUDES = -I$(SRCDIR) -I$(XPDFDIR) -I$(GOODIR) -I$(XPDFPDF) -I$(FOFIDIR) -I$(PNGDIR) -I$(ZLIBDIR)
LDFLAGS = -static

# Lint-like flags to suppress known legacy warnings
WARNFLAGS = -Wno-write-strings -Wno-format-security

# --- C++ sources (.cc and .cpp) ---
XPDF_CCS = \
	$(GOODIR)/gfile.cc \
	$(GOODIR)/GHash.cc \
	$(GOODIR)/GList.cc \
	$(GOODIR)/gmempp.cc \
	$(GOODIR)/GString.cc \
	$(FOFIDIR)/FoFiBase.cc \
	$(FOFIDIR)/FoFiEncodings.cc \
	$(FOFIDIR)/FoFiTrueType.cc \
	$(FOFIDIR)/FoFiType1.cc \
	$(FOFIDIR)/FoFiType1C.cc \
	$(XPDFPDF)/Annot.cc \
	$(XPDFPDF)/Array.cc \
	$(XPDFPDF)/BuiltinFont.cc \
	$(XPDFPDF)/BuiltinFontTables.cc \
	$(XPDFPDF)/Catalog.cc \
	$(XPDFPDF)/CharCodeToUnicode.cc \
	$(XPDFPDF)/CMap.cc \
	$(XPDFPDF)/Decrypt.cc \
	$(XPDFPDF)/Dict.cc \
	$(XPDFPDF)/Error.cc \
	$(XPDFPDF)/FontEncodingTables.cc \
	$(XPDFPDF)/Function.cc \
	$(XPDFPDF)/Gfx.cc \
	$(XPDFPDF)/GfxFont.cc \
	$(XPDFPDF)/GfxState.cc \
	$(XPDFPDF)/GlobalParams.cc \
	$(XPDFPDF)/JArithmeticDecoder.cc \
	$(XPDFPDF)/JBIG2Stream.cc \
	$(XPDFPDF)/JPXStream.cc \
	$(XPDFPDF)/Lexer.cc \
	$(XPDFPDF)/Link.cc \
	$(XPDFPDF)/NameToCharCode.cc \
	$(XPDFPDF)/Object.cc \
	$(XPDFPDF)/Outline.cc \
	$(XPDFPDF)/OutputDev.cc \
	$(XPDFPDF)/Page.cc \
	$(XPDFPDF)/Parser.cc \
	$(XPDFPDF)/PDFDoc.cc \
	$(XPDFPDF)/PDFDocEncoding.cc \
	$(XPDFPDF)/PSTokenizer.cc \
	$(XPDFPDF)/SecurityHandler.cc \
	$(XPDFPDF)/Stream.cc \
	$(XPDFPDF)/UnicodeMap.cc \
	$(XPDFPDF)/XRef.cc

# Main source
MAIN_CPP = $(SRCDIR)/pdf2xml.cpp

# --- C sources ---
C_SRCS = \
	$(GOODIR)/gmem.c \
	$(PNGDIR)/png.c \
	$(PNGDIR)/pngerror.c \
	$(PNGDIR)/pnggccrd.c \
	$(PNGDIR)/pngget.c \
	$(PNGDIR)/pngmem.c \
	$(PNGDIR)/pngpread.c \
	$(PNGDIR)/pngread.c \
	$(PNGDIR)/pngrio.c \
	$(PNGDIR)/pngrtran.c \
	$(PNGDIR)/pngrutil.c \
	$(PNGDIR)/pngset.c \
	$(PNGDIR)/pngtrans.c \
	$(PNGDIR)/pngvcrd.c \
	$(PNGDIR)/pngwio.c \
	$(PNGDIR)/pngwrite.c \
	$(PNGDIR)/pngwtran.c \
	$(PNGDIR)/pngwutil.c \
	$(ZLIBDIR)/adler32.c \
	$(ZLIBDIR)/compress.c \
	$(ZLIBDIR)/crc32.c \
	$(ZLIBDIR)/deflate.c \
	$(ZLIBDIR)/gzio.c \
	$(ZLIBDIR)/infback.c \
	$(ZLIBDIR)/inffast.c \
	$(ZLIBDIR)/inflate.c \
	$(ZLIBDIR)/inftrees.c \
	$(ZLIBDIR)/trees.c \
	$(ZLIBDIR)/uncompr.c \
	$(ZLIBDIR)/zutil.c

# --- Object files ---
XPDF_OBJS = $(XPDF_CCS:.cc=.o)
MAIN_OBJ = $(MAIN_CPP:.cpp=.o)
C_OBJS = $(C_SRCS:.c=.o)

ALL_OBJS = $(XPDF_OBJS) $(MAIN_OBJ) $(C_OBJS)

TARGET = pdf2xml.exe

# --- Rules ---
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(WARNFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(WARNFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(WARNFLAGS) $(INCLUDES) -c $< -o $@

clean:
	-del /q $(subst /,\,$(ALL_OBJS)) 2>nul
	-del /q $(subst /,\,$(TARGET)) 2>nul
