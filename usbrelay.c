/*
usbrelay: Control USB HID connected electrical relay modules
Copyright (C) 2014  Darryl Bond
Library version
Copyright (C) 2019  Sean Mollet

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <hidapi/hidapi.h>
#include "libusbrelay.h"
#include "usbrelay.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int main(int argc, char *argv[])
{
	struct relay *relays = 0;
	int debug = 0;
	int verbose = 1;
	int i,c;
	int exit_code = 0;

	while ((c = getopt (argc, argv, "qd")) != -1) {
    switch (c) {
      case 'q':
        verbose = 0;
        break;
      case 'd':
        debug = 1;
        break;
      }
	}
	/* allocate the memory for all the relays */
	if ((argc - optind) >= 1) {
		relays = calloc(argc - optind + 1, sizeof(struct relay));	/* Yeah, I know. Not using the first member */
		/* relays is zero-initialized */
	} 
	/* loop through the command line and grab the relay details */
	for (i = 0 ; i < (argc - optind); i++) {
		char *arg = argv[i + optind];
		struct relay *relay = &relays[i];

		char *underscore = strchr(arg, '_');
		char *equal_sign = strchr(arg, '=');

		if (underscore && equal_sign && underscore > equal_sign) {	/* e.g. ASDFG=QWERT_1 */
			fprintf(stderr, "Invalid relay specification: %s\n",
				argv[i + optind]);
			exit(1);
		}

		size_t size;
		if (underscore)
			size = underscore - arg;
		else if (equal_sign)
			size = equal_sign - arg;
		else
			size = strlen(arg);

		relay->this_serial = malloc(size + 1);
		strncpy(relay->this_serial, arg, size);

		/* Parse relay number */
		if (underscore)
			relay->relay_num = atoi(underscore + 1);

		if (equal_sign) {
			if (relay->relay_num == 0) {	/* command to change the serial - remaining token is the new serial */
				strncpy(relay->new_serial, equal_sign + 1,
					sizeof(relay->new_serial) - 1);
			} else {
				if (atoi(equal_sign + 1)) {
					relays[i].state = CMD_ON;
				} else {
					relays[i].state = CMD_OFF;
				}
			}
		}
		if (debug) {
			fprintf(stderr, "Orig: %s, Serial: %s, Relay: %d State: %x\n",
				arg, relay->this_serial, 
				relay->relay_num,
				relay->state);
			relay->found = 0;
		}
	}

	//Locate and identify attached relay boards
	if(debug) fprintf(stderr,"Version: %s\n",gitversion);
	enumerate_relay_boards(getenv("USBID"), verbose, debug);

	/* loop through the supplied command line and try to match the serial */
	for (i = 0; i < (argc - optind); i++) {
		if (debug ) {
			fprintf(stderr, "main() arg %d Serial: %s, Relay: %d State: %x \n",
				i,relays[i].this_serial, 
				relays[i].relay_num,
				relays[i].state);
		}
		relay_board *board = find_board(relays[i].this_serial, debug );

		if (board) {
			if (debug == 2)
				fprintf(stderr, "%d HID Serial: %s ", i, board->serial);
			if (relays[i].relay_num == 0) {
				if (!relays[i].new_serial[0]) {
					fprintf(stderr, "\n \n New serial can't be empty!\n");
				} else {
					fprintf(stderr, "Setting new serial\n");
					set_serial(board->serial,
						   relays[i].new_serial,debug);
				}
			} else {
				if (debug)
					fprintf(stderr,
						"main() operate: %s, Relay: %d State: %x\n",
						relays[i].this_serial,
						relays[i].relay_num,
						relays[i].state);
				if (operate_relay(relays[i].this_serial, relays[i].relay_num,relays[i].state,debug) < 0)
					exit_code++;
				relays[i].found = 1;
			}
		}
	}

	shutdown();

	for (i = 1; i < (argc - optind); i++) {
		if (debug)
			fprintf(stderr,"Serial: %s, Relay: %d State: %x ",
				relays[i].this_serial, relays[i].relay_num,
				relays[i].state);
		if (relays[i].found) {
			if (debug)
				fprintf(stderr, "--- Found\n");
		} else {
			if (debug)
				fprintf(stderr, "--- Not Found\n");
			exit_code++;
		}
	}
	if (relays) {
		for (i = 1; i < (argc - optind) ; i++) {
			free(relays[i].this_serial);
		}
		free(relays);
	}

	exit(exit_code);
}
