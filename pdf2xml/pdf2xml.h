//**************************************************************
//*  Copyright (c) 2005 Mobipocket.com
//*  Authors: Martin Görner, Fabien Hertschuh
//*  File: pdf2xml.h
//*  Description: conversion from PDF to XML using xpdf parser
//*  Platform: cross
//*
//*  This project is an open source project,
//*  it is licensed under the GNU General Public License (GPL)
//*    Copyright (c) 2005 Mobipocket.com
//*    http://www.mobipocket.com/pdf2xml/
//*
//*  This project uses the open source project xpdf,
//*  xpdf is licensed under the GNU General Public License (GPL)
//*    Copyright (c) 1996-2004 Glyph & Cog, LLC.
//*    derekn@foolabs.com
//*    http://www.foolabs.com/xpdf/
//*
//*  This project uses the open source project libpng
//*    Copyright (c) 1998-2004 Glenn Randers-Pehrson
//*    Copyright (c) 1996-1997 Andreas Dilger
//*    Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.
//*    glennrp@users.sourceforge.net
//*    http://www.libpng.org/
//*
//*  The libpng uses the open source project zlib
//*    Copyright (c) 1995-2003 Jean-loup Gailly and Mark Adler
//*    jloup@gzip.org
//*    madler@alumni.caltech.edu
//*    http://www.zlib.org/
//*
//*  PDF is a registered trademark of Adobe Systems, Inc.
//**************************************************************

#ifndef _PDF2XML_H
#define _PDF2XML_H

// GNU pdf libs
#include "GList.h"
#include "PDFDoc.h"
#include "OutputDev.h"

// PNG lib
#include "png.h"

//-------------- classes --------------------

// Simle class to describe rectangular bounds
class Rect
{
public:

	// Surface of the rectangle
	inline int surface() const { return width * height; }

	// Compute the intersection of this rectangle and <other>, result is in <rect>
	bool is_intersecting (const Rect &other, Rect &rect) const;

	// Enlarge the current rectangle to contain <contained>
	void enlarge_to_contain (const Rect &contained);

	int x;
	int y;
	int width;
	int height;
};

// Simple class to save picture references
class PictureReference
{
public:

	PictureReference (int ref, int flip, int number, const char* const extension) :
		reference_number(ref),
		picture_flip(flip),
		picture_number(number),
		picture_extension(extension)
	{}

	int					reference_number;
	int					picture_flip;		// 0 = none, 1 = flip X, 2 = flip Y, 3 = flip both
	int					picture_number;
	const char *const	picture_extension;
};

// Output XML in a file
class XmlOutput
{
public:

	// Constructor
	XmlOutput ();

	// returns true on error
	bool open (GString& filename);
	bool write (const char* content);
	bool write (int number);
	bool close ();

	// glue function for loading a PDF
	// returns true on error
	bool load_from_pdf (GString& pdf_file_name, GString& picture_base_name);

	// Add a meta tag <tag> if <value> is not NULL
	// This looks for a Byte Order Mark at the begining of <value> to convert the
	// text to UTF8 and XML encode < > and &
	// returns true on error (not added)
	bool add_metatag (const char* tag, GString* value);

	// create a new page
	// return true on error
	bool start_page (int width, int height);

	// 
	bool change_font (GString* face, int size, int color, bool bold, bool italic);

	// add an absolute position link (dest page is zero-based)
	bool add_link (const Rect& rect, int dest_page, int dest_x, int dest_y);

	// add an external link
	bool add_link (const Rect& rect, GString& dest_url);

	// Add a block of text. The block is attached to the current page.
	// An error occurs if there is no current page.
	// return true on error
	bool add_text_block (GString& str, const Rect& rect);

	// Add a picture
	// An error occurs if there is no current page.
	// return true on error
	bool add_image_block(GString& filename, const Rect& rect);

private:

	// the underlying file
	FILE* xml_file;

	// A page tag has been opened yet
	bool page_opened;

	// A font tag has been opened yet
	bool font_opened;
};

// subclass the output device class
class MbpOutputDev: public OutputDev
{
public:
	// constructor
	MbpOutputDev(XmlOutput& target, GString& picture_base_name);

	// destructor
	~MbpOutputDev();

	//---- get info about output device
	
	// Does this device use upside-down coordinates?
	// (Upside-down means (0,0) is the top left corner of the page.)
	virtual GBool upsideDown() { return gTrue; }
	
	// Does this device use drawChar() or drawString()?
	virtual GBool useDrawChar() { return gFalse; }
	
	// Does this device use beginType3Char/endType3Char?  Otherwise,
	// text in Type 3 fonts will be drawn with drawChar/drawString.
	virtual GBool interpretType3Chars() { return gFalse; }
	
	// Does this device need non-text content?
	virtual GBool needNonText() { return gTrue; }

	//----- image drawing
	virtual void drawImageMask(GfxState *state, Object *ref, Stream *str,
				int width, int height, GBool invert,
				GBool inlineImg);
	virtual void drawImage(GfxState *state, Object *ref, Stream *str,
				int width, int height, GfxImageColorMap *colorMap,
				int *maskColors, GBool inlineImg);

	//----- initialization and control
	
	// Start a page.
	virtual void startPage (int pageNum, GfxState *state);
	
	// End a page.
	virtual void endPage ();
	
	// Update text state
	virtual void updateFont (GfxState *state);
	
	// Text drawing
	virtual void drawString (GfxState *state, GString *s);

	// Links
	virtual void drawLink (Link *link, Catalog *catalog);

	// round off to closest integer
	static inline int round (double x)
	{
		return (int)(x+0.5);
	}

	// clamp to uint8
	static inline int clamp (int x)
	{
		if (x > 255) return 255;
		if (x < 0) return 0;
		return x;
	}

private:

	// validate the block being coealesced
	void flush_coalesc_blocks ();

	// invalidate the block being coealesced
	void invalidate_coalesc_blocks ();

	// true if 'str' is equal to the string currently in the coalescence block
	// the comparison ignores spaces
	bool compare_with_coalesc (GString& str);

	// append a chunk to the block being coealesced
	void append_coalesc_block (GString& str, const Rect& rect, bool prepend_space);

	// output a picture block into the block stream
	void append_image_block (int x, int y, int width, int height, GString& pic_filename);

	// dimensions of a string
	// also converts the string and stores it into "output_text"
	// returned text will be encoded in UTF8
	// Returns a reference to the output string
	GString& handle_string (GfxState *state, GString *s, double& width, double& height);

	// build the name for a file from a base namen a number and an extension
	static void compose_image_filename (GString& base_name, int num, const char *const ext, GString& result);

	// utility function used by drawImage and drawImageMask
	void drawImageOrMask (GfxState *state, Object *ref, Stream *str,
						  int width, int height,
						  GfxImageColorMap *colorMap,
						  int *maskColors, GBool inlineImg, bool mask);

	// utility function to save raw data to a png file using the ong lib
	bool save_png (GString& file_name,
				   unsigned int width, unsigned int height, unsigned int row_stride,
				   unsigned char* data,
				   unsigned char bpp = 24, unsigned char color_type = PNG_COLOR_TYPE_RGB,
				   png_color* palette = NULL, unsigned short color_count = 0);

	// XML output stream
	XmlOutput&	dev_output;
	GfxState*	dev_page_state;

	// current font information
	GString		dev_current_font_face;
	bool		dev_current_font_bold;
	bool		dev_current_font_italic;
	int			dev_current_font_size;
	int			dev_current_font_color;
	bool		dev_font_has_changed;

	// string coalescence computations
	double		last_x, last_y, last_w, last_h;
	double		last_page_w, last_page_h;
	Rect		last_rect;

	// string coalescence
	GString		dev_coalesc_content;
	Rect		dev_coalesc_rect;
	bool		dev_coalesc_valid;

	// pictures
	GList		dev_picture_references;
	GString&	dev_picture_base;
	int			dev_picture_number;

	// conversion buffers, used internally by "handle_string"
	GString		dev_conversion_buffer;

};

#endif // _PDF2XML_H
