/*
time-signal.c - a JJY/MSF/WWVB/DCF77 radio transmitter for Raspberry Pi
Copyright (C) 2023 Pierre Brial <p.brial@tethys.re>

Parts of this code is based on txtempus code written by Henner Zeller
Source: https://github.com/hzeller/txtempus
Copyright (C) 2018 Henner Zeller <h.zeller@acm.org>
Licensed under the GNU General Public License, version 3 or later

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include "time-services.h"
#include "clock-control.h"
#include <inttypes.h>
//#include <time.h>


// Define Timezone Difference to add to JST when using JJY
#define TZ_DIFF 1 // add 1hour when in Singapore/Taipei

int usage(const char *msg, const char *progname)
	{
	fprintf(stderr, "%susage: %s [options]\n"
		"Options:\n"
		"\t-s <service>          : Service; one of "
		"'DCF77', 'WWVB', 'JJY40', 'JJY60', 'MSF'\n"
		"\t-v                    : Verbose.\n"
		"\t-c                    : Carrier wave only.\n"
		"\t-h                    : This help.\n",
	msg, progname);
	return 1;
	}

void signaux(int sigtype)
	{
	switch(sigtype)
		{
		case SIGINT : printf ("\nSIGINT");break;
		case SIGTERM : printf ("\nSIGTERM");break;
		default : printf ("\nUnknow %d",sigtype);
		}
	StopClock();
	printf (" signal received - Programme terminé\n");
	exit(0);
	}

int main(int argc, char *argv[])
{
bool
	verbose = false,
	carrier_only = false;

int
	modulation,
	frequency=60000,
	opt;
	
uint64_t minute_bits;
char
	*time_source="",
	date_string[] = "1969-07-21 00:00:00";	//,*lineptr;
//size_t n=0;
enum time_service service;
time_t now,minute_start;
struct timespec target_wait;
struct tm tv;
//FILE *f;

signal(SIGINT,signaux);
signal(SIGTERM,signaux);

puts("time-signal, a JJY/MSF/WWVB/DCF77 radio transmitter");
puts("Copyright (C) 2023 Pierre Brial");
puts("This program comes with ABSOLUTELY NO WARRANTY.");
puts("This is free software, and you are welcome to");
puts("redistribute it under certain conditions.\n");

while ((opt = getopt(argc, argv, "vs:hc")) != -1)
	{
	switch (opt)
		{
		case 'v':
			verbose = true;
		break;
		case 's':
			time_source = optarg;
		break;
		case 'c':
			carrier_only = true;
		break;
		default:
			return usage("", argv[0]);
		}
	}

if (strcasecmp(time_source, "DCF77") == 0)
	{
	frequency = 77500;
	service = DCF77;
	}
else if (strcasecmp(time_source, "WWVB") == 0) service = WWVB;
else if (strcasecmp(time_source, "JJY40") == 0)
	{
	frequency = 40000;
	service = JJY;
	}
else if (strcasecmp(time_source, "JJY60") == 0) service = JJY;
else if (strcasecmp(time_source, "MSF") == 0) service = MSF;
else return usage("Please choose a service name with -s option\n", argv[0]);

GPIO_init ();
StartClock(frequency);

if (carrier_only) EnableClockOutput(1);

// Give max priority to this programm
struct sched_param sp;
sp.sched_priority = 99;
sched_setscheduler(0, SCHED_FIFO, &sp);

//time_t now_lc;
//struct tm *info_lc;
//time(&now_lc);
//info_lc = localtime(&now_lc);

//info_lc->tm_hour += 1;

//now = mktime(info_lc);

// Add TZ_DIFF to JST
now = time(NULL) + TZ_DIFF * 3600;
//struct tm now_jst = now;
//now_jst.tm_sec += 3600;
//mktime(&now_jst);
minute_start = now - now % 60; // round to minute
//minute_start = &now_jst - &now_jst % 60; // round to minute
printf("%ld\n", (long)now);
//printf("%ld\n", (long)now + TZ_DIFF*3600);
printf("%ld\n", (long)minute_start);

for(int i = 0; i < 15; i++)
	{
	if (carrier_only) continue;
	localtime_r(&minute_start, &tv);
  	strftime(date_string, sizeof(date_string), "%Y-%m-%d %H:%M:%S", &tv);
	printf("%s\n",date_string);
	minute_bits = prepareMinute(service,minute_start);
	//printf("%" PRIu64 "\n", minute_bits);

	for (int second = 0; second < 60; ++second)
		{
		modulation = getModulationForSecond(service,minute_bits,second);
		//printf("%d\n", modulation);
			
		// First, let's wait until we reach the beginning of that second
		target_wait.tv_sec = minute_start - TZ_DIFF * 3600 + second; // adjust back TZ_DIFF
		target_wait.tv_nsec = 0;
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target_wait, NULL);
		
		if (service == JJY) EnableClockOutput(1);	// Set signal to HIGH
		else EnableClockOutput(0);					// Set signal to LOW
				
		if (verbose)
			{
			fprintf(stderr,"%03d ",modulation);
			if ((second+1)%15 == 0) fprintf(stderr,"\n");
			}
		
		target_wait.tv_nsec = modulation*1e6;
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target_wait, NULL);
		
		if (service == JJY) EnableClockOutput(0);	//signal to LOW
		else EnableClockOutput(1);		 			// Set signal to HIGH
		}
		
	minute_start += 60;
	}
}	
