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
* @file SensorListener.h
*
* This defines API for camerahal to get sensor events
*
*/

#ifndef ANDROID_ACTIVITY_RECOGNITION_LISTENER_H
#define ANDROID_ACTIVITY_RECOGNITION_LISTENER_H

#include <android/sensor.h>
#include <gui/Sensor.h>
#include <gui/SensorManager.h>
#include <gui/SensorEventQueue.h>
#include <utils/Looper.h>
#include <hardware/activity_recognition.h>
#include "activity_recognition_hal.h"

namespace android {

/**
 * SensorListner class - Registers with sensor manager to get sensor events
 */

class SensorLooperThread : public Thread {
    public:
        SensorLooperThread(Looper* looper)
            : Thread(false) {
            mLooper = sp<Looper>(looper);
        }
        ~SensorLooperThread() {
            mLooper.clear();
        }

        virtual bool threadLoop() {
            int32_t ret = mLooper->pollOnce(-1);
            return true;
        }

        // force looper wake up
        void wake() {
            mLooper->wake();
        }
    private:
        sp<Looper> mLooper;
};


class SensorListener : public RefBase
{
/* public - types */
public:
    struct sensorEnabled {
        bool type[2];
    };

    struct sensorEnabled enabledList[6];
/* public - functions */
public:
    SensorListener();
    ~SensorListener();
    status_t initialize();
    void setCallbacks(activity_recognition_callback_procs_t *activity_recognition_cb);
    int enableSensor(uint32_t activity_handle, uint32_t event_type, int report_latency_us);
    int disableSensor(uint32_t activity_handle, uint32_t event_type);
    int flush();
    activity_recognition_callback_procs_t* getCallBack();

/* public - member variables */
public:
    sp<SensorEventQueue> mSensorEventQueue;
/* private - member variables */
private:
    activity_recognition_callback_procs_t *mActivity_recognition_cb;
    sp<Looper> mLooper;
    sp<SensorLooperThread> mSensorLooperThread;
    Mutex mLock;
};

}

#endif
