/*****************************************************/
/*
**   Author: lirui
**   Date: 2019/08/30
**   File: DisplayExVSync.h
**   Function:  Interface of DisplayExVSync for user
**   History:
**       2019/08/30 create by lirui
**
**   Copy Right: iris corp
**
*/
/*****************************************************/
#ifndef  INTERFACE_DISPLAY_EXTERNAL_VSYNC_H
#define  INTERFACE_DISPLAY_EXTERNAL_VSYNC_H

#include <IDisplayVSync.h>
#include <utils/Thread.h>
#include <utils/Mutex.h>

namespace iris {

class DisplayExVSync: public IDisplayVSync {
protected:
    static int        static_default_destroy(struct IDisplayVSync* vc);
    static int        static_default_init(struct IDisplayVSync* vc);
    static int        static_default_exit(struct IDisplayVSync* vc);
    static int        static_default_wait(struct IDisplayVSync* vc, int64_t phase);
    static int64_t    static_default_remainingTime(struct IDisplayVSync* vc, int64_t phase);
public:
    DisplayExVSync();

    virtual ~DisplayExVSync();

    virtual int  init();
    virtual int  exit();
    virtual int  wait(int64_t phase);
protected:
    int64_t remainingVSyncTime(int64_t phase);
    int64_t remainingTime(int64_t phase);
    void waitRemaining(int64_t rt) ;
    int displayVsyncEvent(unsigned int width, unsigned int height);
    virtual bool threadLoop();
	void addSyncSample(nsecs_t timestamp);
	void calibrate();

    static void* receiveThread(void *arg) ;

private:
    static const int VR_SYNC_MIN_SAMPLES = 10;
    static const int VR_SYNC_MAX_SAMPLES = 60;
    const int64_t NANOSECOND = 1000000000;
	android::Mutex mMutex;
	android::Condition mCond;
	int64_t mPeriod;
	int64_t mPhase;
	int64_t mSyncSamples[VR_SYNC_MAX_SAMPLES];
	size_t mNumSamples;
	size_t mFirstSample;
	bool mEnoughSamples;
	bool mNeedResync;
	int64_t PERIOD;
    int64_t mSampleTimestamp;
    int     mExit;
    pthread_t mRThreadID;
} ;

}


#endif //INTERFACE_DISPLAY_EXTERNAL_VSYNC_H
