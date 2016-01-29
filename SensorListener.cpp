/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
* @file SensorListener.cpp
*
* This file listens and propogates sensor events to CameraHal.
*
*/

#include "SensorListener.h"

#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <hardware/activity_recognition.h>
#include "activity_recognition_hal.h"

namespace android {

static int event_type_to_index(int event_type) {
    switch (event_type) {
        case ACTIVITY_EVENT_ENTER:
            return 0;
        case ACTIVITY_EVENT_EXIT:
            return 1;
    }
    // unknow type
    return -1;
}

static char* activity_to_string(uint32_t activity_handle) {
    switch (activity_handle) {
        case IN_VEHICLE:
            return ACTIVITY_TYPE_IN_VEHICLE;
        case ON_BICYCLE:
            return ACTIVITY_TYPE_ON_BICYCLE;
        case WALKING:
            return ACTIVITY_TYPE_WALKING;
        case RUNNING:
            return ACTIVITY_TYPE_RUNNING;
        case STILL:
            return ACTIVITY_TYPE_STILL;
        case TILTING:
            return ACTIVITY_TYPE_TILTING;
    }

    return NULL;
}

static int string_to_activity(const char* type_string) {
    if (strcmp(type_string, ACTIVITY_TYPE_IN_VEHICLE))
        return IN_VEHICLE;
    else if (strcmp(type_string, ACTIVITY_TYPE_ON_BICYCLE))
        return ON_BICYCLE;
    else if (strcmp(type_string, ACTIVITY_TYPE_WALKING))
        return WALKING;
    else if (strcmp(type_string, ACTIVITY_TYPE_RUNNING))
        return RUNNING;
    else if (strcmp(type_string, ACTIVITY_TYPE_STILL))
        return STILL;
    else if (strcmp(type_string, ACTIVITY_TYPE_TILTING))
        return TILTING;

    return -1;
}

static int handler_to_type(uint32_t activity_handle) {
    SensorManager& mgr(SensorManager::getInstanceForPackage(android::String16()));
    Sensor const* const* list;
    char *activity_handle_string;
    ssize_t count = mgr.getSensorList(&list);

    activity_handle_string = activity_to_string(activity_handle);
    for (int i = 0; i < count; i++) {
        if (!strcmp(list[i]->getStringType().string(), activity_handle_string)) {
            return list[i]->getType();
        }
    }

    return -1;
}

static int type_to_handler(uint32_t sensor_type) {
    SensorManager& mgr(SensorManager::getInstanceForPackage(android::String16()));
    Sensor const* const* list;
    char *activity_handle_string;
    ssize_t count = mgr.getSensorList(&list);

    for (int i = 0; i < count; i++) {
        if (list[i]->getType() == sensor_type)
            return string_to_activity(list[i]->getStringType().string());
    }

    return -1;
}

static int sensor_events_listener(int fd, int events, void* data)
{
    SensorListener* listener = (SensorListener*) data;
    ssize_t num_sensors;
    int event_size = 0;
    ASensorEvent sen_events[8];
    activity_event_t report_event[8];
    while ((num_sensors = listener->mSensorEventQueue->read(sen_events, 8)) > 0) {
        for (int i = 0; i < num_sensors; i++) {
            if (sen_events[i].type == SENSOR_TYPE_META_DATA) {
                report_event[event_size].event_type = ACTIVITY_EVENT_FLUSH_COMPLETE;
                report_event[event_size].activity = 0;
                report_event[event_size].timestamp = 0;
                event_size++;
                continue;
            }

            int activity = type_to_handler(sen_events[i].type);
            int type = event_type_to_index(sen_events[i].u64.data[0]);

            if (activity < 0 || type < 0)
                continue;

            if (listener->enabledList[activity].type[type]) {
                if (type)
                    report_event[event_size].event_type = ACTIVITY_EVENT_EXIT;
                else
                    report_event[event_size].event_type = ACTIVITY_EVENT_ENTER;
                report_event[event_size].activity = activity;
                report_event[event_size].timestamp = sen_events[i].timestamp;
                event_size++;
            }
        }

        if (event_size > 0 && listener->getCallBack() != NULL) {
            listener->getCallBack()->activity_callback(listener->getCallBack(), report_event, event_size);
            event_size = 0;
        }
    }

    return 1;
}
/****** public - member functions ******/
SensorListener::SensorListener() {

    mActivity_recognition_cb = NULL;
    mSensorEventQueue = NULL;
    mSensorLooperThread = NULL;
    memset(enabledList, 0, sizeof(struct sensorEnabled) * 6);

}

SensorListener::~SensorListener() {

    if (mSensorLooperThread.get()) {
        // 1. Request exit
        // 2. Wake up looper which should be polling for an event
        // 3. Wait for exit
        mSensorLooperThread->requestExit();
        mSensorLooperThread->wake();
        mSensorLooperThread->join();
        mSensorLooperThread.clear();
        mSensorLooperThread = NULL;
    }

    if (mLooper.get()) {
        mLooper->removeFd(mSensorEventQueue->getFd());
        mLooper.clear();
        mLooper = NULL;
    }

}

status_t SensorListener::initialize() {
    status_t ret = NO_ERROR;
    SensorManager& mgr(SensorManager::getInstanceForPackage(android::String16()));


    sp<Looper> mLooper;

    mSensorEventQueue = mgr.createEventQueue();
    if (mSensorEventQueue == NULL) {
        ret = NO_INIT;
        goto out;
    }

    mLooper = new Looper(false);
    mLooper->addFd(mSensorEventQueue->getFd(), 0, ALOOPER_EVENT_INPUT, sensor_events_listener, this);

    if (mSensorLooperThread.get() == NULL)
            mSensorLooperThread = new SensorLooperThread(mLooper.get());

    if (mSensorLooperThread.get() == NULL) {
        ret = NO_MEMORY;
        goto out;
    }

    ret = mSensorLooperThread->run("sensor looper thread", PRIORITY_URGENT_DISPLAY);
    if (ret == INVALID_OPERATION){
    } else if (ret != NO_ERROR) {
        goto out;
    }

 out:
    return ret;
}

void SensorListener::setCallbacks(activity_recognition_callback_procs_t *activity_recognition_cb) {

    if (activity_recognition_cb) {
        mActivity_recognition_cb = activity_recognition_cb;
    }
}

int SensorListener::enableSensor(uint32_t activity_handle, uint32_t event_type, int report_latency_us) {
    Sensor const* sensor;
    SensorManager& mgr(SensorManager::getInstanceForPackage(android::String16()));
    int index = event_type_to_index(event_type);

    Mutex::Autolock lock(&mLock);

    for (int i = 0; i < 2; i++) {
        if (enabledList[activity_handle].type[i] == true) {
            enabledList[activity_handle].type[index] = true;
            return 0;
        }
    }

    sensor = mgr.getDefaultSensor(handler_to_type(activity_handle));
    mSensorEventQueue->enableSensor(sensor->getHandle(), ms2ns(100), report_latency_us, false);
    enabledList[activity_handle].type[index] = true;

    return 0;
}

int SensorListener::disableSensor(uint32_t activity_handle, uint32_t event_type) {
    Sensor const* sensor;
    SensorManager& mgr(SensorManager::getInstanceForPackage(android::String16()));
    int index = event_type_to_index(event_type);

    Mutex::Autolock lock(&mLock);

    if (index < 0) {
        return index;
    }

    enabledList[activity_handle].type[index] = false;

    for (int i = 0; i < 2; i++) {
        if (enabledList[activity_handle].type[i] == true) {
            return 0;
        }
    }

    sensor = mgr.getDefaultSensor(handler_to_type(activity_handle));
    mSensorEventQueue->disableSensor(sensor->getHandle());

    return 0;
}

int SensorListener::flush() {
    mSensorEventQueue->flush();

    return 0;
}

activity_recognition_callback_procs_t *SensorListener::getCallBack() {
    return mActivity_recognition_cb;
}

}
