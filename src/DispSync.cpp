/*
 * Copyright (C) 2013 The Android Open Source Project
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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

// This is needed for stdint.h to define INT64_MAX in C++
#define __STDC_LIMIT_MACROS

#include <math.h>

#include <cutils/log.h>

#include <ui/Fence.h>

#include <utils/String8.h>
#include <utils/Thread.h>
#include <utils/Trace.h>
#include <utils/Vector.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <stdlib.h>     /* atoll */
#include <time.h>
#include <sys/poll.h>

#include <algorithm>

#include "DispSync.h"


using std::max;
using std::min;

namespace android {


// This is the threshold used to determine when hardware vsync events are
// needed to re-synchronize the software vsync model with the hardware.  The
// error metric used is the mean of the squared difference between each
// present time and the nearest software-predicted vsync.
static const nsecs_t kErrorThreshold = 160000000000;    // 400 usec squared

// This is the offset from the present fence timestamps to the corresponding
// vsync event.

#undef LOG_TAG
#define LOG_TAG "DispSync"


int DispSync::static_default_destroy(struct IDisplayVSync* vc) {
    DispSync* dVSync = (DispSync*) vc;
    dVSync->exit();
    delete dVSync;
    return 0;
}

int DispSync::static_default_init(struct IDisplayVSync* vc) {
    DispSync* dVSync = (DispSync*) vc;
    return dVSync->init();
}

int DispSync::static_default_exit(struct IDisplayVSync* vc) {
    DispSync* dVSync = (DispSync*) vc;
    return dVSync->exit();
}

int DispSync::static_default_wait(struct IDisplayVSync* vc, int64_t phase) {
    DispSync* dVSync = (DispSync*) vc;
    return dVSync->wait(phase);
}

int64_t DispSync::static_default_remainingTime(struct IDisplayVSync* vc, int64_t phase) {
    DispSync* dVSync = (DispSync*) vc;
    return dVSync->remainingTime(phase);
}

DispSync::DispSync() :  mPeriod(16666666),   mRefreshSkipCount(0) , mThread(NULL) {

    IDisplayVSync::destroy = DispSync::static_default_destroy;
    IDisplayVSync::init = DispSync::static_default_init;
    IDisplayVSync::exit = DispSync::static_default_exit;
    IDisplayVSync::wait = DispSync::static_default_wait;
    IDisplayVSync::remainingTime = DispSync::static_default_remainingTime;

    mLastTime = 0;
    mExit = 0;
    VR_DELAY_MARGIN = 1500000;
    VR_BUSY_WAIT_THREADHOLD = 1000000;
    NANOSECOND = 1000000000;
    reset();
    beginResync();
}

DispSync::~DispSync() {}


int DispSync::init() {
    mExit = 0;
    return init("/sys/devices/virtual/graphics/fb0/vsync_event");
}

int DispSync::exit() {
    mExit = 1;
    return 0;
}

int  DispSync::init(const char *fName) {
    mFName = fName;
    mPeriod = 16666666;
    mRefreshSkipCount = 0;
    mLastTime = 0;
    VR_DELAY_MARGIN = 1500000;
    VR_BUSY_WAIT_THREADHOLD = 1000000;
    NANOSECOND = 1000000000;
    reset();
    beginResync();

    mThread = new std::thread(&DispSync::pollThread, this);
    return 0;
}

int  DispSync::pollThread()  {
        pollfd poll_fd;
        poll_fd.fd = open(mFName.c_str(), O_RDONLY);
        poll_fd.events = POLLPRI | POLLERR;
        char readbuf[32] = {0};
        int64_t timestamp = 0, current = 0, now = 0;
   //       prctl(PR_SET_NAME,"read vsync", 0, 0, 0);
  //setpriority(PRIO_PROCESS, 0, -9);

    	while (mExit == 0) {
            int code =  poll(&poll_fd, 1, -1);
            if (code <= 0) {
                  continue;
            }

            if (!(poll_fd.revents & POLLPRI) ) {
                continue;
            }

            int sz = pread(poll_fd.fd, readbuf, 32, 0);//			ILOGI("displayExternalVsyncEvent %s %d", readbuf, sz);
            if (sz <= 0) {
                 continue;
            }

            if (sz > 16) {
                 timestamp = atoll(readbuf + 6);
            } else {
                 timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
            }

            addResyncSample(timestamp);

       //     now = systemTime(SYSTEM_TIME_MONOTONIC);
       //   		printf("main :%ld  %ld  %ld\n", now ,timestamp, (timestamp - current) / 1000000);
       //       current = timestamp;
		}

    return 0;
}

void DispSync::reset() {
    Mutex::Autolock lock(mMutex);

    mPhase = 0;
    mReferenceTime = 0;
    mModelUpdated = false;
    mNumResyncSamples = 0;
    mFirstResyncSample = 0;
}

void DispSync::beginResync() {
    Mutex::Autolock lock(mMutex);
    ALOGV("beginResync");
    mModelUpdated = false;
    mNumResyncSamples = 0;
}

bool DispSync::addResyncSample(nsecs_t timestamp) {
    Mutex::Autolock lock(mMutex);

    ALOGV("addResyncSample(%" PRId64 ")",  ns2us(timestamp));
     if (mLastTime == 0) {
        mLastTime = timestamp ;
    } else  {
        if ((timestamp - mLastTime) > 20000000) {
            mModelUpdated = false;
            mNumResyncSamples = 0;
        } 
        mLastTime = timestamp ;
    }

    size_t idx = (mFirstResyncSample + mNumResyncSamples) % MAX_RESYNC_SAMPLES;
    mResyncSamples[idx] = timestamp;
    if (mNumResyncSamples == 0) {
        mPhase = 0;
        mReferenceTime = timestamp;
        ALOGV(" First resync sample: mPeriod = %" PRId64 ", mPhase = 0, "
                "mReferenceTime = %" PRId64,  ns2us(mPeriod),
                ns2us(mReferenceTime));
    }

    if (mNumResyncSamples < MAX_RESYNC_SAMPLES) {
        mNumResyncSamples++;
    } else {
        mFirstResyncSample = (mFirstResyncSample + 1) % MAX_RESYNC_SAMPLES;
    }

    updateModelLocked();

    // Check against kErrorThreshold / 2 to add some hysteresis before having to
    // resync again
    bool modelLocked = mModelUpdated ;
   // ALOGV("[%s] addResyncSample returning %s", mName,
   //         modelLocked ? "locked" : "unlocked");
    return !modelLocked;
}

void DispSync::endResync() {
}

void DispSync::setRefreshSkipCount(int count) {
    Mutex::Autolock lock(mMutex);
    ALOGD("setRefreshSkipCount(%d)", count);
    mRefreshSkipCount = count;
    updateModelLocked();
}

nsecs_t DispSync::getPeriod() {
    // lock mutex as mPeriod changes multiple times in updateModelLocked
    Mutex::Autolock lock(mMutex);
    return mPeriod;
}

void DispSync::updateModelLocked() {
    ALOGV("updateModelLocked %zu",  mNumResyncSamples);
    if (mNumResyncSamples >= MIN_RESYNC_SAMPLES_FOR_UPDATE) {
        ALOGV(" Computing...");
        nsecs_t durationSum = 0;
        nsecs_t minDuration = INT64_MAX;
        nsecs_t maxDuration = 0;
        for (size_t i = 1; i < mNumResyncSamples; ++i) {
            size_t idx = (mFirstResyncSample + i) % MAX_RESYNC_SAMPLES;
            size_t prev = (idx + MAX_RESYNC_SAMPLES - 1) % MAX_RESYNC_SAMPLES;
            nsecs_t duration = mResyncSamples[idx] - mResyncSamples[prev];
            durationSum += duration;
            minDuration = min(minDuration, duration);
            maxDuration = max(maxDuration, duration);
        }

        // Exclude the min and max from the average
        durationSum -= minDuration + maxDuration;
        mPeriod = durationSum / (mNumResyncSamples - 3);

        ALOGV(" mPeriod = %" PRId64,  ns2us(mPeriod));

        double sampleAvgX = 0;
        double sampleAvgY = 0;
        double scale = 2.0 * M_PI / double(mPeriod);
        // Intentionally skip the first sample
        for (size_t i = 1; i < mNumResyncSamples; i++) {
            size_t idx = (mFirstResyncSample + i) % MAX_RESYNC_SAMPLES;
            nsecs_t sample = mResyncSamples[idx] - mReferenceTime;
            double samplePhase = double(sample % mPeriod) * scale;
            sampleAvgX += cos(samplePhase);
            sampleAvgY += sin(samplePhase);
        }

        sampleAvgX /= double(mNumResyncSamples - 1);
        sampleAvgY /= double(mNumResyncSamples - 1);

        mPhase = nsecs_t(atan2(sampleAvgY, sampleAvgX) / scale);

        //ALOGV("[%s] mPhase = %" PRId64, mName, ns2us(mPhase));

    //    if (mPhase < -(mPeriod / 2)) {
        if (mPhase < 0) {
            mPhase += mPeriod;
         //   ALOGV("[%s] Adjusting mPhase -> %" PRId64, mName, ns2us(mPhase));
        }

        // Artificially inflate the period if requested.
 //       mPeriod += mPeriod * mRefreshSkipCount;
        mModelUpdated = true;
    }
}

nsecs_t DispSync::computeNextRefresh(int periodOffset) const {
    Mutex::Autolock lock(mMutex);
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    nsecs_t phase = mReferenceTime + mPhase;
//    printf("period phase reftime %ld  %ld %ld\n", mPeriod, mPhase, mReferenceTime);
    return (((now - phase) / mPeriod) + periodOffset + 1) * mPeriod + phase;
}

nsecs_t DispSync::remainingTime(nsecs_t phase) {
        Mutex::Autolock lock(mMutex);
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        nsecs_t refTime = mReferenceTime + mPhase;
    	return ((phase + mPeriod - ( (now - refTime) % mPeriod)) % mPeriod);
    }

 int  DispSync::wait(nsecs_t phase) {
        nsecs_t refTime = remainingTime(phase);;
/*    	{
            Mutex::Autolock lock(mMutex);
    		if ((refTime + VR_DELAY_MARGIN) > mPeriod) {
    			refTime = 0;
    		}
    	}*/

		struct timespec now, target;
    	// unsigned int ut;
    	clock_gettime(CLOCK_MONOTONIC, &now);
    	// Use nanosleep if remaining time longer than VR_BUSY_WAIT_THREADHOLD
    	if (refTime > VR_BUSY_WAIT_THREADHOLD) {  // 1ms
    		target.tv_sec = 0;
    		target.tv_nsec = refTime - VR_BUSY_WAIT_THREADHOLD;
    		nanosleep(&target, NULL);
    	}
    	// Busy wait until target time
    	target.tv_sec = now.tv_sec;
    	target.tv_nsec = now.tv_nsec + refTime;
    	if (target.tv_nsec > NANOSECOND) {
    		target.tv_sec += target.tv_nsec / NANOSECOND;
    		target.tv_nsec %= NANOSECOND;
    	}

    	while (true) {
    		clock_gettime(CLOCK_MONOTONIC, &now);
    		if (now.tv_sec > target.tv_sec ||
    			(now.tv_sec == target.tv_sec && now.tv_nsec >= target.tv_nsec)) {
    			break;
    		}
    	}

     return 0;
 }

} // namespace android



#if defined(__cplusplus)
extern "C" {
#endif


__attribute__((visibility("default"))) struct IDisplayVSync* allocateIDisplayVSync() {
    return (struct IDisplayVSync*) new android::DispSync();
}


#if defined(__cplusplus)
}
#endif
