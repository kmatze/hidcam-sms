>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

**HIDCAM-SMS:**	a simple music scripting (sms) language 
			to generate midi file by ma.ke 2024-10-09
			- without warranty
			- use it on your own risk
            - do what ever you want with this
            - be happy
			
**ONE-PAGER HELP**  >>>> for more in-depth information read file smsManual.sms  <<<<
commands and parameter for HIDCAM's simple music scripting language (HIDCAM-SMS)
--------------------------------------------------------------------------------
define header           H: name bpm=x ppqn=x bar=x/x
define inst track       I: name chn=x bnk=x  prg=x   key=x
define drum key         D: name key=x
define chordtype       	C: name n0 .. n6     (max. 7 notes, optional octave, #)
define arpreggio        A: name 0-6 ... 0-6  (optional pitch, duration, volume)
define macro            M: name { list ... } (for list see the following events)
comment line / block    //                   /* ... */	
bar / multiplier        | start new bar      *[1..n] repeat last word n times
time group/block        ( n1 n2  ... )       [ track ...  / basenote: ... ]
midi controller         @ccc=x    or    predefined: @vol=x @bal=x @pan=x @dly=x
--------------------------------------------------------------------------------
event as one word ...  ¦ note          ¦ drum ¦ tab   ¦ chord           ¦ pause
                symbol ¦ c d e f g a b ¦  x   ¦ 0-127 ¦ C D E F G A B   ¦ p o -
opt:  octave/sign/base ¦ [0-10] # :    ¦      ¦       ¦ [0-10] #        ¦
       qualifier pitch ¦ + - > <       ¦      ¦       ¦                 ¦
              duration ¦ /x .          ¦ /x . ¦ /x .  ¦                 ¦ /x .
                volume ¦ !x            ¦ !x   ¦ !x    ¦                 ¦
hold with next evt dur ¦ evt_          ¦      ¦       ¦                 ¦
            chord type ¦               ¦      ¦       ¦ mandatory info  ¦
  chord arpreggio link ¦               ¦      ¦       ¦ ~ eg. C3maj~arp ¦
--------------------------------------------------------------------------------

>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

**manual.sms   HIDCAM-SMS

HIDCAM-SMS:	a simple music scripting (sms) language 
			to generate midi file by ma.ke 2024-10-09
			- without warranty
			- use it on your own risk
            - do what ever you want with this
            - be happy


**header command                   name is a file name too
-----------------------------------------------------------------------------
H:		define header       	name bpm=x bar=x/x ppqn=x

        name    name_of_song
        bpm     tempo in beats (quarter notes) per minute   
                default: 120 [30 - 240]   
                to redefine use it separatly outside of header command
        bar     bar, time signature                         
                default: 4/4 [2-8] / [2, 4, 8, 16, 32]
                to redefine use it separatly outside of header command
        ppqn    pulse per quarter note                      
                default: 96  [24, 48, 96, 192, 384, 768]


**track instrument command        name is a new instrument track command
-----------------------------------------------------------------------------
I:     	create inst track       name chn=x bnk=x prg=x drk=x
                                     chn   default: 0   [0-(9=drum)-15]    
                                     bnk   default: 0   [0-127, 128(drum bank)] 
									 prg   default: 0   [0-127] sound patch
									 key   default: 0   [0-127] default drum key
				
    hints
    - standard/default tracks called INST and DRUM, 
      INST is set automatically for every new line
      DRUM is set automatically by use of drum key (see drum key command)
		I: INST chn=0, bnk=0,   prg=0
		I: DRUM chn=9, bnk=128, prg=0, key=31
    - playing without instrument definition is using INST on chn = 0
    - tracks with same MIDI channel in a time block may be play cacophonically


**(sub track) drum key command    name is a new drum key for DRUM track
-----------------------------------------------------------------------------
D:      create drum key         name key=x
                                     key   	default: 31 [0-127] - drum key (sound)      

    hints:
    - there is default drum track called DRUM (see track instrument command)
    - is set automatically as active track by use of drum key
    - standard/default drum key called TICK: (drk=31)
                                            

**chord command                   name is a new chord**
-----------------------------------------------------------------------------
C:      create chord type	    name note0 ... note6
                                noteX [0 .. 24]   0      main tone of chord
                                                  1-24   relative to main tone
example:    C: maj  0 4 7

(implemented standard chord types - using the example of note C)
 major chords:   Cmaj    C7      Cmaj7   C6      C6/9
                 C5      C9      Cmaj9   C11     C13
                 Cmaj13  Cadd    C7-5    C7+5    Csus
                 Cdim    Cdim7   Caug    Caug7

 minor chords:   Cm      Cm7     CmM7    Cm6     Cm9 
                 Cm11    Cm13    Cm7b5

 hints:    
		- chord uses without arp: play tones as 1/1 notes at same time (like as time group)


**arpreggio command               name is a new arpreggio command**
-----------------------------------------------------------------------------
A:      arpreggio of chord:     name chord-note_list (position of note 
                                defined in chord, begins with #0)
                                optional shift octave, duration and
								volume qualifier

C: maj 	 0 4 7
A: arp | 0 1 2 0 |
useage in composition: Cmaj~arp


**macro commands                  name is a new user command**
-----------------------------------------------------------------------------
M:      macro                   name { ... }
                                all things except macros and commands


**note and there qualifier commands (all as one word, no separators!)
-----------------------------------------------------------------------------
notes: 
    c d e f g a b               - note value, optional with
    nn								- octave [0-10] and/or
    :								- base note flag define base note of 
									  tabulature line (without qualifier!!!)
    p -                         - pause / empty
    x o                         - beat on / off (only to use with drum key) 
	0-24						- offset to basenote: 
								  only to use in combination with base note

qualifier:
 	pitch       >               - pitch shift octave up
     	        <               - pitch shift octave down
         	    #               - pitch half tone up   (# sign)
             	+               - pitch half tone up   (# sign)
             	-               - pitch half tone down (b sign)

 	duration    /xx             - duration divisor of whole note time
                                  xx: 1,2,4,8,16,32,64
      	        .               - current duration +50%
                _              	- hold duration and ends with next event duration
                                  in same track
                                - must be last qualifier symbol in event definition

 	volume      !xxx            - volume
                                  xx: 0-127 


**chord and there qualifier commands
-----------------------------------------------------------------------------
chord: 
	C D E F G A B               - chord value, optional with

qualifier:
	pitch       #               - pitch half tone up   (# sign)
	octave     	nn				- octave [0-10]

	ctype       name            - name of chord type 
                                  mandatory information, e. g. Cmaj


**midi controller commands for midi channel (track)
-----------------------------------------------------------------------------
    predefined @... standard controller:
    -------------------------------------------
    @vol=xxx     - midi-cc   7   xxx: 0-127  ->  change main volume
    @bal=xxx     - midi-cc   8   xxx: 0-127  ->  change balance
    @pan=xxx     - midi-cc  10   xxx: 0-127  ->  change pan position
    @dly=xxx     - midi-cc  91   xxx: 0-127  ->  change stereo delay

    general controller:
    -------------------------------------------
    @ccc=xxx     - midi-cc ccc   ccc: 0-127  ->  set controller to
                                 xxx: 0-127  ->  value


**miscellaneous commands 
----------------------------------------------------------------------
 comment1     //                - line comment, 
                                  after '//' all words are comments
                                  until end of line
 comment2     /* ... */			- block comment
                                  after '/*' all words are comments
                                  until end of block comment '*/'
 bar          |                 - define start of new bar inside track, arpreggio, 
                                  check correct timing of previous bar
 group time   ( n1 n2  ... )    - notes begin at same time inside 
                                  track, arpreggio
 chord link   ~                 - chord linked with arpreggio, e.g.
                                  Am~arp1
 assign       =                 - assign value with parameter or midi controller
                                  assigned with value, e.g. @vol=127 or @007=127 
 multiplier	*[1-n]				- repeat last word n times 
                                  only note, chord or macro will be repeated
 time block   [                 - simultaneous play, block start 
                                  (rest of line possible for comment)
                 track/base note   - track / base note with list of 
				...					 notes / offsets of base note, 
                 ...                 chord~arpreggio, 
                 ...                 midi controller
                 track/base note  
              ]                 - simultaneous play, block end 
                                  (rest of line possible for comment)   


**general rules
-----------------------------------------------------------------------------
 - empty lines are allowed
 - comments for 
   - line comments for full or rest of lines
   - block comments for multi line
 - every line 
   - begins with command / user command / macro / note or base note / midi controller
   - continues with parameters
   - must end with <RETURN = NEWLINE>
     - fills the rest of last bar is one or more bar symbol is used in current line
 - bpm=xxx or bar=x/y changes standard header parameter at every place 
 - macros are described in curly braces, e.g. "M: name { ... }", can be multi lined  
 - all things are a words (in context of command type),
   separators are space, tab and newline
 - the number format is always decimal
 - usr commands are tracks, drum keys, chords, arpreggios or macros 
 - all user commands have to be a general unique name
 - notes and there qualifiers are one word without separators,
   all qualifiers are optional
 - qualifiers except halfton and dot of previous note are default 
   settings for current note in same line
 - midi controller or parameter with there value are one word 
   without separators
 - chords (notes in capital letter with chord type) and there qualifiers are one 
   word without separators 
   - chords can linked (~) with arpregio otherwise play as one chord at bar
   - octave is optional and defines only for current chord
 - arpreggio defines the play order of chord notes, 
   optional with octave pitch, duration and volume qualifier
 - the time size of arpreggio must have a multiple of bar, 
   the bar symbol (|) or newline fills the rest of bar, 
   alternative use note symbol (o p -) for pause with duration qualifier 
 - the repeater can handle only notes, chords or macros
 - a track can fill with notes, chord~arpreggio, midi controller
 - tracks and/or base notes can time bundled in a block [ ... ] to play simultaneous
   - the longest track inside block defines the whole block time
   - tabulatures use the last used track/instrument
   - nested blocks are not allowed
 - all tracks will be compiled to a midi file type 1


**background: midi note numbers
-----------------------------
			┌──▄▄▄─▄▄▄──┬──▄▄▄─▄▄▄─▄▄▄──┐
			│  ███ ███  │  ███ ███ ███  │
			│  ███ ███  │  ███ ███ ███  │
			│  ███ ███  │  ███ ███ ███  │
			│	│	│	│	│	│	│	│
note 		│ c │ d │ e │ f │ g │ a │ b │
chord		│ C │ D │ E │ F │ G │ A │ B │
			└───┴───┴───┴───┴───┴───┴───┘
octave  0	   0   2   4   5   7   9  11 
octave  1     12  14  16  17  19  21  23
octave  2     24  26  28  29  31  33  35
octave  3     36  38  40  41  43  45  47
octave  4     48  50  52  53  55  57  59
octave  5     60  62  64  65  67  69  71
octave  6     72  74  76  77  79  81  83
octave  7     84  86  88  89  91  93  95
octave  8     96  98 100 101 103 105 107
octave  9    108 110 112 113 115 117 119
octave 10    120 122 124 125 127   x   x


=============================================================================

// simple music script example
// -----------------------------------------------------------------------------
// 
// header define
// -----------------------------------------------------------------------------
H:  Name_of_Song bpm=120 ppqn=96 bar=4/4

// track define
// -----------------------------------------------------------------------------
I: Piano    chn=0 bnk=0 prg=3
I: Guitar   chn=0 bnk=0 prg=87
D: HH:      key=42

M: testmacro {
   [
	   Piano   @vol=50 | a b c d | e f g b |
	   Guitar  @vol=50 | a/1     | c/1     |
	   HH:             | x o x o | o x x x |
   ]
}

testmacro *2
