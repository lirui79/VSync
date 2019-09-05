
#include <math.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <pthread.h>
#include "DisplayVSync.h"
#include "../../includes/config.h"
#include "../ALog.h"


namespace iris {

int DisplayVSync::static_default_destroy(struct IDisplayVSync* vc) {
    DisplayVSync* dVSync = (DisplayVSync*) vc;
    dVSync->exit();
    delete dVSync;
    return 0;
}

int DisplayVSync::static_default_init(struct IDisplayVSync* vc) {
    DisplayVSync* dVSync = (DisplayVSync*) vc;
    return dVSync->init();
}

int DisplayVSync::static_default_exit(struct IDisplayVSync* vc) {
    DisplayVSync* dVSync = (DisplayVSync*) vc;
    return dVSync->exit();
}

int DisplayVSync::static_default_wait(struct IDisplayVSync* vc, int64_t phase) {
    DisplayVSync* dVSync = (DisplayVSync*) vc;
    return dVSync->wait(phase);
}

int64_t DisplayVSync::static_default_remainingTime(struct IDisplayVSync* vc, int64_t phase) {
    DisplayVSync* dVSync = (DisplayVSync*) vc;
    return dVSync->remainingVSyncTime(phase);
}

DisplayVSync::DisplayVSync() {
    IDisplayVSync::destroy = DisplayVSync::static_default_destroy;
    IDisplayVSync::init = DisplayVSync::static_default_init;
    IDisplayVSync::exit = DisplayVSync::static_default_exit;
    IDisplayVSync::wait = DisplayVSync::static_default_wait;
    IDisplayVSync::remainingTime = DisplayVSync::static_default_remainingTime;

    mNeedResync = true;
    mReceiver = new android::DisplayEventReceiver();
    mPeriod = NANOSECOND / 60;
    mPhase = 0;
    mNumSamples = 0;
    mFirstSample = 0;
    mEnoughSamples = false;
    PERIOD = NANOSECOND / 60;
    mSampleTimestamp = 0;
    mExit = 1;

    int wakeFds[2];
    int result = pipe(wakeFds);

    mReadPipeFd  = wakeFds[0];
    mWritePipeFd = wakeFds[1];
}

DisplayVSync::~DisplayVSync() {
    if (mReceiver != NULL)
       delete mReceiver;
    mReceiver = NULL;
    if (mReadPipeFd >= 0)
      close(mReadPipeFd);
    mReadPipeFd = -1;
    if (mWritePipeFd >= 0)
       close(mWritePipeFd);
    mWritePipeFd = -1;
}

int DisplayVSync::init() {
    ALOGI("DisplayVSync::init");
    if (mExit == 0)
        return 1;
    if (mReceiver->initCheck() == android::NO_ERROR)
       mNeedResync = false ;
    mReceiver->setVsyncRate(1);
    mExit = 0;//    run("IrisVSyncEx");
	pthread_create(&(mRThreadID), NULL, &receiveThread, this);
    return 0;
}

int DisplayVSync::exit() {
    ALOGI("DisplayVSync::exit");
    if (mExit != 0)
        return 1;
    mExit = 1;
    mNeedResync = true;
    const char WAKE_MESSAGE = 'W';
    write(mWritePipeFd, &WAKE_MESSAGE, 1);
    usleep(100);
    return 0;
}

int DisplayVSync::wait(int64_t phase) {
    int64_t rt = 0;
    {
        android::Mutex::Autolock lock(mMutex);
        while (!mEnoughSamples) {
            mCond.wait(mMutex);
        }
        rt = remainingTime(phase);
        if (rt > mPeriod - VR_DELAY_MARGIN) {
            rt = 0;
        }
    }
    waitRemaining(rt);
    return 0;
}

int64_t DisplayVSync::remainingVSyncTime(int64_t phase) {
    android::Mutex::Autolock lock(mMutex);
    return remainingTime(phase);
}

int64_t DisplayVSync::remainingTime(int64_t phase) {
    struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	int64_t nanoNow = (int64_t) now.tv_sec * NANOSECOND + now.tv_nsec;
	return (mPhase + phase - nanoNow % mPeriod + mPeriod) % mPeriod;
}

void  DisplayVSync::waitRemaining(int64_t rt)  {
	struct timespec now, target;
	clock_gettime(CLOCK_MONOTONIC, &now);
	// Use nanosleep if remaining time longer than VR_BUSY_WAIT_THREADHOLD
	if (rt > VR_BUSY_WAIT_THREADHOLD) {  // 1ms
		target.tv_sec = 0;
		target.tv_nsec = rt - VR_BUSY_WAIT_THREADHOLD;
		nanosleep(&target, NULL);
	}
	// Busy wait until target time
	target.tv_sec = now.tv_sec;
	target.tv_nsec = now.tv_nsec + rt;
	if (target.tv_nsec > NANOSECOND) {
		target.tv_sec += target.tv_nsec / NANOSECOND;
		target.tv_nsec %= NANOSECOND;
	}

	/*
	struct timespec now1;
		clock_gettime(CLOCK_MONOTONIC, &now1);
		static int count = 0;
		if (((double)(now1.tv_nsec - target.tv_nsec +
					  NANOSECOND * (now1.tv_sec - target.tv_sec))) <
				-VR_BUSY_WAIT_THREADHOLD ||
			((double)(now1.tv_nsec - target.tv_nsec +
					  NANOSECOND * (now1.tv_sec - target.tv_sec))) >
				VR_BUSY_WAIT_THREADHOLD) {
			++count;
			ALOGE("wait prob %d wrong %.0lf", count,
				  ((double)(now1.tv_nsec - target.tv_nsec +
							NANOSECOND * (now1.tv_sec - target.tv_sec))));
		}
		if (now.tv_sec > target.tv_sec ||
			(now.tv_sec == target.tv_sec && now.tv_nsec > target.tv_nsec)) {
			// if (((double)(now.tv_nsec - target.tv_nsec)) > 0000000)  //
			// ALOGE("prob %d %.0lf\n", count++,
			//        ((double)(now.tv_nsec - target.tv_nsec)));
		}//*/

	while (true) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (now.tv_sec > target.tv_sec ||
			(now.tv_sec == target.tv_sec && now.tv_nsec > target.tv_nsec)) {
			break;
		}
	}
}

int DisplayVSync::displayVsyncEvent() {
    android::DisplayEventReceiver::Event  event[1] ;
    size_t sz = 0;
    fd_set fds;
    struct timeval timeout={1,0};
    int fd =  mReceiver->getFd();
    int maxfd = (mReadPipeFd > fd) ? mReadPipeFd : fd;

    while (mExit == 0) {
        FD_ZERO(&fds); //每次循环都要清空集合，否则不能检测描述符变化
        FD_SET(fd, &fds); //添加描述符
        FD_SET(mReadPipeFd, &fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int code = select(maxfd + 1, &fds, NULL, NULL, &timeout);
        if (code <= 0) {
            continue;
        }

        if (FD_ISSET(mReadPipeFd, &fds)) {
            ALOGE("displayVsyncEvent receive exit\n");
            break;
        }

        sz = mReceiver->getEvents(event, 1);
        if (sz <= 0) {
            continue;
        }
//        ALOGI("event[%x %x %lX %x]\n", event[0].header.type, event[0].header.id, event[0].header.timestamp, event[0].vsync.count);

        for (size_t i = 0 ; i < sz ; ++i) {

            if (event[i].header.type == android::DisplayEventReceiver::DISPLAY_EVENT_VSYNC) {
                int64_t timestamp = event[i].header.timestamp;
                addSyncSample(timestamp);
                if (mEnoughSamples) {
                    calibrate();
                }
            }
        }
    }

    return 0;
}

int DisplayVSync::readFromVFile(int fd, int64_t &id, int64_t &timestamp, int64_t &lasttimestamp) {
    char readbuf[32] = {0};
    if (id < 10)
        return 1;
    int sz = pread(fd, readbuf, 32, 0);//			ILOGI("displayExternalVsyncEvent %s %d", readbuf, sz);
    if (sz <= 0) {
        return 0;
    }
    if (sz > 16)
        timestamp = atoll(readbuf + 6);
    else
        timestamp = systemTime(CLOCK_MONOTONIC);

    addVsync(timestamp, lasttimestamp);
    if (mEnoughSamples) {
        calibrate();
    }
    return 2;
}

int DisplayVSync::readDisplayEvent(int64_t &id) {
    android::DisplayEventReceiver::Event  event[1] ;
    size_t sz = 0;
    sz = mReceiver->getEvents(event, 1);
    if (sz <= 0) {
        return 0;
    }
    //ILOGI("event[%x %x %lX %x]\n", event.header.type, event.header.id, event.header.timestamp, event.vsync.count);
    for (size_t i = 0 ; i < sz ; ++i) {
        if (event[i].header.type != android::DisplayEventReceiver::DISPLAY_EVENT_VSYNC) {
            continue;
        }

        if (id >= 10) {
            continue;
        }
        addSyncSample(event[i].header.timestamp);
        if (mEnoughSamples) {
            calibrate();
        }
    }
    return 1;
}

int DisplayVSync::displayExternalVsyncEvent() {
    fd_set fds;
    int fd[2] = {-1, -1};
    const char* vsyncName = "/sys/devices/platform/soc/ae00000.qcom,mdss_mdp/svsync";
    struct timeval timeout={1,0};
    fd[0] = open(vsyncName, O_RDWR);

    int64_t timestamp = systemTime(CLOCK_MONOTONIC), lasttimestamp = 0;
    int64_t VID = 0;
    fd[1] = mReceiver->getFd();
    int    sz = 0;
    int maxfd = (fd[0] > fd[1]) ? fd[0] : fd[1];
    maxfd = (maxfd > mReadPipeFd) ? maxfd : mReadPipeFd;

    while (mExit == 0) {
        FD_ZERO(&fds); //每次循环都要清空集合，否则不能检测描述符变化
        FD_SET(fd[0], &fds); //添加描述符
        FD_SET(fd[1], &fds);
        FD_SET(mReadPipeFd, &fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        sz = select(maxfd + 1, &fds, NULL, NULL, &timeout);
        if (sz <= 0) {
            ALOGE("select sz<=0");
            continue;
        }
        if (FD_ISSET(mReadPipeFd, &fds)) {
            ALOGE("displayExternalVsyncEvent receive exit\n");
            break;
        }

        int code = 0;
        if (FD_ISSET(fd[0], &fds)) {
            code = readFromVFile(fd[0], VID, timestamp, lasttimestamp);
            FD_CLR(fd[0], &fds);
        }

        if (FD_ISSET(fd[1], &fds)) {
            code = readDisplayEvent(VID);
            FD_CLR(fd[1], &fds);
        }
        ++VID;
        if (code < 0)
           break;
    }
    close(fd[0]);

    return 0;
}

void DisplayVSync::addVsync(int64_t timestamp, int64_t &lasttimestamp) {
    static int64_t recentValidTs = 0;
    int64_t threshold = 1000000;
    if ((lasttimestamp == 0) || ((timestamp < lasttimestamp + PERIOD + threshold) && (timestamp + threshold > lasttimestamp + PERIOD))) {
        addSyncSample(timestamp);
        recentValidTs = timestamp;
        lasttimestamp = timestamp;
        return ;
    }

    int64_t value = 3 * threshold;
    if (timestamp >= recentValidTs)
       value =((timestamp - recentValidTs) % PERIOD);
    if(value < threshold) {
        int64_t count = ((timestamp + threshold - recentValidTs) / PERIOD);
        for(int64_t i = 0; i < count - 1; ++i) {
            int64_t newTs = recentValidTs + (i + 1) * PERIOD;
            addSyncSample(newTs);
        }
        addSyncSample(timestamp);
        recentValidTs = timestamp;
        lasttimestamp = timestamp;
        return ;
    }

    int64_t count = ((timestamp + 0.5 * PERIOD - recentValidTs) / PERIOD);
    if (count == 0) {
        return ;
    }
    lasttimestamp = timestamp;

    for(int64_t i = 0; i < count; ++i) {
        int64_t newTs = recentValidTs + PERIOD;
        addSyncSample(newTs);
        recentValidTs = newTs;
    }
}

void* DisplayVSync::receiveThread(void *arg) {
    DisplayVSync* displayVSnc = (DisplayVSync*) arg;
    displayVSnc->threadLoop();
    return NULL;
}

bool DisplayVSync::threadLoop()  {
    ALOGI("IrisVSyncEx thread start\n");
#ifdef TARGET_BUILD_DEBUG
    prctl(PR_SET_NAME, "IrisVSyncEx", 0, 0, 0);
#endif

#if defined(TARGET_PRODUCT_NREAL)
   displayExternalVsyncEvent();
#else
   displayVsyncEvent();
#endif
    ALOGI("IrisVSyncEx thread exit\n");
    return false;
}

void DisplayVSync::addSyncSample(int64_t timestamp) {
    if (timestamp == mSampleTimestamp)
        return ;

    mSampleTimestamp = timestamp;
    android::Mutex::Autolock lock(mMutex);
    if (mNeedResync) {
        mNumSamples = 0;
        mFirstSample = 0;
        mEnoughSamples = false;
        mNeedResync = false;
    }
    if (mNumSamples < VR_SYNC_MAX_SAMPLES) {
        mSyncSamples[mNumSamples] = timestamp;
        ++mNumSamples;
        if (!mEnoughSamples && mNumSamples > VR_SYNC_MIN_SAMPLES) {
            mEnoughSamples = true;
            mCond.broadcast();
        }
    } else {
        mSyncSamples[mFirstSample] = timestamp;
        mFirstSample = (mFirstSample + 1) % VR_SYNC_MAX_SAMPLES;
    }
}

void DisplayVSync::calibrate() {
    int64_t durationSum = 0;
    for (size_t i = 1; i < mNumSamples; i++) {
        size_t idx = (mFirstSample + i) % VR_SYNC_MAX_SAMPLES;
        size_t prev = (idx + VR_SYNC_MAX_SAMPLES - 1) % VR_SYNC_MAX_SAMPLES;
        durationSum += mSyncSamples[idx] - mSyncSamples[prev];
    }

//    int64_t period = PERIOD;//durationSum / (mNumSamples - 1);
	int64_t period = durationSum / (mNumSamples - 1);

    double sampleAvgX = 0;
    double sampleAvgY = 0;
    double scale = 2.0 * M_PI / double(period);
    for (size_t i = 0; i < mNumSamples; i++) {
        size_t idx = (mFirstSample + i) % VR_SYNC_MAX_SAMPLES;
        int64_t sample = mSyncSamples[idx];
        double samplePhase = double(sample % period) * scale;
        sampleAvgX += cos(samplePhase);
        sampleAvgY += sin(samplePhase);
    }

    sampleAvgX /= double(mNumSamples);
    sampleAvgY /= double(mNumSamples);

    int64_t phase = int64_t(atan2(sampleAvgY, sampleAvgX) / scale);
//    	int64_t phase = 0;
    if (phase < 0) {
        phase += period;
    }

    {
        android::Mutex::Autolock lock(mMutex);
        mPhase = phase;
        mPeriod = period;
    }
//    ALOGI("IrisVSyncEx::calibrate phase period [%ld  %ld]\n", mPhase, mPeriod);
}


}



#if defined(__cplusplus)
extern "C" {
#endif


__attribute__((visibility("default"))) struct IDisplayVSync* allocateIDisplayVSync1() {
    return (struct IDisplayVSync*) new iris::DisplayVSync();
}


#if defined(__cplusplus)
}
#endif
