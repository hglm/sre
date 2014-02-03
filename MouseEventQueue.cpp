/*
* Copyright (c) 2012 VirtualCode.es / David Crespo
*
* Licensed to the Apache Software Foundation (ASF) under one or more
* contributor license agreements.  See the NOTICE file distributed with
* this work for additional information regarding copyright ownership.
* The ASF licenses this file to You under the Apache License, Version 2.0
* (the "License"); you may not use this file except in compliance with
* the License.  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* Small changes made by Harm Hanemaaijer in 2013 to integrate into SRE framebuffer
* platform back-ends.
**
*
* A class for obtaining mouse events in a linux console environment.
* 
*/
#include "MouseEventQueue.h"
#include <SafeQueue.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// raw event type, it is not affected by limits
// x and y coordinates are deltas (increments)
struct DeltaMouseEvent
{
	short dx;	// horizontal movement delta
	short dy;	// vertical movement delta
	bool lbp;	// left button pressed flag
	bool rbp;	// right button pressed flag
	bool mbp;	// middle button pressed flag
	
	DeltaMouseEvent() : dx(0), dy(0), lbp(false), rbp(false), mbp(false) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// static variables for mouse state
//
// current mouse position
static int currX;
static int currY;
// current button mask
static int currMask;
// limits for x, y;
static int xmin, xmax;
static int ymin, ymax;
static double currDate;

////////////////////////////////////////////////////////////////////////////////////////////////////
// queue logic
//
// queue initialization state
static bool inited = false;
// event queue (thread-safe)
static SafeQueue<MouseEvent> evt_queue;
// reader thread object
static pthread_t met;
// reader thread procedure
static void *metProc(void* arg);
// reader thread run check flag
static bool metProcRunning = false;

////////////////////////////////////////////////////////////////////////////////////////////////////
// MouseEvent constructor
MouseEvent::MouseEvent()
{
	// type and button are default values
	type = MouseEvent::Empty;
	button = MouseEvent::NoButton;
	// button mask and position initialized from current values
	buttonMask = currMask;
	x = currX;
	y = currY;
        date = currDate;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// static function prototypes
static void processDeltaMouseEvent(DeltaMouseEvent& delta);
static void dumpDeltaMouseEvent(DeltaMouseEvent& delta);

////////////////////////////////////////////////////////////////////////////////////////////////////
// reader thread procedure: reads mouse events, processes them, and dispatches them.
static void *metProc(void* arg)
{
	// we will read binary input directly from mouse device
	char mousedata[3];
	FILE *fmouse = fopen("/dev/input/mice","rb");
	
	// check if we can open the device
	if (fmouse == NULL) {
		printf("MouseEventQueue: cannot open /dev/input/mice\n");
		pthread_exit(NULL);
	}
	printf("MouseEventQueue: opened /dev/input/mice\n");
	
	// main loop
	metProcRunning = true;
	while(metProcRunning) {
		DeltaMouseEvent evt;
		// read 3 bytes from device (blocking call if nothing to be read)
		fread(mousedata,sizeof(char),3,fmouse);
		// first byte: bitfield, lowest 3 bits represent mouse buttons
		evt.lbp = (mousedata[0]&1)>0 ? true : false;
		evt.rbp = (mousedata[0]&2)>0 ? true : false;
		evt.mbp = (mousedata[0]&4)>0 ? true : false;
		// second byte represents horizontal delta
		evt.dx  = mousedata[1];
		evt.dx = evt.dx < 128 ? evt.dx : evt.dx - 256;
		// second byte represents vertical delta
		evt.dy  = mousedata[2];
		evt.dy = evt.dy < 128 ? evt.dy : evt.dy - 256;
		// process event
		processDeltaMouseEvent(evt);
	}
	
	// clean the house
	fclose(fmouse);
	pthread_exit(NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// process function: generates one or more events from a delta event.
static void processDeltaMouseEvent(DeltaMouseEvent& delta)
{
	// Get the current time.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	currDate = (double)tv.tv_sec + (double)tv.tv_usec / 1000000;

	// update horizontal position from delta event, taking limits into account
	currX += delta.dx;
	if (currX < xmin) currX = xmin;
	if (currX > xmax) currX = xmax;

	// update vertical position from delta event, taking limits into account
	currY += delta.dy;
	if (currY < ymin) currY = ymin;
	if (currY > ymax) currY = ymax;
	
	// remember button mask, before changing it
	int oldMask = currMask;
	
	// update button mask from delta event
	if (delta.lbp) currMask |=  MouseEvent::LeftButton;
	else           currMask &= ~MouseEvent::LeftButton; 
	if (delta.rbp) currMask |=  MouseEvent::RightButton;
	else           currMask &= ~MouseEvent::RightButton; 
	if (delta.mbp) currMask |=  MouseEvent::MiddleButton;
	else           currMask &= ~MouseEvent::MiddleButton; 
	
	// if mask has not changed, no buttons have been pressed or released.
	if (oldMask == currMask)
	{
		// create default event, which has current position and button mask implicitly set.
		MouseEvent evt;
		// determine if event type is passive (no buttons pressed) or move (any button pressed)
		if (currMask == 0) evt.type = MouseEvent::Passive;
		else               evt.type = MouseEvent::Move;
		// enqueue event and exit this function
		evt_queue.push(evt);
		return;
	}
	
	// if execution reaches here, one or _more_ mouse buttons have changed.
	
	bool oldLeftState    = oldMask  & MouseEvent::LeftButton   ? true : false;
	bool oldRightState   = oldMask  & MouseEvent::RightButton  ? true : false;
	bool oldMiddleState  = oldMask  & MouseEvent::MiddleButton ? true : false;
	bool currLeftState   = currMask & MouseEvent::LeftButton   ? true : false;
	bool currRightState  = currMask & MouseEvent::RightButton  ? true : false;
	bool currMiddleState = currMask & MouseEvent::MiddleButton ? true : false;

	// we will start from old mask, changing it bit by bit until obtaining desired mask.
	currMask = oldMask;
	
	// check if left button has changed
	if (oldLeftState != currLeftState)
	{
		// transform one bit of mask
		if (currLeftState) currMask |=  MouseEvent::LeftButton;
		else               currMask &= ~MouseEvent::LeftButton;
		// create default event, which has current position and button mask implicitly set.
		MouseEvent evt;
		// set event button to left
		evt.button = MouseEvent::LeftButton;
		// determine if event type is press (current state is pressed) or release
		if (currLeftState) evt.type = MouseEvent::Press;
		else               evt.type = MouseEvent::Release;
		// enqueue event
		evt_queue.push(evt);
	}

	// check if right button has changed
	if (oldRightState != currRightState)
	{
		// transform one bit of mask
		if (currRightState) currMask |=  MouseEvent::RightButton;
		else                currMask &= ~MouseEvent::RightButton;
		// create default event, which has current position and button mask implicitly set.
		MouseEvent evt;
		// set event button to right
		evt.button = MouseEvent::RightButton;
		// determine if event type is press (current state is pressed) or release
		if (currRightState) evt.type = MouseEvent::Press;
		else                evt.type = MouseEvent::Release;
		// enqueue event
		evt_queue.push(evt);
	}

	// check if middle button has changed
	if (oldMiddleState != currMiddleState)
	{
		// transform one bit of mask
		if (currMiddleState) currMask |=  MouseEvent::MiddleButton;
		else                 currMask &= ~MouseEvent::MiddleButton;
		// create default event, which has current position and button mask implicitly set.
		MouseEvent evt;
		// set event button to middle
		evt.button = MouseEvent::MiddleButton;
		// determine if event type is press (current state is pressed) or release
		if (currMiddleState) evt.type = MouseEvent::Press;
		else                 evt.type = MouseEvent::Release;
		// enqueue event
		evt_queue.push(evt);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////

// initialize must be called before using the queue for the first time
void MouseEventQueue::initialize()
{
	// check for duplicate initialization
	if (inited) {
		printf("MouseEventQueue::initialize: already inited\n");
		return;
	}
	
	// default screen size
	setScreenSize(640, 480);
	currMask = 0;
	
	// create reader thread
	int res = pthread_create(&met, NULL, metProc, NULL);
	if (res) {
		printf("MouseEventQueue::initialize ERROR; return code from pthread_create() is %d\n", res);
		return;
	}
	
	// mark as initialized
	inited = true;
	printf("MouseEventQueue::initialize: successfully inited\n");
}

// screen size, for limiting mouse position
void MouseEventQueue::setScreenSize(int width, int height)
{
	xmin = 0;
	ymin = 0;
	xmax = width-1;
	ymax = height-1;
	currX = width / 2;
	currY = height / 2;
}

// terminate must be called when the queue is no longer going to be used.
void MouseEventQueue::terminate()
{
	if (!inited) return;
	metProcRunning = false;
	inited = false;
}

// call this to know if an event is available
bool MouseEventQueue::isEventAvailable()
{
	return !evt_queue.isEmpty();
}

// obtain a mouse event, if available
MouseEvent MouseEventQueue::getEvent()
{
	// if there is an event in the queue, extract it and return it.
	if (!evt_queue.isEmpty())
		return evt_queue.pop();
		
	// empty queue, return default empty event (with correct position and button mask)
	return MouseEvent();
}

static void dumpDeltaMouseEvent(DeltaMouseEvent& delta)
{
	printf("DeltaMouseEvent [%c%c%c]     x = %5d     y = %5d\n",
		delta.lbp ? 'L' : ' ',
		delta.mbp ? 'M' : ' ',
		delta.rbp ? 'R' : ' ',
		(int)delta.dx, (int)delta.dy);
}

void MouseEventQueue::dumpEvent(const MouseEvent& evt)
{
	printf("MouseEvent [Type:%s] [Mask:%c%c%c] [Button:%c] [x:%4d] [y:%4d]\n",
		(evt.type == MouseEvent::Move ? " Move  " :
			(evt.type == MouseEvent::Press ? " Press " :
				(evt.type == MouseEvent::Release ? "Release" :
					(evt.type == MouseEvent::Passive ? "Passive" :
					"Invalid")
				)
			)
		),
		evt.buttonMask & MouseEvent::LeftButton ? 'L' : ' ',
		evt.buttonMask & MouseEvent::MiddleButton ? 'M' : ' ',
		evt.buttonMask & MouseEvent::RightButton ? 'R' : ' ',
		(evt.button == MouseEvent::LeftButton ? 'L' :
			(evt.button == MouseEvent::MiddleButton ? 'M' :
				(evt.button == MouseEvent::RightButton ? 'R' :
				' ')
			)
		),
		(int)evt.x, (int)evt.y
	);
}

void MouseEventQueue::setPosition(int x, int y) {
    currX = x;
    currY = y;
}
