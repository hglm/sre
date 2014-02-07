/*

Copyright (c) 2014 Harm Hanemaaijer <fgenfb@yahoo.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/

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
**
* 
* A thread-safe queue for generic data types. 
* Its main purpose is serving for safe inter-thread communication.
*
* Use the push() member function for inserting an element in the queue.
* Use the pop() member function for extracting an element from the queue.
* 
* The queue size can be infinite (by default), or have a maximum size.
* If queue size is limited, and has reached its maximum, then a push()
* operation forces removal of LRU element.

* Data type for queue may be a pointer (or not).
* When using pointers:
* - If an object inserted into the queue was created with new() (for example,
*   a packet of data read from a device, in a "source" thread) it should be
*   delete()d after being extracted from the queue, in the "destination" thread.
* - When the queue size is limited, pointer objects are automatically destroyed
*   using delete(), thanks to the IsPointer helper class.
* 
*/

#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H

#include <deque>
#include <iostream>

#include "CriticalSection.h"

// A smart C++ template contributed by Fermin Gonzalez.
// Its purpose is to determinate if some type is a pointer or not.
// Check the const field 'value' to know.
// The 'kill' member funcion deletes the object if it is a pointer.
template<typename T>
struct IsPointer
{
	static const bool value = false;
	static void kill(T) {}
};

template<typename T>
struct IsPointer<T *>
{
	static const bool value = true;
	static void kill(T *elem) {
		if(elem) {
			delete elem;
			elem = 0;
		}
	}
};

// The template thread safe queue implementation.
template<typename T>
class SafeQueue
{
public:

	// Constructor
	SafeQueue()
	{
		maxSize_ = -1;
	}

	// Set maximum size for the queue (negative values mean no limits)
	void setMaxSize(int theMaximumSize = -1)
	{
		critSect_.enter();
		maxSize_ = theMaximumSize;
		critSect_.leave();
	}

	// Destructor; destroys any elements pending to be extracted.
	~SafeQueue()
	{
		critSect_.enter();
		if(!queue_.empty())	{
			if(IsPointer<T>::value)	{
				while(!queue_.empty()) {
					T elem = pop();
					IsPointer<T>::kill(elem);
				}
			}
			queue_.clear();
		}
		maxSize_ = -1;
		critSect_.leave();
	}

	// Size of queue (number of elements to be extracted)
	int size() const
	{
		int res;
		critSect_.enter();
		res = (int) queue_.size();
		critSect_.leave();
		return res;
	}

	// Emptiness test for convenience
	bool isEmpty() const
	{
		bool res;
		critSect_.enter();
		res = queue_.empty();
		critSect_.leave();
		return res;
	}

	// Insert an element into queue
	void push(T elem)
	{
		critSect_.enter();

		if(int(queue_.size()) == maxSize_)
		{
			T remainingElem = queue_.back();
			queue_.pop_back();
			if(IsPointer<T>::value)
				IsPointer<T>::kill(remainingElem);
		}

		queue_.push_front(elem);

		critSect_.leave();
	}

	// Extract an element from queue
	T pop()
	{
		T res;
		critSect_.enter();
		if(!queue_.empty())
		{
			res = queue_.back();
			queue_.pop_back();
		}
		critSect_.leave();
		return res;
	}

	typedef typename std::deque<T>::value_type value_type;

private:
	CriticalSection critSect_;
	std::deque<T> queue_;
	int maxSize_;
};

#endif // SAFEQUEUE_H
