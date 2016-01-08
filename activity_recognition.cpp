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

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif
#define NS_PER_SEC 1000000000LL

enum {
    IN_VEHICLE = 0,
    ON_BICYCLE,
    WALKING,
    RUNNING,
    STILL,
    TILTING,
};

int fd;

static pthread_t read_thread;

static activity_recognition_callback_procs_t activity_recognition_callback;

static void activity_recognition_event_report(activity_event_t* events, int count) {
    if (activity_recognition_callback.activity_callback)
        activity_recognition_callback.activity_callback(&activity_recognition_callback, events, count);
}

void *read_task(void* ptr)
{
    activity_event_t test_event;

    test_event.event_type = ACTIVITY_EVENT_ENTER;
    test_event.activity = 0;
    test_event.timestamp = 0;
    struct timespec t;

    while(1) {

        clock_gettime(CLOCK_BOOTTIME, &t);
        t.tv_sec * NS_PER_SEC + t.tv_nsec;
        activity_recognition_event_report(&test_event, 1);
        sleep(1);
    }
}

void activity_recognition_register_callback(const struct activity_recognition_device* dev,
        const activity_recognition_callback_procs_t* callback)
{
    ALOGE("%s", __func__);
    activity_recognition_callback = *callback;
}

int activity_recognition_enable(const struct activity_recognition_device* dev,
        uint32_t activity_handle, uint32_t event_type, int64_t max_batch_report_latency_ns)
{
    ALOGE("%s", __func__);

    uint32_t buf[5];
    buf[0] = 1; //enable
    buf[1] = event_type;
    buf[2] = activity_handle;
    *((int64_t*)(buf + 3)) = max_batch_report_latency_ns;

    write(fd, buf, 20);


    return 0;
}

int activity_recognition_disable(const struct activity_recognition_device* dev,
        uint32_t activity_handle, uint32_t event_type)
{
    ALOGE("%s", __func__);

    uint32_t buf[5];
    buf[0] = 0; //disable
    buf[1] = event_type;
    buf[2] = activity_handle;

    write(fd, buf, 20);

    return 0;
}

int activity_recognition_flush(const struct activity_recognition_device* dev)
{
    ALOGE("%s", __func__);

    uint32_t buf[5];
    buf[0] = 2; //fluch

    write(fd, buf, 20);
    return 0;
}

static int close_activity_recognition(hw_device_t *dev)
{
    ALOGE("%s", __func__);
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
    ALOGE("%s", __func__);
    *activity_list = sSupportActivitiesList;
	return ARRAY_SIZE(sSupportActivitiesList);
}

static int open_activity_recognition(const struct hw_module_t* module, const char* id,
                        struct hw_device_t** device)
{
    ALOGE("%s", __func__);
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

    fd = open("/dev/spich2", O_RDWR);

    pthread_create(&read_thread, NULL, read_task, NULL);

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

