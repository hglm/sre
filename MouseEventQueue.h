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
* Small changes made by Harm Hanemaaijer in 2013 to integrate into SRE front-end.
**
*
* A class for obtaining mouse events in a linux console environment.
* 
* It is implemented as a class with static methods (pseudo-singleton)
*
* The class must be initialize()d before use, and terminate()d after use.
* The screen dimensions must be set, for setting limits for mouse coordinates.
* 
* The class implements an event queue. You should first check if an event is
* available with isEventAvailable(); if it is, you can retrieve it with getEvent().
* 
* The retrieved mouse event has features commonly found in GUI libraries, such
* as an event type (Press, Release, Move, Passive move), screen coordinates,
* active button, and a button mask for knowing the state of all buttons.
* 
*/
#ifndef MouseEventQueue_h
#define MouseEventQueue_h

// The mouse event that can be obtained from the queue.
struct MouseEvent
{
	MouseEvent();

	// mouse event types
	enum {
		Empty    = 0,	// no event was available but getEvent was called anyway.
		Passive  = 1,	// the mouse has moved with no buttons pressed
		Move     = 2,	// the mouse has moved with any button pressed
		Press    = 4,	// a mouse button has been pressed
		Release  = 8,	// a mouse button has been released
	};
	
	// mouse buttons
	enum {
		NoButton     = 0x00,
		LeftButton   = 0x01,
		RightButton  = 0x02,
		MiddleButton = 0x04,
	};
	
	short type;			// the type of this event
	short button;		// the button referred by this event (or NoButton for move events)
	short buttonMask;	// for all event types, the bit mask representing pressed buttons.
	
	short x;			// horizontal position
	short y;			// vertical position
	double date;			// date in seconds of the event
};


class MouseEventQueue
{
public:
	// initialize must be called before using the queue for the first time
	static void initialize();
	
	// screen size must be set (it defaults to 640x480)
	static void setScreenSize(int width, int height);
	
	// terminate must be called when the queue is no longer going to be used.
	static void terminate();

	
	// call this to know if an event is available
	static bool isEventAvailable();
	
	// obtain a mouse event. a dummy event is returned if none was available
	static MouseEvent getEvent();
	
	// debug: dump a event to stdout
	static void dumpEvent(const MouseEvent& evt);

        static void setPosition(int x, int y);
};

#endif
