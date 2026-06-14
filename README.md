# pdf2xml

**Convert PDF files to XML** — extract text, fonts, images, and links from PDF documents.

## History

Originally developed in 2005 by [Mobipocket.com](http://www.mobipocket.com/dev/pdf2xml/) (authors: Martin Görner, Fabien Hertschuh). This was an early open source PDF-to-XML converter built on the [xpdf](http://www.foolabs.com/xpdf/) library.

## Usage

```
pdf2xml FILE
```

Converts `FILE` (a PDF) to an XML file and extracted images in the current directory.

### Example

```
pdf2xml document.pdf
```

Produces:
- `document.xml` — the extracted content
- `document_picXXXX.png` / `document_picXXXX.jpg` — embedded images

### XML Output Format

The output XML describes the document structure:

```xml
<pdf2xml pages="N">
  <title>Document Title</title>
  <page width="..." height="...">
    <font size="..." face="..." color="..." bold="..." italic="...">
      <text x="..." y="..." width="..." height="...">Extracted text</text>
      <link x="..." y="..." width="..." height="..." href="..."/>
      <img x="..." y="..." width="..." height="..." src="..."/>
    </font>
  </page>
</pdf2xml>
```

- **Text** is coalesced into blocks with font, size, and color metadata
- **Links** capture internal page-to-page links and external URLs
- **Images** (JPEG, monochrome, and color) are extracted to PNG/JPEG files

## Building

### Windows (MinGW)

Requires [MSYS2](https://www.msys2.org/) with gcc/g++ and mingw32-make.

```
mingw32-make
```

## Dependencies (bundled)

| Library  | Version | Copyright                               |
|----------|---------|-----------------------------------------|
| xpdf     | 3.01    | 1996–2005 Glyph & Cog, LLC             |
| libpng   | 1.2.7   | 1995–2004 Glenn Randers-Pehrson et al.  |
| zlib     | 1.2.1   | 1995–2003 Jean-loup Gailly, Mark Adler  |

All sources are included in the repository.

## License

Licensed under the [GNU General Public License (GPL)](https://www.gnu.org/licenses/gpl-3.0.html), following the licenses of xpdf, libpng, and zlib.
