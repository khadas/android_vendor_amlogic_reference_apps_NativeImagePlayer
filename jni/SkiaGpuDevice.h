/******************************************************************
 *
 *Copyright (C) 2012  Amlogic, Inc.
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 ******************************************************************/

#ifndef ANDROID_CPP_PROJECTS_SKIAGPUDEVICE_H
#define ANDROID_CPP_PROJECTS_SKIAGPUDEVICE_H

#include <utils/RefBase.h>
#include <SkCanvas.h>
#include <SkSurface.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace android {
class SkiaGpuDevice : public RefBase {
public:
    SkiaGpuDevice(int width, int height);
    virtual ~SkiaGpuDevice();

    sk_sp<SkSurface> getSurface() { return mSurface; }
    bool initCheck() { return mInitOk && mSurface != nullptr; }
    bool initWithTarget(SkBitmap& targetBitmap);
    void flush();
    void submit();
    void checkAsyncCompletion();

private:
    struct EGLEnv {
        EGLDisplay display{EGL_NO_DISPLAY};
        EGLContext context{EGL_NO_CONTEXT};
        EGLSurface surface{EGL_NO_SURFACE};

    public:
        void destroy() {
            if (display == EGL_NO_DISPLAY)
                return;

            if (surface) {
                eglDestroySurface(display, surface);
                surface = EGL_NO_SURFACE;
            }

            if (context) {
                eglDestroyContext(display, context);
                context = EGL_NO_CONTEXT;
            }

            eglTerminate(display);
            display = EGL_NO_DISPLAY;
            eglReleaseThread();
        }
    } mEGLEnv;

private:
    virtual void onFirstRef() override;
    virtual void onLastStrongRef(const void *id) override;

    bool init();
    bool initSkiaGL();

private:
    int mWidth;
    int mHeight;
    bool mInitOk{false};

    sk_sp<GrDirectContext> mGrContext;
    sk_sp<SkSurface> mSurface;
};
}


#endif //ANDROID_CPP_PROJECTS_SKIAGPUDEVICE_H
