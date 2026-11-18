
/*===================================================================
		Include File...	
===================================================================*/

#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>

#include "math/new3d.h"

#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "game/ships.h"
#include "game/main.h"
#include "ui/title.h"
#include "rendering/text.h"
#include "rendering/screenpolys.h"

#include "ui/local.h"
#include "game/stats.h"
#include "util/util.h"
#include "rendering/render.h"
#include "util/timer.h"
#include "game/goal.h"
#include "game/oct2.h"

constexpr int MSG_VERSION_NUMBER = 1;

// color names are defined in text.h colors_t
u_int8_t Colourtrans[MAXFONTCOLOURS][3] = {
	// r, g, b   values...
	{ 192,192,192 },	// gray
	{ 255,64,64 },		// red 
	{ 64,255,64 },		// green 
	{ 64,64,255 },		// blue 
	{ 255,255,64 },		// yellow
	{ 64,255,255 },		// cyan
	{ 255,64,255 },		// purple
	{ 128,255,128 },	// off green
	{ 64,64,64 },		// dark gray
	{ 0,0,0 },		// black
	{ 255,255,255 }		// white
};

/*===================================================================
		Globals ...
===================================================================*/

float MaxMessageTime = 5.0f;

int16_t	TextActivatedOrder = 0;
int16_t	CurrentTextActivated = 0;
int	TeamCol[MAX_TEAMS] = { 1, 2, 3, 4 };
const char *TeamName[ MAX_TEAMS ] = // team colour names should match TeamCol[] colours (given in Colourtrans[][] below)
{
	LT_TEAM_1, // red
	LT_TEAM_2, // green
	LT_TEAM_3, // blue
	LT_TEAM_4  // yellow
};

int FontWidth = 8;
int FontHeight = 8;


int	PermX;
u_int16_t big[4] = { 10000 , 1000 , 100 , 10 };
u_int16_t little[4] = { 1000 , 100 , 10 , 1 };

// player text messages
float PlayerMessageTime[MAX_MESSAGES] = { 0.0F , 0.0F , 0.0F , 0.0F };
char PlayerMessageBank[MAX_MESSAGES][MAXPERLINE];
int PlayerMessageColours[MAX_MESSAGES];
// normal text messages short que
float MessageTime[MAX_MESSAGES] = { 0.0F , 0.0F , 0.0F , 0.0F };
int MessageColour[MAX_MESSAGES];
char MessageBank[MAX_MESSAGES][MAXPERLINE];
// all messages long que
char MessageBankLong[MAX_MESSAGES_LONG][MAXPERLINE];
int MessageColourLong[MAX_MESSAGES_LONG];

u_int8_t	CharTrans[256];

u_int16_t NumOfTextMessages = 0;
const char * TextMessagesPnt[MAXTEXTMESSAGES];
char * TextMessages = NULL;
TEXTMSGINFO * TextMsgInfo = NULL;


/*===================================================================
	Procedure	:		Print a int16_t number in small 4x4 chars..
	Input		:		int16_t num, u_int16_t x , u_int16_t y
	Output		:		nothing
===================================================================*/

int Printint16_t( int16_t num , int x , int y , int color )
{
	static char buf[ 128 ];

	if( num != -32767 )
	{
		sprintf( buf, "%hd", num );
	}else{
		sprintf( buf, "%hd", 0 );
	}
	Print4x5Text( buf, x, y, color );
	return strlen(buf);
}


/*===================================================================
	Procedure	:		Print a u_int16_t number in small 4x4 chars..
	Input		:		u_int16_t num, u_int16_t x , u_int16_t y
	Output		:		nothing
===================================================================*/

void Printu_int16_t( u_int16_t tempnum , int x , int y , int col )
{
	int i;
	int num;
	int	Zeros= 0 ;
	u_int8_t r , g , b;

	if( (y + FontHeight ) >= render_info.window_size.cy )
		return;
	
	r = Colourtrans[col][0];
	g = Colourtrans[col][1];
	b = Colourtrans[col][2];

	for( i = 0 ; i < 4 ; i++ )
	{
		num = tempnum % big[i];
		num /= little[i];
		if( ( num + Zeros != 0 ) || ( i == 3 ) )
		{
			Zeros = 1;
			AddScreenPolyText( (u_int16_t) (num+1), (float) x , (float) y, r, g, b, 255 );
			x += FontWidth;
		}
	}
}

/*===================================================================
	Procedure	:		Print some Centered text at a specified Y
	Input		:		char * Text, u_int16_t y
	Output		:		nothing
===================================================================*/
void CenterPrint4x5Text( const char * Text , int y, int col )
{
	const char * Text2;
	int x;
	int	i;

	Text2 = Text;
	i = 0;
	while( *Text2++ != 0 ) i++;

	x = (render_info.window_size.cx >> 1 ) - (i * ( FontWidth >> 1 )  ); // i *2 is half the size of the chars...
	
	Print4x5Text( Text , x , y , col );	
}

/*===================================================================
	Procedure	:		Print some Centered text at a specified Y
	Input		:		char * Text, u_int16_t y
	Output		:		nothing
===================================================================*/
void CenterPrint4x5Text2( char * Text , int x, int y, int col )
{
	char * Text2;
	int	i;

	Text2 = Text;
	i = 0;
	while( *Text2++ != 0 ) i++;

	x -= (i * ( FontWidth >> 1 )  ); // i *2 is half the size of the chars...
	
	Print4x5Text( Text , x , y , col );
}


void Print4x5TextSmall( char * Text , int x , int y , int color )
{
		int OldFontHeight = FontHeight;
		int OldFontWidth = FontWidth;
		int oldScale = TextScaleSlider.value;

		FontHeight = 8.0F;
		FontWidth = 8.0F;
		TextScaleSlider.value = 0;

		Print4x5Text( Text , x , y , color );

		FontHeight = OldFontHeight;
		FontWidth = OldFontWidth;
		TextScaleSlider.value = oldScale;
}

/*===================================================================
	Procedure	:		Print some text at a specified or the last pos
	Input		:		char * Text, u_int16_t x , u_int16_t y
	Output		:		last x position
===================================================================*/
int Print4x5Text( const char * Text , int x , int y , int color )
{
	u_int8_t num;
	u_int8_t r , g , b;

	if( (y + FontHeight ) >= render_info.window_size.cy )
		return PermX;

	r = Colourtrans[color][0];
	g = Colourtrans[color][1];
	b = Colourtrans[color][2];

	if( x != -1 )
		PermX = x;
	while( ( num = *Text++ ) != 0 )
	{
		num = CharTrans[num];

		if ( num )
				AddScreenPolyText( (u_int16_t) num, (float) PermX , (float) y, r , g , b, 255 );
	
		PermX += FontWidth;
	}

	return PermX;
}


/*===================================================================
	Procedure	:		Print All Message in the player message Que (last 4 messages)...
	Input		:		nothing
	Output		:		nothing
===================================================================*/
void	PlayerMessageQuePrint( void )
{
	int i,y,z,MAX, ypos;
	char MessageBuff[150];

	/* @vfp-exempt R11 — HUD message wrap width, a few messages per frame at most; FontWidth constant per font. measured n/a. Verified 2026-10-25. */
	MAX = floorf((render_info.window_size.cx-FontWidth*27)/FontWidth);
	if(MAX < 10) MAX = 10;
	else if(MAX > 149) MAX = 149;

	ypos = render_info.window_size.cy- FontHeight;

	for( i=MAX_MESSAGES-1, y=0; i>-1; i-- )
	{
		PlayerMessageTime[i] -= (real_framelag/5.0F);
		if( PlayerMessageTime[i] <= 0.0F )
		{
			PlayerMessageTime[i] = 0.0F;
			PlayerMessageBank[i][0] = 0;
		}
		else
		{
				for(z=0; z*MAX < MAXPERLINE; z++)
				{
					if(strcmp(&PlayerMessageBank[i][MAX*z],(const char*) "\0") != 0)
						ypos-= FontHeight;
					else
						break;
				}	
		}
	}

	for( i=MAX_MESSAGES-1, y=0; i>-1; i-- )
	{
		if( PlayerMessageTime[i] > 0.0F )
		{
				for(z=0; z*MAX < MAXPERLINE; z++)
				{
					if(strcmp(&PlayerMessageBank[i][MAX*z],(const char*) "\0") != 0)
					{
						strncpy(&MessageBuff[0],&PlayerMessageBank[i][MAX*z],MAX);
						MessageBuff[MAX] = 0;	// a full-width chunk has no terminator of its own
						
						Print4x5Text( &MessageBuff[0], FontWidth*12 , ypos+FontHeight*y , PlayerMessageColours[i] ); // left-aligned
						y++;
					}
					else
						break;
				}	
		}
	}
}
				
/*===================================================================
	Procedure	:		Print All Message in the normal Que (last 4 messages)...
	Input		:		nothing
	Output		:		nothing
===================================================================*/
void	MessageQuePrint( void )
{
	int i,y,z,MAX;
	char MessageBuff[150];

	/* @vfp-exempt R11 — HUD message wrap width, a few messages per frame at most; FontWidth constant per font. measured n/a. Verified 2026-10-25. */
	MAX = floorf((render_info.window_size.cx-FontWidth*25)/FontWidth);
	if(MAX < 10) MAX = 10;
	else if(MAX > 149) MAX = 149;

	for( i=0, y=0; i<MAX_MESSAGES; i++ )
	{
		MessageTime[i] -= real_framelag;
		if( MessageTime[i] <= 0.0F )
		{
			MessageTime[i] = 0.0F;
			MessageBank[i][0] = 0;
		}
		else
		{
				for(z=0; z*MAX < MAXPERLINE; z++)
				{
					if(strcmp(&MessageBank[i][MAX*z],(const char*) "\0") != 0)
					{
						strncpy(&MessageBuff[0],&MessageBank[i][MAX*z],MAX);
						MessageBuff[MAX] = 0;	// a full-width chunk has no terminator of its own
						CenterPrint4x5Text( &MessageBuff[0],((y*(FontHeight+1))+16) , MessageColour[i] );	// original centered
						y++;
					}
					else
						break;
				}	
		}
	}
}

/*===================================================================
	Procedure	:		Print All Message in the long Que (last 50 messages)...
	Input		:		nothing
	Output		:		nothing
===================================================================*/
void	MessageQuePrintAll( void )
{
	int i,y,z,MAX;
	char MessageBuff[150];

	/* @vfp-exempt R11 — HUD message wrap width, a few messages per frame at most; FontWidth constant per font. measured n/a. Verified 2026-10-25. */
	MAX = floorf((render_info.window_size.cx-FontWidth*25)/FontWidth);
	if(MAX < 10) MAX = 10;
	else if(MAX > 149) MAX = 149;

	for (i=0, y=0; i < MAX_MESSAGES_LONG; i++)
	{
			for(z=0; z*MAX < MAXPERLINE; z++)
			{
				if(strcmp(&MessageBankLong[i][MAX*z],(const char*) "\0") != 0)
				{
					strncpy(&MessageBuff[0],&MessageBankLong[i][MAX*z],MAX);
					MessageBuff[MAX] = 0;	// a full-width chunk has no terminator of its own
					Print4x5Text( &MessageBuff[0], FontWidth*23 , ((y*(FontHeight+1))+16) , MessageColourLong[i] );
					y++;
				}
				else
					break;
			}
	}
}

char TempMessage[512];
float MessageSize;
float ThisMessageTime;

/*===================================================================
	Procedure	:		Add Colour Player Message to the Que (text messages)...
	Input		:		char * Text
	Output		:		nothing
===================================================================*/

void AddPlayerMessageToQueShort( char ** Text, int Colour )
{
	char * Pnt;
	int i,x,j;
	Pnt = *Text;

	for(x = MAX_MESSAGES-1; x > 0; x--)
	{
		for(j = 0; j < MAXPERLINE; j++)
			PlayerMessageBank[x][j] = 0;
		strcpy((char *)PlayerMessageBank[x], (char *)PlayerMessageBank[x-1]);
		PlayerMessageColours[x] = PlayerMessageColours[x-1];
		PlayerMessageTime[x] = PlayerMessageTime[x-1];
		PlayerMessageColours[x] = PlayerMessageColours[x-1];
	}

	for(j = 0; j < MAXPERLINE; j++)
			PlayerMessageBank[0][j] = 0;
	PlayerMessageTime[0] = ThisMessageTime;
	PlayerMessageColours[0] = Colour;
	MessageColourLong[0] = Colour;

	for( i = 0 ; i < MAXPERLINE ; i++ )
	{
		PlayerMessageBank[0][i] = *Pnt;
		MessageBankLong[0][i] = *Pnt;

		if( *Pnt == 0 )
		{
			*Text = NULL;
			return;
		}
		Pnt++;
	}
}

/*===================================================================
	Procedure	:		Add Colour Message to the Que Short...
	Input		:		char * Text
	Output		:		nothing
===================================================================*/
void AddColourMessageToQueShort( char ** Text, int Colour )
{
	char * Pnt;
	int i,x,j;
	Pnt = *Text;

	for(x = MAX_MESSAGES-1; x > 0; x--)
	{
		for(j = 0; j < MAXPERLINE; j++)
			MessageBank[x][j] = 0;
		strcpy((char *)MessageBank[x], (char *)MessageBank[x-1]);
		MessageColour[x] = MessageColour[x-1];
		MessageTime[x] = MessageTime[x-1];
	}

	for(j = 0; j < MAXPERLINE; j++)
			MessageBank[0][j] = 0;
	MessageTime[0] = ThisMessageTime;
	MessageColour[0] = Colour;
	MessageColourLong[0] = Colour;

	for( i = 0 ; i < MAXPERLINE ; i++ )
	{
		MessageBank[0][i] = *Pnt;
		MessageBankLong[0][i] = *Pnt;

		if( *Pnt == 0 )
		{
			*Text = NULL;
			return;
		}
		Pnt++;
	}
}


void AddPlayerMessageToQue( int Colour, const char * Text, ... )
{
	int x, j;

	// -1 = do not display
	if(Colour > -1)
	{
		char * Pnt;
		va_list args;		

		for(x = MAX_MESSAGES_LONG-1; x > 0; x--)
		{
			for(j = 0; j < MAXPERLINE; j++)
				MessageBankLong[x][j] = 0;

			strcpy((char *)MessageBankLong[x], (char *)MessageBankLong[x-1]);
			MessageColourLong[x] = MessageColourLong[x-1];
		}

		for(j = 0; j < MAXPERLINE; j++)
				MessageBankLong[0][j] = 0;

		va_start( args, Text );
		vsnprintf( &TempMessage[0], sizeof(TempMessage)-1, Text, args);
		va_end( args );

		TempMessage[sizeof(TempMessage)-1] = 0;

		Pnt = &TempMessage[0];

		MessageSize = (float) strlen(&TempMessage[0]);
		ThisMessageTime = MaxMessageTime * (MessageSize / MAXPERLINE);
		if( ThisMessageTime < MaxMessageTime )
			ThisMessageTime = MaxMessageTime;

		while( Pnt )
			AddPlayerMessageToQueShort( &Pnt, Colour );
	}
}

void AddColourMessageToQue( int Colour, const char * Text, ... )
{
	int x, j;

	// -1 = do not display
	if(Colour > -1)
	{
		char * Pnt;
		va_list args;		

		for(x = MAX_MESSAGES_LONG-1; x > 0; x--)
		{
			for(j = 0; j < MAXPERLINE; j++)
				MessageBankLong[x][j] = 0;

			strcpy((char *)MessageBankLong[x], (char *)MessageBankLong[x-1]);
			MessageColourLong[x] = MessageColourLong[x-1];
		}

		for(j = 0; j < MAXPERLINE; j++)
				MessageBankLong[0][j] = 0;

		va_start( args, Text );
		vsnprintf( &TempMessage[0], sizeof(TempMessage)-1, Text, args);
		va_end( args );

		TempMessage[sizeof(TempMessage)-1] = 0;

		Pnt = &TempMessage[0];

		MessageSize = (float) strlen(&TempMessage[0]);
		ThisMessageTime = MaxMessageTime * (MessageSize / MAXPERLINE);
		if( ThisMessageTime < MaxMessageTime )
			ThisMessageTime = MaxMessageTime;

		while( Pnt )
			AddColourMessageToQueShort( &Pnt, Colour );
	}
}
	
/*===================================================================
	Procedure	:		Print the Scores....
	Input		:		nothing
	Output		:		nothing
===================================================================*/
constexpr float FLASH_RATE = ( 15.0F );
float	FlashSpeed = 0.0F;
bool	FlashToggle = false;

void PrintScoreSort( void )
{
	bool teamOK[MAX_TEAMS];
	bool teamleft[MAX_TEAMS];
	int i,e, pos;
	int16_t TeamBounty[MAX_TEAMS];
	char buf[256];
	int col;
	u_int16_t scorewidth = 1;
	u_int16_t tempwidth = 0;
	BYTE ShipID;

	// single player
	if( MyGameStatus == STATUS_SinglePlayer )
	{
		Print4x5Text( LIVES , 8 , FontHeight , 2 );
		Printu_int16_t( (u_int16_t) Lives , 8+(8*FontWidth) , FontHeight , 1 );
		return;
	}

	// flash players name
	FlashSpeed += framelag;
	if( FlashSpeed >= FLASH_RATE )
	{
		FlashToggle = !FlashToggle;
		FlashSpeed = FMOD( FlashSpeed, FLASH_RATE );
	}
	
	if( !TeamGame )
	{
		if( ShowPlayersOnHUD )
 		{
			int top_offset = FontHeight; // initial gap
			int line_height = FontHeight+1;	

			// calculate the width of the score column
			if(ShowPlayersOnHUDbyKills)
			{
				for(i=0; i<MAX_PLAYERS; i++)
				{
					tempwidth = (u_int16_t) log10(abs(GetKills(i)))+1;
					if(GetKills(i) < 0)	tempwidth++;
					if(tempwidth > scorewidth) scorewidth = tempwidth;			
				}
			}
			else
			{
				for(i=0; i<MAX_PLAYERS; i++)
				{
					tempwidth = (u_int16_t) log10(abs(GetRealScore(i)))+1;
					if(GetRealScore(i) < 0)	tempwidth++;
					if(tempwidth > scorewidth) scorewidth = tempwidth;
				}
			}
			scorewidth *= FontWidth;

			// print player lines
			for( i = 0 ; i < MAX_PLAYERS ; i++ )
			{
				int left_offset = 0; // offset from left

				// get player id
				ShipID = GetPlayerByRank(i);

				// make sure it's a valid player
				if( GameStatus[ShipID] != STATUS_Normal )
					continue;

				// blue dot for bad ping
				DisplayConnectionStatus( ShipID, 2, top_offset );

				// give packet loss flag a space
				left_offset = FontWidth;

				// print name
				if ( !( Ships[ShipID].Object.Flags & SHIP_CarryingBounty ) || FlashToggle )
					Print4x5Text( &Names[ShipID][0], left_offset, top_offset, (( WhoIAm == ShipID ) ? WHITE : RED) );
				else
					Print4x5Text( &Names[ShipID][0], left_offset, top_offset, WHITE );

				left_offset += ( 8 * FontWidth );
				
				// print real score
				if(ShowPlayersOnHUDbyKills)
					// kills - suicides - friendly
					Printint16_t( GetKills(ShipID), left_offset, top_offset, GRAY ); 
				else
					// points + kills - suicides - friendly - deaths
					Printint16_t( GetRealScore(ShipID), left_offset, top_offset, GRAY );

				left_offset += scorewidth+8.0F; // give a padding space

				// Show pings for everyone except yourself
				if( ShipID != WhoIAm )
				{
					if( Ships[ShipID].network_player != NULL )
					{
						// Show ping
						sprintf( (char*) &buf[0] ,"%*dms", 4, (u_int16_t) Ships[ShipID].network_player->ping);
						Print4x5TextSmall( &buf[0], left_offset, top_offset+((FontHeight-8.0F)/2.0F), ((GameStatus[ShipID] == STATUS_Left) ? DARKGRAY : GREEN) );

                        // Show % Health
                        if(ShowPlayerHealthByScores && GameStatus[ShipID] != STATUS_Left)
                        {
							sprintf( (char*) &buf[0], "H:%d", (u_int16_t) ceilf(((PlayerHealths[ShipID].Shield + PlayerHealths[ShipID].Hull) /2.56F)));
							Print4x5TextSmall( &buf[0], left_offset+60.0F, top_offset+((FontHeight-8.0F)/2.0F), ShipHealthColour[ShipID] );
                        }

					}
				}

				top_offset += line_height;
			}
		}
	}

	// team game
	else
	{
		for (i = 0; i < MAX_TEAMS; i++)
		{	teamOK[i] = false;
			teamleft[i] = true;
			TeamCol[i] = i + 1;
			TeamBounty[ i ] = 0;
		}
		
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			if ( (TeamNumber[i] < MAX_TEAMS) && (GameStatus[i] == STATUS_Normal ) )
			{
				teamOK[TeamNumber[i]] = true;
				switch ( GameStatus[i] )
				{
					case STATUS_Left:
					case STATUS_LeftCrashed:
						break;
					default:
						teamleft[TeamNumber[i]] = false;
						if ( Ships[ i ].Object.Flags & SHIP_CarryingBounty )
							TeamBounty[ TeamNumber[ i ] ]++;
						break;
					}
			}
		}

		pos = 0;
		buf[ 0 ] = 0;

		for (i = 0; i < MAX_TEAMS; i++)
		{
			if (teamOK[i])
			{
				bool show;

				show = false;
				if ( CaptureTheFlag || CTF )
				{
					if ( !TeamFlag[ i ] || FlashToggle )
						show = true;
				}
				else if ( BountyHunt )
				{
					if ( !TeamBounty[ i ] || FlashToggle )
						show = true;
				}
				else
					show = true;
				if ( show )
				{
					col = ( teamleft[i] ) ? 8 :
						  ( TeamNumber[ WhoIAm ] == i && FlashToggle ) ? 0 : TeamCol[i] ;
					Print4x5Text( TeamName[ i ] , 8 , pos*(FontHeight+1)+FontHeight , col );
					Printint16_t( GetTeamScoreByTeamNumber(i) , 8+(7*FontWidth) , pos*(FontHeight+1)+FontHeight , col );
				}
				pos++;
			}
		}

		// show team info
		if( ShowTeamInfo )
		{
			e = pos + 1;
			for( i = 0 ; i < MAX_PLAYERS ; i++ )
			{
				if( ( TeamNumber[WhoIAm] == TeamNumber[i] || SwitchedToWatchMode) 
						&& (GameStatus[i] == STATUS_Normal) && (Ships[i].Object.Mode != LIMBO_MODE) )
				{
					col = ( WhoIAm == i ) ? 0 : TeamCol[ TeamNumber[ i ] ];
					Print4x5Text( &Names[i][0] , FontWidth , e*(FontHeight+1)+(FontHeight*4) , col );

					Print4x5TextSmall( &Mloadheader.Group[Ships[i].Object.Group].name[0] , 8+(FontWidth*8), e*(FontHeight+1)+(FontHeight*4)+((FontHeight-8.0F)/2.0F), 2 );

					DisplayConnectionStatus( i , 2 , e*(FontHeight+1)+(FontHeight*4) );
					e++;
				}
			}
		}
	}
}

/*===================================================================
	Procedure	:		Read in the level specific text messages.......
	Input		:		char *Filename
	Output		:		bool true/false
===================================================================*/
bool ReadTxtFile( char *Filename )
{
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;

	if ( Debug )
	{
		int i;
		for( i = 0 ; i < MAXTEXTMESSAGES ; i ++ )
			TextMessagesPnt[i] = "\n";
	}


	TextMessages = NULL;
	File_Size = Get_File_Size( Filename );
	if( !File_Size )
	{
		// dont need water...
		return true;
	}
	Buffer = (char*)calloc( File_Size+1 , 1 );
	if( !Buffer )
	{
		Msg( "ReadTxtFile : Unable to allocate file buffer\n", Filename );
		return( false );
	}
	Read_Size = Read_File( Filename, Buffer, File_Size );
	if( Read_Size != File_Size )
	{
		Msg( "ReadTxtFile Load Error reading %s\n", Filename );
		return( false );
	}
	TextMessages = Buffer;


	TextMessagesPnt[0] = strtok( TextMessages , "\n" );

	NumOfTextMessages = 0;

	while( TextMessagesPnt[NumOfTextMessages] )
	{

		TextMessagesPnt[++NumOfTextMessages] = strtok( NULL , "\n" );
	}
	return true;
}
/*===================================================================
	Procedure	:		Free level specific text messages.......
	Input		:		void
	Output		:		void
===================================================================*/
void FreeTxtFile( void )
{
	if( TextMessages )
	{
		free( TextMessages );
		TextMessages = NULL;
	}
	NumOfTextMessages = 0;
}
/*===================================================================
	Procedure	:		Trigger text message.......
	Input		:		u_int16_t * Data
	Output		:		void
===================================================================*/
void TriggerTextMessage( u_int16_t * Data )
{
	TEXTMSGINFO * Tmi;
	bool	DoneBefore = false;

	Tmi = TextMsgInfo;
	if( Tmi )
	{
		Tmi += *Data;
		if( Tmi->Type )
		{
			DoneBefore = Tmi->Activated;
			Tmi->Activated = true;
		}
	}

	if(!DoneBefore)
	{
		PlaySfx( SFX_MessageBeep, 1.0F );
		AddColourMessageToQue( SystemMessageColour, "%s\n", TextMessagesPnt[*Data] );
		if( Tmi->ActivatedOrder == -1 )
		{
			Tmi->ActivatedOrder = TextActivatedOrder++;
		}
	}
}


/*===================================================================
	Procedure	:		Init the text messages......
	Input		:		void
	Output		:		void
===================================================================*/
void InitTextMessages( void )
{
	int i;
	for( i = 0 ; i < MAX_MESSAGES ; i++ )
	{
		MessageTime[i] = 0.0F;
		MessageBank[i][0] = 0;
		PlayerMessageTime[i] = 0.F;
		PlayerMessageBank[i][0] = 0;
		MessageBankLong[i][0] = 0;
	}
}
typedef struct {
	const char *keyword;
	int  num;
} MESSAGE_NUM_LOOKUP;

typedef struct {
	const char *keyword;
	void *var_ptr;
} MESSAGE_VAR_LOOKUP;


/*===================================================================
	Procedure	:		Read in the level specific text message Info.......
	Input		:		char *Filename
	Output		:		bool true/false
===================================================================*/
bool ReadMsgFile( char *Filename )
{
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	char		*	OrgBuffer;
	int16_t		*	int16Pnt;
	u_int32_t		MagicNumber;
	u_int32_t		VersionNumber;
	u_int32_t		* u_int32Pnt;
	TEXTMSGINFO * Tmi;
	int i;

	TextActivatedOrder = 0;
	CurrentTextActivated = 0;
	TextMsgInfo = NULL;
	if( !NumOfTextMessages)
		return true;
	
	File_Size = Get_File_Size( Filename );
	if( !File_Size )
	{
		// dont need water...
		return true;
	}
	Buffer = (char*)calloc( File_Size+1 , 1 );
	if( !Buffer )
	{
		Msg( "ReadMsgFile : Unable to allocate file buffer\n", Filename );
		return( false );
	}
	Read_Size = Read_File( Filename, Buffer, File_Size );
	if( Read_Size != File_Size )
	{
		Msg( "ReadMsgFile Load Error reading %s\n", Filename );
		return( false );
	}
	OrgBuffer = Buffer;

	u_int32Pnt = (u_int32_t*) Buffer;
	MagicNumber = *u_int32Pnt++;
	VersionNumber = *u_int32Pnt++;

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != MSG_VERSION_NUMBER  ) )
	{
		Msg( "ReadMsgFile() Incompatible msg file %s", Filename );
		free( OrgBuffer );
		return( false );
	}

	int16Pnt = (int16_t*) u_int32Pnt;
	if( *int16Pnt++ != NumOfTextMessages )
	{
		Msg( "ReadMsgFile : .Msg not compatible with .Txt\n" );
		return( false );
	}
	TextMsgInfo = (TEXTMSGINFO*) malloc( NumOfTextMessages * sizeof(TEXTMSGINFO) );
	if( !TextMsgInfo )
	{
		Msg( "ReadMsgFile : Unable to allocate memory buffer\n", Filename );
		return( false );
	}
	Tmi = TextMsgInfo;

	for( i = 0 ; i < NumOfTextMessages ; i++ )
	{
		Tmi->Activated = false;
		Tmi->Type = *int16Pnt++;
		Tmi->ActivatedOrder = -1;
		Tmi++;
	}
	free( OrgBuffer );
	return true;
}
/*===================================================================
	Procedure	:		Free level specific text message info.......
	Input		:		void
	Output		:		void
===================================================================*/
void FreeMsgFile( void )
{
	if( TextMsgInfo )
	{
		free( TextMsgInfo );
		TextMsgInfo = NULL;
	}
}


/*===================================================================
	Procedure	:		Trigger text message.......
	Input		:		u_int16_t * Data
	Output		:		void
===================================================================*/
void DisplayOldTextMessage( void )
{
	TEXTMSGINFO * Tmi;
	int16_t	i;

	Tmi = TextMsgInfo;
	if( !Tmi )
		return;

	for( i = 0 ; i < ((Debug)?MAXTEXTMESSAGES:NumOfTextMessages) ; i++ )
	{
		if( Tmi->ActivatedOrder == CurrentTextActivated )
		{
			PlaySfx( SFX_MessageBeep, 1.0F );
			AddColourMessageToQue( SystemMessageColour, "%s\n", TextMessagesPnt[i] );
			return;
		}
		Tmi++;
	}
}

/*===================================================================
	Procedure	:		Show next previously Triggerd text message.......
	Input		:		void
	Output		:		void
===================================================================*/
void NextTextMessage( void )
{
	CurrentTextActivated++;
	if( CurrentTextActivated >= TextActivatedOrder )
	{
		CurrentTextActivated = 0;
	}
	if( TextActivatedOrder )
	{
		DisplayOldTextMessage();
	}
}
/*===================================================================
	Procedure	:		Show Last previously Triggerd text message.......
	Input		:		void
	Output		:		void
===================================================================*/
void LastTextMessage( void )
{
	CurrentTextActivated--;
	if( CurrentTextActivated < 0 )
	{
		CurrentTextActivated = TextActivatedOrder;
		if( TextActivatedOrder )
			CurrentTextActivated--;
	}
	if( TextActivatedOrder )
	{
		DisplayOldTextMessage();
	}
}

/*===================================================================
	Procedure	:		Save all text...
	Input		:		FILE * fp
	Output		:		bool
===================================================================*/
FILE * SaveAllText( FILE * fp )
{
	TEXTMSGINFO * Tmi;
	int i;

	fwrite( &NumOfTextMessages, sizeof( NumOfTextMessages ), 1, fp );
	fwrite( &TextActivatedOrder, sizeof( TextActivatedOrder ), 1, fp );
	fwrite( &CurrentTextActivated, sizeof( CurrentTextActivated ), 1, fp );

	Tmi = TextMsgInfo;
	if( !Tmi )
		return fp;

	for( i = 0 ; i < NumOfTextMessages ; i++ )
	{
		fwrite( &Tmi->Activated, sizeof( Tmi->Activated ), 1, fp );
		fwrite( &Tmi->ActivatedOrder, sizeof( Tmi->ActivatedOrder ), 1, fp );
		Tmi++;
	}
	return fp;
}

/*===================================================================
	Procedure	:		Save all text...
	Input		:		FILE * fp
	Output		:		bool
===================================================================*/
FILE *	LoadAllText( FILE * fp )
{
	TEXTMSGINFO * Tmi;
	int i;
	u_int16_t TempNumOfTextMessages = 0;


	fread( &TempNumOfTextMessages, sizeof( NumOfTextMessages ), 1, fp );
	if( TempNumOfTextMessages != NumOfTextMessages )
		return NULL;

	fread( &TextActivatedOrder, sizeof( TextActivatedOrder ), 1, fp );
	fread( &CurrentTextActivated, sizeof( CurrentTextActivated ), 1, fp );

	Tmi = TextMsgInfo;
	if( !Tmi )
		return fp;

	for( i = 0 ; i < NumOfTextMessages ; i++ )
	{
		fread( &Tmi->Activated, sizeof( Tmi->Activated ), 1, fp );
		fread( &Tmi->ActivatedOrder, sizeof( Tmi->ActivatedOrder ), 1, fp );
		Tmi++;
	}
	return fp;
}

/*===================================================================
	Procedure	:		Fill Status messages..
	Input		:		void
	Output		:		void
===================================================================*/
char * StatusTab[256];

/*===================================================================
	Procedure	:		Display a connection setting...
	Input		:		void
	Output		:		void
===================================================================*/
void DisplayConnectionStatus( int num , int x , int y)
{
    u_int16_t dot;

	// protect against bad input
	if( num < 0 || num > MAX_PLAYERS-1 || num == WhoIAm || Ships[num].network_player == NULL )
		return;

	// calculate which colour flag to use
    if(Ships[num].network_player->packets_lost == 0)
        dot = 84; // green
    else if(Ships[num].network_player->packets_lost == 1)
        dot = 83; // blue
    else
        dot = 85; // red
	
	// display
    AddScreenPolyText( dot, (float) x , (float) y+2, 255, 255, 255, 255 );
}


// maps ascii character codes to location on texture
void InitFont( void )
{
	u_int16_t i;
	u_int8_t e;

	ZERO_STACK_MEM(CharTrans);

	for( i = 0x30, e = 1 ; i < 0x3a ; i++ , e++ ) CharTrans[i] = e; // 0 - 9
	for( i = 0x41, e = 11; i < 0x5b ; i++ , e++ ) CharTrans[i] = e; // A - Z
	for( i = 0x61, e = 11; i < 0x7b ; i++ , e++ ) CharTrans[i] = e; // a - z

	CharTrans[(u_int8_t)'.'] = 37;

	e = 73;
	CharTrans[(u_int8_t)','] = e++;
	CharTrans[(u_int8_t)';'] = e++;
	CharTrans[(u_int8_t)'['] = CharTrans[(u_int8_t)'('] = e++; // render same
	CharTrans[(u_int8_t)']'] = CharTrans[(u_int8_t)')'] = e++; // render same
	CharTrans[(u_int8_t)'*'] = e++;
	CharTrans[(u_int8_t)'/'] = e++;
	CharTrans[(u_int8_t)'+'] = e++;
	CharTrans[(u_int8_t)'-'] = e++;
	CharTrans[(u_int8_t)'='] = e++;
	CharTrans[(u_int8_t)'\''] = e++;

	e = 86;
	CharTrans[(u_int8_t)'?'] = e++;
	CharTrans[(u_int8_t)'!'] = e++;
	CharTrans[(u_int8_t)'"'] = e++;
	CharTrans[(u_int8_t)'#'] = e++;
	CharTrans[(u_int8_t)'$'] = e++;
	CharTrans[(u_int8_t)'%'] = e++;
	CharTrans[(u_int8_t)'&'] = e++;
	CharTrans[(u_int8_t)':'] = e++;
	CharTrans[(u_int8_t)'@'] = e++;
	CharTrans[(u_int8_t)'\\'] = e++;
	CharTrans[(u_int8_t)'<'] = e++;
	CharTrans[(u_int8_t)'>'] = e++;
	CharTrans[(u_int8_t)'{'] = e++;
	CharTrans[(u_int8_t)'}'] = e++;
	CharTrans[(u_int8_t)'^'] = e++;
	CharTrans[(u_int8_t)'_'] = e++;
	CharTrans[(u_int8_t)'`'] = e++;
	CharTrans[(u_int8_t)'~'] = e++;
}
