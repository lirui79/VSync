
#include <math.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ui/GraphicBuffer.h>
#include <gui/IGraphicBufferProducer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>

#include <gui/BufferQueue.h>
#include <ui/DisplayInfo.h>
#include <gui/ISurfaceComposerClient.h>
#include <hardware/hwcomposer_defs.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "DisplayExVSync.h"
#include "../../includes/config.h"
#include "../ALog.h"


namespace iris {
using namespace android;

    GLuint programID = -1;// program id
    GLuint vertexShader = -1; // vertex shader id
    GLuint fragmentShader = -1; // fragment shader id

    GLuint vertexID  = -1;// vertex  id
    GLuint textureID = -1;// texture id

    GLint  vertexLoc = -1;// gpu vertex handle
    GLint  textureLoc = -1;//gpu texture handle
    GLint  yuvtextLoc = -1;//gpu yuv texture handle
    GLint drawOrder[] = { 0, 1, 2, 0, 2, 3 };
    // vertex array
    GLfloat vextexData[] = {
           -1.0f,  -1.0f,
           1.0f, -1.0f,
            1.0f, 1.0f,
            -1.0f,  1.0f,
    };
    //顶点颜色数组
    float colorData[] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,
    };

    const char* vertexShaderCode =
                "attribute vec4 aPosition;\n"
                "attribute vec2 aTextureCoord;\n"
                "varying vec2   vTextureCoord;\n"
                "void main()\n"
                "{\n"
                    "gl_Position = aPosition;\n"
                    "vTextureCoord = aTextureCoord;\n"
                "}";

    const char* fragmentShaderCode =
                "#extension GL_OES_EGL_image_external : require\n"
                "precision mediump float;\n"
                "varying vec2 vTextureCoord;\n"
                "uniform samplerExternalOES yuvTextureID;\n"
                "void main() {\n"
                "  gl_FragColor = texture2D(yuvTextureID, vTextureCoord);\n"
                "}";

    int useShader(const char *shader, GLenum type, GLuint *vShader) {
    //创建着色器对象：顶点着色器
    	  vShader[0] = glCreateShader(type);
    //将字符数组绑定到对应的着色器对象上
    	  glShaderSource(vShader[0], 1, &shader, NULL);
    //编译着色器对象
        glCompileShader(vShader[0]);
    //检查编译是否成功
        GLint compileResult;
        glGetShaderiv(vShader[0], GL_COMPILE_STATUS, &compileResult);
        if (GL_FALSE == compileResult) {
            printf("useShader %s %d %d fail\n", shader, type, vShader[0]);
            glDeleteShader(vShader[0]);
            return -1;
        }
        return 0;
    }

    int useProgram() {
        if (useShader(vertexShaderCode, GL_VERTEX_SHADER, &vertexShader) < 0) {
          return -1;// vertex shader id
        }

        if (useShader(fragmentShaderCode, GL_FRAGMENT_SHADER, &fragmentShader) < 0) {
          return -1;// fragment shader id
        }

        programID = glCreateProgram() ;// program id

        glAttachShader(programID, vertexShader);
        glAttachShader(programID, fragmentShader);

        glLinkProgram(programID);

        glDeleteShader(vertexShader);
    	glDeleteShader(fragmentShader);
        vertexShader = -1;
        fragmentShader = -1;

        vertexLoc = glGetAttribLocation(programID, "aPosition");
        textureLoc = glGetAttribLocation(programID, "aTextureCoord");
        yuvtextLoc = glGetUniformLocation(programID, "yuvTextureID");

        return 0;
    }

    GLuint useTextureID() {
        GLuint texture = -1;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);
        glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return texture;
    }

    int draw() {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glUseProgram(programID);

        glVertexAttribPointer(vertexLoc, 2, GL_FLOAT, GL_FALSE, 0, vextexData);
        glEnableVertexAttribArray(vertexLoc);

        glVertexAttribPointer(textureLoc, 2, GL_FLOAT, GL_FALSE, 0, colorData);
        glEnableVertexAttribArray(textureLoc);

        glUniform1i(yuvtextLoc, 0);
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureID);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, drawOrder);

        glDisableVertexAttribArray(vertexLoc);
        glDisableVertexAttribArray(textureLoc);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        glUseProgram(0);
      return 0;
    }

    int initDisplay(EGLDisplay *display, const EGLint* configAttribs, EGLConfig* config) {
        display[0] = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint majorVersion;
        EGLint minorVersion;
        eglInitialize(display[0], &majorVersion, &minorVersion);
        EGLint numConfigs = 0;
        eglChooseConfig(display[0], configAttribs, config, 1, &numConfigs);
        return 0;
    }


int DisplayExVSync::static_default_destroy(struct IDisplayVSync* vc) {
    DisplayExVSync* dVSync = (DisplayExVSync*) vc;
    dVSync->exit();
    delete dVSync;
    return 0;
}

int DisplayExVSync::static_default_init(struct IDisplayVSync* vc) {
    DisplayExVSync* dVSync = (DisplayExVSync*) vc;
    return dVSync->init();
}

int DisplayExVSync::static_default_exit(struct IDisplayVSync* vc) {
    DisplayExVSync* dVSync = (DisplayExVSync*) vc;
    return dVSync->exit();
}

int DisplayExVSync::static_default_wait(struct IDisplayVSync* vc, int64_t phase) {
    DisplayExVSync* dVSync = (DisplayExVSync*) vc;
    return dVSync->wait(phase);
}

int64_t DisplayExVSync::static_default_remainingTime(struct IDisplayVSync* vc, int64_t phase) {
    DisplayExVSync* dVSync = (DisplayExVSync*) vc;
    return dVSync->remainingVSyncTime(phase);
}

DisplayExVSync::DisplayExVSync() {
    IDisplayVSync::destroy = DisplayExVSync::static_default_destroy;
    IDisplayVSync::init = DisplayExVSync::static_default_init;
    IDisplayVSync::exit = DisplayExVSync::static_default_exit;
    IDisplayVSync::wait = DisplayExVSync::static_default_wait;
    IDisplayVSync::remainingTime = DisplayExVSync::static_default_remainingTime;

    mNeedResync = true;
    mPeriod = NANOSECOND / 60;
    mPhase = 0;
    mNumSamples = 0;
    mFirstSample = 0;
    mEnoughSamples = false;
    PERIOD = NANOSECOND / 60;
    mSampleTimestamp = 0;
    mExit = 1;
}

DisplayExVSync::~DisplayExVSync() {
}

int DisplayExVSync::init() {
    ALOGI("DisplayExVSync::init");
    if (mExit == 0)
        return 1;
    mExit = 0;//    run("IrisVSyncEx");
	pthread_create(&(mRThreadID), NULL, &receiveThread, this);
    return 0;
}

int DisplayExVSync::exit() {
    ALOGI("DisplayVSync::exit");
    if (mExit != 0)
        return 1;
    mExit = 1;
    mNeedResync = true;
    usleep(100);
    return 0;
}

int DisplayExVSync::wait(int64_t phase) {
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

int64_t DisplayExVSync::remainingVSyncTime(int64_t phase) {
    android::Mutex::Autolock lock(mMutex);
    return remainingTime(phase);
}

int64_t DisplayExVSync::remainingTime(int64_t phase) {
    struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	int64_t nanoNow = (int64_t) now.tv_sec * NANOSECOND + now.tv_nsec;
	return (mPhase + phase - nanoNow % mPeriod + mPeriod) % mPeriod;
}

void  DisplayExVSync::waitRemaining(int64_t rt)  {
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

int DisplayExVSync::displayVsyncEvent(unsigned int width, unsigned int height) {
        sp<SurfaceComposerClient> mSurfaceComposerClient = new SurfaceComposerClient();
        int layerStack = 0;
#if defined(TARGET_PRODUCT_NREAL)
    	sp<IBinder> dtoken(SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdHdmi));
    	layerStack = 10;
#else
    	sp<IBinder> dtoken(SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain));
    	layerStack = 0;
#endif

    	DisplayInfo dinfo;
    	//获取屏幕的宽高等信息
    	status_t status = SurfaceComposerClient::getDisplayInfo(dtoken, &dinfo);
    	ALOGI("w=%d,h=%d,xdpi=%f,ydpi=%f,fps=%f,ds=%f,orientation=%d\n",
    		dinfo.w, dinfo.h, dinfo.xdpi, dinfo.ydpi, dinfo.fps, dinfo.density,dinfo.orientation);
    	if (status != NO_ERROR)
    		return -1;
    	Rect crop(0, 0, width, height);

    	float x = 0, y = 0;
        android::sp<android::SurfaceControl>  mSurfaceControl = mSurfaceComposerClient->createSurface(String8("IRIS_OS"), width, height, PIXEL_FORMAT_RGBA_8888, 0);
    //    SurfaceComposerClient::setDisplayPowerMode(dtoken, HWC_POWER_MODE_NORMAL);// power on
         unsigned int mZOrder = 2200000;
#ifdef PLATFORM_VERSION_9
        SurfaceComposerClient::Transaction transaction ;
    	transaction.setDisplayLayerStack(dtoken, layerStack);
    	transaction.setLayerStack(mSurfaceControl, layerStack);
    	transaction.setLayer(mSurfaceControl, mZOrder);//设定Z坐标
    	transaction.setCrop(mSurfaceControl, crop);
    	transaction.setPosition(mSurfaceControl, x, y);//以左上角为(0,0)设定显示位置
    	transaction.show(mSurfaceControl);
    	transaction.apply();
#else
    	SurfaceComposerClient::setDisplayLayerStack(dtoken, layerStack);
        SurfaceComposerClient::openGlobalTransaction();
    	mSurfaceControl->setLayerStack(layerStack);
        mSurfaceControl->setLayer(mZOrder);//设定Z坐标
        mSurfaceControl->setCrop(crop);
    	mSurfaceControl->setPosition(x, y);//以左上角为(0,0)设定显示位置
    	mSurfaceControl->show();
        SurfaceComposerClient::closeGlobalTransaction();//感觉没有这步,图片也能显示
#endif //PLATFORM_VERSION_9
    	android::sp<android::Surface> mSurface = mSurfaceControl->getSurface();
    	android::sp<ANativeWindow> mAWindow = mSurface;


    const EGLint configAttribs[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE };

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE };

    EGLDisplay mEglDisplay = EGL_NO_DISPLAY;
    EGLSurface mEglSurface = EGL_NO_SURFACE;
    EGLContext mEglContext = EGL_NO_CONTEXT;
    EGLConfig  mGlConfig = NULL;

    if (initDisplay(&mEglDisplay, configAttribs, &mGlConfig) < 0) {
      printf("opengl initdisplay error!\n");
      return -2;
    }

    mEglSurface = eglCreateWindowSurface(mEglDisplay, mGlConfig, mAWindow.get() , NULL);
    mEglContext = eglCreateContext(mEglDisplay, mGlConfig, EGL_NO_CONTEXT, contextAttribs);
    eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext);

    const int yuvTexUsage = GraphicBuffer::USAGE_SW_WRITE_RARELY;
    const int yuvTexFormat = HAL_PIXEL_FORMAT_YV12;
    GraphicBuffer*  yuvTexBuffer = new GraphicBuffer(width, height, yuvTexFormat, yuvTexUsage);
    EGLClientBuffer clientBuffer = (EGLClientBuffer)yuvTexBuffer->getNativeBuffer();
    EGLImageKHR img = eglCreateImageKHR(mEglDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, 0);

    textureID = useTextureID();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)img);

    if (useProgram() < 0) {
          return -1;
    }
    glViewport(0, 0, width, height);
    int64_t nowTime = systemTime(CLOCK_MONOTONIC) / 1000, curTime, diff;

    unsigned char* buf = NULL;
    status_t err;
    const int yuvTexWidth = width;
    const int yuvTexHeight = height;
    const int yuvTexOffsetY = 0;
    const int yuvTexStrideY = (yuvTexWidth + 0xf) & ~0xf;
    const int yuvTexOffsetV = yuvTexStrideY * yuvTexHeight;
    const int yuvTexStrideV = (yuvTexStrideY/2 + 0xf) & ~0xf;
    const int yuvTexOffsetU = yuvTexOffsetV + yuvTexStrideV * yuvTexHeight/2;
    const int yuvTexStrideU = yuvTexStrideV;
    const bool yuvTexSameUV = false;
    int blockWidth = yuvTexWidth > 16 ? yuvTexWidth / 16 : 1;
    int blockHeight = yuvTexHeight > 16 ? yuvTexHeight / 16 : 1;
    err = yuvTexBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)(&buf));

    for (int x = 0; x < yuvTexWidth; x++) {
        for (int y = 0; y < yuvTexHeight; y++) {
                int parityX = (x / blockWidth) & 1;
                int parityY = (y / blockHeight) & 1;
                unsigned char intensity = (parityX ^ parityY) ? 63 : 191;
                buf[yuvTexOffsetY + (y * yuvTexStrideY) + x] = intensity;
                if (x < yuvTexWidth / 2 && y < yuvTexHeight / 2) {
                    buf[yuvTexOffsetU + (y * yuvTexStrideU) + x] = intensity;
                    if (yuvTexSameUV) {
                        buf[yuvTexOffsetV + (y * yuvTexStrideV) + x] = intensity;
                    } else if (x < yuvTexWidth / 4 && y < yuvTexHeight / 4) {
                        buf[yuvTexOffsetV + (y*2 * yuvTexStrideV) + x*2 + 0] =
                        buf[yuvTexOffsetV + (y*2 * yuvTexStrideV) + x*2 + 1] =
                        buf[yuvTexOffsetV + ((y*2+1) * yuvTexStrideV) + x*2 + 0] =
                        buf[yuvTexOffsetV + ((y*2+1) * yuvTexStrideV) + x*2 + 1] = intensity;
                    }
                }
        }
    }
    err = yuvTexBuffer->unlock();

    while (mExit == 0) {
        draw();

        glFlush();

        eglSwapBuffers(mEglDisplay, mEglSurface);

        curTime = systemTime(CLOCK_MONOTONIC) / 1000;
        diff = curTime - nowTime;
//        printf("timestamp %ld %ld render\n", curTime / 1000, diff / 1000);
        nowTime = curTime;
       addSyncSample(curTime);
       if (mEnoughSamples) {
                    calibrate();
        }
    }
    return 0;
}

void* DisplayExVSync::receiveThread(void *arg) {
    DisplayExVSync* displayVSnc = (DisplayExVSync*) arg;
    displayVSnc->threadLoop();
    return NULL;
}

bool DisplayExVSync::threadLoop()  {
    ALOGI("IrisVSyncEx thread start\n");
#ifdef TARGET_BUILD_DEBUG
    prctl(PR_SET_NAME, "IrisVSyncEx", 0, 0, 0);
#endif

   displayVsyncEvent(16, 16);
    ALOGI("IrisVSyncEx thread exit\n");
    return false;
}

void DisplayExVSync::addSyncSample(int64_t timestamp) {
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

void DisplayExVSync::calibrate() {
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


__attribute__((visibility("default"))) struct IDisplayVSync* allocateIDisplayVSync2() {
    return (struct IDisplayVSync*) new iris::DisplayExVSync();
}


#if defined(__cplusplus)
}
#endif
