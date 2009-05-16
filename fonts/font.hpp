/* Definition of font types, as used both by conversion and the application
 *
 */


struct font_s {
	unsigned short nrGlyphs;	//total number of glyphs
	unsigned short height;		//height, of all glyphs
	unsigned short *offset;	//offset into *data (in bytes)
	char *data;				//bit-packed data.

	/// Print a character in byte-buffer, given stride of the buffer, and the width.
	/// returns the width of the character (in pixels)
	int render(char *buffer, int bwidth, int bstride, char c) const
	{
		uint16_t c16 = (uint8_t)c;	//prevent minus signs
		if( c16 >= nrGlyphs)
			return 0;

		int nrBytes = offset[ c16 + 1] - offset[ c16 ];
		int aHeight = ((height-1)>>3) + 1;	//aligned height, in bytes
		int cWidth  = nrBytes / aHeight;	//character width, in pixels
		int cHeight = height;				//character height, in pixels

		if( buffer == NULL)	//allow fast counting of string-length
			return cWidth;


		for(int x=0; x < min(cWidth, bwidth); x++)
		{
			for(int y=0; y < cHeight; y++)
			{
				int bit = 7-(y%8);
				char v = data[ offset[c16] + aHeight * x + (y>>3) ];
				buffer[ y*bstride + x] =  (v>>bit) & 1;
			}
		}
		return cWidth;	//width in pixels
	}


	//width of a character, in bytes:
	int width(uint16_t c) const
	{
		if( c >= nrGlyphs)
			return 0;
		int nrBytes = offset[ c + 1 ] - offset[c];
		int aHeight = ((height-1)>>3) + 1;	//aligned height, in bytes
		return nrBytes / aHeight;
	}
};


/*
unsigned short testFont_offset[] = {0, 3, 15};
char testFont_data[] = {0x12, 0x34 };
const font_s testFont  = {1,8, testFont_offset, testFont_data };
*/

