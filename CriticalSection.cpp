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
* Simple C++ implementation of a critical section, for protecting sections
* of code from concurrent execution.
*
* Implementation is just a wrapper around the mutex provided by pthread.
*
*/
#include <pthread.h>
#include "CriticalSection.h"

struct CSImpl
{
	pthread_mutex_t mutex;
};

CriticalSection::CriticalSection()
{
	im = new CSImpl;
	
	pthread_mutex_init(&im->mutex, 0);
}

CriticalSection::~CriticalSection()
{
	pthread_mutex_destroy(&im->mutex);

	delete im;
	im = 0;
}
	
void CriticalSection::enter() const
{
	pthread_mutex_lock(&im->mutex);
}

void CriticalSection::leave() const
{
	pthread_mutex_unlock(&im->mutex);
}
	
bool CriticalSection::tryEnter() const
{
	if (0 == pthread_mutex_trylock(&im->mutex))
		return true;
	else
		return false;
}

