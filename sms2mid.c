// sms.c
// 		HIDCAM
//# 
//# 	 - SMS: a simple music scripting (sms) language 
//# 	 		to generate midi file by ma.ke 2023
//#   	      		- without warranty
//#   	       		- use it on your own risk
//#             	- do what ever you want with this
//#   	       		- be happy
//#
// 
// description of sms commands see manual.sms

#include <windows.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

#include "sms2mid.h"		// midi and sms api for simple music script language

/***********************************************************name************
 * main function
 ***************************************************************************/
 
int main(int argc, char **argv) {
	int midiSMS = TRUE;
	
	printf("sms2midi with included sms version %s (c) ma.ke.\n", SMSVERSION);
	
	if (argc < 3) {
	printf("usage: %s input.sms output.mid\n", argv[0]);
		return -1;
	}
	
	char  *msg;
	char  *data 	= get_file_to_mem(argv[1]);
	struct BUF *smf = sms2midi(data, &msg);
	
	if (!smf) {
		printf("%s\n", msg);
		return -2;
	}
	
	writeSMF(argv[2], smf);
	free(data);
	printf("%s ready\n", msg);
	return 0;
}	
