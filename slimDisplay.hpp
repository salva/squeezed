


/**
 * \ingroup slimProto
 *@{*/


#ifndef _DISPLAY_HPP_
#define _DISPLAY_HPP_


#include <vector>
#include <string>



#include <time.h>

#include "debug.h"

#include "slimProto.hpp"	//needs to be first, to include winsock.h before windows.h
#include "slimIPC.hpp"
enum commands_e;		//slimProto includes this file.



struct rect_t {
    int x1,x2;
    int y1,y2;
};

//--------------------------- Utility functions --------------------------

/// key-handling for number-to-text conversion
void keyPress(char key, string *text, bool *newSymbol);

bool compareT9(char a, int number);

extern const char *t9List[];



/** Handling of squeezebox display */
class slimDisplay
{
public:
    //enum displayState {dOff, dTime, dMenu, dWPS};
    slimConnectionHandler* slimConnection;

    //displayState state;
	bool refreshAfterAnim;

private:
    //bit buffer:
    // Total display is 320x32, the buffer has
    // 320 columns, with 4 times 8 pixels.
	static const int screenWidth = 320;
	static const int screenHeight = 32;
	char screen[screenWidth * screenHeight];		//display as 8-bit data, for easy indexing

	// display is binary:
	static const int bitBufSize = (screenWidth) * (screenHeight/8);
	char packet[ 4 + bitBufSize ];	//full packet
	char cpacket[4 + bitBufSize ];	//compressed packet


    /// Cursor position, relative to window
    struct {
        int x;
        int y;
    } pos;


public:

    slimDisplay(slimConnectionHandler* slimConnection)
    {
        this->slimConnection = slimConnection;
		//bitBuf = &packet[4];	//first 4 bytes are header
        //state = dOff;
        cls(false);
        gotoxy(0,0);
    }


    /// Update screen, send to device
	/// transition codes (from softsqueeze source code)
	///	'r','l':	full scroll, no parameters
	/// 'u','d':	vertical scroll, over 'param' pixels
	/// 'L','R':	small bump, no parameters
	/// 'U','D':	small bump, no parameters
	///	'c'		:   no transition
    void draw(char transition='c', int8_t param=0);


    /// Clear screen
    void cls(bool send=false)
    {
		memset(screen, 0, sizeof(screen) );
        //memset(bitBuf, 0, bitBufSize );
		pos.x = pos.y = 0;
        if(send) draw();
    }

	/// draw a filled rectangle
    void rect(rect_t w, char color)
    {
		int y1 = util::clip(w.y1, 0, screenHeight-1);
		int y2 = util::clip(w.y2, 0, screenHeight-1);
		int x1 = util::clip(w.x1, 0, screenWidth-1);
		int x2 = util::clip(w.x2, 0, screenWidth-1);
        for(int y = y1; y < y2; y++)
            for(int x = x1; x < x2; x++)
				screen[ y * screenWidth + x] = color;
            //    putPixel(x,y,color);
    }

	/// Progress bar, progress ranges from 0..1
	void progressBar(int x, int y, int len, bool horizontal, float progress)
	{
		//check input parameters:
		const int thickness = 12;
		const int border = 2;
		len = util::max(len, 2*border);
		progress = util::clip(progress, 0.f, 1.f );

		rect_t b;
		b.x1 = x;
		b.x2 = horizontal ? x + len: x + thickness;
		b.y1 = y;
		b.y2 = horizontal ? y + thickness: y + len;
		rect(b,1);	//clear background

		//give it a rounded corner:
		int corner[][2] = { {0,0},{0,1},{1,0} };
		for( size_t i=0; i < array_size(corner); i++)
		{
			int dx = corner[i][0];
			int dy = corner[i][1];
			screen[ (b.y1+dy)  * screenWidth + b.x1 + dx  ] = 0;	//ul
			screen[ (b.y1+dy)  * screenWidth + b.x2 - dx-1  ] = 0;	//ur
			screen[ (b.y2-dy-1)* screenWidth + b.x1 + dx	] = 0;	//ll
			screen[ (b.y2-dy-1)* screenWidth + b.x2 - dx-1 ] = 0;	//lr
		}

		//clear the 'remaining' area
		int split = (int) ((float)(len-2*border)*progress + .5 );
		if( horizontal )
		{
			b.x1 = x + split + border;
			b.x2 = x + len   - border;
			b.y1 = y + border;
			b.y2 = y + thickness - border;
		} else {
			b.y1 = y + split + border;
			b.y2 = y + len   - border;
			b.x1 = x + border;
			b.x2 = x + thickness - border;
		}
		rect(b, 0);

	}

    /// Set cursor position
    void gotoxy(int x, int y)
    {
        pos.x = x;
        pos.y = y;
    }

	/// Color a single pixel
	void putPixel(int x, int y, char color=1)
	{
		x = util::clip(x, 0, screenWidth-1);
		y = util::clip(y, 0, screenHeight-1);
		screen[ y * screenWidth + x] = color;
	}

	/// Get a single pixel
	char getPixel(int x, int y)
	{
		x = util::clip(x, 0, screenWidth-1);
		y = util::clip(y, 0, screenHeight-1);
		return screen[ y * screenWidth + x];
	}


	/// get width of a string
	int strWidth(const char *str, int fontsize = 11);

	/// print character to screen, returns width in pixels of c
    int putChar(char c, int fontsize = 11, bool send=false);

	/// Print a string to the display, return width in pixels of *msg
    int print(const char *msg, int fontsize = 11, bool send=false);
};



/// a full-screen window:
class slimScreen {
protected:
    slimDisplay *display;
public:
	slimScreen(slimDisplay *display): display(display)
	{ }

	virtual void draw(char transition='c', int8_t param=0)=0;

	virtual bool command(commands_e cmd)
	{
		bool handled = false;
		return handled;
	}

};



/// Generic menu-system
class slimGenericMenu : public slimScreen
{
public:
	/// generic callback function for a menu entry
	class callback {
	public:
		virtual void run(void)=0;
	};

public:
	size_t currentItem;        //current selected menu-item
private:
	string menuName;
	slimScreen *parentScreen;

    // Coordinates of window, and fontsize:
    rect_t window;
	int fontSize;

	/// Get current menu-item name
	virtual string getName(size_t index)=0;
	/// Get number of items in the menu
	virtual size_t itemSize(void) = 0;
	/// Run current menu
	virtual void run(void) = 0;

	//make it possible to have a dynamic menu name:
	virtual const string& getMenuName(void)
	{
		return menuName;	//per default, a static one
	}




public:
	slimGenericMenu(slimDisplay *display, string name, slimScreen *parent=NULL):
			slimScreen(display),
			menuName(name),
			parentScreen(parent)
	{
		currentItem = 0;

        window.x1 = 20;
        //window.x2 = 320;
        //window.y1 = 15;
        //window.y2 = 32;
	}

	/// update screen
	//void draw(void)	{	draw('c');	}

	/// Internal drawing function, with animation
    void draw(char transition, int8_t param=0)
    {
		int fontSizes[] = {11,19}; //fontsizes, per line
		char index[8];
		sprintf(index, "%llu/%llu", (LLU)(currentItem+1), (LLU)itemSize() );

		display->cls();
		display->gotoxy( 0, 0);
		display->print( getMenuName().c_str() , fontSizes[0] );

		display->gotoxy( 320-8*strlen(index), 0 );
		display->print(index, fontSizes[0]);

        string name = getName(currentItem);
        display->gotoxy(window.x1, fontSizes[0] + 1);
        display->print( name.c_str() ,  fontSizes[1] );

		if(transition != 'c')
			param = 32-fontSizes[0];	//depends on font size, so set it here
        display->draw(transition, param);
    }

	/// process a user-control-command
    virtual bool command(commands_e cmd)
    {
        bool handled = true;
		int newCurrent;

		//allow fast scrolling, by typing in first letter of the menu:
		if( (cmd >= cmd_0) && (cmd <= cmd_9) )
		{
			string cName = getName(currentItem);
			int keyNr = commandsStr[cmd][0] - '0';
			cName.resize(1);	//make sure it has at leat one character.
			if( compareT9(cName[0], keyNr) )
			{
				cmd = cmd_down;
			} else {
				//search first menu entry that matches the T9 code:
				for(size_t i=0; i< itemSize(); i++)
				{
					string cName = getName(i);
					cName.resize(1);
					if( compareT9(cName[0], keyNr) )
					{
						currentItem = i;
						draw('c');
						break;
					}
				}
			} //if compareT9
		} //if cmd = 0..9

        switch(cmd)
        {
        case cmd_up:
			newCurrent = util::clip<int>(currentItem-1, 0, itemSize()-1);
			if(currentItem == (size_t)newCurrent)
				draw('D');
			else {
				currentItem = newCurrent;
				draw('u');
			}
            break;
        case cmd_down:
			newCurrent = util::clip<int>(currentItem+1, 0, itemSize()-1 );
			if(currentItem == (size_t)newCurrent)
				draw('U');
			else {
				currentItem = newCurrent;
				draw('d');
			}
            break;
		case cmd_left:
			if( parentScreen != NULL) {
				display->slimConnection->setMenu( parentScreen , 'l');
			} else
				draw('L');	//bump left
			break;
		case cmd_right:
			run();
			break;
        default:
            handled = false;
        }
        return handled;
	} //command()
};



/// A menu system on top of the low-level display
class slimMenu : public slimGenericMenu
{
private:
	string menuName;

	/// Single menu item
	struct item {
		string name;
		callback *action;
		item(string s, callback *c): name(s), action(c) {}
	};
	/// The complete menu
	std::vector<item> items;

	// Callback function for sub-menus
	class callbackSubMenu: public callback
	{
	private:
		slimConnectionHandler	*connection;
		slimScreen	*subMenu;
	public:
		callbackSubMenu(slimConnectionHandler *c, slimScreen *s): connection(c), subMenu(s) {}
		void run(void)
		{
			connection->setMenu(subMenu, 'r' );
		}
	};

	/// Storage for submenu callbacks
	std::list<callbackSubMenu> subMenus;
public:
    slimMenu(slimDisplay *display, string name, slimScreen *parent=NULL):
					slimGenericMenu(display,name,parent)
    {   }

	/// Add a generic action
	void addItem(std::string name, callback* action)
	{
		items.push_back( item(name,action) );
	}

	/// Add a sub-menu
	void addItem(std::string name, slimScreen *subMenu)
	{
		subMenus.push_back( callbackSubMenu(display->slimConnection , subMenu ) );
		items.push_back( item(name, &subMenus.back() ) );
	}

	/// Get current menu-item name
	string getName(size_t index)
	{
		string name;
		if( index < items.size() )
			name = items[index].name;
		return name;
	}

	/// Get number of items in the menu
	size_t itemSize(void)
	{	return items.size();	}

	/// Run current menu
	void run(void)
	{
		if( items[currentItem].action != NULL )
			items[currentItem].action->run();
		else
			draw('R');
	}
};



/// Menu system where the items are iterators, used to browse playlist and
/// other possibly large selections.
template <class iteratorType>
class slimIteratorMenu : public slimGenericMenu
{
public:
	//typedef int iteratorType;

	// The callback that is used to inform the parent menu about the selection
	class callbackIterator: public callback
	{
	public:
		void run(void) {};		//this function is not used
		virtual void run(iteratorType result, commands_e cmd = cmd_play)=0;
	};

private:
	callbackIterator *callback;

	//returns iterator, depending on origin: SEEK_SET: begin(), SEEK_CUR: current(), SEEK_END: end()
	virtual iteratorType getIterator(int origin)=0;
	virtual string getName(iteratorType it)=0;
	//vector<string>::iterator begin,end, current;

public:
	/// intialized the menu. *callback is deleted by the destructor
	slimIteratorMenu(slimDisplay *display,
					string name,
					slimScreen *parent,
					callbackIterator *callback
					):
			slimGenericMenu(display, name, parent),
			callback(callback)
	{ }

	~slimIteratorMenu()
	{
		delete callback;
	}


	/// Get current menu-item name
	string getName(size_t index)
	{
		string name;
		if( index < itemSize() )
			name = getName( getIterator(SEEK_SET) + index );
		return name;
	}

	/// Get number of items in the menu
	size_t itemSize(void)
	{
		return getIterator(SEEK_END) - getIterator(SEEK_SET);
	}

	/// Execute current menu-item
	void run(void)
	{
		if( currentItem < itemSize() )
			callback->run( getIterator(SEEK_SET) + currentItem );
	}

};


/// Browse through the playlist
/// TODO: add winamp-like queue system, when 'add' is pressed instead of 'play'
class slimPlayListMenu  : public slimIteratorMenu< std::vector<musicFile>::const_iterator >
{
private:
	typedef vector<musicFile>::const_iterator listIterator;

	slimConnectionHandler *device;

	/// The callback, to execute the menu-item
	class listCallback: public slimPlayListMenu::callbackIterator
	{
	private:
		slimConnectionHandler	*device;
		slimScreen				*parent;	//parent menu
	public:
		listCallback( slimConnectionHandler *device, slimScreen	*parent ):
						device(device), parent(parent)
		{ }

		void run(listIterator result, commands_e cmd)
		{
			const playList *list = device->ipc->getList( device->uuid() );
			if( list != NULL)
			{
				int newIndex = result - list->items.begin();
				device->ipc->seekList( device->currentGroup() , newIndex, SEEK_SET);
				device->setMenu( parent , 'l');
			}
		}
	};	//class listCallback


public:
	slimPlayListMenu(slimDisplay *display, slimScreen *parent):
		slimIteratorMenu<listIterator>(display, "Current Playlist", parent,
					new listCallback( display->slimConnection, parent ) )
	{
		device = display->slimConnection;
	}


	listIterator getIterator(int origin)
	{
		listIterator it;
		const playList *list = device->ipc->getList( device->uuid() );
		if( origin == SEEK_SET)
			it = list->begin();
		else if( origin == SEEK_CUR)
			it = list->begin() + currentItem;
		else if( origin == SEEK_END)
			it = list->end();
		return it;
	}


	/// Generate a name for the menu item:
	string getName(listIterator it)
	{
		const playList *list = device->ipc->getList( device->uuid() );
		string displayName;
		if( it != list->end() )
			displayName = it->displayTitle(); // it->title + " (" + it->artist + ")";
		return displayName;
	}

	/// Override some of the commands:
	bool command(commands_e cmd)
	{
		bool handled = false;
		if( cmd == cmd_play )
		{
			run();
			handled = true;
		} else {
			handled = slimIteratorMenu<listIterator>::command(cmd);
		}
		return handled;
	}

};



/// Information shown while playing:
class slimPlayingMenu: public slimScreen
{
	slimIPC *ipc;
	slimMenu *parent;
	slimScreen *playListBrowser;

public:
	slimPlayingMenu(slimDisplay *display, slimMenu *parent):
		slimScreen(display),
		parent(parent),
		playListBrowser(NULL)
	{
		ipc = display->slimConnection->ipc;
	}


	void setPlayMenu( slimScreen *playListBrowser )
	{
		this->playListBrowser = playListBrowser;
	}

	virtual void draw(char transition, int8_t param);

	virtual bool command(commands_e cmd);
};




/// Menu system to handles searches through the music database
///	It handles cascaded searches (search on result of search)
/// TODO: use the generic slimIteratorMenu to display folder contents.
class slimSearchMenu: public slimScreen
{
private:
	slimIPC	*ipc;	//needed to adjust playlist with search results
	musicDB *db;
	slimScreen *parent;

	std::list<dbQuery> query;
	string match;	//current match criterion, for the current query.
	bool	newSymbol;	//current position is at empty postion after match

	int resultCursor;	//when browsing results, this is the current position
	int menuCursor;		//depth of the search (left/right keys)

	enum menuMode {textEntry, fieldSelect, browseResults} mode;
	//toggle between the two basic menu modes:
	//		editing a query, selecting a field, viewing all results
public:
	slimSearchMenu(slimDisplay *display, slimScreen *parent, dbField startField ):
	  slimScreen(display), parent(parent)
	{
		resultCursor = 0;
		menuCursor = 0;

		mode = textEntry;
		ipc = display->slimConnection->ipc;
		db  = display->slimConnection->ipc->db;

		//initialize the search
		match = "a";
		newSymbol = false;
		query.push_back( dbQuery(db, startField, match.c_str() ) );

		//for debug:
		//mode = browseResults;
	}

	virtual void draw(char transition, int8_t param);

	bool command(commands_e cmd);
};



/// Browse the file-system
class slimBrowseMenu  : public slimGenericMenu
{
private:
	//string menuName;
	slimIPC *ipc;
	string basePath;		//root of music directory

	std::vector<string> subDirs;	//current subdir relative to base
	std::vector<std::string> items; //current menu items:
	//size_t itemsPos;

	//Helper functions:

	string fullPath(void)
	{
		string out = basePath;
		for(size_t i=0; i < subDirs.size(); i++)
			out = path::join(out, subDirs[i]);
		return out;
	}

	//last part of the pathname
	string lastPath(void)
	{
		if( subDirs.size() == 0)
			return basePath;
		else
			return subDirs.back();
	}

	bool stringLessThan( std::string a,  std::string b);

	void setItems(const std::string&  path )
	{
		items = path::listdir( path , true );
		//std::sort( items.begin(), items.end(), stringLessThan );	// Sort case insensitive
	}

public:
    slimBrowseMenu(slimDisplay *display, string name, slimScreen *parent=NULL):
					slimGenericMenu(display,name,parent)
    {
		basePath = display->slimConnection->ipc->db->getBasePath();
		ipc = display->slimConnection->ipc;
		currentItem = 0;

		setItems(  fullPath() );
	}


	/// Get current menu-item name
	string getName(size_t index)
	{
		string name;
		if( index < items.size() )
			name = items[index];
		return name;
	}

	/// Get number of items in the menu
	size_t itemSize(void)
	{	return items.size();	}


	/// Override key-handling with some special commands
	bool command(commands_e cmd);


	/// Run current menu
	void run(void)
	{
		string url;
		if( items.size() > currentItem)
			url = path::join(fullPath(), items[currentItem] );
		else
			url = fullPath();

		if( path::isdir( url  )  )
		{
			subDirs.push_back( items[currentItem] );
			setItems( fullPath() );
			currentItem = 0;
			draw('r');
		} else
			draw('R');
	}
};



/// Generic playlist menu
class favoritesMenu: public slimGenericMenu
{
private:
	slimIPC *ipc;
public:
	struct playListItem
	{
		std::string name;
		std::string url;

		playListItem(std::string name, std::string url):
			name(name), url(url)
		{ }

	};

//private:
	std::vector<playListItem> playlist;

public:
	favoritesMenu(slimDisplay *display, string name, slimScreen *parent=NULL):
					slimGenericMenu(display,name,parent)
    {
    	ipc = display->slimConnection->ipc;
		currentItem = 0;
	}


	//Overloaded methods:

	/// Get current menu-item name
	virtual string getName(size_t index)
	{
		if( index < playlist.size() )
			return playlist[index].name;
		return "";
	}

	/// Get number of items in the menu
	virtual size_t itemSize(void)
	{
		return playlist.size();
	}

	/// Run current menu
	virtual void run(void)
	{
	}

	/// override 'play' and 'add' commands:
	bool command(commands_e cmd)
	{
		bool handled = false;
		switch(cmd)
		{
		case cmd_play:
			{
				std::vector<musicFile> entries;
				musicFile f( playlist[currentItem].url.c_str() );
				f.title = playlist[currentItem].name;
				entries.push_back( f );

				//clear list, and start playing:
				ipc->setGroup( display->slimConnection->currentGroup(),  entries);
				ipc->seekList( display->slimConnection->currentGroup(), 0, SEEK_SET );

				handled = true;
				break;
			}
		case cmd_add:
			{
				std::vector<musicFile> entries;
				musicFile f( playlist[currentItem].url.c_str() );
				f.title = playlist[currentItem].name;
				entries.push_back( f );
				ipc->addToGroup( display->slimConnection->currentGroup(),  entries);
				handled = true;
				break;
			}
		default:
			handled = slimGenericMenu::command(cmd);
		}
		return handled;
	}

};




class slimVolumeScreen: public slimScreen
{
private:
	slimScreen *parent;
	//slimConnectionHandler::state_s *state;
	slimConnectionHandler *connection;
	const static int delta = 2;

	static const float timeOut; ///< time-out in seconds, after which this menu disappears
	time_t	startTime;
public:
	slimVolumeScreen(slimDisplay *display):
		slimScreen(display),
	    parent(NULL)
	{
		connection = display->slimConnection;
		//state = display->slimConnection->state;
		startTime = time(NULL);
	}

	void draw(char transition='c', int8_t param=0);

	/// Since this is a popup, the parent can be set dynamically:
	void setParent(slimScreen *newParent)
	{
		parent = newParent;
		startTime = time(NULL);		//reset timeout counter
	}

	bool command(commands_e cmd);
};


/**@}
 *end of doxygen slim-group
 */


#endif
