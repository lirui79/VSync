/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ANDROID_DISPSYNC_H
#define ANDROID_DISPSYNC_H

#include <stddef.h>

#include <utils/Mutex.h>
#include <utils/Timers.h>
#include <utils/RefBase.h>
#include <thread>
#include <string>

#include <IDisplayVSync.h>

namespace android {

// DispSync maintains a model of the periodic hardware-based vsync events of a
// display and uses that model to execute period callbacks at specific phase
// offsets from the hardware vsync events.  The model is constructed by
// feeding consecutive hardware event timestamps to the DispSync object via
// the addResyncSample method.
//
// The model is validated using timestamps from Fence objects that are passed
// to the DispSync object via the addPresentFence method.  These fence
// timestamps should correspond to a hardware vsync event, but they need not
// be consecutive hardware vsync times.  If this method determines that the
// current model accurately represents the hardware event times it will return
// false to indicate that a resynchronization (via addResyncSample) is not
// needed.
class DispSync : public IDisplayVSync{
protected:
    static int        static_default_destroy(struct IDisplayVSync* vc);
    static int        static_default_init(struct IDisplayVSync* vc);
    static int        static_default_exit(struct IDisplayVSync* vc);
    static int        static_default_wait(struct IDisplayVSync* vc, int64_t phase);
    static int64_t    static_default_remainingTime(struct IDisplayVSync* vc, int64_t phase);
public:
    virtual int  init();
    virtual int  exit();

    DispSync();
   virtual  ~DispSync();

    // reset clears the resync samples and error value.
    void reset();

    int  init(const char *fName) ;

    // The beginResync, addResyncSample, and endResync methods are used to re-
    // synchronize the DispSync's model to the hardware vsync events.  The re-
    // synchronization process involves first calling beginResync, then
    // calling addResyncSample with a sequence of consecutive hardware vsync
    // event timestamps, and finally calling endResync when addResyncSample
    // indicates that no more samples are needed by returning false.
    //
    // This resynchronization process should be performed whenever the display
    // is turned on (i.e. once immediately after it's turned on) and whenever
    // addPresentFence returns true indicating that the model has drifted away
    // from the hardware vsync events.
    void beginResync();
    bool addResyncSample(nsecs_t timestamp);
    void endResync();

    // The getPeriod method returns the current vsync period.
    nsecs_t getPeriod();

    // setRefreshSkipCount specifies an additional number of refresh
    // cycles to skip.  For example, on a 60Hz display, a skip count of 1
    // will result in events happening at 30Hz.  Default is zero.  The idea
    // is to sacrifice smoothness for battery life.
    void setRefreshSkipCount(int count);
    // computeNextRefresh computes when the next refresh is expected to begin.
    // The periodOffset value can be used to move forward or backward; an
    // offset of zero is the next refresh, -1 is the previous refresh, 1 is
    // the refresh after next. etc.
    nsecs_t computeNextRefresh(int periodOffset) const;

   nsecs_t remainingTime(nsecs_t phase) ;

   int  wait(nsecs_t phase);

private:

    int  pollThread() ;

    void updateModelLocked();

    enum { MAX_RESYNC_SAMPLES = 32 };
    enum { MIN_RESYNC_SAMPLES_FOR_UPDATE = 6 };
    enum { NUM_PRESENT_SAMPLES = 8 };

    // mPeriod is the computed period of the modeled vsync events in
    // nanoseconds.
    nsecs_t mPeriod;

    // mPhase is the phase offset of the modeled vsync events.  It is the
    // number of nanoseconds from time 0 to the first vsync event.
    nsecs_t mPhase;

    // mReferenceTime is the reference time of the modeled vsync events.
    // It is the nanosecond timestamp of the first vsync event after a resync.
    nsecs_t mReferenceTime;

    nsecs_t  mLastTime ;

    // mError is the computed model error.  It is based on the difference
    // between the estimated vsync event times and those observed in the
    // mPresentTimes array.
    nsecs_t mError;

    // Whether we have updated the vsync event model since the last resync.
    bool mModelUpdated;

    // These member variables are the state used during the resynchronization
    // process to store information about the hardware vsync event times used
    // to compute the model.
    nsecs_t mResyncSamples[MAX_RESYNC_SAMPLES];
    size_t mFirstResyncSample;
    size_t mNumResyncSamples;

    int mRefreshSkipCount;

     nsecs_t  VR_DELAY_MARGIN;
    nsecs_t  VR_BUSY_WAIT_THREADHOLD;
    nsecs_t NANOSECOND ;
    std::thread   *mThread;
    std::string       mFName;
    // mMutex is used to protect access to all member variables.
    mutable Mutex mMutex;
    int    mExit;
};

}

#endif // ANDROID_DISPSYNC_H
