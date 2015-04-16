/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/input.h>
#include <linux/delay.h>
#include "fpc1020_common.h"
#include "fpc1020_input.h"
#include "fpc1020_capture.h"

#define TP 1
/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
static int fpc1020_write_nav_setup(fpc1020_data_t *fpc1020);
#if TP
static int get_nav_liteon(int* pCoordX, int* pCoordY, int subarea_bits);
#endif

#endif


/* -------------------------------------------------------------------- */
/* driver constants							*/
/* -------------------------------------------------------------------- */
/* Threshold value use for detection of moving the finger */
#define FPC1020_CENTER_GRAVITY_THRESHOLD	5

#define FPC1020_KEY_POWER_UP	KEY_POWER //116
#define FPC1020_KEY_CLICK	KEY_ENTER //28

#define FPC1020_INPUT_POLL_TIME_MS	1000u  /*1000 ms*/

/* The following defines the size of the sensor */
#define FPC1020_MAX_X 10
#define FPC1020_MIN_X -10
#define FPC1020_MAX_Y 10
#define FPC1020_MIN_Y -10
#define FPC1020_MID_X (FPC1020_MIN_X + (FPC1020_MAX_X - FPC1020_MIN_X) / 2)
#define FPC1020_MID_Y (FPC1020_MIN_Y + (FPC1020_MAX_Y - FPC1020_MIN_Y) / 2)
#define FPC1020_WAKEUP_POLL_TIME_MS	50u
#define FPC1020_LONG_PRESS_TIME  0//(100u/FPC1020_WAKEUP_POLL_TIME_MS)
/* Define how much bigger the screen is compared to the sensor
** Increasing this value means that more iterations are required
** in order to make the virtual finger cross the whole screen
*/
#define SCREEN_SCALING_FACTOR 3


#define FPC1020_NAV_PERIOD 40 /* In millisecs */
#define TP_UP_AND_DOWN_PERIOD 60 /* In millisecs */


/* Set the duration of the delay before going to sleep
   after navigating.
   During this delay the sensor is still polled and will
   be percieved as more responsive.
   The longer the delay, the larger the battery consumption.
*/
#define FPC1020_FADING_TIMEOUT 3 /* In seconds */
#define FPC1020_FADING_INIT_VAL (FPC1020_FADING_TIMEOUT * 1000 / FPC1020_NAV_PERIOD)

#define CONFIG_INPUT_FPC1020_KEY_EV
#define CONFIG_INPUT_FPC1020_CLICK

#ifdef CONFIG_INPUT_FPC1020_NAV
#ifdef CONFIG_INPUT_FPC1020_KEY_EV
/* 1 = up, 2 = right, 3 = down, 4 = left */
char *key_ev[] = {"NULL", "KEY_UP", "KEY_RIGHT", "KEY_DOWN", "KEY_LEFT"};
unsigned int key_ev_val[] = {0, KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT};

#endif
#endif

/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
#ifdef CONFIG_INPUT_FPC1020_KEY_EV
int inline get_dir( int x, int y)
{
	if(y >= 0)
		if(x >= 0)
			return (y > x)?3:2;
		else
			return (y > -x)?3:4;
	else
		if(x >= 0)
			return (-y > x)?1:2;
		else
			return (-y > -x)?1:4;
}
#endif

int  fpc1020_input_init(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->input_dev = input_allocate_device();

	if (!fpc1020->input_dev) {
		dev_err(&fpc1020->spi->dev, "Input_allocate_device failed.\n");
		error  = -ENOMEM;
	}

	if (!error) {
		fpc1020->input_dev->name = FPC1020_INPUT_NAME;

		/* Set event bits according to what events we are generating */
		set_bit(EV_KEY, fpc1020->input_dev->evbit);
		set_bit(EV_ABS, fpc1020->input_dev->evbit);
		/* Set abs bits for x and y */
		set_bit(ABS_X | ABS_Y, fpc1020->input_dev->absbit);
		/* Set key bits according to what key events we are generating */
		set_bit(BTN_TOUCH, fpc1020->input_dev->keybit);
#ifdef CONFIG_INPUT_FPC1020_CLICK
		set_bit(FPC1020_KEY_CLICK,	fpc1020->input_dev->keybit);
#endif
#ifdef CONFIG_INPUT_FPC1020_KEY_EV
		set_bit(KEY_UP,	fpc1020->input_dev->keybit);
		set_bit(KEY_RIGHT,	fpc1020->input_dev->keybit);
		set_bit(KEY_LEFT,	fpc1020->input_dev->keybit);
		set_bit(KEY_DOWN,	fpc1020->input_dev->keybit);
#endif
		set_bit(FPC1020_KEY_POWER_UP, fpc1020->input_dev->keybit);

		/* Set the ranges of our "touchScreen" */
		input_set_abs_params(fpc1020->input_dev, ABS_X, FPC1020_MIN_X * SCREEN_SCALING_FACTOR, 
														FPC1020_MAX_X * SCREEN_SCALING_FACTOR, 
														0, 0);
		input_set_abs_params(fpc1020->input_dev, ABS_Y, FPC1020_MIN_Y * SCREEN_SCALING_FACTOR, 
														FPC1020_MAX_Y * SCREEN_SCALING_FACTOR, 
														0, 0);

		/* Register the input device */
		error = input_register_device(fpc1020->input_dev);
	}

	if (error) {
		dev_err(&fpc1020->spi->dev, "Input_register_device failed.\n");
		input_free_device(fpc1020->input_dev);
		fpc1020->input_dev = NULL;
	}

	return error;
}
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
void  fpc1020_input_destroy(fpc1020_data_t *fpc1020)
{
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	if (fpc1020->input_dev != NULL)
		input_free_device(fpc1020->input_dev);
}
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
void fpc1020_input_enable(fpc1020_data_t *fpc1020, bool enabled)
{
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->nav.enabled = enabled;

	return ;
}
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
int get_start_coords( int* abs_x, int* abs_y )
{
	*abs_x = 8 * (FPC1020_MID_X + *abs_x) * SCREEN_SCALING_FACTOR / 10;
	*abs_y = 8 * (FPC1020_MID_Y + *abs_y) * SCREEN_SCALING_FACTOR / 10;
	return 0;
}


int fpc1020_input_task(fpc1020_data_t *fpc1020)
{
#if TP
	int fingerPresentCounter = 0;
	static int abs_x = -1;
	static int abs_y = -1;
#endif
	int error = 0;
	int finger_raw;
	int subzones_covered = 0;
	#ifdef CONFIG_INPUT_FPC1020_KEY_EV
	int dir = 0; /* 1 = up, 2 = right, 3 = down, 4 = left */
	#endif
	int statusofkey = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020_wake_up(fpc1020);
	while (!fpc1020->worker.stop_request &&
		fpc1020->nav.enabled && (error >= 0)) {

		fpc1020_write_nav_setup(fpc1020);
		finger_raw = fpc1020_check_finger_present_raw(fpc1020);
		if(finger_raw < 0){
			error = finger_raw;
			goto err;
		}

		subzones_covered = finger_raw & 0x666; /* remove corners (0x999) */     

#if TP
		if( subzones_covered )
		{
			/* A finger is touching the sensor */
			if( fingerPresentCounter == 0 )
			{
				int dx;
				int dy;
				/* A finger was detected */
				dev_dbg(&fpc1020->spi->dev, "Finger present!\n" );
				input_report_key( fpc1020->input_dev, FPC1020_KEY_CLICK, 1 );
				input_sync( fpc1020->input_dev );

				++fingerPresentCounter;

				/* set starting point for navigation movement */
				if( get_nav_liteon( &dx, &dy, finger_raw ) < 0 )
				{
					/* The call above failed for some reason */
					/* Fall back to zero and retry the next lap */
					abs_x = FPC1020_MID_X;
					abs_y = FPC1020_MID_Y;
				}
				else
				{
					/* Make sure we are navigating in some direction 
					 * Otherwise a long press will be generated 
					 * when the screen is woken up 
					*/
					if( dx || dy )
					{
						/* Set "finger down" position on screen */
						abs_x = dx;
						abs_y = dy;
						get_start_coords( &abs_x, &abs_y );

						input_report_key( fpc1020->input_dev, BTN_TOUCH, 1 );
						input_report_abs( fpc1020->input_dev, ABS_X, abs_x );
						input_report_abs( fpc1020->input_dev, ABS_Y, abs_y );
						input_sync( fpc1020->input_dev );
						/* Send a movement directly to avoid long press */
						abs_x -= dx;
						abs_y -= dy;
						input_report_abs( fpc1020->input_dev, ABS_X, abs_x );
						input_report_abs( fpc1020->input_dev, ABS_Y, abs_y );
						input_sync( fpc1020->input_dev );
					}
				}
			}
			else
			{
				/* The finger is still present so we should navigate */
				int dx;
				int dy;
				int error = get_nav_liteon( &dx, &dy, finger_raw );
				if( !(error < 0) )
				{
					/* If we reach the screen edge, lift "finger" and start over */
					if( (abs_x - dx) >= (FPC1020_MAX_X * SCREEN_SCALING_FACTOR) ||
						(abs_x - dx) <= (FPC1020_MIN_X * SCREEN_SCALING_FACTOR) ||
						(abs_y - dy) >= (FPC1020_MAX_Y * SCREEN_SCALING_FACTOR) ||
						(abs_y - dy) <= (FPC1020_MIN_Y * SCREEN_SCALING_FACTOR) )
					{
						dev_dbg( &fpc1020->spi->dev, "Near edge abs(%d,%d), d(%d,%d)\n",
						         abs_x, abs_y, dx, dy );
						abs_x = dx;
						abs_y = dy;
						get_start_coords( &abs_x, &abs_y );
						dev_dbg( &fpc1020->spi->dev, "Setting new coords (%d,%d)\n",
						         abs_x, abs_y );
						input_report_key( fpc1020->input_dev, BTN_TOUCH, 0 );
						input_sync( fpc1020->input_dev );
						msleep(TP_UP_AND_DOWN_PERIOD);
#ifdef CONFIG_INPUT_FPC1020_KEY_EV
						if(statusofkey == 0)
						{
							statusofkey = 1;
							dir =get_dir(dx, dy);
							dev_dbg( &fpc1020->spi->dev, "--> %s(%d)\n", key_ev[dir], key_ev_val[dir]);
							input_report_key( fpc1020->input_dev, key_ev_val[dir], 1 ); 
							input_sync( fpc1020->input_dev );
						}
						
						msleep(TP_UP_AND_DOWN_PERIOD);
#endif
						input_report_key( fpc1020->input_dev, BTN_TOUCH, 1 );
					}
					else
					{
						/* Calculate new position of "finger" */
						dev_dbg( &fpc1020->spi->dev, "Moving (%d,%d)\n",
								dx, dy );
						abs_x -= dx;
						abs_y -= dy;
					}

					dev_dbg(&fpc1020->spi->dev, "Sending finger position (%d,%d)\n", 
							abs_x, abs_y );
					/* Report new coordinates */
					input_report_abs( fpc1020->input_dev, ABS_X, abs_x );
					input_report_abs( fpc1020->input_dev, ABS_Y, abs_y );
					input_sync( fpc1020->input_dev );
				}
			}
		}
		else
		{
			if(fingerPresentCounter > 0)
			{
				/* Lift "finger" from screen */
				input_report_key( fpc1020->input_dev, FPC1020_KEY_CLICK, 0 );
				input_sync( fpc1020->input_dev );

				input_report_key( fpc1020->input_dev, BTN_TOUCH, 0 );
				input_sync( fpc1020->input_dev );

				if(statusofkey == 1)
				{
					statusofkey = 0;
					input_report_key( fpc1020->input_dev, key_ev_val[dir], 0 ); 
					input_sync( fpc1020->input_dev );
				}
			}

			fingerPresentCounter = 0;
		}

#else

		if( subzones_covered ){
			input_report_key( fpc1020->input_dev, FPC1020_KEY_CLICK, 1 );
			input_sync( fpc1020->input_dev );
		}
		else{
			input_report_key( fpc1020->input_dev, FPC1020_KEY_CLICK, 0 );
			input_sync( fpc1020->input_dev );
		}

#endif
			msleep(FPC1020_NAV_PERIOD);
	}

err:
	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s FAILED (%d)\n",
			__func__, error);
	}

	fpc1020->nav.enabled = false;
	atomic_set(&fpc1020->taskstate, fp_UNINIT);

	return error;
}
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
static int fpc1020_write_nav_setup(fpc1020_data_t *fpc1020)
{
	return fpc1020_write_sensor_setup(fpc1020);
}
#endif


/* -------------------------------------------------------------------- */



#ifdef CONFIG_INPUT_FPC1020_NAV

#if TP
int get_nav_liteon(int* pCoordX, int* pCoordY, int subarea_bits)
{

	int xThr = 0;
	int yThr = 0;
	int error = 0;
//	int xMapTable[16] = {0, -3, -1, -4, 1, -2, 0, -3, 3, 0, 2, -1, 4, 1, 3, 0};
//	int yMapTable[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

	int xMapTable[16] = {0, 0, -3, -3, 3, 3, 0, 0, 0, 0, -3, -3, 3, 3, 0, 0};
	int yMapTable[16] = {0, 0, 2, 2, 2, 2, 4, 4, 0, 0, 2, 2, 2, 2, 4, 4};

	const int MaxValueX = 10;
	const int MaxValueY = 10;

	int fingerPresenceB0 = subarea_bits & 0xffu;
	int fingerPresenceB1 = (subarea_bits >> 8) & 0xffu;

	int xG = 0;
	int yG = 0;
	int norm = 0;

	// First the x direction
	xG += xMapTable[fingerPresenceB0 & 0x0f];
	xG += xMapTable[(fingerPresenceB0 >> 4) & 0x0f];
	xG += xMapTable[fingerPresenceB1 & 0x0f];

	// Then the y direction
	yG += yMapTable[fingerPresenceB0 & 0x0f];
	//Find the number of subare containing finger
	norm += yG;
	// Then the y direction for the negative part
	yG -= yMapTable[fingerPresenceB1 & 0x0f];

	//Find the number of subare containing finger
	norm += yMapTable[(fingerPresenceB0 >> 4) & 0x0f];
	norm += yMapTable[fingerPresenceB1 & 0x0f];

	if (norm > 0)
	{

		// 3 is the maximum value of the weighting coefficient
		xG = (10 * xG / (norm * 3));
		yG = (10 * yG / norm);

		//Avoid small movement to be reported
		if (abs(xG) < xThr)
		{
			xG = 0;
		}
		//Avoid small movement to be reported
		if (abs(yG) < yThr)
		{
			yG = 0;
		}

	}
	else
	{
		xG = 0;
		yG = 0;
	}


	// Check validity range of the center of gravity
	if (abs(xG) > MaxValueX)
	{
		error = 1;
	}
	if (abs(yG) > MaxValueY)
	{
		error = 1;
	}

	*pCoordX = yG;
	*pCoordY = xG;

	return error;
}
#endif


#endif
#ifdef CONFIG_INPUT_FPC1020_WAKEUP
/* -------------------------------------------------------------------- */
int fpc1020_wakeup_task(fpc1020_data_t *fpc1020)
{
	int error = -1;
	int i = 0;
	fpc1020_wake_up(fpc1020);
	fpc1020_write_sensor_setup(fpc1020);
	if(fpc1020_sleep(fpc1020, false) < 0)
	{
		fpc1020_wake_up(fpc1020);
		fpc1020_sleep(fpc1020, false);
	}
	atomic_set(&fpc1020->taskstate, fp_LONGPRESSWAKEUP);
	
while(1){
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);
	
	error = fpc1020_wait_for_irq(fpc1020,
						FPC1020_DEFAULT_IRQ_TIMEOUT_MS);
	if (error < 0) {
		if (fpc1020->worker.stop_request){
			dev_dbg(&fpc1020->spi->dev,"%s stop Request \n", __func__);
			return -EINTR;
		}
		if (error == -ETIMEDOUT){
			dev_dbg(&fpc1020->spi->dev,"%s ETIMEDOUT \n", __func__);
			continue;
		}
		
		goto err;
	}

	wake_lock(&fpc1020->fp_wake_lock);
	msleep(50);
	fpc1020_wake_up(fpc1020);
	wake_unlock(&fpc1020->fp_wake_lock);

	dev_dbg(&fpc1020->spi->dev, "Finger down\n");

	for(i = 0 ; i < FPC1020_LONG_PRESS_TIME ;i++)
	{
		error = fpc1020_check_finger_present_sum(fpc1020);
		//printk("%s: fpc1020_check_finger_present_sum() = %d\n", __func__, error);
		if (error < 0)
			goto err;
			
		if (error > FPC1020_FINGER_DOWN_THRESHOLD)
			msleep(FPC1020_WAKEUP_POLL_TIME_MS);
		else
			break;
	}
	
	error = fpc1020_check_finger_present_sum(fpc1020);
	printk("%s: fpc1020_check_finger_present_sum() = %d\n", __func__, error);
	if(i == FPC1020_LONG_PRESS_TIME && error > FPC1020_FINGER_DOWN_THRESHOLD)
	{
		printk("%s: Long press!\n", __func__);

		input_report_key(fpc1020->input_dev,
						FPC1020_KEY_POWER_UP, 1);
		input_sync(fpc1020->input_dev);
			//msleep(FPC1020_WAKEUP_POLL_TIME_MS);
		input_report_key(fpc1020->input_dev,
						FPC1020_KEY_POWER_UP, 0);
		input_sync(fpc1020->input_dev);
	}

	error = FPC1020_FINGER_DOWN_THRESHOLD + 1;
	while (!fpc1020->worker.stop_request && error > FPC1020_FINGER_DOWN_THRESHOLD){
		msleep(FPC1020_WAKEUP_POLL_TIME_MS);
		error = fpc1020_check_finger_present_sum(fpc1020);
	}

	msleep(100); // reduce the press again
	if(fpc1020_sleep(fpc1020, false) < 0)
	{
		fpc1020_wake_up(fpc1020);
		fpc1020_sleep(fpc1020, false);
	}
	atomic_set(&fpc1020->taskstate, fp_LONGPRESSWAKEUP);

}
err:
	dev_err(&fpc1020->spi->dev,"%s error = %d \n", __func__,error);
	return 0;
}

#endif
