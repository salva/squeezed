


/**
 * \ingroup slimProto
 *@{*/


#ifndef _DISPLAY_HPP_
#define _DISPLAY_HPP_


#include <vector>
#include <string>

#include "debug.h"


#include "slimIPC.hpp"
#include "slimProto.hpp"
enum commands_e;		//slimProto includes this file.




struct rect_t {
    int x1,x2;
    int y1,y2;
};




/** Handling of squeezebox display */
class slimDisplay
{
public:
    enum displayState {dOff, dTime, dMenu, dWPS};
    slimConnectionHandler* slimConnection;

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
    //char *bitBuf;					//offset into packet

    displayState state;

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
        state = dOff;
        cls(false);
        gotoxy(0,0);
    }


    /// Update screen, send to device
    void draw(char transition='c');

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
		/*int offset = (x<<2) + (y>>3);
		int bit    = (y&7)^7;
		if(color)
            bitBuf[offset] |=  (1<<bit);
        else
            bitBuf[offset] &= (~(1<<bit));    //clear a pixel
		*/
	}

	/// Get a single pixel
	char getPixel(int x, int y)
	{
		x = util::clip(x, 0, screenWidth-1);
		y = util::clip(y, 0, screenHeight-1);
		return screen[ y * screenWidth + x];

		/*int offset = (x<<2) + (y>>3);
		int bit    = (y&7)^7;
		char b     = bitBuf[offset] & (1<<bit);
		return (b != 0);*/
	}


    void putChar(char c, bool send=false);

	/// Print a string to the display
    void print(const char *msg, bool send=false);
};



/// a full-screen window:
class slimScreen {
protected:
    slimDisplay *display;
public:
	slimScreen(slimDisplay *display): display(display)
	{ }

	virtual void draw(void)=0;

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
	virtual string currentName(void)=0;
	/// Get number of items in the menu
	virtual size_t itemSize(void) = 0;
	/// Run current menu
	virtual void run(void) = 0;

	/// Internal drawing function, with animation
    virtual void draw(char transition)
    {
		char index[8];
		sprintf(index, "%zu/%zu", currentItem+1, itemSize() );

		display->cls();
		display->gotoxy( 0, 0); display->print( menuName.c_str() );

		display->gotoxy( 320-8*strlen(index), 0 );
		display->print(index);

        string name = currentName();
        display->gotoxy(window.x1, window.y1);
        display->print( name.c_str() );
        display->draw(transition);
    }


public:
	slimGenericMenu(slimDisplay *display, string name, slimScreen *parent=NULL):
			slimScreen(display),
			menuName(name),
			parentScreen(parent)
	{
		currentItem = 0;
        //select whole screen:
        window.x1 = 20;
        window.x2 = 320;
        window.y1 = 15;
        window.y2 = 32;
	}

	/// update screen
	void draw(void)	{	draw('c');	}

	/// process a user-control-command
    virtual bool command(commands_e cmd)
    {
        bool handled = true;
		int newCurrent;
        switch(cmd)
        {
        case cmd_up:
			newCurrent = util::max<int>(currentItem-1, 0 );
			if(currentItem == (size_t)newCurrent)
				draw('U');	//doesn't work yet. probably need to set parameters also...
			else
				draw('c');
			currentItem = newCurrent;
            break;
        case cmd_down:
			newCurrent = util::min<int>(currentItem+1, itemSize()-1 );
			if(currentItem == (size_t)newCurrent)
				draw('D');
			else
				draw('c');
			currentItem = newCurrent;
            break;
		case cmd_left:
			if( parentScreen != NULL)
				display->slimConnection->setMenu( parentScreen );
			handled = true;
			break;
		case cmd_right:
			run();
			handled = true;
			break;
        default:
            handled = false;
        }
		//hash++;	//force a screen update
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
		void run(void)	{	connection->setMenu(subMenu);}
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
	string currentName(void)
	{	return items[currentItem].name;	}

	/// Get number of items in the menu
	size_t itemSize(void)
	{	return items.size();	}

	/// Run current menu
	void run(void)
	{	items[currentItem].action->run();	}
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
	string currentName(void)
	{
		return getName( getIterator(SEEK_SET) + currentItem );
	}

	/// Get number of items in the menu
	size_t itemSize(void)
	{
		return getIterator(SEEK_END) - getIterator(SEEK_SET);
	}

	/// Execute current menu-item
	void run(void)
	{
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
				device->setMenu( parent );
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
		string displayName = it->title + "(" + it->artist + ")";
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

	virtual void draw(void);

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

	void draw(void);

	bool command(commands_e cmd);
};



/// Browse the file-system
class slimBrowseMenu: public slimScreen
{
private:
	//musicDB *db;
	slimIPC *ipc;

	string basePath;		//root of music directory
	slimScreen *parentScreen;

	std::vector<string> subDirs;	//current subdir relative to base
	std::vector<string> items;		//files in current dir
	int itemsPos;			//position within the current directory


	string fullPath(void)
	{
		string out = basePath;
		for(size_t i=0; i < subDirs.size(); i++)
			out = path::join(out, subDirs[i]);
		return out;
	}

	//last part of the pathname:
	string lastPath(void)
	{
		if( subDirs.size() == 0)
			return basePath;
		else
			return subDirs.back();
	}


public:
	slimBrowseMenu(slimDisplay *display, slimScreen *parent ): slimScreen(display)
	{
		basePath = display->slimConnection->ipc->db->getBasePath();
		ipc = display->slimConnection->ipc;
		itemsPos = 0;
		parentScreen = parent;

		//get file listing:
		items = path::listdir( fullPath() );
	}


	void draw(void);

	/// process a user-control-command
    virtual bool command(commands_e cmd);
};


/**@}
 *end of doxygen slim-group
 */


#endif
