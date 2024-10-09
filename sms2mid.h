// sms2mid.h: 	simple library to convert sms to midi file (type 0, 1)
// 			  	by ma.ke. 2024-10-09
//   			- without warranty
//   			- use at your own risk
//   			- do what ever you want with this
//   			- be happy
//
//	features:	- create, save, midifile type 0 or 1
//				- provide running mode only for reading midi file
//      		- complete channel messages
//				- meta events TXT, CPR, TRK, INS, LYR and TMP
//				- sysex data (F0 ... data ... F7)
//
//  limits:		- time devision only ppqn
//				- other meta events as described not supported
//      		- sysex request not supported

#include <windows.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

/******************************************
 * midi API
 ******************************************/
 
#define BUFSIZE 	16					// smallest unit of memory buffer
#define SYSEXMAX 	128					// max. size of sysex data string

struct BUF {						
	char    	*mem;					// memory buffer
	int			len;					// length of allocated memory
	int 		cnt;					// write byte counter of mem
	struct BUF 	*next; 					// link to next track (linked list)
};

// functions for track/memory file (incl. link list)
struct 	BUF* newTRK();									// create new track
void 	freeTRKs();										// clear memory of all tracks

// functions for midi file
struct 	BUF* newSMF(int ppqn);							// create new SMF buffer
int 	writeSMF(char *filename, struct BUF *smf);		// write SMF buffer to file
void 	freeBUF(struct BUF *smf);						// clear memory

// functions for writing midi data to track
void 	writeMSG(struct BUF *trk, int timediv, BYTE status, BYTE data1, BYTE data2);
void 	writeSYX(struct BUF *trk, BYTE data[]);
void 	writeTMP(struct BUF *trk, int microsec);
void    writeMTA(struct BUF *trk, int type, BYTE data[]);

/******************************************
 * midi internals
 ******************************************/

#pragma pack(push, 1)
struct MTHD {
	DWORD 	id;				// id 
	DWORD 	hdrl;			// header length
	WORD	fmt;			// midi format
	WORD	trks;			// number of tracks
	WORD	ppqn;			// ppqn
};
#pragma pack(pop)

struct MTRK {
	DWORD 		time;		// absolute time of track 
	char		*ptr;		// event data pointer
	BYTE		lastStatus;	// rember last status byte
};

enum MKMIDI {
	EVT_MTHD		= 0x4D546864,		// midi header ID	: 4 byte ("MThd")
	EVT_MTRK		= 0x4D54726B,		// track header ID	: 4 byte ("MTrk")
	EVT_SYX			= 0xF0,				// sysex			: F0 length message
	// meta events
	EVT_TXT			= 0xFF01,			// text				: FF 01 len datastream
	EVT_CPR			= 0xFF02,			// copyright notice : FF 02 len datastream
	EVT_TRK			= 0xFF03,			// track name		: FF 03 len datastream
	EVT_INS			= 0xFF04,			// instrument name	: FF 04 len datastream
	EVT_LYR			= 0xFF05,			// lyrics			: FF 05 len datastream
	EVT_MRK			= 0xFF06,			// marker			: FF 06 len datastream
	EVT_CUE			= 0xFF07,			// cue point		: FF 07 len datastream
	EVT_PRG			= 0xFF08,			// program name 	: FF 08 len datastream
	EVT_DEV			= 0xFF09,			// device name		: FF 09 len datastream
	// ...
	EVT_EOT			= 0xFF2F00,			// end of track		: FF 2F 00
	EVT_TMP			= 0xFF5103,			// tempo			: FF 51 03  tt tt tt (24 bit value)
};

/****************************************************************************************
 *
 * c functions for handle midi data
 *
 ****************************************************************************************/
 
DWORD swap32(DWORD x) {
    return (x>>24)               | 
		   ((x<<8) & 0x00FF0000) |
		   ((x>>8) & 0x0000FF00) |
		   (x<<24);
}
WORD swap16(WORD x) {
	return (x>>8)|(x<<8);
}

/****************************************************************************************
 *
 * c functions for handling TRACK buffers to creating own midifiles 
 *
 ****************************************************************************************/
 
// initialize linklist
struct BUF *head 	= NULL;
struct BUF *current = NULL;

// get number of tracks
int getNumTRK() {
	int i = 0;
	struct BUF *trk = head;							// start from the first link
	if(!trk) return i;							    // list is empty
	while(trk) {									// navigate through list  	 		 					
		i++;										// and count
		trk = trk->next;							// go to next link
	}
	return i;
}

// create new track
struct BUF* newTRK() {
	if( getNumTRK() >= 0xFFFF ) return NULL;					    // more then 65535‬
	struct BUF *link = (struct BUF*) malloc(sizeof(struct BUF));	// create a link
	link->mem 	= malloc(sizeof(char) * BUFSIZE);
	link->len 	= BUFSIZE;
	link->cnt 	= 0;
	link->next 	= head;								// point it to old first node
	head = link;									// point first to new first node
	return link;
}

// clear memory for all tracks
void freeTRKs() {
	struct BUF *current = head;		   				// start from beginning
	if(!current) return;							// list is empty
	while(current) {								// navigate through list and
		struct BUF *trk = current;
		current = current->next;					// go to next link
		free(trk->mem);								// clear current track
		free(trk);
	}
	head = current = NULL;							// initialize linklist
	return;
}

/****************************************************************************************
 *
 * c functions to handling general buffers (writing and read)
 *
 ****************************************************************************************/
 
// write byte to buffer
//
void writeBYTE(struct BUF *buf, BYTE value)	{
	if ( buf->cnt + 1 >= buf->len ) {			// dynamic buffer size
		buf->len += BUFSIZE;
		buf->mem = realloc(buf->mem, sizeof(BYTE) * buf->len); 
	}
	buf->mem[buf->cnt++] = value;
	return;
}

// write variable byte length to buffer (1 ... 4)
void writeVAL(struct BUF *buf, DWORD value, int bytes) {
	BYTE b[4];
	memcpy(b, &value, sizeof(DWORD));
	if (bytes > 3) writeBYTE(buf, b[0]);
	if (bytes > 2) writeBYTE(buf, b[1]);
	if (bytes > 1) writeBYTE(buf, b[2]); 	
	if (bytes > 0) writeBYTE(buf, b[3]);
	return;
}

// write variable length quantity to buffer
void writeVLQ(struct BUF *buf, DWORD value)	{
   DWORD buffer = value & 0x7F;  
   while ( (value >>= 7) ) {
     buffer <<= 8;
     buffer |= ((value & 0x7F) | 0x80);
   }
   while (TRUE) {
	  unsigned char *bytes = (unsigned char *) (&buffer);
	  writeBYTE(buf, bytes[0]);
	  if (buffer & 0x80)
          buffer >>= 8;
      else
          break;
   }
   return;
}
 
/****************************************************************************************
 *
 * c functions specialy for writing MIDI data to TRACK buffer
 *
 ***************************************************************************************/
//      ppqn = pulses (ticks) per quarter note			default:	  96
//       bpm = beats per minute	(1 beat = 1/4 note)		default:	 120
//     tempo = micro sec per quarter note 				default: 500.000
// 			   											( = 60.000.000 microsec / bpm )
 
// write MIDI events to TRK (status byte 0x8n - 0xEn + databyte1 + databyte2)
void writeMSG(struct BUF *trk, int timediv, BYTE status, BYTE data1, BYTE data2) {
	writeVLQ(trk, timediv);
	writeBYTE(trk, status);
	writeBYTE(trk, data1);
	if((status & 0xf0) == 0xc0) return;			// 2. data byte not needed, program change 
	if((status & 0xf0) == 0xd0) return;			// 2. data byte not needed, channel after touch
	writeBYTE(trk, data2);
	return;
}

// get size of sysex data string
int getSyxSize(BYTE data[]) {
	for(int i = 0; i < SYSEXMAX; i++) {
		if(data[i] == 0xF7) return i+1;
	}
	return 0;
}

// write SYSEX data to TRK, last byte of byte string must be 0xF7 (EOX)
void writeSYX(struct BUF *trk, BYTE data[]) {
	int size = getSyxSize(data);
	if(size) {
		writeVLQ(trk, 0);									// meta event always with timediv=0
		writeVAL(trk, swap32(EVT_SYX), 1); 
		writeVLQ(trk, size);
		for( int i = 0; i < size; i++) writeBYTE(trk, data[i]);
	}
	return;
}

// write META data to TRK, last byte of byte string must be 0x00 (end of standard c string)
void writeMTA(struct BUF *trk, int type, BYTE data[]) {
	int size = strlen(data);
	if(size) {
		writeVLQ(trk, 0);									// meta event always with timediv=0
		writeVAL(trk, swap32(type), 2); 
		writeVLQ(trk, size);
		for( int i = 0; i < size; i++) writeBYTE(trk, data[i]);
	}
	return;
}

// write meta event TEMPO to TRK ( FF 51 03 tt tt tt )
void writeTMP(struct BUF *trk, int microsec) {
	writeVLQ(trk, 0);					// meta event always with timediv=0
	writeVAL(trk, swap32(EVT_TMP), 3);
	writeVAL(trk, swap32(microsec), 3);
	return;
}

/****************************************************************************************
 *
 * c functions for midifile and SMF buffer
 *
 ****************************************************************************************/
 
// read properties from midi header, fill header structure, check is valid midi file
struct MTHD *get_MThd(struct BUF *smf) {
	struct MTHD *mthd = malloc(sizeof(struct MTHD));
	if(!smf) return NULL;
	void *ptr = smf->mem;
	mthd->id    = swap32(*(DWORD*)ptr); ptr += sizeof(DWORD);	// id
	if(mthd->id != EVT_MTHD) 			return NULL;			// none midi file
	mthd->hdrl  = swap32(*(DWORD*)ptr);	ptr += sizeof(DWORD);	// header length
	mthd->fmt   = swap16(*(WORD*)ptr); 	ptr += sizeof(WORD);	// midi format
	mthd->trks  = swap16(*(WORD*)ptr); 	ptr += sizeof(WORD);	// number of tracks
	mthd->ppqn  = swap16(*(WORD*)ptr); 	ptr += sizeof(WORD);	// ppqn
	return mthd;
}

// create standard SMF buffer from internal tracks (only type 0 or type 1)
struct BUF* newSMF(int ppqn) {
	//initialize buffer for midi file
	struct BUF *smf = (struct BUF*) malloc(sizeof(struct BUF));
	smf->mem = malloc(sizeof(char) * BUFSIZE);
	smf->len = BUFSIZE;
	smf->cnt = 0;
	// MIDI FILE HEADER create
	int fmt = 0;
	int trkCnt = getNumTRK();	
	if (trkCnt < 1) return NULL;				// no track data
	if (trkCnt > 1) fmt = 1;					// if more than one track file type = 1
	writeVAL(smf, swap32(EVT_MTHD), 4);			// ID,			  4 byte ("MThd")
	writeVAL(smf, swap32(6), 4);				// HEADER LENGTH, 4 byte 
	writeVAL(smf, swap32(fmt), 2);				// FORMAT,		  2 byte (midi file type = 0, 1, or 2)
	writeVAL(smf, swap32(trkCnt), 2);			// TRACK COUNTER  2 byte (number of tracks = 1 - 65535)
	writeVAL(smf, swap32(ppqn), 2); 			// DIVISION,	  2 byte (ticks per quarter-note)
	// TRACK  
	struct BUF *trk = head;		   				// start from beginning
	while( trk ) {
		writeVAL(smf, swap32(EVT_MTRK), 4);		// ID,			  4 byte ("MTrk")
		writeVAL(smf, swap32(trk->cnt + 4), 4);	// TRACK LENGTH,  4 byte length (track size + 4 byte footer)
		for( int i = 0; i < trk->cnt; i++)  	// TRACKDATA,     n byte track stream
			writeBYTE(smf, trk->mem[i]);
		writeVLQ(smf, 0);						// meta event always with timediv=0	
		writeVAL(smf, swap32(EVT_EOT), 3);		// end of track,  3 byte	
		trk = trk->next;						// next track
	}	
	return smf;
}

// write midi file from SMF buffer to file system
int writeSMF(char *filename, struct BUF *smf) {
	if(!smf) return FALSE;
	struct MTHD *mthd = get_MThd(smf);	
	if(!mthd) return FALSE;								// none valid SMF buffer
	FILE *fp = fopen(filename, "wb");
	fwrite(smf->mem, smf->cnt, 1, fp);
	fclose(fp);
	return TRUE;
}

// clear memory for SMF buffer
void freeBUF(struct BUF *smf) {
	if (!smf) return;
	free(smf->mem);
	free(smf);
	return;
} 

/********************************************************************/
// sms.h
// 		HIDCAM ma.ke.
// 
// 	 	- SMS: 	a simple music scripting (sms) language 
// 	 			to generate midi file by ma.ke 2024-10-09
//   	      		- without warranty
//   	       		- use it on your own risk
//             		- do what ever you want with this
//   	       		- be happy
// 
// 				description of sms commands see manual.sms
//
#define SMSVERSION   "2024.10.09"
#define SMSVERSDATUM "09-10-2024"

/******************************************
 * sms API
 ******************************************/
 
char 	   *get_file_to_mem(char fileName[]);
struct BUF *sms2midi(char *data, char **msg);

/******************************************
 * sms internals
 ******************************************/
 
#define BUFFER			 255

#define MAX_MIDI_DEV_OUT    256	// max midi devices
#define DEFAULT_OCTAVE		  5
#define DEFAULT_DURATION	  4
#define DEFAULT_VOLUME		127
#define DEFAULT_KEY			 35                     
#define DEFAULT_BPM	 		120
#define DEFAULT_PPQN	 	 96
#define MIDI_TIME_DIV         1		// pulses between midi not off and on at same time div

// sms commands (token) 	
enum TOKEN {	
	// special notes
	BEAT				=   'x',	// drum beat 1
	PAUSE				=	'o',	// pause
	// qualifier
	HALFTONE_UP			=	'#',	// half tone up (# sign)
	HALFTON_PLUS        =   '+',    // halft tone up 
	HALFTONE_MINUS      =   '-',    // half tone down
	OCTAVE_UP			=	'>',	// octave up
	OCTAVE_DOWN			=	'<',	// octave down
	DURATION_DOT		=	'.',	// duration	current +50%
	DURATION			=	'/',	// duration
	VOLUME				=	'!',	// volume	
	HOLD				=	'_',	// bound notes, eg.: c/4_
	// sms definition commands
	INCLUDE				=	'#',	// define include file
	HEADER				=	'H',	// define SMS header
	INST				=	'I',	// define instrument track
	DRUM				=	'D',	// define drum key
	CHORD				=	'C',	// define chord type, 	uses at lastWordType too
	ARP					=	'A',	// define arpreggio
	MACRO				=   'M',    // define macro,   		uses at lastWordType too
	PARAMETER			=	'P',    // other  parameter
	NOTE				=   'N',	// note					uses at lastWordType 
	// sms system commands	
	BASENOTE			=	':',	// sign for base note
	BARLINE				=	'|',	// note bar line
	MACRO_START			=	'{',	// define makro start
	MACRO_END			=	'}',	// define makro end
	TIME_GROUP_START	=	'(',	// time group start
	TIME_GROUP_END		=	')',	// time group stop
	TIME_BLOCK_START	=	'[',	// define block bundle start
	TIME_BLOCK_END		=	']',	// define block bundle end
	// special characters to separate a word
	NEWLINE				=	'\n',	// newline
	CARRIAGE_RETURN		=   '\r',   // carriage return
	TAB					=	'\t',	// horizontal tab
	SPACE			    =	 32,	// space / blank
};

// sms status
enum STATUS {
	// flag status 
	EMPTY 				=	255,	// flag for empty or unused position/entry
	UNKNOWN				=   254,	// unknown, uses at lastWordType too
	EOD					=   253,	// end of data
	TIME_OFF			=   252,	// flag for not set time
	// process typ
	DEFINING			=   240,	// macro / block defining
	PASSING	            =   239,	// macro / block passing
	IDLE				=   238,    // macro / block idle 
	// others
	OFF					=     1,	// off
	ON					=	  2,	// on
	NOTE_MAX_OFFSET		=    24,	// for chord or tabulature base note 0 - NOTE_MAX_OFFSET	
};

enum CHORD_PROPERTIES {
	CHORD_KEYS			=     7,
	CHORD_OCTAVE		=	  3,
};

enum ERR {
	ERR_NOERROR,								//  0
	ERR_OPEN_FILE,								//  1
	ERR_NO_COMMAND,								//  2
	ERR_ARP_SYMBOL,								//	3
	ERR_MACRO_NESTED,							//	4
	ERR_OCTAVE,									//  5
	ERR_QUALIFIER_SYMBOL,						//  6	
	ERR_EMPTY1,									//  7
	ERR_TIME_BLOCK,								//  8
	ERR_DURATION_DOT,							//  9
	ERR_DURATION,								// 10
	ERR_VOLUME,									// 11
	ERR_VALUE,									// 12
	ERR_DEF_PARAMETER,							// 13
	ERR_MCC_PARAMETER,							// 14
	ERR_PARSER,									// 15
	ERR_DRUM_SYMBOL,							// 16
	ERR_BLOCK,									// 17
	ERR_TIME_GROUP,								// 18
	ERR_NOT_ALLOWED,							// 19
	ERR_NAME,									// 20
	ERR_MACRO,									// 21
	ERR_CHORD,									// 22
	ERR_CHORDSYNTAX,							// 23
	ERR_LIST_MAX,								// 24
	ERR_ARP,									// 25
	ERR_MACRO_BRACES,							// 26
	ERR_ARP_MULTI_LINES,						// 27
	ERR_BAR,									// 28
	ERR_NOTE,									// 29
	ERR_KEYCHORD,								// 30
	ERR_NAME2,									// 31
	ERR_BLOCKCOMMENT,							// 32
	ERR_BASENOTE_SYMBOL,						// 33
	ERR_NOTE_OFFSET,							// 34
	ERR_REPEATER,								// 35
	ERR_REPEATER_LASTWORD,						// 36
	ERR_BASENOTE,								// 37
	ERR_HOLD_NOT_LAST,							// 38
	ERR_HOLDOFF_MISSING,						// 39
	ERR_ELEMENTS,
};

const char * ERRMSG[] = { 
	"no error", 											//  0
	"file open error",										//  1
	"wrong command",										//  2
	"wrong arp qualifier (+ - . / !)",						//  3
	"nested macro not allowed",								//  4
	"octave value out of range (0-10)",						//  5
	"wrong note qualifier symbol (# < >. / !)",				//  6
	"empty1",												//  7
	"time block (missing close square bracket)",			//  8
	"duration dot was previously set in line",      		//  9
	"duration invalid value (1 2 4 8 16 32)",				// 10
	"volume invalid value (0-127)",							// 11
	"none ore invalid value", 								// 12
	"wrong parameter",										// 13
	"wrong mcc parameter",									// 14
	"parser error",											// 15
	"wrong drum qualifier (. / !)",							// 16
	"block error (open/close/nested)",						// 17
	"time group error (open/close/nested)",					// 18
	"symbol isn't allowed here",							// 19
	"name allways in use",									// 20
	"wrong macro syntax",									// 21
	"wrong chord",											// 22
	"wrong chord syntax",									// 23
	"parameter/data list maximum overflow",					// 24
	"arpeggio not defined",									// 25
	"macro definition (missing close curly brace)", 		// 26
	"multi liner as arpeggio not allowed",					// 27
	"to many events in previous bar",	     				// 28
	"note / offset out of range",							// 29
	"invalid key chord",									// 30
	"name needs to begin with [A-Za-z]",					// 31
	"comment error (start/end/nested)", 					// 32
	"wrong base offset qualifier (. / !)",					// 33
	"invalid note offset [0..24]",							// 34
	"invalid repeater value (>0)",							// 35
	"wrong lastword for repeater (note, chord, macro)",		// 36
	"wrong base note syntax (note[oct][#]:)",				// 37
	"hold on isn't last qualifier in note",					// 38
	"hold off missing",										// 39
};

/***************************************************************************
 * sms environment and structures
 ***************************************************************************/
 
typedef struct SMS_OBJECT { 
	char 	*name;					// name of object
	BYTE    type;					// type of object 		(xxx)
	void	*obj;					// pointer to object 	(xxx)
	struct  SMS_OBJECT *next;		// link to next object
} smsObject;

typedef struct SMS_MACRO {
	char 		*name;				// track name
	int  		startline;			// start line of defining
	int         lines;				// size of macro in lines
	int     	 cmd;               // type of user command (TKN_DEF_ARP/_VOICE/ _BLOCK)
	char		*list;				// word list of macro
	int			size;				// number of words
}smsMacro;

typedef struct SMS_NOTE {
	int 	key;					// key				[0 ... 127]
	int 	hft;					// halftone 		[-1, 0 +1]
	int		oct;					// octave    		[0 ... 10]
	int 	dur;					// duration 		[1,2,4,8,16,32]
	int     hold;					// hold key   		[EMPTY, 0 ... 127]
	int    	dot;					// duration dot 	[0, 1]
	int   	vol;					// volume 			[0 ... 100]
}smsNote;

typedef struct SMS_CHORD {
  char  *name;						// chord name (type of chord or tab chord name)
  BYTE  *keys;						// keys of chord
} smsChord;

typedef struct SMS_CHORD_NOTE {
	int 		  key;				// main key			[0 ... 127]
	int 		  hft;				// halftone 		[-1, 0 +1]
	smsChord     *chord;			// link to chord
	smsMacro     *arp;				// link to arp
}smsChordNote;

typedef struct SMS_TRACK {
	char    	 *name;				// name of track
	int 		  chn;				//      channel
	int 		  bnk;				//		bank 
	int 		  prg;				//		program
	smsNote 	 *note;				// last note: properties
	smsChordNote *cnote;			// last chord note: properties
}smsTrack;

typedef struct SMS_DRUMKEY {
	char    *name;					// name of drum
	int 	 key;					// key of drum
}smsDrumKey;

typedef struct SMS_HEADER {
	char   *name;					// name of song
	int   	bpm;					// base tempo -> beat (1/4 note) per minute
	int   	ppqn;					// pulse per quarter note (beat)
	int  	bar;					// time signature in pulse
	int     drk;					// drumkit to use
	int     sngTime;                // current song
	int 	trks;					// number of tracks
	int 	drumkeys;				// number of drumkeys
	int     macs;					// number of macros
	int 	evts;					// number of events
	int 	chords;					// number of chord definitions
	int 	arps;					// number of arp definitions
} smsHeader;

typedef struct SMS_EVENT {
	char 		*trkname;			// track name
	int			evtId;				// absolute event number
	int 		time;				// absolute time in ticks
	BYTE		status;				// three bytes for midi message
	BYTE		data1;				//
	BYTE		data2;				//
	int			bpm;				// change tempo with new bpm value
	struct SMS_EVENT *next;			// link to next event
}smsEvent;

smsObject *objFirst, *objLast;		// object link list
smsEvent  *evtFirst, *evtLast;		// event link list
int		   parserPos   = 0;			// for multiple use

/***************************************************************************
 * sms functions
 ***************************************************************************/
 
// create sms object
smsObject *newSmsObject(char *name, BYTE type, void *object) {
	smsObject *obj = (smsObject*)calloc(1, sizeof(smsObject));
		obj->name 	= (char*)malloc(strlen(name)); strcpy(obj->name, name);
		obj->type 	= type;
		obj->obj  	= object;
		obj->next	= NULL;
	if ( !objFirst ) objFirst = obj;
	if (  objLast  ) objLast->next = obj;
	objLast = obj;
	return obj;
}

// get pointer for existing command (object) name
void *getObject(char *name, int *type) {
	smsObject *obj = objFirst;
	while (obj) {
		if(strcmp(obj->name, name) == 0) {
			*type = obj->type;
			return obj->obj;
		}
		obj = obj->next;
	}
	return NULL;
}

// free objects
void freeObjects() {
	void *p;
	smsObject *obj = objFirst;
	while (obj) {
		switch (obj->type) {
			case INST:	{	smsTrack *p = obj->obj;
							free(p->name);
							free(p);
							break;
						}
			case DRUM: 	{	smsDrumKey *p = obj->obj;
							free(p->name);
							free(p);
							break;
						}
			case CHORD: {	smsChord *p = obj->obj;
							free(p->name);
							free(p);
							break;
						}
			case ARP: 
			case MACRO: {	smsMacro *p = obj->obj;
							free(p->name);
							free(p->list);
							free(p);
							break;
						}
			default:	break;
		}
		smsObject *obj_old = obj;
		obj = obj_old->next;
		free(obj_old->name);
		free(obj_old);
	}
	objFirst = NULL; objLast = NULL;			// reset object link list
	return;
}

// create sms note event with default values
smsNote *newSmsNote() {
	smsNote *n = (smsNote*)calloc(1, sizeof(smsNote));
		n->key 		= 0;
		n->hft 		= 0;
		n->oct 		= DEFAULT_OCTAVE;
		n->dur 		= DEFAULT_DURATION;
		n->dot 		= 0;
		n->hold 	= EMPTY;
		n->vol 		= DEFAULT_VOLUME;
	return n;
}

// create sms chord note event with default values
smsChordNote *newSmsCNote() {
	smsChordNote *cn = (smsChordNote*)calloc(1, sizeof(smsChordNote));
		cn->key = 0;
		cn->hft = 0;
		cn->chord = NULL;
		cn->arp   = NULL;
	return cn;
}

// create sms chord with default values
smsChord *newSmsChord(char* name) {
	int type;
	if ( getObject(name, &type) != NULL ) return NULL;						// check if name exist
	smsChord *c = (smsChord*)calloc(1, sizeof(smsChord));
		c->name   = (char*)malloc(strlen(name)); strcpy(c->name, name);
		c->keys   = (BYTE*)malloc(CHORD_KEYS);
		for(int i = 0; i < CHORD_KEYS; i++) c->keys[i] = EMPTY;
	newSmsObject(name, CHORD, c);
	return c;
}

// create sms macro 
smsMacro *newSmsMacro(char *name, int mode) {
	int type;
	if ( getObject(name, &type) != NULL ) return NULL;						// check if name exist
	smsMacro *mac = (smsMacro*)calloc(1, sizeof(smsMacro));
		mac->name 		= (char*)malloc(strlen(name)); strcpy(mac->name, name);
		mac->startline 	= 0;
		mac->lines 		= 0;
		mac->cmd  		= mode;
		mac->list 		= (char*)calloc(1, sizeof(char));
		mac->size 		= 0;
	newSmsObject(name, mode, mac);
	return mac;
}

// create sms event
smsEvent *newSmsEvent(smsTrack *trk, int evtId, int time, BYTE status, BYTE data1, BYTE data2) {
	smsEvent *evt = (smsEvent*)calloc(1, sizeof(smsEvent));
		evt->trkname = trk->name;
		evt->evtId	 = evtId;
		evt->time 	 = time;
		evt->status  = status;
		evt->data1   = data1;
		evt->data2   = data2;
		evt->bpm     = 0;
		evt->next	 = NULL;
	if ( !evtFirst ) evtFirst      = evt;
	if ( evtLast )   evtLast->next = evt;
	evtLast = evt;
	return evt;
}

void freeEvents() {
	smsEvent *evt = evtFirst, *evt_old;
	while (evt) { 
		evt_old = evt;
		evt     = evt_old->next;
		free(evt_old->trkname);
		free(evt_old);
	}
	evtFirst = NULL; evtLast = NULL;			// reset event link list
}

// create new sms instrument track with default values
smsTrack *newSmsTrk(char *name) {
	int type;
	if ( getObject(name, &type) != NULL ) 					return NULL;	// check if name exist
	smsTrack *trk = (smsTrack*)calloc(1, sizeof(smsTrack));
		trk->name  = (char*)malloc(strlen(name)); strcpy(trk->name, name);
		trk->chn   = 0;	 
		trk->bnk   = 0; 
		trk->prg   = 0;
		trk->note  = newSmsNote();
		trk->cnote = newSmsCNote();
	newSmsObject(name, INST, trk);
	return trk;
}

// create new sms drum key with default values
smsDrumKey *newSmsDrumKey(char *name) {
	int type;
	if ( getObject(name, &type) != NULL ) 					return NULL;	// check if name exist
	smsDrumKey *dkey = (smsDrumKey*)calloc(1, sizeof(smsDrumKey));
		dkey->name 	 = (char*)malloc(strlen(name)); strcpy(dkey->name, name);
		dkey->key    = 31;				// tick		
	newSmsObject(name, DRUM, dkey);
	return dkey;
}

// create sms header with default values
smsHeader *initSMS(char *name) {
	smsHeader *sms  = (smsHeader*) malloc(sizeof(smsHeader));
		sms->name 		= (char*)malloc(strlen(name)); strcpy(sms->name, name);
		sms->bpm		=	DEFAULT_BPM;
		sms->ppqn		=	DEFAULT_PPQN;
		sms->bar		=	sms->ppqn * 4;		// 4/4 -> 4 * 96
		sms->drk		=     0;
		sms->trks		=	  0;
		sms->drumkeys 	=     0;
		sms->macs		=	  0;
		sms->evts		=	  0;
		sms->chords		=	  0;
		sms->arps		=	  0;
	// reset internal variables	
	parserPos = 0;								// for multiple use
	freeObjects();
	freeEvents();
	return sms;
}

void freeSMS(smsHeader *sms) {
	freeObjects();
	freeEvents();
	free(sms->name);
	free(sms);
}

/***************************************************************************
 * read file to memory
 ***************************************************************************/
 
char *get_file_to_mem(char fileName[]) {
	FILE  *fp = fopen(fileName, "r");
	if(!fp) return NULL;
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	char *buf = (char*)calloc(size+1, sizeof(char));
	fseek(fp, 0, SEEK_SET);
	fread(buf, size, 1, fp);
	fclose(fp);
	return buf;
}

int clear_mem(char *buf) { free(buf); }

/***************************************************************************
 * token parser functions
 ***************************************************************************/

// read the next word, return token for single_char word
int parser_next(char **word, char *data) {	
	int pos = parserPos;								// current pointer position
	int c;												// current single_char
	int strSize = strlen(data);							// data size
	if (pos >= strSize) return EOD;						// end of data

	char *buf = (char*)calloc(255, sizeof(char));		// initialize pointer for buffer with \0
	int cnt = 0;										// counter of read characters
	int eow = FALSE;									// flag for end of word
	while ( pos < strSize ) {							// read loop
		if(cnt > 253) 	break;							// word ist to long
		c 	= data[pos++];								// get next character
		switch ( c ) {
			case CARRIAGE_RETURN:
			case NEWLINE:	if(cnt)  pos--; 			// new line at end of word
							if(!cnt) buf[cnt++] = c;	// new line at begin of word
							eow = TRUE;
							break;
			case TAB:
			case SPACE:		if(cnt) eow = TRUE; break;			
			default:		buf[cnt++]  = c; 	break;
		}
		if(eow) break;									// end of word
	}
	*word = buf;
	parserPos = pos; 
	return (cnt == 1) ? buf[0] : UNKNOWN;				// return token
}
		
// check if char alphanumeric
int parser_isChar(BYTE c) {
	// A = 65 ... Z = 90, // a = 97 ... z = 122
	if(c > 64 && c <  91) return TRUE;
	if(c > 96 && c < 123) return TRUE;
	return FALSE;
}

// get number from start of string, return count of size in char
int parser_getNumber(char *p, int *value) {
	if (strcmp(p, "EMPTY")  == 0) {
		*value = EMPTY;
		return strlen(p);
	}
	int v = p[0]; int count = 0;
	*value = 0;
	while( v-48 >=0 &&  v-48 <= 9) {
		*value = *value * 10 + (v-48);
		p++; count++;
		v = p[0];
	}
	return count;	
}

// check repeater command 
int parser_isRepeater(char *word, int *value) {
	int v=-1;
	int res = sscanf(word, "*%d", &v);
	if ( res !=1 ) 						return ERR_DEF_PARAMETER;
	*value = v;
	return ERR_NOERROR;
}

// check dynamic parameter bpm and get value 
int parser_isBPM(char *word, int *value) {
	char *par = (char*)calloc(16, sizeof(char));	// initialize pointer for buffer with \0
	int v   = -1, err;
	int res = sscanf(word, "%15[^=]=%d", par, &v);
	if(res  == 0) 									return ERR_DEF_PARAMETER;
	if (strcmp(par, "bpm")  == 0) {
		if(res  == 1) 								return ERR_VALUE;
		if ( v < 30 || v > 240 )					return ERR_VALUE;
	} else 											return ERR_DEF_PARAMETER;
	*value = v;
	return ERR_NOERROR;
}

// check dynamic parameter bar and get value 
int parser_isBAR(char *word, int*value) {
	char *par = (char*)calloc(16, sizeof(char));	// initialize pointer for buffer with \0
	int v   =-1, v2 = -1, err;
	int res = sscanf(word, "%15[^=]=%d/%d", par, &v, &v2); 
	if ( res == 0 )									return ERR_DEF_PARAMETER;
	if (strcmp(par, "bar")  == 0) {
		if ( res <  3 ) 							return ERR_VALUE;
		if ( v < 1 || v > 8)						return ERR_VALUE;
		if ( v2 != 2 && v2 !=  4&&
			 v2 != 8 && v2 != 16 ) 					return ERR_VALUE;
	} else 											return ERR_DEF_PARAMETER;
	*value = 4 * v / v2;
	return ERR_NOERROR;
}

// check parameter name and value and set in in given structure,
// returns TRUE or error code
// parameter: 	word	word with parameter name and value (syntax: &name=value, example: &prg=0)
//				cmdType	HEADER, INSTRUMENT or DRUM
//				s		pointer of given structure to merge parameter value
//
int parser_isParameter(char *word, int cmdType, void *s) {
	char *par = (char*)calloc(16, sizeof(char));	// initialize pointer for buffer with \0
	int v=-1, v2=-1, value, err;
	int res = sscanf(word, "%15[^=]=%d/%d", par, &v, &v2); 
	
	// proof is valid parameter for header	
	if ( cmdType == HEADER) {
		smsHeader *sms = s;
		// check ppqn 
		if (strcmp(par, "ppqn") == 0) {
			if ( res !=2 ) 							return ERR_DEF_PARAMETER;			
			if( v !=  24 && v !=  48 && 
				v !=  96 && v != 192 &&
				v != 384 && v != 768 ) 				return ERR_VALUE;
			sms->bar  = sms->bar / sms->ppqn * v;
			sms->ppqn = v;
			return ERR_NOERROR;
		}
		// check bpm
		err = parser_isBPM(word, &value);
		if(!err) {
			sms->bpm = value;
			return ERR_NOERROR;
		} else if(err == ERR_VALUE)					return err;
		// check bar
		err = parser_isBAR(word, &value);
		if(!err) {
			sms->bar = sms->ppqn * value;
			return ERR_NOERROR;
		} else if(err == ERR_VALUE)					return err;		
		// check drumkit
		if (strcmp(par, "drk")  == 0) {
			if( v < 0 || v > 127) 					return ERR_VALUE;
			sms->drk = v;
			return ERR_NOERROR;
		}
	}

	// proof is valid parameter for instrument	
	if ( cmdType == INST ) {
		smsTrack *trk = s;	
		if ( res !=2 ) 							return ERR_DEF_PARAMETER;		
		if (strcmp(par, "bnk")  == 0) {
			if( v < 0 || v > 127) 				return ERR_VALUE;
			trk->bnk = (trk->chn == 9) ? 0 : v;
			return ERR_NOERROR;
		}
		if (strcmp(par, "prg")  == 0) {
			if( v < 0 || v > 127)				return ERR_VALUE;
			trk->prg = v;		
			return ERR_NOERROR;
		}
		if (strcmp(par, "chn")  == 0) {
			if( v < 0 || v > 15 || v == 9) 		return ERR_VALUE;
			trk->chn = v;
			if(v == 9) trk->bnk = 0;
			return ERR_NOERROR;
		}	
	}
	
	// proof is valid parameter for drum key
	if ( cmdType == DRUM ) {
		smsDrumKey *drum = s;
		if ( res !=2 ) 							return ERR_DEF_PARAMETER;
		if (strcmp(par, "key")  == 0) {
			if(v < 0 || v > 127) 				return ERR_VALUE;
			drum->key = v;
			return ERR_NOERROR;
		}
	}

	return ERR_DEF_PARAMETER;	
}

// get midi_cc and value
// 		syntax:    @name=value, example: @vol=100
//              or @cc  =value,            @7=100
int parser_isMidiCC(char *word, int *cc, int *val) {	
	// midi_cc name and value
	char *p = (char*)calloc(16, sizeof(char));					// initialize pointer for buffer with \0
	int   v = -1;
	if (sscanf(word, "@%15[^=]=%d", p, &v) < 2) 				return ERR_NO_COMMAND;
	// check value
	if (v < 0 || v > 127 ) 										return ERR_VALUE;
	*val = v;
	// check is midi_cc name a valid midi_cc number 
	int ctrl = UNKNOWN;
	if (parser_getNumber(p, &ctrl) == 3) {
			if ( ctrl > 127 )									return ERR_MCC_PARAMETER;	
			*cc = ctrl;
			return ERR_NOERROR;
	}
	// check is midi_cc name a defined standard midi_cc
	if (strcmp(p, "vol")  == 0) 	{ *cc =    7; return ERR_NOERROR; }
	if (strcmp(p, "bal")  == 0) 	{ *cc =    8; return ERR_NOERROR; }
	if (strcmp(p, "pan")  == 0) 	{ *cc =   10; return ERR_NOERROR; }
	if (strcmp(p, "dly")  == 0) 	{ *cc =   91; return ERR_NOERROR; }
	return ERR_NOERROR;
}

// check is valid note
int parser_isNote(char *data, smsNote *n, int type ) {
	int value = 0, size = 0, flgHold = FALSE;
	// check is valid note symbol for inst 
	if (type == INST) {
		switch (data[0]) {
			case '-':
			case 'o':
			case 'p':	n->key = PAUSE; break;
			case 'c':	n->key =  0; 	break;
			case 'd':	n->key =  2;	break;
			case 'e':	n->key =  4; 	break;
			case 'f':	n->key =  5; 	break;
			case 'g':	n->key =  7; 	break;
			case 'a':	n->key =  9; 	break;
			case 'b':	n->key = 11; 	break;
			default: 	return ERR_NO_COMMAND;
		}
		data++;
	}
	// proof max of chord note value(24) / tab offset value (24)
	if( type == BASENOTE || type == ARP) {
		size = parser_getNumber(data, &value);
		if(size) {
			if ( value > NOTE_MAX_OFFSET ) 								return ERR_NOTE_OFFSET; 
			n->key = value;
			data  += size;
		} else {
			if(data[0] != 'p' && data[0] != 'o' && data[0] != '-')		return ERR_NO_COMMAND;
			n->key = PAUSE;
			data++;
		}
	}
	// check is valid drum symbol
	if ( type == DRUM ) {
		if(data[0] == 'p' || data[0] == '-') data[0] = PAUSE;
		if(data[0] != BEAT && data[0] != PAUSE ) return ERR_NO_COMMAND; 
		n->key = data[0];
		data++;
	}

	n->hft 	=   0;
	n->dot	=	0;
	
	//check absolute octave a3>+/16.!46
	if(type != ARP && type != BASENOTE) {
		size = parser_getNumber(data, &value);
		if(size) {
			if (value > 10 ) 										return ERR_OCTAVE;
			n->oct = value;
			data += size;
		}
	}

	// check qualifier
	while(strlen(data)) {
		switch (data[0]) {
			case HALFTONE_UP:
			case HALFTON_PLUS:
				if(type == ARP)										return ERR_ARP_SYMBOL;
				if(type == DRUM)									return ERR_DRUM_SYMBOL;	
				if(type == BASENOTE)								return ERR_BASENOTE_SYMBOL;
				n->hft = n->hft + 1;
				++data;
				break;	
			case HALFTONE_MINUS:
				if(type == ARP)										return ERR_ARP_SYMBOL;	
				if(type == DRUM)									return ERR_DRUM_SYMBOL;
				if(type == BASENOTE)								return ERR_BASENOTE_SYMBOL;
				n->hft = n->hft - 1;
				++data;
				break;				
			case OCTAVE_UP:
				if(type == DRUM)									return ERR_DRUM_SYMBOL;
				if(type == BASENOTE)								return ERR_BASENOTE_SYMBOL;
				n->oct = n->oct + 1;
				++data; 
				if(type == ARP) break;
				if(n->oct > 10) 									return ERR_OCTAVE;
				break;
			case OCTAVE_DOWN:
				if(type == DRUM)									return ERR_DRUM_SYMBOL;
				if(type == BASENOTE)								return ERR_BASENOTE_SYMBOL;
				n->oct = n->oct - 1;
				++data;
				if(type == ARP) break;
				if(n->oct < 1) 										return ERR_OCTAVE; 
				break;
			case DURATION_DOT:
				if(n->dot)											return ERR_DURATION_DOT;
				n->dot = 1;
				++data;
				break;
			case DURATION:
				size = parser_getNumber(++data, &value);
				if(!size) 											return ERR_DURATION;
				if( value !=  1 && value !=  2 &&  
					value !=  4 && value !=  8 && 
					value != 16 && value != 32 &&
					value != 64 ) 									return ERR_DURATION;
				n->dur = value;
				n->dot = 0;
				data += size;
				break;
			case VOLUME:
				size = parser_getNumber(++data, &value);
				if(!size) 											return ERR_VOLUME;
				if (value > 127 ) 									return ERR_VOLUME;
				n->vol = value;
				data += size;
				break;
			case HOLD:
				if(data[1]  != '\0')								return ERR_HOLD_NOT_LAST;
				n->hold = n->key;
				++data;
				flgHold = TRUE;
				break;
			default: 
				return ERR_QUALIFIER_SYMBOL;
		}	
	}
	
	if(!flgHold) n->hold = EMPTY;
	return 	ERR_NOERROR;
}

// check is valid base note, e.g. a5#:
int parser_isBaseNote(char *data) {
	int bs = EMPTY, pos = 0, oct = 0; 
	int strSize  = strlen(data);
	if(strSize < 2)	return EMPTY;

	// check is valid note symbol 
	switch (data[0]) {
		case 'c':	bs =    0; 		break;
		case 'd':	bs =    2;		break;
		case 'e':	bs =    4; 		break;
		case 'f':	bs =    5; 		break;
		case 'g':	bs =    7; 		break;
		case 'a':	bs =    9; 		break;
		case 'b':	bs =   11; 		break;
		default: 	return EMPTY; 	break;
	}
	data++; pos++;
	
	//check absolute octave
	int size = parser_getNumber(data, &oct);
	if(oct > 10) 									return EMPTY;
	data += size; pos += size;
	if(pos >= strSize) 								return EMPTY;
	
	//check halftone sign '#'
	if(data[0] == HALFTONE_UP) { bs++; data++, pos++; }
	if(pos >= strSize) 								return EMPTY;
	
	//check final sign ':'
	if(data[0] != BASENOTE) 						return EMPTY;
	if(++pos != strSize)							return ERR_BASENOTE;

	return bs + (oct * 12);
}

// check word is valid chord type
int parser_isChord(char *word, smsChordNote *cNote) {
	int type = UNKNOWN;
	char *c  = (char*)calloc(64, sizeof(char));			// subsegment chord in word
	char *a  = (char*)calloc(64, sizeof(char));			// subsegment arp   in word
	int res  = sscanf(word, "%15[^~]~%s", c, a);
	smsChord *chord;

	switch (c[0]) {										// check main key
		case 'C':	cNote->key =  0; break;
		case 'D':	cNote->key =  2; break;
		case 'E':	cNote->key =  4; break;
		case 'F':	cNote->key =  5; break;
		case 'G':	cNote->key =  7; break;
		case 'A':	cNote->key =  9; break;
		case 'B':	cNote->key = 11; break;
		default :   return ERR_NO_COMMAND;
	}
	c++;
	if (c[0]==HALFTONE_UP || c[0]==HALFTON_PLUS) {cNote->hft = 1; c++;};	// check halftone
	if ( !strlen(c) ) 								return ERR_KEYCHORD;	// no given key chord 	
	chord  = getObject(c, &type);											// search chord
	if ( !chord || type != CHORD) 					return ERR_KEYCHORD;	// wrong key chord

	cNote->chord = chord;
	cNote->arp = NULL;
	if ( strlen(a) ) {															// check arpreggio
		void *p = getObject(a, &type);
		if ( !p || type != ARP )						return ERR_ARP;			// word as arp macro not found
		cNote->arp = (smsMacro*)p;
	}
	
	return ERR_NOERROR;
}

/***************************************************************************
 * sms2midi compiler
 ***************************************************************************/

// create event list, sorted by track, then time, then evtId
int evt_compare (const void * left, const void * right) {
	
	smsEvent * evtLeft  = (smsEvent *) left;
	smsEvent * evtRight = (smsEvent *) right;

	int res = strcmp( evtLeft->trkname, evtRight->trkname );
	if (res) return res;
	
	if( evtLeft->time < evtRight->time ) 	return -1;
	if( evtLeft->time > evtRight->time ) 	return  1;
	
	if ( evtLeft->evtId < evtRight->evtId )	return -1;
	if ( evtLeft->evtId > evtRight->evtId )	return  1;

	return 0;
}

struct BUF *parser_createMidi(smsHeader *sms) {
	smsEvent *evt = evtFirst, *evt_old;
	smsEvent evtList[sms->evts];

    // prepare sort list and sorting
	for ( int i = 0; i < sms->evts; i++) {
		evtList[i].trkname = evt->trkname;
		evtList[i].evtId   = evt->evtId;
		evtList[i].time	   = evt->time;
		evtList[i].status  = evt->status;
		evtList[i].data1   = evt->data1;
		evtList[i].data2   = evt->data2;
		evtList[i].bpm	   = evt->bpm;
		evt 	= evt->next;
	}
	qsort(evtList, sms->evts, sizeof(smsEvent), evt_compare);

	// generate midi tracks
	int cntTrk = 0;
	struct BUF *mtrk;												// pointer for midi tracks
	smsTrack *strk;													// pointer for sms track			
	smsEvent last  = { .trkname = "SMS", .time = 0};
	int songTime, type, device;
	float ms = 60000000.0 / sms->bpm;								// calculate base tempo in microsec
	for ( int i = 0; i < sms->evts; i++) {
		if ( strcmp( last.trkname, evtList[i].trkname ) != 0) {		// new midi track
			strk = getObject(evtList[i].trkname, &type);
			mtrk = newTRK();
			if (i == 0) {			
				//write global midi file informations only in first track
				writeTMP(mtrk, (int)ms);							// set tempo in first track
				writeMTA(mtrk, EVT_CPR, "(c) ma.ke. 2024"); 		// set copyright note
				writeMTA(mtrk, EVT_PRG, "created with HIDCAM-SMS"); // set program name
			}
			writeMTA(mtrk, EVT_DEV, evtList[i].trkname);
			
			// set drum kit or instrument
			writeMSG(mtrk, 0, 0xB0 + strk->chn,         0, strk->bnk);
			writeMSG(mtrk, 0, 0xC0 + strk->chn, strk->prg, 0);
			songTime = 0;
			cntTrk++;
		}
		int timediv = evtList[i].time - songTime;
		songTime    = evtList[i].time;
		// change tempo
		if (evtList[i].bpm > 0) {									
			ms = 60000000.0 / evtList[i].bpm;
			writeTMP(mtrk, (int)ms);					
		} else {
			// write midi message
			writeMSG(mtrk, timediv, evtList[i].status, evtList[i].data1, evtList[i].data2);
		}
		last = evtList[i];
	}
	struct BUF *smf = newSMF(sms->ppqn);
	freeTRKs();
	free(evtList);
	return smf;
}

struct BUF *sms2midi(char *data, char **msg) {  
// initialize global variables
	int cntLINE      = 1, cntLINE_WORD     = 0, cntWORD = 0; 
	int cntMACLINE   = 1, cntMACLINE_WORD  = 0;
	int cntINC		 = 0, cntINC_WORD	   = 0;
	int cntARPLINE	 = 1, cntARPLINE_WORD  = 0; char *ARPWORD  = NULL;
	int cntHOLDLINE  = 1, cntHOLDLINE_WORD = 0;
	char *SMSWORD    = NULL;
	
	// used for repeating
	char  LASTWORD[BUFFER];						// merge last word
	int   lastWordType = UNKNOWN;				// flag: UNKNOWN, MACRO_END
	int   macroRepeater = 0;					// number of repetitions

	int err = ERR_NOERROR;
	int token, res, type, value = 0;	
	
	int   P_REPEAT			= FALSE;
	int   P_COMMENT  		= FALSE;
	int   P_BLOCKCOMMENT    = FALSE;
	int   P_CMDTYPE	 		= UNKNOWN;
	int   P_NEXTWORD 		= FALSE;
	int   P_MACRO 	 		= IDLE;
	char *P_MACRO_COMMANDS	= NULL;
	int   P_INC				= FALSE;
	char *P_INC_COMMANDS	= NULL;
	int	  P_TIMEBLOCK 		= IDLE;
	int   P_TIMEGROUP		= IDLE;
	int   P_EVENTTYPE		= UNKNOWN;
	    
	int sngTime     = 0;		// last position song time in ticks
	int barTime     = 0;		// last position bar  time in ticks
	
	int blkTimeStart = TIME_OFF, blkTimeEnd = TIME_OFF;
	int grpTimeStart = TIME_OFF, grpTimeEnd = TIME_OFF, grpTimeBar = TIME_OFF;
	
	// default header setup
	smsHeader *sms  = initSMS("SMS");	
	
	// initialize standard key chord types major and minor 
	smsChord *c;
	c = newSmsChord("maj");   (*c).keys = (char[]) { 0, 4,   7,   EMPTY, EMPTY, EMPTY, EMPTY };
	c = newSmsChord("7");     (*c).keys = (char[]) { 0, 4,   7,    10,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("maj7");  (*c).keys = (char[]) { 0, 4,   7,    11,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("6");     (*c).keys = (char[]) { 0, 4,   7,     9,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("6/9");   (*c).keys = (char[]) { 0, 4,   7,     9,    14,   EMPTY, EMPTY };	
	c = newSmsChord("5");     (*c).keys = (char[]) { 0, 7, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY };	
	c = newSmsChord("9");     (*c).keys = (char[]) { 0, 4,   7,    10,    14,   EMPTY, EMPTY };	
	c = newSmsChord("maj9");  (*c).keys = (char[]) { 0, 4,   7,    10,    13,   EMPTY, EMPTY };	
	c = newSmsChord("11");    (*c).keys = (char[]) { 0, 4,   7,    10,    14,    16,   EMPTY };
	c = newSmsChord("13");    (*c).keys = (char[]) { 0, 4,   7,    10,    14,    17,    21   };	
	c = newSmsChord("maj13"); (*c).keys = (char[]) { 0, 4,   7,    11,    14,    21,   EMPTY };
	c = newSmsChord("add");   (*c).keys = (char[]) { 0, 4,   7,    14,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("7-5");   (*c).keys = (char[]) { 0, 4,   6,    10,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("7+5");   (*c).keys = (char[]) { 0, 4,   8,    10,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("sus");   (*c).keys = (char[]) { 0, 5,   7,   EMPTY, EMPTY, EMPTY, EMPTY };
	c = newSmsChord("dim");   (*c).keys = (char[]) { 0, 3,   6,   EMPTY, EMPTY, EMPTY, EMPTY };
	c = newSmsChord("dim7");  (*c).keys = (char[]) { 0, 3,   6,     9,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("aug");   (*c).keys = (char[]) { 0, 3,   8,   EMPTY, EMPTY, EMPTY, EMPTY };
	c = newSmsChord("aug7");  (*c).keys = (char[]) { 0, 3,  10,   EMPTY, EMPTY, EMPTY, EMPTY };
	// minor chords
	c = newSmsChord("m");     (*c).keys = (char[]) { 0, 3,   7,   EMPTY, EMPTY, EMPTY, EMPTY };
	c = newSmsChord("m7");    (*c).keys = (char[]) { 0, 3,   7,    10,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("mM7");   (*c).keys = (char[]) { 0, 3,   7,    11,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("m6");    (*c).keys = (char[]) { 0, 3,   7,     9,   EMPTY, EMPTY, EMPTY };
	c = newSmsChord("m9");    (*c).keys = (char[]) { 0, 3,   7,    10,    14,   EMPTY, EMPTY };
	c = newSmsChord("m11");   (*c).keys = (char[]) { 0, 3,   7,    10,    14,    16,   EMPTY };
	c = newSmsChord("m13");   (*c).keys = (char[]) { 0, 3,   7,    10,    14,    17,    21   };
	c = newSmsChord("m7b5");  (*c).keys = (char[]) { 0, 3,   6,    10,   EMPTY, EMPTY, EMPTY };
	sms->chords = 27;

	// set default instrument, drum track and drumkey
    smsTrack  	*defaultInstTrk 	= newSmsTrk("INST"); 		// create default instrument track
	// set drum track and default drumkey
	smsTrack  	*DrumTrk 			= newSmsTrk("DRUM"); 		// create drum track and
				 DrumTrk->chn		= 9;						// set midi drum channel to 9
	smsDrumKey	*defaultDKey		= newSmsDrumKey("TICK:");	// create standard drum key
				
	sms->trks 		+=2;
	sms->drumkeys 	+=1;

	// variables for current events to process
	smsTrack     *currentTrk 		= defaultInstTrk;	// current process instrument track
	smsDrumKey   *currentDKey  		= defaultDKey;		// current process drum key
	smsChord     *currentChord 		= NULL;				// current process chord definition
	smsMacro	 *currentMac   		= NULL;				// current process macro
	smsMacro	 *currentArp   		= NULL;				// current process arp
	int           currentBaseNote 	= EMPTY;			// current base note value
	int           boundStart   		= 0;				// current bound start (word number)
	
// ----------------------------------------------------------------------------------------	
// BEGIN word read main loop until end of data
// ----------------------------------------------------------------------------------------

	while ( 1 ) {
		// check is repeater and use last word or macro otherwise read next word
		if(P_REPEAT) {
			switch (lastWordType) {
				case MACRO:		macroRepeater = P_REPEAT;
								SMSWORD       = LASTWORD;
								P_REPEAT      = FALSE;
								break;
				case NOTE:
				case CHORD:		P_REPEAT--;
								SMSWORD = LASTWORD;
								break;
				default:		err = ERR_REPEATER_LASTWORD; 
								break;
			}
			if(err) break;
			token = UNKNOWN;
			goto NEXT_WORD_READY;
		}
	
NEXT_WORD_READ:
		if(SMSWORD) strcpy(LASTWORD, SMSWORD); 		// prepare for possible repetition
		if ( P_MACRO == PASSING )  {
			SMSWORD = ( !SMSWORD ) ? strtok(P_MACRO_COMMANDS, " ") : strtok(NULL, " ");
			if ( SMSWORD ) {
				token = ( strlen(SMSWORD) == 1 ) ? SMSWORD[0] : UNKNOWN;
				cntMACLINE_WORD++;
			} else {
				P_MACRO 	= IDLE;
				strcpy(LASTWORD, currentMac->name);
				lastWordType = MACRO;				
				if(macroRepeater) { 
					P_REPEAT = --macroRepeater;
					continue;	
				}
				token = parser_next(&SMSWORD, data); cntLINE_WORD++;
			}
		} else {
			token = parser_next(&SMSWORD, data);     
			cntLINE_WORD++;
		}
	
		if ( token == EOD) break;
	
NEXT_WORD_READY:
		cntWORD++;	
		P_NEXTWORD = FALSE;
		
// REPEATER: check '*[1..n]' and define last word
		if (P_MACRO == IDLE || P_MACRO == PASSING) {
			if (!P_COMMENT && !P_BLOCKCOMMENT) {
				if(parser_isRepeater(SMSWORD, &value) == ERR_NOERROR) {
					if(value < 1) { err = ERR_REPEATER; break; }		
					P_REPEAT = value - 1;
					continue;
		} 	}	}
		lastWordType = UNKNOWN;
//
// general handling newline, comment, command
//
		switch ( token ) {
			case CARRIAGE_RETURN:
			case NEWLINE:
				if ( P_MACRO == PASSING ) {
					cntMACLINE++; cntMACLINE_WORD = 0;
				} else {
					cntLINE++; 
					cntLINE_WORD  = 0;
				}

				if(barTime) {
					if(barTime > sms->bar) barTime = barTime % sms->bar;
					sngTime += sms->bar - barTime;
					barTime  = 0;
				}
				
				P_COMMENT = FALSE;
				if ( P_MACRO == DEFINING ) break;
				P_NEXTWORD	= TRUE;
				// check block and group time
				if ( P_TIMEBLOCK == PASSING ) {
					if ( P_TIMEBLOCK == PASSING && blkTimeEnd < sngTime ) blkTimeEnd = sngTime;
					if ( P_TIMEGROUP == PASSING && grpTimeEnd < sngTime ) grpTimeEnd = sngTime;
					sngTime = blkTimeStart;
				}
				if ( P_TIMEGROUP == PASSING ) 			{ err =  ERR_TIME_GROUP; break; }
				
				// default settings for new lines
				if (P_MACRO != PASSING && P_TIMEBLOCK != PASSING) currentTrk = defaultInstTrk;
				currentTrk->note->hft 	= 0;
				currentTrk->note->oct 	= DEFAULT_OCTAVE;
				currentTrk->note->dur 	= DEFAULT_DURATION;
				currentTrk->note->dot 	= 0;
				currentTrk->note->vol 	= DEFAULT_VOLUME;
				//
				currentBaseNote       = EMPTY;
				P_CMDTYPE			  = UNKNOWN;
				break;
			default:
				// check comments
				if (strcmp(SMSWORD, "//") == 0)       P_COMMENT  = TRUE;
				if (strcmp(SMSWORD, "/*") == 0) {
					if (P_BLOCKCOMMENT)			   	{ err = ERR_BLOCKCOMMENT; break; }
					P_BLOCKCOMMENT = TRUE;
				}
				if (strcmp(SMSWORD, "*/") == 0) {
					if (!P_BLOCKCOMMENT)		    { err = ERR_BLOCKCOMMENT; break; }
					P_BLOCKCOMMENT = FALSE; 
					P_COMMENT = TRUE;
				}
				if ( P_COMMENT || P_BLOCKCOMMENT)   { P_NEXTWORD = TRUE; break; }
				
				// check HIDCAM commands
				if ( cntLINE_WORD == 1 ) {
					if (strcmp(SMSWORD, "H:") == 0) { P_CMDTYPE  = HEADER; P_NEXTWORD = TRUE; break; }
					if (strcmp(SMSWORD, "I:") == 0) { P_CMDTYPE  = INST;   P_NEXTWORD = TRUE; break; }
					if (strcmp(SMSWORD, "D:") == 0) { P_CMDTYPE  = DRUM;   P_NEXTWORD = TRUE; break; }
					if (strcmp(SMSWORD, "C:") == 0) { P_CMDTYPE  = CHORD;  P_NEXTWORD = TRUE; break; }
					if (strcmp(SMSWORD, "A:") == 0) { P_CMDTYPE  = ARP;    P_NEXTWORD = TRUE; break; }
					if (strcmp(SMSWORD, "M:") == 0) { P_CMDTYPE  = MACRO;  P_NEXTWORD = TRUE; break; }
				}
				break;			
		}

		if(err) break;

		if ( P_NEXTWORD ) continue;
//		
// handling system command
//	
		P_NEXTWORD = TRUE;
		switch ( P_CMDTYPE ) {
			case HEADER:
				if ( cntLINE_WORD == 2) {
					if(!parser_isChar(SMSWORD[0]))							{ err = ERR_NAME2; break; }
					sms->name = (char*)realloc(sms->name, strlen(SMSWORD));
					strcpy(sms->name, SMSWORD);
					break;
				}
				err = parser_isParameter(SMSWORD, P_CMDTYPE, sms); 
				break;
			case INST:
				if ( cntLINE_WORD == 2) { 
					if(!parser_isChar(SMSWORD[0]))						{ err = ERR_NAME2; break; }
					currentTrk = newSmsTrk(SMSWORD);
					if (!currentTrk)  									{ err = ERR_NAME; break; }
					sms->trks++;
					break; 
				}
				err = parser_isParameter(SMSWORD, P_CMDTYPE, currentTrk);
				break;
			case DRUM:
				if ( cntLINE_WORD == 2) { 
					if(!parser_isChar(SMSWORD[0]))						{ err = ERR_NAME2; break; }
					currentDKey = newSmsDrumKey(SMSWORD);
					if (!currentDKey)  									{ err = ERR_NAME; break; }
					sms->drumkeys++;
					break; 
				}
				if(!err) DrumTrk->prg = sms->drk;
				err = parser_isParameter(SMSWORD, P_CMDTYPE, currentDKey); 
				if(!err) DrumTrk->prg = sms->drk;
				break;	
			case CHORD:
				if ( cntLINE_WORD == 2) { 
					if(!parser_isChar(SMSWORD[0]))						{ err = ERR_NAME2; break; }
					currentChord = newSmsChord(SMSWORD);
					if (!currentChord)  								{ err = ERR_NAME; break; }
					sms->chords++;
					break; 
				}
				int idx = cntLINE_WORD - 3;
				if ( idx >= CHORD_KEYS )					    		{ err = ERR_LIST_MAX; break; }
				res = parser_getNumber(SMSWORD, &value);
				if ( res != strlen(SMSWORD) )							{ err = ERR_CHORDSYNTAX; break; }
				if ( value > NOTE_MAX_OFFSET ) 							{ err = ERR_CHORDSYNTAX; break; }	
				currentChord->keys[idx] = value;
				break;
			case ARP:
				if ( cntLINE_WORD == 2) {
					if(!parser_isChar(SMSWORD[0]))						{ err = ERR_NAME2; break; }
					currentArp = newSmsMacro(SMSWORD, P_CMDTYPE);
					if (!currentArp)  									{ err = ERR_NAME; break; }
					sms->arps++;
					currentArp->startline = cntLINE;
					cntARPLINE		= 0;
					cntARPLINE_WORD	= 2;
					break; 
				}
				// add word to macro list (without checking)
				// memory allocation for oldlist + blank + new word + null terminator
				if ( token == MACRO_START 		||
					 token == MACRO_END   		||
					 token == TIME_BLOCK_START 	||
					 token == TIME_BLOCK_END 	)						{ err = ERR_ARP_SYMBOL; break; }
					 
				P_MACRO_COMMANDS = malloc(strlen(currentArp->list) + 1 + strlen(SMSWORD) + 1);
				strcpy(P_MACRO_COMMANDS, currentArp->list);
				strcat(P_MACRO_COMMANDS, " ");
				strcat(P_MACRO_COMMANDS, SMSWORD);
				free(currentArp->list);
				currentArp->list = P_MACRO_COMMANDS;
				break;
			case MACRO:
				if ( P_MACRO == IDLE && cntLINE_WORD == 2 ) {
					if(!parser_isChar(SMSWORD[0]))						{ err = ERR_NAME2; break; }
					currentMac = newSmsMacro(SMSWORD, P_CMDTYPE);
					if (!currentMac)  									{ err = ERR_NAME; break; }
					sms->macs++;
					currentMac->startline = cntLINE;
					break; 
				}
				if ( P_MACRO == IDLE && cntLINE_WORD == 3 ) {
					if ( token != MACRO_START ) 						{ err = ERR_MACRO; break; }
					P_MACRO     	= DEFINING;
					cntMACLINE		= 0;
					cntMACLINE_WORD	= 3;	
					break;
				}
				// check curly braces, otherwise add word to macro list
				switch ( token ) {	
					case MACRO_START:									{ err = ERR_MACRO; break; }
					case MACRO_END:
						if ( P_MACRO != DEFINING ) 						{ err = ERR_MACRO; break; }
						P_MACRO 			= IDLE;
						P_CMDTYPE 			= UNKNOWN;
						currentMac->lines 	= cntMACLINE;
						P_COMMENT 			= TRUE;
						break;
					default: {
						// check nested macro (not allowed)
						void *p = getObject(SMSWORD, &type);
						if ( p && type == MACRO) 						{ err = ERR_MACRO_NESTED; break; }
						// add word to macro list (without checking)
						// memory allocation for oldlist + blank + new word + null terminator
						P_MACRO_COMMANDS = malloc(strlen(currentMac->list) + 1 + strlen(SMSWORD) + 1);
						strcpy(P_MACRO_COMMANDS, currentMac->list);
						strcat(P_MACRO_COMMANDS, " ");
						strcat(P_MACRO_COMMANDS, SMSWORD);
						free(currentMac->list);
						currentMac->list = P_MACRO_COMMANDS;
						break;
					}
				}
				break;

			default:
				P_NEXTWORD = FALSE;
				break;
		}
		if ( err ) break;
		if ( P_NEXTWORD ) continue;
//		
// handling user command
//
		smsTrack *trk 	= currentTrk;
		smsEvent *evt;
		BYTE status, data1, data2;

		void *p = getObject(SMSWORD, &type);
		if ( p ) {
			if ( type == INST || type == DRUM ) {
				if ( type == INST ) {
					currentTrk = trk = p;
					//set bank
					status 	= 0xB0 + trk->chn; data1 = 0; data2 = trk->bnk;  
					evt = newSmsEvent(trk, sms->evts++, sngTime, status, data1, data2);
					//set prg or drum kit
					status 	= 0xC0 + trk->chn; data1 = trk->prg; data2 = 0;				  
					evt = newSmsEvent(trk, sms->evts++, sngTime, status, data1, data2);
				}
				if ( type == DRUM ) {
					currentDKey  = p;
					currentTrk   = trk = DrumTrk;
				}
				// timing
				if(barTime) sngTime += sms->bar - barTime;
				barTime  = 0;
				continue;
				
			} else if ( type == MACRO) {							// macro
				if(P_MACRO != IDLE)									{ err = ERR_MACRO_NESTED; break; }
				currentMac 			  = p;
				P_MACRO_COMMANDS      = (char*)calloc(strlen(currentMac->list)+1, sizeof(char));
				strcpy(P_MACRO_COMMANDS, currentMac->list);			
				P_MACRO      		  = PASSING;
				SMSWORD         	  = NULL;
				cntMACLINE 	 		  = 0;
				cntMACLINE_WORD		  = 3;	
				// default settings for macro (like new line)
				currentTrk->note->hft = 0;
				currentTrk->note->oct = 5;
				currentTrk->note->dur = 4;
				currentTrk->note->dot = 0;
				currentTrk->note->vol = 127;
				currentBaseNote       = EMPTY;
				P_CMDTYPE			  = UNKNOWN;
				continue;
			}
		}
		
		if ( err ) break;
//		
// handling timer commands
//
		if ( token == TIME_BLOCK_START ) {
			if (P_TIMEBLOCK != IDLE)	 					{ err = ERR_BLOCK; break; }
			P_TIMEBLOCK	 = PASSING;
			blkTimeStart = blkTimeEnd = sngTime;
			P_COMMENT  	 = TRUE;
			continue;
		} else if ( token == TIME_BLOCK_END ) {
			if (P_TIMEBLOCK != PASSING) 					{ err = ERR_BLOCK; break; }
			P_TIMEBLOCK  = IDLE;
			// handling blocks and time groups
			if ( blkTimeEnd < sngTime ) blkTimeEnd = sngTime;
			if ( grpTimeEnd < sngTime ) grpTimeEnd = sngTime;
			sngTime	     = blkTimeEnd;
			blkTimeStart = blkTimeEnd = TIME_OFF;
			P_COMMENT  	 = TRUE;
			continue;
		} else if ( token == TIME_GROUP_START ) {
			if (P_TIMEGROUP != IDLE) 						{ err = ERR_TIME_GROUP; break; }
			P_TIMEGROUP  = PASSING;
			grpTimeStart = grpTimeEnd = sngTime;
			grpTimeBar   = barTime;
			continue;
		} else if ( token == TIME_GROUP_END ) {
			if (P_TIMEGROUP != PASSING) 					{ err = ERR_TIME_GROUP; break; }
			P_TIMEGROUP  = IDLE;
			sngTime      = grpTimeEnd;
			barTime      = grpTimeBar + (grpTimeEnd - grpTimeStart);
			grpTimeStart = grpTimeEnd = grpTimeBar = TIME_OFF;
			continue;
		} else if ( token == BARLINE ) {;
			if (P_TIMEGROUP == PASSING) 					{ err = ERR_TIME_GROUP; break; }
			if(barTime > sms->bar) 							{ err = ERR_BAR; break; }
			if(barTime) sngTime += sms->bar - barTime;
			barTime  = 0;
			currentTrk->note->dot = 0;
			continue;
		}
//	
// process word as other (dynamic) header parameter 
//
		// change tempo with bpm= 
		err = parser_isBPM(SMSWORD, &value);
		if(!err) {
			smsEvent *evt;
			// send all notes off for channel of current track
			evt = newSmsEvent(trk, sms->evts++, sngTime, 0xB0, 0x7B, 0);
			// send tempo change
			evt = newSmsEvent(trk, sms->evts++, sngTime, 0, 0, 0);
			evt->bpm = value;
			continue;
		} else if(err == ERR_VALUE) break;
		
		// change bar type with bar=		
		err = parser_isBAR(SMSWORD, &value);
		if(!err) {
			sms->bar = sms->ppqn * value;
			continue;
		} else if(err == ERR_VALUE) break;

// MIDICC: process word as midi controller
		int cc, v;
		err = parser_isMidiCC(SMSWORD, &cc, &v);
		if ( !err ) {	
			status  	  = 0xB0 + trk->chn;
			data1    	  = cc;
			data2    	  = v;
			smsEvent *evt = newSmsEvent(trk, sms->evts++, sngTime, status, data1, data2);
			continue; 
		}
		if ( err != ERR_NOERROR && err != ERR_NO_COMMAND ) break;

// BASENOTE: process word as base note		
		int bs = parser_isBaseNote(SMSWORD);
		if(bs == ERR_BASENOTE) { err = bs; break; }
		err = (bs == EMPTY) ? ERR_NO_COMMAND : ERR_NOERROR;
		if(!err) { 
			currentBaseNote = bs; 
			continue; 
		}	

// NOTE: process word as note
		int trkType = (trk->chn == 9) ? DRUM : INST;
		int holdKey = trk->note->hold;
		if(currentBaseNote == EMPTY) err = parser_isNote( SMSWORD, trk->note, trkType ); 	// key note or drum note
		if(currentBaseNote != EMPTY) err = parser_isNote( SMSWORD, trk->note, BASENOTE );  	// tab 
		if( err != ERR_NOERROR && err != ERR_NO_COMMAND ) break;

		if ( !err ) { 
			smsNote *n = trk->note;	
			float dot  = ( n->dot ) ? 1.5 : 1.0;
			int   dur  = (int)(sms->ppqn * 4 / n->dur * dot);
			if ( grpTimeStart != TIME_OFF ) sngTime = grpTimeStart;
			// is pause
			if ( n->key == PAUSE ) {
				sngTime += dur;
				barTime += dur;
				if(holdKey != EMPTY)
					evt = newSmsEvent(trk, sms->evts++, sngTime, 0x80 + trk->chn, holdKey, 0);
			} else {
				// set note on
				status = 0x90 + trk->chn;
				if(currentBaseNote == EMPTY) data1  = ( n->key == BEAT) ? currentDKey->key : n->key + n->hft + n->oct * 12;
				if(currentBaseNote != EMPTY) data1  = n->key + currentBaseNote;
				if(data1 > 128) { err = ERR_NOTE ; break;	}
				data2 = n->vol;
				evt = newSmsEvent(trk, sms->evts++, sngTime, status, data1, data2);
				// set current note off
				sngTime    += dur;
				barTime    += dur;
				status    	= 0x80 + trk->chn;
				
				if(n->hold == EMPTY) {
					evt = newSmsEvent(trk, sms->evts++, sngTime-MIDI_TIME_DIV , status, data1, data2);
				} else {
					n->hold 		 = data1;
				}
				
				// set last hold note off
				if(holdKey != EMPTY)
					evt = newSmsEvent(trk, sms->evts++, sngTime-MIDI_TIME_DIV , status, holdKey, 0);
			} 	
			
			// handling blocks and time groups
			if ( P_TIMEBLOCK == PASSING && blkTimeEnd < sngTime ) blkTimeEnd = sngTime;
			if ( P_TIMEGROUP == PASSING && grpTimeEnd < sngTime ) grpTimeEnd = sngTime;
			lastWordType = NOTE;
			continue;
		}	
	
// CHORD: process word as key chord and arp
		smsChordNote *c = trk->cnote;
		err = parser_isChord(SMSWORD, c);	
		if(err) { err = ERR_NO_COMMAND ; break;	}				// word is not a chord

		BYTE *ckeys     = c->chord->keys;

		// chord play without arp, all tones as 1/1 notes at same time
		if (!c->arp) {
			int delay = 0;
			for(int i = 0; i < CHORD_KEYS; i++) {
				if( ckeys[i] == EMPTY ) continue;
				status 	= 0x90 + trk->chn;
				data1   = (CHORD_OCTAVE * 12) + c->key + c->hft + ckeys[i]; 
				data2 	= 127;
				evt = newSmsEvent(trk, sms->evts++, sngTime + delay, status, data1, data2);

				status 	= 0x80 + trk->chn;
				evt = newSmsEvent(trk, sms->evts++, sngTime-MIDI_TIME_DIV  + sms->bar, status, data1, data2);
			}
			sngTime += sms->bar;
			barTime += sms->bar;
			// handling blocks and time groups
			if ( P_TIMEBLOCK == PASSING && blkTimeEnd < sngTime ) blkTimeEnd = sngTime;
			if ( P_TIMEGROUP == PASSING && grpTimeEnd < sngTime ) grpTimeEnd = sngTime;

			lastWordType = CHORD;
			continue;
		} 
		
		// chord play with arp
		if(c->arp) {
			char *arplist	= c->arp->list;
			smsNote *n      = newSmsNote();
			n->oct          = 0;
			ARPWORD			= (char*)calloc(64, sizeof(char));
			P_EVENTTYPE 	= ARP;
			cntARPLINE_WORD = 2;

			while (strlen(arplist)) {
				cntARPLINE_WORD++;
				int res  = sscanf(arplist, "%s", ARPWORD);
				int size = strlen(ARPWORD);
				arplist = arplist + size + 1;

				// handling blocks and time groups
				if ( size == 1 && ARPWORD[0] == TIME_GROUP_START ) {
					if (P_TIMEGROUP != IDLE) 						{ err = ERR_TIME_GROUP; break; }
					P_TIMEGROUP  = PASSING;
					grpTimeStart = grpTimeEnd = sngTime;
					grpTimeBar	 = barTime;
					continue;
				} else if ( size == 1 && ARPWORD[0] == TIME_GROUP_END ) {
					if (P_TIMEGROUP != PASSING) 					{ err = ERR_TIME_GROUP; break; }
					P_TIMEGROUP  = IDLE;
					sngTime  	 = grpTimeEnd;
					barTime      = grpTimeBar + (grpTimeEnd - grpTimeStart);
					grpTimeStart = grpTimeEnd = TIME_OFF;
					continue;
				} else if ( size == 1 && ARPWORD[0] == BARLINE ) {
					if (P_TIMEGROUP == PASSING) 					{ err = ERR_TIME_GROUP; break; }
					if(barTime > sms->bar) 							{ err = ERR_BAR; break; }
					if(barTime) sngTime += sms->bar - barTime;
					barTime  = 0;
					trk->note->dot = 0;
					continue;
				}
			
				// play arpeggio		
				err = parser_isNote( ARPWORD, n, ARP );
				if (err) break;

				int   oct   = CHORD_OCTAVE + n->oct;
				if(oct < 1 || oct > 10 ) 	{ err = ERR_OCTAVE; break; }
				float dot 	= ( n->dot ) ? 1.5 : 1.0;
				int   dur 	= (int)(sms->ppqn * 4 / n->dur * dot);

				if ( n->key == PAUSE || n->key == EMPTY) {	// is pause
					sngTime += dur;
					barTime += dur;
				} else {													// is note
					if (oct < 1 || oct > 10) { err = ERR_OCTAVE; break; }
					// set note on
					status 	= 0x90 + trk->chn;
					data1 = (oct * 12) + c->key + c->hft + ckeys[n->key];
					data2 	= n->vol;
					if ( grpTimeStart != TIME_OFF )  sngTime = grpTimeStart;
					evt = newSmsEvent(trk, sms->evts++, sngTime, status, data1, data2);
					// set note off
					sngTime  += dur;
					barTime  += dur;
					status    = 0x80 + trk->chn;
					evt = newSmsEvent(trk, sms->evts++, sngTime, status, data1, data2);
				}
				// handling blocks and time groups
				if ( P_TIMEBLOCK == PASSING && blkTimeEnd < sngTime ) blkTimeEnd = sngTime;
				if ( P_TIMEGROUP == PASSING && grpTimeEnd < sngTime ) grpTimeEnd = sngTime;		
			} 

			if (err) break;
			P_EVENTTYPE = UNKNOWN;
			
			lastWordType = CHORD;
			continue;
		} 
		break;
	}
	
// ----------------------------------------------------------------------------------------	
// END word read main loop, end of data
// ----------------------------------------------------------------------------------------
	// generate successful message
	char *buf = (char*)calloc(1024, sizeof(char));
	char *str = (char*)calloc(1024, sizeof(char));
	if ( !err && P_MACRO 	 == DEFINING)	err = ERR_MACRO_BRACES;
	if ( !err && P_TIMEBLOCK == PASSING)    err = ERR_TIME_BLOCK;
	if ( P_BLOCKCOMMENT)					err = ERR_BLOCKCOMMENT;
	
	if ( !err ) { 
		// fill rest of bar with pause
		if(barTime) sngTime += sms->bar - barTime;
		currentTrk->note->dot = 0;
		// send all notes off for channel of current track
		smsEvent *evt = newSmsEvent(currentTrk, sms->evts++, sngTime, 0xB0, 0x7B, 0);
	}

	if ( !err ) {
		sprintf(str, "compiler result:\n");											strcat(buf, str);
		sprintf(str, "song '%s' ", sms->name); 								strcat(buf, str);
		sprintf(str, "lines %i words %i\n",  cntLINE, cntWORD); 			strcat(buf, str);
		sprintf(str, "bpm %i ppqn %i ",sms->bpm, sms->ppqn);				strcat(buf, str);
		sprintf(str, "tracks %i drumkeys %i\n", sms->trks, sms->drumkeys); 	strcat(buf, str);
		sprintf(str, "chordtypes %i ", sms->chords);  						strcat(buf, str);
		sprintf(str, "macros %i events %i", sms->macs, sms->evts);			strcat(buf, str);
		*msg = buf;
		struct BUF *smf = parser_createMidi(sms);
		freeSMS(sms);
		return smf;
	}

	// generate detailed error message
	sprintf(str, "compiler error:\n");										strcat(buf, str);												
	if ( err == ERR_MACRO_BRACES || err == ERR_BLOCKCOMMENT ) {
		sprintf(str, "%s\n", ERRMSG[err]);									strcat(buf, str);
	} else {
		sprintf(str, "line %3i pos %2i ", cntLINE, cntLINE_WORD);			strcat(buf, str);				
		if ( P_MACRO == PASSING ) {
			sprintf(str, "macro '%s'\n", currentMac->name);					strcat(buf, str);
			int mline = cntMACLINE+currentMac->startline;
			sprintf(str, "line %3i pos %2i ", mline, cntMACLINE_WORD);		strcat(buf, str);
		}
		if ( P_EVENTTYPE == ARP ) {
			char *arpname = currentTrk->cnote->arp->name;
			sprintf(str, "arp '%s'\n", SMSWORD);							strcat(buf, str);
			int mline = currentTrk->cnote->arp->startline;
			sprintf(str, "line %3i pos %2i ", mline, cntARPLINE_WORD);		strcat(buf, str);
			SMSWORD = ARPWORD;
		}
		sprintf(str, "word '%s'\nerr-message: %s", SMSWORD, ERRMSG[err]);	strcat(buf, str);
	}
	*msg = buf;
	freeSMS(sms);
	return NULL;
}

