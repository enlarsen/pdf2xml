//**************************************************************
//*  Copyright (c) 2005 Mobipocket.com
//*  Authors: Martin Görner, Fabien Hertschuh
//*  File: pdf2xml.cpp
//*  Description: conversion from PDF to XML using xpdf parser
//*  Platform: cross
//**************************************************************

#include "pdf2xml.h"

// General libs
#include "stdio.h"

// GNUpdf general libs
#include "GString.h"
#include "gmem.h"

// GNUpdf PDF libs
#include "GlobalParams.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "Link.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "CharTypes.h"
#include "UnicodeMap.h"
#include "UTF8.h"
#include "Error.h"
#include "config.h"

//------------------------------------------------------------

const char HEXADECIMAL_CHARACTERS[] = "0123456789ABCDEF";

//------------------------------------------------------------

bool Rect::is_intersecting (const Rect &other, Rect &rect) const
{
	int temp;

	rect.width  = x + width;
	temp = other.x + other.width;
	if (temp < rect.width) rect.width = temp;

	rect.x = (x > other.x) ? x : other.x;
	rect.width -= rect.x;

	rect.height  = y + height;
	temp = other.y + other.height;
	if (temp < rect.height) rect.height = temp;

	rect.y = (y > other.y) ? y : other.y;
	rect.height -= rect.y;

	return (rect.width > 0) && (rect.height > 0);
}

//------------------------------------------------------------

void Rect::enlarge_to_contain (const Rect &contained)
{
	if (height ==0 || width == 0)
	{
		x = contained.x;
		y = contained.y;
		width = contained.width;
		height = contained.height;
	}
	else if (contained.width != 0 && contained.height != 0)
	{
		int cur_bottom, cur_right;
		cur_right  = x + width;
		cur_bottom = y + height;
		
		// top left corner
		if (x > contained.x)
		{
			width += x - contained.x;
			x = contained.x;
		}
		if (y > contained.y)
		{
			height += y - contained.y;
			y = contained.y;
		}

		// dimensions
		int i;
		if ((i=contained.y+contained.height-cur_bottom) > 0)
			height += i;

		if ((i=contained.x+contained.width-cur_right) > 0)
			width += i;
	}
}

#define WRITE_BOUNDS \
	error |= write(rect.x); \
	error |= write("\" y=\""); \
	error |= write(rect.y); \
	error |= write("\" width=\""); \
	error |= write(rect.width); \
	error |= write("\" height=\""); \
	error |= write(rect.height);

//------------------------------------------------------------

XmlOutput::XmlOutput () :
	xml_file(NULL),
	page_opened(false),
	font_opened(false)
{
}

//------------------------------------------------------------

bool XmlOutput::open (GString& filename)
{
	xml_file = fopen(filename.getCString(), "wb");

	if (xml_file == NULL) return true;

	return write("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
}

//------------------------------------------------------------

bool XmlOutput::write (const char* content)
{
	size_t towrite = strlen(content);
	size_t written = fwrite(content, sizeof(char), towrite, xml_file);

	return (towrite != written);
}

//------------------------------------------------------------

bool XmlOutput::write (int number)
{
	static char char_number[12];

	char_number[11] = 0;
	if (number == 0)
	{
		char_number[10] = '0';

		return write(&(char_number[10]));
	}

	int index;
	bool negative = false;
	if (number < 0)
	{
		number = -number;
		negative = true;
	}

	for (index = 10; (index >= 0) && (number != 0); index--)
	{
		char_number[index] = (char)('0' + (number % 10));
		number = number / 10;
	}

	if (negative)
	{
		char_number[index] = '-';
		index--;
	}

	return write(&(char_number[index + 1]));
}

//------------------------------------------------------------

bool XmlOutput::close ()
{
	if (xml_file != NULL)
	{
		fclose(xml_file);
		xml_file = NULL;
	}

	return false;
}

//------------------------------------------------------------

bool XmlOutput::add_metatag (const char* tag, GString* value)
{
	if (value == NULL)
	{
		return true;
	}

	write("  <");
	write(tag);
	write(">");

	bool is_unicode  = false;
	bool endianness  = false;

	char* char_value = value->getCString();
	int start_index  = 0;
	int end_index    = value->getLength();

	if (value->getLength() >= 3)
	{
		if (   (char_value[0] == '\xEF')
			&& (char_value[1] == '\xBB')
			&& (char_value[2] == '\xBF'))
		{
			// UTF8 value, skip BOM
			start_index = 3;
		}
	}

	if ((start_index == 0) && (value->getLength() >= 2))
	{
		if (   (char_value[0] == '\xFF')
			&& (char_value[1] == '\xFE'))
		{
			// UTF16
			is_unicode = true;
			endianness = true;
			start_index = 2;
			end_index   = (end_index >> 1) << 1;
		}
		else if (   (char_value[0] == '\xFE')
			     && (char_value[1] == '\xFF'))
		{
			// UTF16
			is_unicode  = true;
			start_index = 2;
			end_index   = (end_index >> 1) << 1;
		}
	}

	// worst case is all &amp;
	// going for UTF16 to UTF8 does not multiply by 5
	// + 1 is for NULL terminator
	int   utf8_length = value->getLength() * 6 + 1;
	int   utf8_index  = 0;
	char* utf8_value  = new char[utf8_length];
	Unicode u;

	for (int i = start_index; i < end_index; i += (is_unicode ? 2 : 1))
	{
		if (is_unicode)
		{
			if (endianness)
			{
				u = ((Unicode) (unsigned char) char_value[i]) + (((Unicode) (unsigned char) char_value[i + 1]) << 8);
			}
			else
			{
				u = (((Unicode) (unsigned char) char_value[i]) << 8) + ((Unicode) (unsigned char) char_value[i + 1]);
			}
		}
		else
		{
			u = (Unicode) char_value[i];
		}

		switch (u)
		{
		case L'<':
			utf8_value[utf8_index++] = '&';
			utf8_value[utf8_index++] = 'l';
			utf8_value[utf8_index++] = 't';
			utf8_value[utf8_index++] = ';';
			break;

		case L'>':
			utf8_value[utf8_index++] = '&';
			utf8_value[utf8_index++] = 'g';
			utf8_value[utf8_index++] = 't';
			utf8_value[utf8_index++] = ';';
			break;

		case L'&':
			utf8_value[utf8_index++] = '&';
			utf8_value[utf8_index++] = 'a';
			utf8_value[utf8_index++] = 'm';
			utf8_value[utf8_index++] = 'p';
			utf8_value[utf8_index++] = ';';
			break;

		default:
			if (is_unicode)
			{
				utf8_index += mapUTF8(u, &(utf8_value[utf8_index]), utf8_length - utf8_index);
			}
			else
			{
				utf8_value[utf8_index++] = (char) u;
			}
			break;
		}
	}
	utf8_value[utf8_index] = '\0';

	write(utf8_value);
	delete [] utf8_value;

	write("</");
	write(tag);
	write(">\n");

	return false;
}

//------------------------------------------------------------

bool XmlOutput::start_page (int width, int height)
{
	bool error = false;

	if (font_opened)
	{
		error |= write("    </font>\n");
	}
	font_opened = false;

	if (page_opened)
	{
		error |= write("  </page>\n");
	}
	page_opened = true;

	error |= write("  <page width=\"");
	error |= write(width);
	error |= write("\" height=\"");
	error |= write(height);
	error |= write("\">\n");

	return error;
}

//------------------------------------------------------------

bool XmlOutput::change_font (GString* face, int size, int color, bool bold, bool italic)
{
	bool error = false;

	if (font_opened)
	{
		error |= write("    </font>\n");
	}
	font_opened = true;

	error |= write("    <font size=\"");
	error |= write(size);

	if ((face != NULL) && (face->getLength() > 0))
	{
		error |= write("\" face=\"");
		error |= write(face->getCString());
	}

	if (color != 0)
	{
		char number[7];
		for (int i = 0; i < 6; i++)
			number[i] = HEXADECIMAL_CHARACTERS[(color >> ((5 - i) << 2)) & 0x0F];
		number[6] = 0;

		error |= write("\" color=\"#");
		error |= write(number);
	}

	if (bold)
	{
		error |= write("\" bold=\"true");
	}

	if (italic)
	{
		error |= write("\" italic=\"true");
	}

	error |= write("\">\n");

	return error;
}

//------------------------------------------------------------

bool XmlOutput::add_link (const Rect& rect, int dest_page, int dest_x, int dest_y)
{
	bool error = false;

	error |= write("      <link x=\"");
	WRITE_BOUNDS
	error |= write("\" dest_page=\"");
	error |= write(dest_page);
	error |= write("\" dest_x=\"");
	error |= write(dest_x);
	error |= write("\" dest_y=\"");
	error |= write(dest_y);
	error |= write("\"/>");

	return error;
}

//------------------------------------------------------------

bool XmlOutput::add_link (const Rect& rect, GString& dest_url)
{
	bool error = false;

	error |= write("      <link x=\"");
	WRITE_BOUNDS
	error |= write("\" href=\"");
	error |= write(dest_url.getCString());
	error |= write("\"/>");

	return error;
}

//------------------------------------------------------------

bool XmlOutput::add_text_block (GString& str, const Rect& rect)
{
	bool error = false;

	error |= write("      <text x=\"");
	WRITE_BOUNDS
	error |= write("\">");
	error |= write(str.getCString());
	error |= write("</text>\n");

	return error;
}

//------------------------------------------------------------

bool XmlOutput::add_image_block(GString& filename, const Rect& rect)
{
	bool error = false;

	error |= write("      <img x=\"");
	WRITE_BOUNDS
	error |= write("\" src=\"");
	error |= write(filename.getCString());
	error |= write("\"/>\n");

	return error;
}

//------------------------------------------------------------

bool XmlOutput::load_from_pdf (GString& pdf_file_name, GString& picture_base_name)
{
	PDFDoc *doc = NULL;
	//UnicodeMap *uMap = NULL;
	MbpOutputDev *mbpOut = NULL;
	
	bool error = false;
	
	// default configuration
	globalParams = new GlobalParams(NULL);
	if (globalParams != NULL)
	{
		// text encoding ???
		//globalParams->setTextEncoding(textEncName);
		// EOL config ???
		//globalParams->setTextEOL(textEOL));
		
		// get mapping to output encoding
		//uMap = globalParams->getTextEncoding();
		//if (uMap != NULL)
		//{	
			GString *owner_password = NULL;
			GString *user_password  = NULL;
			
			doc = new PDFDoc(new GString(&pdf_file_name), owner_password, user_password); // created string deleted by ~PDFDoc

			if (doc != NULL && doc->isOk() && (doc->okToCopy() || doc->okToPrint()))
			{
				// number of pages
				int nb_pages = doc->getNumPages();
				GString* title = NULL;
				
				// various metadata are accessible here in "info" with the following dict keys:
				Object info;
				doc->getDocInfo(&info);
				if (info.isDict())
				{
					//	"Title"; "Subject"; "Keywords"; "Author"; "Creator"; "Producer"; "CreationDate"; "LastModifiedDate"
					Dict* dict = info.getDict();
					Object title_obj;
					dict->lookup("Title", &title_obj);

					if (title_obj.isString())
					{
						title = title_obj.getString()->copy();
					}

					title_obj.free();
				}
				info.free();
				
				// extract information
				mbpOut = new MbpOutputDev(*this, picture_base_name);
				if (mbpOut != NULL)
				{
					// open main tag
					write("<pdf2xml pages=\"");
					write(nb_pages);
					write("\">\n");

					// title tag
					add_metatag("title", title);

					// launch the parsing
					doc->displayPages(mbpOut, 1, nb_pages, 72, 72, 0, gFalse, gFalse, gTrue);

					bool error = false;

					if (font_opened)
					{
						error |= write("    </font>\n");
					}
					font_opened = false;

					if (page_opened)
					{
						error |= write("  </page>\n");
					}
					page_opened = false;

					// close main tag
					error |= write("</pdf2xml>\n");
				}

				delete title;
				delete mbpOut;
			}
			else error = true; // ERROR: Couldn't open the PDF file (file error / password protection / data corruption / copy protection)
		//}
		//else error = true; // ERROR: Couldn't get text encoding
	}
	else error = true; // ERROR: could not init globals
	
	// clean up
	delete doc;
	//uMap->decRefCnt();
	delete globalParams;
	
	return error;
}

//------------------------------------------------------------

MbpOutputDev::MbpOutputDev(XmlOutput& target, GString& picture_base_name) :
	dev_output(target),
	dev_page_state(NULL),
	dev_picture_references(16),
	dev_picture_base(picture_base_name),
	dev_picture_number(0),
	dev_current_font_face(),
	dev_current_font_bold(false),
	dev_current_font_italic(false),
	dev_current_font_size(0)
{
}

//------------------------------------------------------------

MbpOutputDev::~MbpOutputDev()
{
	for (int i = 0; i < dev_picture_references.getLength(); i++)
	{
		delete ((PictureReference*) dev_picture_references.get(i));
	}
}

//------------------------------------------------------------

void MbpOutputDev::drawLink(Link *link, Catalog *catalog)
{
	bool handled = true;

	if (link != NULL && link->isOk() && dev_page_state != NULL)
	{
		// border
		double x1,y1,x2,y2;
		// destination
		double x, y, dx, dy;
		link->getRect(&x1,&y1,&x2,&y2);
		Rect active_rect;

		// active area
		dev_page_state->transform(x1, y1, &x, &y);
		dev_page_state->transformDelta(x2 - x1, y2 - y1, &dx, &dy);
		active_rect.x		= round(x);
		active_rect.y		= round(y);
		active_rect.width	= round(dx);
		active_rect.height	= round(dy);

		// action
		LinkAction* action = link->getAction();

		if (action != NULL && action->isOk())
		{
			switch (action->getKind())
			{
			// destination is on the web
			case actionURI:
				{
					LinkURI* uri = (LinkURI*)action;
					if (uri->isOk())
					{
						GString* dest = uri->getURI();
						if (dest != NULL)
						{
							dev_output.add_link(active_rect, *dest);
						}
					}
					break;
				}

			// destination in the book
			case actionGoTo:
				{
					LinkGoTo* goto_link = (LinkGoTo*)action;
					if (goto_link->isOk())
					{
						bool newlink = false;
						LinkDest* link_dest = goto_link->getDest();
						GString*  name_dest = goto_link->getNamedDest();
						if (name_dest != NULL && catalog != NULL)
						{
							link_dest = catalog->findDest(name_dest);
							newlink   = true;
						}
						if (link_dest != NULL && link_dest->isOk())
						{
							// find the destination page number (counted from 1)
							int page;
							if (link_dest->isPageRef())
							{
								Ref pref = link_dest->getPageRef();
								page = catalog->findPage(pref.num, pref.gen);
							}
							else
								page = link_dest->getPageNum();

							// other data depend in the link type
							switch (link_dest->getKind())
							{
							case destXYZ:
								{
									// find the location on the destination page
									if (link_dest->getChangeLeft() && link_dest->getChangeTop())
										// TODO FH 25/01/2006 apply transform matrix of destination page, not current page
										dev_page_state->transform(link_dest->getLeft(), link_dest->getTop(), &x, &y);
									else
										handled = false; // what's this ?

									if (handled)										
										dev_output.add_link(active_rect, page-1, round(x), round(y)); // page counted from 0
								}
								break;

							// link to the page, without a specific location. PDF Data Destruction has hit again!
							case destFit:  case destFitH: case destFitV: case destFitR:
							case destFitB: case destFitBH: case destFitBV:
								dev_output.add_link(active_rect, page-1, 0, 0); // page counted from 0
								break;
							}

							// must delete the link object if it comes from the catalog
							if (newlink)
								delete link_dest;
						}
					}
					break;
				}

			case actionGoToR:
				{
					// link to an external file
					// nothing can be done with it here
					// LinkGoToR* goto_link = (LinkGoToR*)action;
					break;
				}

			default:
				handled = false;
				break;
			}
		}
	}
}

//------------------------------------------------------------

void MbpOutputDev::startPage(int /* pageNum */, GfxState *state)
{
	dev_page_state = state;
	double page_w = state->getPageWidth();
	double page_h = state->getPageHeight();
	last_page_w = page_w;
	last_page_h = page_h;

	// reinit state
	last_x = -1000;
	last_y = -1000;
	last_w = -1000;
	last_h = -1000;

	last_rect.x = 0;
	last_rect.y = 0;
	last_rect.width = 0;
	last_rect.height = 0;

	dev_font_has_changed = true;
	dev_current_font_bold = false;
	dev_current_font_italic = false;
	dev_current_font_size = 0;
	dev_current_font_color = 0;
	dev_current_font_face.clear();

	invalidate_coalesc_blocks();

	dev_output.start_page(round(page_w), round(page_h));
}

//------------------------------------------------------------

void MbpOutputDev::endPage()
{
	flush_coalesc_blocks();
	dev_page_state = NULL;
}

//------------------------------------------------------------

void MbpOutputDev::updateFont(GfxState *state)
{
	GfxFont *font = state->getFont();

	if (font == NULL || !font->isOk()) return;

	GString* name = font->getName();

	if (name == NULL) return;
	
	bool bold   = font->isBold()		== gTrue;
	bool italic = font->isItalic()		== gTrue;

	/* Not used for now
	bool serif  = font->isSerif()		== gTrue;
	bool symbol = font->isSymbolic()	== gTrue;
	bool fixed  = font->isFixedWidth()	== gTrue;
	*/

	double tdx, tdy, temp, font_size;
	state->textTransformDelta(0.0, state->getFontSize(), &tdx, &tdy);
	state->transformDelta(tdx, tdy, &temp, &font_size);
	int int_size = round(-font_size);

	// determine if the font has changed
	if (   bold		!= dev_current_font_bold
		|| italic	!= dev_current_font_italic
		|| int_size	!= dev_current_font_size
		|| (dev_current_font_face.cmp(name) != 0))
	{
		dev_font_has_changed = true;

		dev_current_font_bold	= bold;
		dev_current_font_italic	= italic;
		dev_current_font_size	= int_size;
		dev_current_font_face.clear();
		dev_current_font_face.append(name);
	}
}

//------------------------------------------------------------

void MbpOutputDev::drawString(GfxState *state, GString *s)
{
	Rect rect;
	double width, height;
	double dx, dy, tdx, tdy, x, y;
	double temp, font_size;
	double current_space = 0.0;
	int i;

	// measure physical size of string and get content
	GString& text_content = handle_string(state, s, width, height);

	// identify blank strings or strings converted to blank characters
	{
		bool all_blanks = true;
		const char* p = text_content.getCString(); // text_content.get_pointer();
		if (p!= NULL)
			for (i=0; i<text_content.getLength() && all_blanks; i++)
			{
				char c = p[i];
				if ((c != 0x20) && (c != 0x0A) && (c != 0x0D)) // TODO c != 0x3000 // ideographic space
					all_blanks = false;
			}

		if (all_blanks)
			return;
	}

	// compute the size of a space in this font
	GfxFont *font = state->getFont();
	
	if (font != NULL && font->isOk())
	{
		CharCode code;
		int n, uLen;
		Unicode u;
		n = font->getNextChar(" ", 1, &code, &u, 1, &uLen, &dx, &dy, &x, &y);

		if (dx == 0.0)
		{
			n = font->getNextChar("A", 1, &code, &u, 1, &uLen, &dx, &dy, &x, &y);
			dx *= 0.5;
		}

		dx = dx * state->getFontSize() + state->getCharSpace() + state->getWordSpace();
		dx *= state->getHorizScaling();
		state->textTransformDelta(dx, dy, &tdx, &tdy);
		state->transformDelta(tdx, tdy, &dx, &dy);
		current_space = dx;
	}

	rect.width  = round(width);
	rect.height = round(height);

	// displacement from glyph origin to top-left of line box
	// WARNING: this is only valid for horizontal writing mode
	dx = 0;
	dy = (1 + state->getFont()->getDescent()) * state->getFontSize();
	state->textTransformDelta(dx, dy, &tdx, &tdy);
	state->transform(state->getCurX() + tdx, state->getCurY() + tdy, &x, &y);

	rect.x = round(x);
	rect.y = round(y);

	state->transformDelta(tdx, tdy, &dx, &dy);
	bool new_italic = (dx != 0.0);

	if (dev_current_font_italic != new_italic)
	{
		dev_current_font_italic = new_italic;
		dev_font_has_changed = true;
	}

	// are we on the same line with the same font ?
	state->textTransformDelta(0.0, state->getFontSize(), &tdx, &tdy);
	state->transformDelta(tdx, tdy, &temp, &font_size);
	int int_size = round(-font_size);

	// determine color
	GfxRGB rgb;
	state->getFillRGB(&rgb);
	int rgb_color = (clamp(rgb.r >> 8) << 16) + (clamp(rgb.g >> 8) << 8) + clamp(rgb.b >> 8);

	// determine if drawing parameters have changed
	if (rgb_color != dev_current_font_color)
	{
		dev_current_font_color = rgb_color;
		dev_font_has_changed = true;
	}

	if (int_size != dev_current_font_size)
	{
		dev_current_font_size = int_size;
		dev_font_has_changed = true;
	}

	// detect blocks printed on top of each other (ex: drop-shadows): same text + 75% overlap
	bool overprint = false;
	Rect rinter;
	if (rect.is_intersecting(last_rect, rinter))
		if ((float)rinter.surface() > 0.5 * rect.surface())
			if (compare_with_coalesc(text_content))
				overprint = true;

	// determine if new block or coalesce with previous block
	double spacing = x - (last_x + last_w);
	bool append = false;
	bool prepend_space = false;
	bool stitch_blocks = false;
	bool horizontal_intersect = (last_y+last_h >= y && last_y+last_h <= y+height) || (y+height >= last_y && y+height <= last_y+last_h);
	if (y == last_y && spacing > -current_space && spacing < 0.75*current_space) // if on the same line and less than two 3/4 space apart
	{
		if (dev_font_has_changed)
			stitch_blocks = true;
		else
			append = true;
	}
	else if (horizontal_intersect && spacing > -current_space && spacing < 2.4*current_space)
	{
		if (dev_font_has_changed)
			stitch_blocks = true;
		else
			append = true;

		if (spacing >= 0.75*current_space)
			prepend_space = true;
	}

	// stick blocks together if just font change
	if (stitch_blocks)
	{
		int old_right = rect.x + rect.width;
		rect.x = round(last_x) + round(last_w);
		rect.width = old_right - rect.x;
	}
	
	// append the new string
	if (!append)
	{
		if (overprint)
			invalidate_coalesc_blocks();
		else
			flush_coalesc_blocks();
	}

	// If font has chamged, this is a good time to add the tag
	if (dev_font_has_changed)
	{
		dev_font_has_changed = false;
		dev_output.change_font(&dev_current_font_face, dev_current_font_size, dev_current_font_color, dev_current_font_bold, dev_current_font_italic);
	}

	append_coalesc_block(text_content, rect, prepend_space);

	// save the current block geometry and features
	last_x = x;
	last_y = y;
	last_w = width;
	last_h = height;
	last_rect = rect;
}

//------------------------------------------------------------

void MbpOutputDev::flush_coalesc_blocks()
{
	if (dev_coalesc_valid)
	{
		dev_output.add_text_block(dev_coalesc_content, dev_coalesc_rect);
	}
	invalidate_coalesc_blocks();
}

//------------------------------------------------------------

void MbpOutputDev::invalidate_coalesc_blocks()
{
	dev_coalesc_valid = false;
}

//------------------------------------------------------------

bool MbpOutputDev::compare_with_coalesc(GString& str)
{
	int i,j;

	if (!dev_coalesc_valid)
		return false;

	const char* s = str.getCString();
	const char* t = dev_coalesc_content.getCString();

	if (s == NULL || t == NULL)
		return false;

	// This comparison works in UTF8 because 0x20 < 0x80
	for (i=j=0; i<str.getLength() && j<dev_coalesc_content.getLength();)
	{
		if (s[i] == 0x20)
			i++;
		else if (t[j] == 0x20)
			j++;
		else if (s[i] == t[j])
		{
			i++; j++;
		}
		else
			break;
	}

	return i == str.getLength() && j == dev_coalesc_content.getLength();
}

//------------------------------------------------------------

void MbpOutputDev::append_image_block(int x, int y, int width, int height, GString& pic_filename)
{
	char* pic_chars = pic_filename.getCString();
	int last_path_separator_index = -1;

	for (int index = 0; ; index++)
	{
		if (pic_chars[index] == 0) break;
		if (   (pic_chars[index] == '\\')
			|| (pic_chars[index] == '/'))
		{
			last_path_separator_index = index;
		}
	}

	GString relname(&(pic_chars[last_path_separator_index + 1]));

	// flush previous text blocks
	flush_coalesc_blocks();

	Rect rect;
	rect.x = x;
	rect.y = y;
	rect.width  = width;
	rect.height = height;

	// append the block
	dev_output.add_image_block(relname, rect);
}

//------------------------------------------------------------

void MbpOutputDev::append_coalesc_block(GString& str, const Rect& rect, bool prepend_space)
{
	if (!dev_coalesc_valid)
	{
		dev_coalesc_content.clear();
		dev_coalesc_rect.x = 0;
		dev_coalesc_rect.y = 0;
		dev_coalesc_rect.width = 0;
		dev_coalesc_rect.height = 0;
	}

	if (prepend_space)
		dev_coalesc_content.append(' ');

	dev_coalesc_content.append(&str);
	dev_coalesc_rect.enlarge_to_contain(rect);
	dev_coalesc_valid = true;
}

//------------------------------------------------------------

GString& MbpOutputDev::handle_string(GfxState *state, GString *s, double& width, double& height)
{
	GfxFont *font;
	int wMode;
	CharCode code;
	Unicode u[8];
	double dx, dy, tdx, tdy;
	double originX, originY;
	char *p;
	int len, i, n, uLen, ulen, nChars, nSpaces;
	const int UBUF_LEN = 16;
	char ubuf[UBUF_LEN];

	dev_conversion_buffer.clear();

	dx = dy = 0;
	font = state->getFont();
	wMode = font->getWMode();
    p = s->getCString();
    len = s->getLength();
    nChars = nSpaces = 0;
    while (len > 0)
	{
		n = font->getNextChar(p, len, &code, u, (int)(sizeof(u) / sizeof(Unicode)), &uLen, &tdx, &tdy, &originX, &originY);

		for (i=0; i<uLen; i++)
		{
			ulen = mapUTF8(u[i], ubuf, UBUF_LEN);

			if (ulen == 1)
			{
				char uu = ubuf[0];
				// if we only need to convert characters harmful in XML, we do it here
				switch (uu)
				{
					case L'<': dev_conversion_buffer.append("&lt;"); break;
					case L'>': dev_conversion_buffer.append("&gt;"); break;
					case L'&': dev_conversion_buffer.append("&amp;"); break;
					default: dev_conversion_buffer.append(uu);
				}
			}
			else
				dev_conversion_buffer.append((char*) &ubuf, ulen);
		}

		dx += tdx;
		dy += tdy;
		if (n == 1 && *p == ' ')
			++nSpaces;
		++nChars;
		p += n;
		len -= n;
    }
    if (wMode)
	{
		dx *= state->getFontSize();
		dy = dy * state->getFontSize() + (nChars-1) * state->getCharSpace() + nSpaces * state->getWordSpace();
    }
	else
	{
		dx = dx * state->getFontSize() + (nChars-1) * state->getCharSpace() + nSpaces * state->getWordSpace();
		dx *= state->getHorizScaling();
		dy *= state->getFontSize();
    }

	// transform the displacement vector into a string size vector
	if (wMode)
		dx = state->getFontSize() + dx; // I don't really know the meaning of original dx component
	else
		dy = state->getFontSize() + dy; // I don't really know the meaning of original dy component

	state->textTransformDelta(dx, dy, &tdx, &tdy);
	state->transformDelta(tdx, tdy, &width, &height);
	height = -height;

	return dev_conversion_buffer;
}

//------------------------------------------------------------

void MbpOutputDev::drawImageMask(GfxState *state, Object *ref, Stream *str,
					int width, int height, GBool /* invert */,
					GBool inlineImg)
{
	drawImageOrMask(state, ref, str, width, height, NULL, NULL, inlineImg, true); // mask
}

//------------------------------------------------------------

void MbpOutputDev::drawImage(GfxState *state, Object *ref, Stream *str,
					int width, int height,
					GfxImageColorMap *colorMap,
					int *maskColors, GBool inlineImg)
{
	drawImageOrMask(state, ref, str, width, height, colorMap, maskColors, inlineImg, false); // not a mask
}

//------------------------------------------------------------

void MbpOutputDev::drawImageOrMask(GfxState *state, Object* ref, Stream *str,
			       int width, int height,
			       GfxImageColorMap *colorMap,
			       int* /* maskColors */, GBool inlineImg, bool mask)
{
	GString pic_file;

	// register the block in the block structure of the page
	double x1, y1, x2, y2, temp;
	bool flip_x = false;
	bool flip_y = false;
	int flip = 0;
	// when drawing a picture, we are in the scaled coordinates of the picture
	// in which the top-left corner is at coordinates (0,1) and
	// and  the bottom-right corner is at coordinates (1,0).
	state->transform(0, 1, &x1, &y1);
	state->transform(1, 0, &x2, &y2);

	// Detect if the picture is printed flipped
	if (x1 > x2)
	{
		flip |= 1;
		flip_x = true;
		temp = x1;
		x1 = x2;
		x2 = temp;
	}

	if (y1 > y2)
	{
		flip |= 2;
		flip_y = true;
		temp = y1;
		y1 = y2;
		y2 = temp;
	}

	int reference = -1;
	if ((ref != NULL) && (ref->isRef()))
	{
		reference = ref->getRefNum();

		for (int i = 0; i < dev_picture_references.getLength(); i++)
		{
			PictureReference* pic_reference = (PictureReference*) dev_picture_references.get(i);

			if (   (pic_reference->reference_number == reference)
				&& (pic_reference->picture_flip == flip))
			{
				// We already created a file for this picture
				compose_image_filename(dev_picture_base,
									   pic_reference->picture_number,
									   pic_reference->picture_extension,
									   pic_file);
				break;
			}
		}
	}

	if (pic_file.getLength() == 0)
	{
		// picture filename is empty, which means this reference was not found
		// ouput the file
		const char* extension = NULL;

		// ------------------------------------------------------------
		// dump JPEG file
		// ------------------------------------------------------------

		if (str->getKind() == strDCT && (mask || colorMap->getNumPixelComps() == 3) && !inlineImg)
		{
			// TODO, do we need to flip Jpegs too?

			// open image file
			extension = "jpg";
			compose_image_filename(dev_picture_base, ++dev_picture_number, extension, pic_file);

			FILE* img_file = fopen(pic_file.getCString(), "wb");
			if (img_file != NULL)
			{
				// initialize stream
				str = ((DCTStream *)str)->getRawStream();
				str->reset();

				int c;

				// copy the stream
				while ((c = str->getChar()) != EOF)
				{
					fputc(c, img_file);
				}

				// cleanup
				str->close();
				// file cleanup
				fclose(img_file);
			}
			// else TODO report error
		}

		// ------------------------------------------------------------
		// dump black and white image
		// ------------------------------------------------------------

		else if (mask || (colorMap->getNumPixelComps() == 1 && colorMap->getBits() == 1))
		{
			extension = "png";
			compose_image_filename(dev_picture_base, ++dev_picture_number, extension, pic_file);

			int stride = (width + 7) >> 3;
			unsigned char* data = new unsigned char[stride * height];

			if (data != NULL)
			{
				str->reset();

				// Prepare increments and initial value for flipping
				int k, x_increment, y_increment;

				if (flip_x)
				{
					if (flip_y)
					{
						// both flipped
						k = height * stride - 1;
						x_increment = -1;
						y_increment = 0;
					}
					else
					{
						// x flipped
						k = (stride - 1);
						x_increment = -1;
						y_increment = 2 * stride;
					}
				}
				else
				{
					if (flip_y)
					{
						// y flipped
						k = (height - 1) * stride;
						x_increment = 1;
						y_increment = -2 * stride;
					}
					else
					{
						// not flipped
						k = 0;
						x_increment = 1;
						y_increment = 0;
					}
				}

				// Retrieve the image raw data (columnwise monochrome pixels)
				for (int y = 0; y < height; y++)
				{
					for (int x = 0; x < stride; x++)
					{
						data[k] = (unsigned char) str->getChar();
						k += x_increment;
					}

					k += y_increment;
				}

				// there is more if the image is flipped in x...
				if (flip_x)
				{
					int total = height * stride;
					unsigned char a;

					// bitwise flip of all bytes:
					for (k = 0; k < total; k++)
					{
						a		= data[k];
						a		= ( a                         >> 4) + ( a                         << 4);
						a		= ((a & 0xCC /* 11001100b */) >> 2) + ((a & 0x33 /* 00110011b */) << 2);
						data[k]	= ((a & 0xAA /* 10101010b */) >> 1) + ((a & 0x55 /* 01010101b */) << 1);
					}

					int complementary_shift = (width & 7);

					if (complementary_shift != 0)
					{
						// now shift everything <shift> bits
						int shift = 8 - complementary_shift;
						unsigned char mask = 0xFF << complementary_shift;	// mask for remainder
						unsigned char b;
						unsigned char remainder = 0; // remainder is part that comes out when shifting
													 // a byte which is reintegrated in the next byte

						for (k = total - 1; k >= 0; k--)
						{
							a = data[k];
							b = (a & mask) >> complementary_shift;
							data[k] = (a << shift) | remainder;
							remainder = b;
						}
					}
				}

				str->close();

				// Set a B&W palette
				png_color palette[2];
				palette[0].red = palette[0].green = palette[0].blue = 0;
				palette[1].red = palette[1].green = palette[1].blue = 0xFF;

				// Save PNG file
				save_png(pic_file, width, height, stride, data, 1, PNG_COLOR_TYPE_PALETTE, palette, 2);
                delete data;
			}
		}

		// ------------------------------------------------------------
		// dump color or greyscale image
		// ------------------------------------------------------------

		else
		{
			extension = "png";
			compose_image_filename(dev_picture_base, ++dev_picture_number, extension, pic_file);

			unsigned char* data = new unsigned char[width * height * 3];

			if (data != NULL)
			{
				ImageStream* imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
				imgStr->reset();

				GfxRGB rgb;

				// Prepare increments and initial value for flipping
				int k, x_increment, y_increment;

				if (flip_x)
				{
					if (flip_y)
					{
						// both flipped
						k = 3 * height * width - 3;
						x_increment = -6;
						y_increment = 0;
					}
					else
					{
						// x flipped
						k = 3 * (width - 1);
						x_increment = -6;
						y_increment = 6 * width;
					}
				}
				else
				{
					if (flip_y)
					{
						// y flipped
						k = 3 * (height - 1) * width;
						x_increment = 0;
						y_increment = -6 * width;
					}
					else
					{
						// not flipped
						k = 0;
						x_increment = 0;
						y_increment = 0;
					}
				}

				// Retrieve the image raw data (RGB pixels)
				for (int y = 0; y < height; y++)
				{
					Guchar* p = imgStr->getLine();
					for (int x = 0; x < width; x++)
					{
						colorMap->getRGB(p, &rgb);
						data[k++] = clamp(rgb.r >> 8);
						data[k++] = clamp(rgb.g >> 8);
						data[k++] = clamp(rgb.b >> 8);
						k += x_increment;
						p += colorMap->getNumPixelComps();
					}

					k += y_increment;
				}

				delete imgStr;

				// Save PNG file
				save_png(pic_file, width, height, width * 3, data, 24, PNG_COLOR_TYPE_RGB, NULL, 0);
                delete data;
			}
		}

		if ((extension != NULL) && (reference != -1))
		{
			// Save this in the references
			dev_picture_references.append(new PictureReference(reference, flip, dev_picture_number, extension));
		}
	}

	append_image_block(round(x1), round(y1), round(x2-x1), round(y2-y1), pic_file);
}

//------------------------------------------------------------

void file_write_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
	FILE* file = (FILE*) png_ptr->io_ptr;

	if (fwrite(data, 1, length,file) != length)
		png_error(png_ptr, "Write Error");
}

//------------------------------------------------------------

void file_flush_data (png_structp png_ptr)
{
	FILE* file = (FILE*) png_ptr->io_ptr;

	if (fflush(file))
		png_error(png_ptr, "Flush Error");
}

//------------------------------------------------------------

bool MbpOutputDev::save_png (GString& file_name,
							 unsigned int width, unsigned int height, unsigned int row_stride,
							 unsigned char* data,
							 unsigned char bpp, unsigned char color_type, png_color* palette, unsigned short color_count)
{
	png_struct *png_ptr;
	png_info *info_ptr;

	// Create necessary structs
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
	{
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		return false;
	}

	// Open file
	FILE* file = fopen(file_name.getCString(), "wb");
	if (file == NULL)
	{
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		return false;
	}

	if (setjmp(png_ptr->jmpbuf))
	{
		png_destroy_write_struct(&png_ptr, (png_infopp) &info_ptr);
		fclose(file);
		return false;
	}
 
	// Writing functions
    png_set_write_fn(png_ptr, file, (png_rw_ptr) file_write_data, (png_flush_ptr) file_flush_data);

	// Image header
	info_ptr->width				= width;
	info_ptr->height			= height;
	info_ptr->pixel_depth		= bpp;
	info_ptr->channels			= (bpp>8) ? (unsigned char)3: (unsigned char)1;
	info_ptr->bit_depth			= (unsigned char)(bpp/info_ptr->channels);
	info_ptr->color_type		= color_type;
	info_ptr->compression_type	= info_ptr->filter_type = 0;
	info_ptr->valid				= 0;
	info_ptr->rowbytes			= row_stride;
	info_ptr->interlace_type	= PNG_INTERLACE_NONE;

	// Background
	png_color_16 image_background={ 0, 255, 255, 255, 0 };
	png_set_bKGD(png_ptr, info_ptr, &image_background);

	// Metrics
	png_set_pHYs(png_ptr, info_ptr, 3780, 3780, PNG_RESOLUTION_METER); // 3780 dot per meter

	// Palette
	if (palette != NULL)
	{
		png_set_IHDR(png_ptr, info_ptr, info_ptr->width, info_ptr->height, info_ptr->bit_depth, 
					 PNG_COLOR_TYPE_PALETTE, info_ptr->interlace_type, 
					 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
		info_ptr->valid |= PNG_INFO_PLTE;
		info_ptr->palette = palette;
		info_ptr->num_palette = color_count;
	}  

	// Write the file header
	png_write_info(png_ptr, info_ptr);
 
	// Interlace handling
	int num_pass = png_set_interlace_handling(png_ptr);
	for (int pass = 0; pass < num_pass; pass++){
		for (unsigned int y = 0; y < height; y++)
		{
			png_write_row(png_ptr, &data[row_stride * y]);
		}
	}
	
	// Finish writing
	png_write_end(png_ptr, info_ptr);

	// Cleanup
	png_destroy_write_struct(&png_ptr, (png_infopp) &info_ptr);

	fclose(file);

	return true;
}

//------------------------------------------------------------

void MbpOutputDev::compose_image_filename(GString& base_name, int num, const char *const ext, GString& result)
{
	result.clear();
	result.append(&base_name);
	result.append("_pic");

	for (int i = 0; i < 4; i++)
		result.append(HEXADECIMAL_CHARACTERS[(num >> ((3 - i) << 2)) & 0x0F]);

	result.append('.');
	result.append(ext);
}

//------------------------------------------------------------

int main (int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("Usage: pdf2xml FILE\n"
			   "Convert the pdf FILE to an xml file.\n"
			   "The xml file and images are created in the current directory.\n\n"

			   "pdf2xml comes with ABSOLUTELY NO WARRANTY; This is free software,\n"
			   "and you are welcome to redistribute it under certain conditions.\n"
			   "It is licensed under the GNU General Public License (GPL)\n"
			   "  Copyright (c) 2005 Mobipocket.com\n"
			   "  http://www.mobipocket.com/dev/pdf2xml/\n\n"

			   "This project uses the open source project xpdf,\n"
			   "xpdf is licensed under the GNU General Public License (GPL)\n"
			   "  Copyright (c) 1996-2004 Glyph & Cog, LLC.\n"
			   "  derekn@foolabs.com\n"
			   "  http://www.foolabs.com/xpdf/\n\n"

			   "This project uses the open source project libpng\n"
			   "  Copyright (c) 1998-2004 Glenn Randers-Pehrson\n"
			   "  Copyright (c) 1996-1997 Andreas Dilger\n"
			   "  Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.\n"
			   "  glennrp@users.sourceforge.net\n"
			   "  http://www.libpng.org/\n\n"

			   "The libpng uses the open source project zlib\n"
			   "  Copyright (c) 1995-2003 Jean-loup Gailly and Mark Adler\n"
			   "  jloup@gzip.org\n"
			   "  madler@alumni.caltech.edu\n"
			   "  http://www.zlib.org/\n\n"

			   "PDF is a registered trademark of Adobe Systems, Inc.\n");

		return 1;
	}

	char* output_start = argv[1];

	for (int index = 0; ; index++)
	{
		if (argv[1][index] == 0) break;
		if (   (argv[1][index] == '\\')
			|| (argv[1][index] == '/'))
		{
			output_start = &(argv[1][index + 1]);
		}
	}

	GString input_file(argv[1]);
	GString output_file(output_start);
	GString images_base(output_start);

	int length = output_file.getLength();

	if ((length >= 4) && (output_file.getChar(length - 4) == '.'))
	{
		// Change file extension to xml
		output_file.setChar(length - 3, 'x');
		output_file.setChar(length - 2, 'm');
		output_file.setChar(length - 1, 'l');

		// remove extension for the picture base name
		images_base.del(length - 4, 4);
	}
	else
	{
		output_file.append(".xml");
	}

	XmlOutput out;
	if (out.open(output_file)) return 1;

	bool error = out.load_from_pdf(input_file, images_base);

	out.close();

	return error ? 1 : 0;
}
