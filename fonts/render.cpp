/* Renders fonts from various sources to a header file which can be included
 *  by the squeezed application
 *
 * currently renders BMP's, as used by squeezecenter, and freetype, using the 
 * free-type library.
 */


#ifdef WIN32
#include <Winsock2.h>	//for htonl
#pragma warning (disable: 4996)
#endif

#include <vector>

#include <ft2build.h>  
#include FT_FREETYPE_H 


/// The font-format as used by the slimServer-application
#include "font.hpp"


//---------------------- basic types -----------------------------------
typedef unsigned short	uint16_t;
typedef unsigned int	uint32_t;
typedef signed int		int32_t;



//---------------------- copy of font-handling as done by squeezecenter -------------------
// see squeezeCenter/Slim/Display/Lib/Fonts

template<class T> 
class shared_array {
private:
	T *data;
	size_t *refCount;
	size_t _size;

	void freeMem(void)
	{
		delete refCount;
		delete data;
	}
public:
	shared_array(size_t len=0)
	{
		refCount = new size_t;
		*refCount = 1;
		data = new T[len];
		_size = len;
	}

	~shared_array()
	{
		*refCount--;
		if(*refCount == 0)
			freeMem();
	}

	shared_array<T>& operator=(const shared_array<T> &rhs)
	{
		if (this == &rhs)      // Same object?
			return *this; 

		//release current data:
		(*refCount)--;
		if(*refCount == 0)
			freeMem();

		//lock onto the new data:
		data = rhs.data;
		refCount = rhs.refCount;
		(*refCount)++;
		_size = rhs._size;
		return *this;
	}

	//get pointer to the data, without changing reference counting
	T* ptr(void)		{ return data;	}

	//size of the array
	size_t size(void)	{ return _size; }
};



//std::vector<char>
shared_array<char> readFile(const char *fname)
{
	//std::vector<char> out;
	shared_array<char> out;
	FILE *f = fopen(fname,"rb");
	if( f != NULL)
	{
		fseek(f, 0, SEEK_END);
		size_t len = ftell(f);
		fseek(f, 0, SEEK_SET);

		out = shared_array<char>(len);
		int n = fread( out.ptr() , len, 1, f);
	//	while( (f!=NULL) && !feof(f) )
	//		out.push_back( fgetc(f) );
		fclose(f);
	}
	return out;
}


size_t align( size_t x, size_t alignment)
{
	return x - (x%alignment) + (x%alignment? alignment: 0);
}



//unpack data from a binary stream
//see:
// http://www-cgi.cs.cmu.edu/afs/cs.cmu.edu/Web/People/rgs/pl-exp-conv.html
void unpack(const char *data, const char *fmt,...)
{
	va_list ap;
	va_start(ap, fmt); 

	const char *f = fmt;
	const char *fStart = f;
	while( *f != 0)
	{
		char type = *f;
		//read length:
		int len = 0;
		if( (f[1] < '0') || (f[1] > '9') )
			len = 1;
		if( f[1] == '*')
			len = -1; //strlen(data);	//doesn't work for bit-strings..
		else
		{
			while( (f[1] >= '0') && (f[1] <= '9') )
			{
				len = 10*len + (f[1]-'0');
				f++;
			}
		}

		//printf("%c", type);
		switch(type)
		{
		case 'a':	//An ascii string,  will be null padded
			{
			char *dst = va_arg(ap, char*);	//pointer to destination
			if( len == -1) len = strlen(data);
			for(int i=0; i<len; i++)
				*(dst++) = *(char*)(data++);
			break;
			}
		case 'v':	//A short in "VAX" (little-endian) order
			{
			uint16_t* dst = va_arg(ap, uint16_t*);
			*dst = *((uint16_t*)data);
			data += 2;
			break;
			}
		case 'V':	//A long in "VAX" (little-endian) order
			{
			uint32_t* dst = va_arg(ap, uint32_t*);
			*dst = *((uint32_t*)data);
			data += 4;
			break;
			}
		case 'x':	//A null byte
			data++;
			break;
		case 'X':	//Back up a byte
			data--;
			break;
		case 'B':	//A bit string (descending bit order).
			{	//'B' vs. 'b' is verified against perl
				char *dst = va_arg(ap, char*);
				if( len == -1) len = strlen(data)*8;
				for(int b=0; b < len; b++)
				{
					char bit = 7 - (b%8);
					char val = ( (*data) >> bit) & 1;
					*(dst++) = val ? '1' : '0';
					if( (b%8)==7 )	data++;
				}
				*dst = 0;	//add zero termination
				break;
			}
		case 'b':	//A bit string (ascending bit order, like vec()).
			{
				char *dst = va_arg(ap, char*);
				if( len == -1) len = strlen(data)*8;
				for(int b=0; b < len; b++)
				{
					char bit = b%8;
					char val = ( (*data) >> bit) & 1;
					*(dst++) = val ? '1' : '0';
					if( (b%8)==7 )	data++;
				}
				*dst = 0;	//add zero termination
				break;
			}
		default:
			break;
		}

		//goto the next char, skip space if it's there:
		if( f[1] != '*')	//all data after * is the same type.
			f++;
		while( *f == ' ')
			f++;
		
	}

	va_end(ap);
}


//pack data into a binary stream 'dst'
void pack(char *dst, const char *fmt,...)
{
	va_list ap;
	va_start(ap, fmt); 

	const char *f = fmt;
	const char *fStart = f;
	while( *f != 0)
	{
		char type = *f;		//read typecode
		
		// Read length
		int len = 0;
		if( (f[1] < '0') || (f[1] > '9') )
			len = 1;
		if( f[1] == '*')
			len = -1;
		else
		{
			while( (f[1] >= '0') && (f[1] <= '9') )
			{
				len = 10*len + (f[1]-'0');
				f++;
			}
		}

		// Pack the data
		switch(type)
		{
		case 'a':	//An ascii string,  will be null padded
			{
			char *data = va_arg(ap, char*);	//pointer to destination
			int i=0;
			if( len < 0) len = strlen(data);
			for(i=0; (i<len) && (*data != 0); i++)
				*(dst++) = *(data++);
			for( ; i < len; i++)	//zero-pad
				*(dst++) = 0;		
			break;
			}
		case 'b':
			{
				char *data = va_arg(ap, char*);
				*dst = 0;
				if( len < 0) len = strlen(data);
				for(int b=0; b < len; b++)
				{
					char bit = (b%8);
					char val = *(data++) != '0';
					(dst[0]) |= (val<<bit);
					if( (b%8)==7 )	
					{
						dst++;
						*dst = 0;
					}
				}
				break;
			}
		case 'B':	//A bit string (descending bit order).
			{
				char *data = va_arg(ap, char*);
				*dst = 0;
				if( len < 0) len = strlen(data);
				for(int b=0; b < len; b++)
				{
					char bit = 7 - (b%8);
					char val = *(data++) != '0';
					(dst[0]) |= (val<<bit);
					if( (b%8)==7 )	
					{
						dst++;
						*dst = 0;
					}
				}
				break;
			}
		default:
			break;
		}

		// Goto the next char, skip space if it's there:
		f++;
		while( *f == ' ') f++;
	} //while f
}



// parses a monochrome, uncompressed BMP file into an array for font data
int parseBMP(const char *fname, std::vector< std::vector<char> > &font, uint32_t *biHeight)
{
	struct bmpHeader {
		char type[2];
		uint32_t fsize, offset, biSize, biWidth;
		uint16_t biPlanes, biBitCount;
		uint32_t biCompression, biSizeImage, biFirstPaletteEntry;
	} h;
	memset( &h, 0, sizeof(h) );
	
	shared_array<char> fontstring = readFile(fname);
	unpack(fontstring.ptr(), "a2 V xx xx V V V V v v V V xxxx xxxx xxxx xxxx V",
		h.type, &h.fsize, &h.offset, &h.biSize, &h.biWidth, biHeight, &h.biPlanes, &h.biBitCount, 
		&h.biCompression, &h.biSizeImage, &h.biFirstPaletteEntry);

	//check input
	if (memcmp( h.type,"BM",2))
		return -1;
	if( h.fsize != fontstring.size() )
		return -2;
	if( h.biPlanes != 1)
		return -3;
	if( h.biBitCount != 1)
		return -4;
	if( h.biCompression != 0)
		return -5;

	//skip over the BMP header and the color table
	//fontstring = fontstring.substr( offset);
	char *bmpData = fontstring.ptr() + h.offset;

	int bitsPerLine =  h.biWidth - ( h.biWidth % 32);
	if( (h.biWidth % 32) != 0) bitsPerLine+=32;	//round up to 32 pixels wide

	font.resize( *biHeight );
	for(size_t i=0; i < *biHeight; i++)
	{
		std::vector< char > line(h.biWidth);

		char typeCode[8];
		char *bitString = new char[bitsPerLine];
		
		sprintf(typeCode,"B%i", h.biWidth);
		unpack(bmpData + i*bitsPerLine/8, typeCode, bitString);
		
		if( h.biFirstPaletteEntry == 0xFFFFFF)
		{
			for(size_t j=0; j< h.biWidth; j++)
				line[j] = (bitString[j] != '0') + '0';
		}
		else
		{
			//reversed color pallete
			for(size_t j=0; j< h.biWidth; j++)
				line[j] = (bitString[j] == '0') + '0';
		}

		font[*biHeight-i-1] = line;
		delete bitString;
	}

	return 0;
}


/// The height of the font is not only determined by the bitmap height, but also
/// by a specific encoding in the pixel values
int getVpos(const std::vector<std::vector<char> >& fontGrid, int &vStart, int &vEnd )
{
	int height = fontGrid.size();
	int width  = fontGrid[0].size();

	int extent= fontGrid.size()-1;
	char extentChar = '\x1f';
	const std::vector<char>* bottomRow = &fontGrid[height-1];

	//seek the extent character:
	int charIndex = -1;
	int i;
	for(i=0; i < width; i++)
	{
		if( (*bottomRow)[i] != '0' ) 
			continue;
		if( i >= (int)bottomRow->size() )
			break;
		charIndex++;

		if( charIndex == extentChar)
			break;				//found the charachter;

		while( (*bottomRow)[i] != '1' )
			i++;
	}

	vStart = 0;
	vEnd   = 0;
	if(  charIndex == extentChar)
	{
		int j;
		for(j=0; j < height; j++)
			if( fontGrid[j][i] == '1' )
				break;
		vStart = j;

		extent = 0;
		for(j=0; j< height; j++)
		{
			if(fontGrid[j][i] == '1')
			{
				vEnd = j;
				extent++;
			}
		}
	}
	return extent;
}



/// Extract font from a bitmap where the bottom line encodes the with per glyph.
/// the vertical start and stop are encoded in symbol '\x1f', which has been parsed
/// into vStart and vStop by getVpos()
font_s parseFont(const std::vector<std::vector<char> >& fontGrid, int vStart, int vStop )
{
	int height = fontGrid.size();
	int width  = fontGrid[0].size();

	font_s font;
	font.height = vStop - vStart + 1;	//bottom line is now converted to offset[] values
	font.offset = new unsigned short[257];
	font.data   = new char[width * align(font.height,8) ];	//data is bit-encoded.

	int bottomIndex = height-1;
	const std::vector<char>* bottomRow = &fontGrid[bottomIndex];
	int charIndex = -1;
	int fontOffset = 0;

	for(int i=0; i < width; i++)
	{
		if( (*bottomRow)[i] != '0' ) 
			continue;
		if( i >= (int)bottomRow->size() )
			break;
		charIndex++;

		std::string column;
		int gWidth = 0;

		int aHeight = align(font.height,8); // aligned height, in bytes
		int padding = aHeight - font.height+1;

		while( (*bottomRow)[i] != '1' )
		{
			for(int j=vStart; j< vStop; j++)
				column.push_back( fontGrid[j][i]  );
			for(int j=0; j < padding; j++)
				column.push_back( '0' );
			i++; gWidth++;
		}

		char typeCode[5] = "B*";						//to byte-align per row of pixels.
		//sprintf(typeCode, "B%i", font.height*gWidth);	//to byte-align per glyph
	
		font.offset[ charIndex ] = fontOffset;
		pack(font.data + fontOffset, typeCode, column.c_str() );
		fontOffset += gWidth * (aHeight/8);	//offset in bytes
	}

	font.offset[ charIndex+1 ] = fontOffset;
	font.nrGlyphs = charIndex+1;
	return font;
}





void testPackUnpack(void)
{
	char tst[64], tst2[64];
	unpack("abcd", "b32", tst );
	printf("abcd unpacked as b32 = %s\n", tst);
	
	unpack("abcd", "B32", tst );
	printf("abcd unpacked as B32 = %s\n", tst);

	pack(tst2,"B32", tst);
	printf("that packed back to B32 is: %s\n", tst2);

	pack(tst2,"b32", tst);
	printf("that packed back to b32 is: %s\n", tst2);
}



void printCinitializer(FILE *dst, size_t len, char *data)
{
	fprintf(dst, "{ ");
	for(size_t i=0; i < len; i++)
		fprintf(dst,"%i, ", data[i] );
	fprintf(dst,"}");
}

void printCinitializer(FILE *dst, size_t len, unsigned short *data)
{
	fprintf(dst, "{ ");
	for(size_t i=0; i < len; i++)
		fprintf(dst,"%u, ", data[i] );
	fprintf(dst,"}");
}


/// convert bmp with font to a c-header file:
int bmp2font(const char *bmpFile, const char *varName, const char *dstFile)
{
	std::vector<std::vector<char> > fontGrid;
	uint32_t biHeight;

	//load BMP into a 2D-array:
	parseBMP(bmpFile, fontGrid, &biHeight);

	if(fontGrid.size() == 0)
		return -1;

/*	//print debug:
	printf("fontGrid = \n");
	for(int y=0; y < 16; y++)
	{
		for(int x=0; x< 79; x++)
			printf("%c", zo[fontGrid[y][x] == '1'] );
		printf("\n");
	}*/

	//get font height:
	int vStart, vStop;
	int extent = getVpos(fontGrid, vStart, vStop);

	printf("font '%s' has vertical range of %i .. %i\n", bmpFile, vStart, vStop);

	//extract glyphs from bitmap data:
	font_s fontOut = parseFont( fontGrid, vStart, vStop );

	//debug: print glyphs to screen:
	char testString[] = "ab1";
	char buffer[80*32];
	memset(buffer, 0, sizeof(buffer) );
	int dx = 0;
	for(int c=0; c < (int)strlen(testString) ; c++)
	{
		dx += fontOut.render( buffer + dx, 80-dx-1, 80, testString[c]) + 1;
		if(dx >= 80)
			break;
	}

/*	printf("\nfontOut '%s' = \n", testString);
	char zo[] = " #";
	for(int y=0; y < 16; y++)
	{
		for(int x=0; x< 79; x++)
			printf("%c", zo[buffer[y*80 + x]] );
		printf("\n");
	}*/

	//print font into a header file:
	FILE *fh = fopen(dstFile, "w");

	//print offset table
	//int offsets[257];
	fprintf(fh,"\nunsigned short %s_offset[] = ", varName);
	printCinitializer(fh, fontOut.nrGlyphs + 1, fontOut.offset);
	fprintf(fh,";\n" );
	
	//print raw bitmap data
	fprintf(fh,"\nchar %s_data[] = ", varName);
	printCinitializer(fh, fontOut.offset[fontOut.nrGlyphs] , fontOut.data );
	fprintf(fh,";\n" );

	//print font structure:
	fprintf(fh,"\nfont_s %s = {%i, %i, %s_offset, %s_data };\n", varName, fontOut.nrGlyphs, fontOut.height, varName, varName );

	fclose(fh);


	return fontOut.nrGlyphs;
}

//---------------------- copy of font-handling as done by squeezecenter -------------------



__declspec(align(1)) struct bmpHeader
{
	char magic[2];	//should be 'BM'
	uint32_t len;
	uint16_t res1;
	uint16_t res2;
	uint32_t offset;	//start of bitmap data

	//standard DIB header:
	uint32_t hrSize;	//header size
	int32_t width;
	int32_t height;
	uint16_t nrPlanes; //must be 1
	uint16_t bits;		//bits per pixel
	uint32_t	compression;	//0=none
	uint32_t	imSize;
	int32_t	hres;	//pixels/meter;
	int32_t vres;
	uint32_t	paletteSize;	//0= 2^bits;
	uint32_t	nrColors;	//0=every color.

	//write uint8 data as monochrome bitmap)
	void writeMono(char *data, int width, int height)
	{
		memset( this, 0, sizeof(*this) );

		size_t tmp = sizeof(bmpHeader);

		magic[0] = 'B'; magic[1] = 'M';
		offset = ( sizeof(bmpHeader));	//should be 54
		hrSize = (40);
		this->width =  (width);
		this->height = (height);
		nrPlanes = (1);
		bits =(8);
		imSize = width*height*bits/8;

		len = sizeof(bmpHeader) + imSize;
		int i=0;
	}
};


char getPixel(FT_Bitmap &bitmap, int x, int y)
{
	char val = 0;
	int pitch = bitmap.pitch;

	if(x<0)
		return 0;
	if( (y<0) || (y>bitmap.rows) )
		return 0;

	switch( bitmap.pixel_mode )
	{
	case( FT_PIXEL_MODE_MONO ):
		{
			int xByte = x / 8;
			int xBit  = 7 - (x%8);
			if( xByte < bitmap.pitch )
				val = (bitmap.buffer[y*pitch + xByte] & (1<<xBit)) != 0;
		}
		break;
	default:
		val = 0;
	}
	return val;
}


void printSlot(FILE *file, FT_GlyphSlot slot, int vsize)
{
	int x,y;
	char cStr[] = {' ', 'x'};
	int width  = slot->advance.x / 64 + 1;
	int height =  vsize;
	fprintf(file,"character %c, [%i, %i] pixels\n", 0, width, height);

	for( y=0; y < vsize; y++)
	{
		for( x=0; x < vsize; x++)
		{
			int fx = x-slot->bitmap_left;
			char p = 0;
			if( fx < slot->advance.x / 64 )
				p = getPixel(slot->bitmap, x-slot->bitmap_left , -vsize+ slot->bitmap_top + y +1 );
			fprintf(file,"%c", cStr[p] );
		}
		fprintf(file,"|\n");
	}
}


void slot2bmp(char *fname,  FT_GlyphSlot slot, int vsize)
{
	__declspec(align(1)) bmpHeader hdr;
	FILE *f = fopen(fname,"wb");

	hdr.writeMono( (char*)slot->bitmap.buffer, vsize, vsize);
	fwrite(&hdr, sizeof(hdr), 1, f);
	fwrite( slot->bitmap.buffer, hdr.imSize, 1, f);
	fclose(f);
}



int renderFont(void)
{
	FT_Library library; 
	FT_Face face; /* handle to face object */  
	int fontSize = 8;

	//load library:
	int error = FT_Init_FreeType( &library );
	if ( error ) 
		return -1;

	error = FT_Init_FreeType( &library ); 
	if ( error ) 
		return -2;
	
	error = FT_New_Face( library, "FreeSans.ttf", 0, &face ); 
	if ( error == FT_Err_Unknown_File_Format )
		return -3;
	else if ( error )
		return -4;

	error = FT_Set_Pixel_Sizes( face,	/* handle to face object */  
								0,		/* pixel_width */  
								fontSize ); /* pixel_height */ 
	if(error)
		return -5;

	for(char charcode = 'a'; charcode < 'f'; charcode++)
	{
		FT_UInt glyph_index = FT_Get_Char_Index( face, charcode ); 
		error = FT_Load_Glyph( face, glyph_index, 
								0 ); /* load flags */ 
		
		error = FT_Render_Glyph( face->glyph, FT_RENDER_MODE_MONO);
		printSlot(stdout, face->glyph, fontSize);
	}

	slot2bmp("dump.bmp", face->glyph, fontSize);
	return 0;
}



int main(void)
{
	//renderFont();
	
	//the upper row, in 'normal' size:
	bmp2font("standard.1.font.bmp", "fontStandard_1", "fontStandard1.h" );

	//the lower row, in 'normal' size:
	bmp2font("standard.2.font.bmp", "fontStandard_2", "fontStandard2.h" );
	
	//fullscreen font, in 'large' size:
	bmp2font("full.2.font.bmp", "fontFull", "fontFull.h" );

	return 0;
}