//---------------------------------------------------------------------
//	File:		commands.c
//
//	Written By:	Lawrence Glaister VE7IT
//
// Purpose: This set of routines deals used to implement
//          the serial command struct for configuring
//          the internal servo parameters.
//---------------------------------------------------------------------
//
// Revision History
//
// Aug 11 2006 --    first version Lawrence Glaister
// Sept 22 2006      added deadband programming
// Sept 25 2006      added programmable servo loop interval
// 
//---------------------------------------------------------------------- 
#include "dspicservo.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef DEMOBOARD
	#warning "H/W is set for demo board!"
#endif

extern volatile unsigned short int timer_test;
extern volatile unsigned short int do_servo;
extern unsigned short int cmd_posn;			// current posn cmd from PC
extern unsigned short int cmd_err;			// number of bogus encoder positions detected
extern unsigned short int cmd_bits;			// a 4 bit number with old and new port values
extern volatile unsigned short int do_servo;
extern struct PID pid;

extern char rxbuff[];		// global rx buffer for serial data
extern char *rxbuffptr;		// local input ptr for storing data
extern short int rxrdy;		// flag to indicate a line of data is available in buffer

extern int save_setup( void );
extern unsigned int ADC10_read(void);

extern void fire_curve();

void reset_buffer();

void print_tuning(void)
{
	printf("\rCurrent Settings:\r\n");
	printf("servo enabled = %d\r\n",	pid.enable);
	printf("(p) = %f\r\n",		(double)pid.pgain);
	printf("(i) = %f\r\n",		(double)pid.igain);
	printf("(d) = %f\r\n",		(double)pid.dgain);
	printf("FF(0) = %f\r\n",	(double)pid.ff0gain);
	printf("FF(1) = %f\r\n",	(double)pid.ff1gain);
    printf("dead(b)and = %f\r\n",(double)pid.deadband);
	printf("(m)ax Output = %.2f%%\r\n",(double)pid.maxoutput);
	printf("(f)ault error = %f\r\n", (double)pid.maxerror);
	printf("(x)pc cmd multiplier = %hu\r\n", pid.multiplier);
    printf("(t)icks per servo cycle= %hu\r\n",pid.ticksperservo);
}

void process_serial_buffer()
{
	switch( rxbuff[0] )
	{
	case 'b':
		if (rxbuff[1])
		{
			pid.deadband = atof(&rxbuff[1]);
			reset_buffer();
			save_setup();
		}
		reset_buffer();
    	printf("dead(b)and = %f\r\n",(double)pid.deadband);
		break;		

	case 'p':
		if (rxbuff[1])
		{
			pid.pgain = atof(&rxbuff[1]);
			reset_buffer();
			save_setup();
		}
		printf("(p) = %f\r\n",		(double)pid.pgain);
		break;		
	case 'i':
		if (rxbuff[1])
		{
			pid.igain = atof(&rxbuff[1]);
			reset_buffer();
			pid.error_i = 0.0;	//reset integrator
			save_setup();
		}
		printf("(i) = %f\r\n",		(double)pid.igain);
		break;		
	case 'd':
		if (rxbuff[1])
		{
			pid.dgain = atof(&rxbuff[1]);
			reset_buffer();
			save_setup();
		}
		printf("(d) = %f\r\n",		(double)pid.dgain);
		break;		
	case '0':
		if (rxbuff[1])
		{
			pid.ff0gain = atof(&rxbuff[1]);
			reset_buffer();
			save_setup();
		}
		printf("FF(0) = %f\r\n",	(double)pid.ff0gain);
		break;		
	case '1':
		if (rxbuff[1])
		{
			pid.ff1gain = atof(&rxbuff[1]);
			reset_buffer();
			save_setup();
		}
		printf("FF(1) = %f\r\n",	(double)pid.ff1gain);
		break;		
	case 'm':
		if (rxbuff[1])
		{
			pid.maxoutput = atof(&rxbuff[1]);
			reset_buffer();
			
	    #ifdef YAPSC_10V
			if(pid.maxoutput>100)
				pid.maxoutput = 100;
	    #else
			if(pid.maxoutput>98)
				pid.maxoutput = 98;
	    #endif
			save_setup();
		}
		printf("(m)ax Output = %.2f%%\r\n",(double)pid.maxoutput);
		break;		

	case 'f':
		if (rxbuff[1])
		{
			pid.maxerror = atof(&rxbuff[1]);
			reset_buffer();
			save_setup();
		}
		printf("(f)ault error = %f\r\n", (double)pid.maxerror);
		break;		

	case 'x':
		if (rxbuff[1])
		{
			pid.multiplier = (short)atof(&rxbuff[1]);
			reset_buffer();
			if ( pid.multiplier < 1 ) pid.multiplier = 1;
			if ( pid.multiplier > 16 ) pid.multiplier = 16;
			save_setup();
		}
		printf("(x)pc cmd multiplier = %hu\r\n", pid.multiplier);
		break;		

    case 't':
		if (rxbuff[1])
		{
			pid.ticksperservo = (short)atof(&rxbuff[1]);
			reset_buffer();
			if ( pid.ticksperservo < 3 ) pid.ticksperservo = 3;		// 300us/servo calc
			if ( pid.ticksperservo > 1000 ) pid.ticksperservo = 1000; // .1 sec/servo calc
			save_setup();
		}
    	printf("(t)icks per servo cycle= %hu\r\n",pid.ticksperservo);
		break;		
	
	case 'r':
//		testS.prediv = atoi(&rxbuff[1]);
//		i=1;
//		while((rxbuff[i] != 'c') & (rxbuff[i] != 0) & (rxbuff[i] != 'C'))
//			i++;
//		testS.delta = atoi(&rxbuff[i]);
//		printf("%i : %i\r\n", testS.prediv, testS.delta);
		if(sscanf(rxbuff, "r%ic%iok1", &testS.prediv, &testS.delta) == 2)
			fire_curve();
		break;

//	case 'e':
//		printf("\rencoder = 0x%04X = %d\r\n",POSCNT, POSCNT & 0xffff);
//		break;	

	case 'l':
		print_tuning();
		break;

	case 's':
//		printf("\rServo Loop Internal Calcs:\r\n");
		printf("command: %ld\r\n",pid.command);
		printf("feedback: %ld\r\n",pid.feedback);
		printf("error: %f\r\n",(double)pid.error);
		printf("error_i: %f\r\n",(double)pid.error_i);
		printf("error_d: %f\r\n",(double)pid.error_d);
		printf("output: %f\r\n",(double)pid.output);
		printf("limit_state: %d\r\n",(int)pid.limit_state);
		printf("servo enabled = %d\r\n",	pid.enable);
		break;	
	case 'v':
		printf(YAPSC_VERSION);
		break;
	default:
		printf("\r\nUSAGE:\r\n");
		printf("p x.x set proportional gain\r\n");
        printf("i x.x set integral gain\r\n"); 
		printf("d x.x set differential gain\r\n"); 
        printf("0 x.x set FF0 gain\r\n"); 
		printf("1 x.x set FF1 gain\r\n"); 
		printf("b x.x set deadband\r\n");
		printf("m x.x set max output current(amps)\r\n"); 
		printf("f x.x set max error before drive faults(counts)\r\n");
		printf("x n   set pc command multiplier (1-16)\r\n");
        printf("t n   set # of 100us ticks/per servo calc(2-100)\r\n");
		printf("e print current encoder count\r\n"); 
		printf("l print current loop tuning values\r\n"); 
        printf("s print internal loop components\r\n");
		printf("? print this help\r\n");
	}

	reset_buffer();
}

void reset_buffer()
{
	// reset input buffer state
	rxrdy = 0;
	rxbuff[0] = 0;
	rxbuffptr = &rxbuff[0];
	putchar('>');
}
