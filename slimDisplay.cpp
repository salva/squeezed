

#include <algorithm>	//for transfom
#include <cctype>		//for toupper


#include "musicDB.hpp"	//for the search menu
#include "slimDisplay.hpp"
#include "fonts/font.hpp"

// include squeezeCenter fonts:
#include "fonts/fontStandard1.h"
#include "fonts/fontStandard2.h"
#include "fonts/fontFull.h"


const font_s *fonts[] = {&fontStandard_1, &fontStandard_2, &fontFull};
static const font_s* fontPerSize[32];

//initialize the fontPerSize structure:
class fontLib_s {
public:
	fontLib_s()
	{
		//setup a default font, for all sizes:
		for(size_t i=0; i < array_size(fontPerSize); i++)
			fontPerSize[i] = &fontStandard_1;
		//override with fonts we have:
		for(size_t i=0; i < array_size(fonts); i++)
		{
			int h = fonts[i]->height;
			fontPerSize[h] = fonts[i];

		}
	}
} fontLib;


//--------------------------- Utility functions -----------------------------

const char *t9List[] = { "", " ", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"};


//test if two a falls under the T9 key 'number'
bool compareT9(char a, int number)
{
	bool isEqual = false;
	a = tolower(a);
	const char *c = t9List[number];
	for(int idx=0; c[idx] != 0; idx++)
		if( c[idx] == a )
		{
			isEqual = true;
			break;
		}
	return isEqual;
}


// text entry system for T9 keyboards, handle keypress 'key' for current string 'text'
void keyPress(char key, string *text, bool *newSymbol)
{
	int nr = key - '0';

	if( (nr >= 0) && (nr <= 9) )
	{
		const char *c = t9List[nr];

		if( *newSymbol )
		{
			(*text) += c[0];
			*newSymbol = false;
		}
		else
		{
			//check if current char in text matches any of chrList[nr],
			char *newText = &(*text)[text->size()-1];
			*newText = tolower( *newText);
			//int cursor = text->size()-1;
			bool sameT9 = false;
			int idx;
			for(idx=0; c[idx] != 0; idx++)
				if( c[idx] == *newText )
				{
					sameT9 = true;
					break;
				}
			if( sameT9 )
				*newText = c[ (idx+1) % strlen(c) ];	// if so, goto next in chrList.
			else
				*newText = c[0];				//else, goto first in chrList
		}
	}
}



//--------------------------- Implementation of external interface -----------


void slimDisplay::draw(char transition, int8_t param)
{
	netBuffer buf(packet);
	buf.write( (uint16_t)0 );	//offset, if ~640 for squeezebox transporter..
	buf.write( transition );	//transition ('c'=constant,'R'=right,'L'=left,'U','D')
	buf.write( (char)param );			//transition height of screen
	assert( buf.idx == 4);

	//convert uint8 to binary data:
	//char *screen = buf.data + buf.idx;
	for(int x=0; x< screenWidth; x++)
	{
		for(int y=0; y< screenHeight; y+=8)
		{
			char bin = 0;
			for(int b=0; b < 8; b++)
			{
				bin = (bin<<1) | (screen[ (y+b) * screenWidth + x]!=0);
			}
			buf.write( &bin, 1);
		}
	}


	if( transition != 'c')
	{
		//new animations override old ones:
		refreshAfterAnim = false;
		slimConnection->state.anim = slimConnectionHandler::state_s::ANIM_HW;
		slimConnection->send("grfe", sizeof(packet), packet);
	}
	else
	{
		//don't update if animation is running
		if( slimConnection->state.anim != slimConnectionHandler::state_s::ANIM_HW)
		{
			refreshAfterAnim = false;	//just send something, no refresh needed
			slimConnection->send("grfe", sizeof(packet), packet);
		} else
			refreshAfterAnim = true;	//send it after animation has completed
	}
}



int slimDisplay::strWidth(const char *msg, int fontsize)
{
	if( (size_t)fontsize >= array_size(fontPerSize) )
		return 0;
	const font_s *font = fontPerSize[ fontsize ];
	int width = 0;
	while( *msg != 0)
		width += font->width( *(msg++) ) + 1;	//slimDisplay::putChar adds 1 whitespace line
	return width;
}


int slimDisplay::putChar(char c, int fontsize, bool send)
{
	if( (size_t)fontsize >= array_size(fontPerSize) )
		return 0;

	const font_s *font = fontPerSize[ fontsize ];

	db_printf(15,"printing %c at %3i, %2i\t",c, pos.x, pos.y);

	//print font, have one pixel space inbetween:
	char *begin = screen + pos.x + screenWidth*pos.y;
	int width = font->render( begin, screenWidth - pos.x, screenWidth, c) + 1;

	// Update cursor:
	pos.x += width;
	/*if(pos.x > 320-8)
	{
		pos.x  = 0;
		pos.y += font->height;
	}
	if(pos.y > 32 - font->height)
		pos.y = 0;
	*/

	if(send) draw();
	return width;
}



int slimDisplay::print(const char *msg, int fontSize, bool send)
{
	int width = 0;
	while( *msg != 0)
		width += putChar(*(msg++) , fontSize );
	if(send) draw();
	return width;
}


//--------------------------- Menu system ------------------------------------


//--------------------- while playing screen -------------------

void splitSeconds(int secTot, int &hours, int &minutes, int &seconds)
{
	minutes = secTot / 60;	//integer division floors
	seconds = secTot % 60;

	hours   = minutes / 60;
	minutes = minutes % 60;
}


void slimPlayingMenu::draw(char transition, int8_t param)
{
	int fontSizes[] = {11, 19};

	char elapsed[30], header[80];
	std::string songInfo;
	// Get song info:
	int elapsed_ms = display->slimConnection->status.songMSec;

	std::string group = display->slimConnection->state.currentGroup;
	const playList *list = display->slimConnection->ipc->getList( display->slimConnection->state.uuid );
	int songNr    = list->currentItem;
	int listSize  = list->items.size();
	musicFile song;
	if( listSize > 0) song =  list->items[ list->currentItem ] ;

	// Prepare strings to display:
	int hrElaps, minElaps, secElaps;
	splitSeconds( elapsed_ms/1000, hrElaps, minElaps, secElaps);

	int hrSong, minSong, secSong;
	splitSeconds( song.length, hrSong, minSong, secSong);

	sprintf(elapsed, " %02i:%02i / %02i:%02i", minElaps, secElaps, minSong, secSong);
	//sprintf(elapsed, " %i / %i", elapsed_ms/1000, song.length );	//for debug

	sprintf(header,  "Now Playing (%i of %i)", songNr+1, listSize);
	//sprintf(songInfo,"%s (%s)", song.title.c_str(), song.artist.c_str() );
	songInfo = song.displayTitle();

	//re-draw the entire screen:
	display->cls();
	display->gotoxy(0,0);
	display->print(header, fontSizes[0] );

	int strLen = display->strWidth(elapsed, fontSizes[0] );
	display->gotoxy( 320 - strLen, 0 );
	display->print(elapsed, fontSizes[0] );

	display->gotoxy(0, fontSizes[0] + 1 );
	display->print( songInfo.c_str() , fontSizes[1] );
	display->draw(transition, param);
}


bool slimPlayingMenu::command(commands_e cmd)
{
	bool handled = true;
	switch(cmd)
	{
	case cmd_left:
		if( parent != NULL) {
			display->slimConnection->setMenu( parent , 'l');
			parent->currentItem = 0;	//scroll to our current position.
		} else
			handled = false;
		break;
	case cmd_right:
		display->draw('R');
		break;
	case cmd_playing:
		//TODO: toggle between playing screens.
		break;

	case cmd_down:
	case cmd_up:
		if( playListBrowser != NULL)
			display->slimConnection->setMenu( playListBrowser );
		//re-send the command, so the playlist browser responds to it:
		handled = playListBrowser->command(cmd);
		break;
	default:
		handled = false;
	}
	return handled;
}


//--------------------- search menu -------------------

bool slimSearchMenu::command(commands_e cmd)
{
	//on key-change: pop last query, add a new one.
	//on new key, push_back a new query, with new match.

	//on right-arrow: show list of current matches
	//on right-arrow: show list of further dbField's to sort on.

	const char *key = commandsStr[cmd];
	bool handled = false;

	char transition = 'c';

	if( (cmd==cmd_left) && (menuCursor == 0) && (match.size() < 2) && (!newSymbol) )
	{
		if( mode == textEntry)
			display->slimConnection->setMenu( parent );
		else
			mode = textEntry;
		handled = true;
	} else {
		switch(mode)
		{
		case textEntry:
			if( cmd == cmd_left )
			{
				if( !newSymbol )
					match.resize( match.size() -1);
				newSymbol = false;
			}
			else if( cmd == cmd_right )
			{
				if( newSymbol )	//search with this criterion
				{
					dbField field = query.back().getField();
					query.pop_back();			//replace the query
					query.push_back( dbQuery(db, field, match.c_str() ) );
					resultCursor = 0;
					mode = browseResults;		//show results
					transition = 'r';
				}
				else
					newSymbol = true;	//start a new character
			} else {
				keyPress( *key, &match, &newSymbol );
			}
			break;
		case fieldSelect:
			break;	//make this a sub-menu of browseResults.
		case browseResults:
			handled = true;
			if( cmd == cmd_left ) {
				mode = textEntry;
				transition = 'l';
			} else if( cmd == cmd_down ) {
				resultCursor = util::min<int>( resultCursor+1, query.back().uSize()-1 );
				transition = 'd';
			} else if( cmd == cmd_up) {
				resultCursor = util::max<int>( resultCursor-1, 0 );
				transition = 'u';
			} else if( cmd == cmd_add ) {
				std::vector<musicFile> entries = makeEntries(db, query.back(), resultCursor );
				ipc->addToGroup( display->slimConnection->currentGroup(),  entries);
			} else if( cmd == cmd_play )  {
				std::vector<musicFile> entries = makeEntries(db, query.back(), resultCursor );
				//only clear playlist if we really have something new:
				if( entries.size() > 0)
				{
					ipc->setGroup( display->slimConnection->currentGroup(),  entries);
					//and start playing:
					ipc->seekList( display->slimConnection->currentGroup(), 0, SEEK_SET );
				}
			} else {
				handled = false;
			}
			break;
		default:
			handled = false;
		} // switch (menu mode)

		if(handled)
			draw(transition,0);

	} //if (still in this menu)
	return handled;
}



void slimSearchMenu::draw(char transition, int8_t param)
{
	int fontSizes[] = {11, 19};
	display->cls();
	char title[80];
	switch(mode)
	{
	case textEntry:
		{
			sprintf(title, "text entry for %s", dbFieldStr[query.back().getField()]);

			display->gotoxy(0,0);
			display->print( title, fontSizes[0] );

			int xw,xl = 9;
			display->gotoxy(xl, fontSizes[0] + 1 );
			xw = display->print(  match.c_str(), fontSizes[1] );
			//Print cursor
			//int x0 = 9 + 8*(match.size() + newSymbol);
			int x0,x1;
			int yBot = fontSizes[0] + fontSizes[1] + 1;
			if( newSymbol ) {
				char c[] = "a";
				x0 = xl + xw;
				x1 = x0 + display->strWidth(c, fontSizes[1] );
			} else {
				const char *c = match.c_str() + match.size() -1;
				x1 = xl + xw;
				x0 = x1 - display->strWidth(c, fontSizes[1] );
			}
			for(int x = x0; x < x1; x++)
				display->putPixel( x, yBot, 1);
			break;
		}
	case fieldSelect:
		break;
	case browseResults:
		{
			dbQuery *result = &query.back();
			dbField field   = result->getField();
			sprintf(title, "%s `%s*' (%lli matches)", dbFieldStr[field], match.c_str(), (LLU)result->uCount(resultCursor) );

			display->gotoxy(0,0);
			display->print( title, fontSizes[0] );

			sprintf(title, "%i of %llu", resultCursor+1, (LLU)result->uSize() );
			int strLen = display->strWidth(title, fontSizes[1] );

			display->gotoxy( 320 - strLen, 0 );
			display->print( title, fontSizes[0] );

			display->gotoxy(20, fontSizes[0] + 1 );
			//int offset = resultCursor;
			dbEntry res = (*db)[ result->uIndex(resultCursor) ];
			string val = res.getField(field);
			display->print( val.c_str() , fontSizes[1] );
			break;
		}
	}
	display->draw(transition,param);
}


//-------------------- Slim browse menu --------------------

bool slimBrowseMenu::stringLessThan( std::string a,  std::string b)
{
	/*for(int i=0; i<a.size(); i++)
		a[i] = toupper(a[i]);
	for(int i=0; i<b.size(); i++)
		b[i] = toupper(b[i]);*/
	std::transform(a.begin(), a.end(), a.begin(), (int(*)(int))toupper);
	std::transform(b.begin(), b.end(), b.begin(), (int(*)(int))toupper);
	return a < b;
}




//--------------------- volume control -------------------
const float slimVolumeScreen::timeOut = 3.5;
