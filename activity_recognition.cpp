/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <cutils/log.h>
#include <hardware/hardware.h>
#include <hardware/activity_recognition.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

enum {
    IN_VEHICLE = 0,
    ON_BICYCLE,
    WALKING,
    RUNNING,
    STILL,
    TILTING,
};

static activity_recognition_callback_procs_t activity_recognition_callback;

void activity_recognition_register_callback(const struct activity_recognition_device* dev,
        const activity_recognition_callback_procs_t* callback)
{
    activity_recognition_callback = *callback;
}

int activity_recognition_enable(const struct activity_recognition_device* dev,
        uint32_t activity_handle, uint32_t event_type, int64_t max_batch_report_latency_ns)
{

    return 0;
}

int activity_recognition_disable(const struct activity_recognition_device* dev,
        uint32_t activity_handle, uint32_t event_type)
{

    return 0;
}

int activity_recognition_flush(const struct activity_recognition_device* dev)
{

    return 0;
}

static int close_activity_recognition(hw_device_t *dev)
{
    free(dev);
    return 0;
}

static const char *sSupportActivitiesList[] = {
    ACTIVITY_TYPE_IN_VEHICLE,
    ACTIVITY_TYPE_ON_BICYCLE,
    ACTIVITY_TYPE_WALKING,
    ACTIVITY_TYPE_RUNNING,
    ACTIVITY_TYPE_STILL,
    ACTIVITY_TYPE_TILTING,
};

static int get_activity_recognition_list(struct activity_recognition_module* module,
            char const* const* *activity_list)
{
    *activity_list = sSupportActivitiesList;
	return ARRAY_SIZE(sSupportActivitiesList);
}

static int open_activity_recognition(const struct hw_module_t* module, const char* id,
                        struct hw_device_t** device)
{
    if (strcmp(id, ACTIVITY_RECOGNITION_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    activity_recognition_device_t *dev = (activity_recognition_device_t*)malloc(sizeof(activity_recognition_device_t));

    if (!dev)
        return -ENOMEM;

    memset(dev, 0, sizeof(activity_recognition_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version  = ACTIVITY_RECOGNITION_API_VERSION_0_1;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close    = close_activity_recognition;

    dev->register_activity_callback = activity_recognition_register_callback;
    dev->enable_activity_event = activity_recognition_enable;
    dev->disable_activity_event = activity_recognition_disable;
    dev->flush = activity_recognition_flush;

    *device = &dev->common;

    return 0;
}

static struct hw_module_methods_t activity_recognition_module_methods = {
    open: open_activity_recognition
};

activity_recognition_module_t HAL_MODULE_INFO_SYM = {
    common: {
            tag: HARDWARE_MODULE_TAG,
            version_major: 1,
            version_minor: 0,
            id: ACTIVITY_RECOGNITION_HARDWARE_MODULE_ID,
            name: "Activity recognition module",
            author: "Electronic Company",
            methods: &activity_recognition_module_methods,
            dso: NULL,
            reserved: { },
    },
    get_supported_activities_list: get_activity_recognition_list,
};

static void activity_recognition_event_report(activity_event_t* events, int count) {
    activity_recognition_callback.activity_callback(&activity_recognition_callback, events, count);
}


