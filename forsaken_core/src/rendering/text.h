#pragma once

/* text.h -- HUD bitmap-font printing and the in-game message queues
 *
 * text.cpp owns the font character map (InitFont, process lifetime), the
 * MAX_MESSAGES-line message queues drawn by MessageQuePrint and
 * PlayerMessageQuePrint, and the per-level trigger text read from the level
 * .txt / .msg files (ReadTxtFile / ReadMsgFile, freed by FreeTxtFile /
 * FreeMsgFile).
 *
 * Coordinates are screen pixels. Print4x5Text with x == -1 continues from the
 * last printed x, returns the next x, and drops a line that would cross the
 * bottom edge. Glyphs are queued as screen polys and appear when that pass
 * draws. Colour arguments index Colourtrans[], which must match colors_t.
 */

#include "game/main.h"
#include "game/constants.h"

/*
 * Defines
 */

constexpr int MAX_MESSAGES = 4;
constexpr int MAX_MESSAGES_LONG = 100;
#ifdef DEBUG_ON
constexpr auto MAXTEXTMESSAGES = (128+256);
#else
constexpr int MAXTEXTMESSAGES = 128;
#endif
constexpr int MAXPERLINE = 512; // message buffer width - line width set in (text.cpp) MessageQuePrint/All

/*
 * Typedefs
 */
struct TEXTMSGINFO {
	bool	Activated;
	int16_t	ActivatedOrder;
	int16_t	Type;
};


/*
 * fn prototypes
 */

void PrintScoreSort( void );
void CenterPrint4x5Text( const char * Text , int y , int color );
void CenterPrint4x5Text2( char * Text , int x, int y, int color );
int	 Print4x5Text( const char * Text , int x , int y , int color );
void Print4x5TextSmall( char * Text , int x , int y , int color );
void Printu_int16_t( u_int16_t tempnum , int x , int y , int color );
void MessageQuePrint( void );
void PlayerMessageQuePrint( void );
void AddColourMessageToQue(  int Colour, const char * Text, ... );
void AddPlayerMessageToQue( int Colour, const char * Text, ... );
void InitTextMessages( void );

void InitFont( void );

bool ReadTxtFile( char *Filename );
void FreeTxtFile( void );
void TriggerTextMessage( u_int16_t * Data );
bool ReadMsgFile( char *Filename );
void FreeMsgFile( void );

void NextTextMessage( void );
void LastTextMessage( void );

FILE * LoadAllText( FILE * fp );
FILE * SaveAllText( FILE * fp );


void DisplayConnectionStatus( int num , int x , int y);


// if you add a color then up this count...
constexpr int MAXFONTCOLOURS = 11;

// must align this enum with rgb values in Colourtrans (text.cpp)
enum colors_t {
	GRAY,
	RED,
	GREEN,
	BLUE,
	YELLOW,
	CYAN,
	PURPLE,
	OFFGREEN,
	DARKGRAY,
	BLACK,
	WHITE
};


/* Defined in text.cpp; shared with the modules that use it. */
int Printint16_t( int16_t num , int x , int y , int color );
extern const char *TeamName[ MAX_TEAMS ];
extern u_int8_t Colourtrans[ MAXFONTCOLOURS ][ 3 ];
extern int TeamCol[ MAX_TEAMS ];
extern char * StatusTab[ 256 ];
void MessageQuePrintAll( void );
extern int FontWidth;
extern int FontHeight;
extern float MaxMessageTime;
