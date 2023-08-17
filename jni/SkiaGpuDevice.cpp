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

#define LOG_TAG "SkiaGpuDevice"

#include <utils/Log.h>

#include <gpu/gl/GrGLInterface.h>
#include <gpu/GrContextOptions.h>
#include <gpu/GrDirectContext.h>

#include "SkiaGpuDevice.h"

#define FUNC_E() ALOGD("%s EEE", __FUNCTION__ )
#define FUNC_X() ALOGD("%s XXX", __FUNCTION__ )

namespace android {

SkiaGpuDevice::SkiaGpuDevice(int width, int height)
        : mWidth(width), mHeight(height) {
    FUNC_E();
}

SkiaGpuDevice::~SkiaGpuDevice() {
    FUNC_E();
}

void SkiaGpuDevice::onFirstRef() {
    FUNC_E();

    mInitOk = init() && initSkiaGL();
    if (!mInitOk) {
        mEGLEnv.destroy();
    }

    FUNC_X();
}

void SkiaGpuDevice::onLastStrongRef(const void */*id*/) {
    FUNC_E();

    if (!mInitOk) // No need
        return;

    eglMakeCurrent(mEGLEnv.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (mGrContext) {
        mGrContext->flushAndSubmit(true);
        mGrContext->abandonContext();
    }

    mEGLEnv.destroy();
}

bool SkiaGpuDevice::init() {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(display, nullptr, nullptr)) {
        ALOGE("failed to initialize EGL");
        return false;
    }

    // Choose config
    const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_STENCIL_SIZE, 0,
            EGL_NONE
    };

    EGLint num = 1;
    EGLConfig config = EGL_NO_CONFIG_KHR;
    eglChooseConfig(display, configAttribs, &config, 1, &num);
    if (num != 1 || config == EGL_NO_CONFIG_KHR) {
        ALOGE("failed to ChooseConfig");
        return false;
    }

    //Create context
    const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        ALOGE("eglCreateContext failed");
        return false;
    }

    //Create surface
    const EGLint surfaceAttribs[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, config, surfaceAttribs);

    EGLBoolean success = eglMakeCurrent(display, surface, surface, context);
    ALOGW_IF(!success, "eglMakeCurrent failed");

    if (success) {
        mEGLEnv.display = display;
        mEGLEnv.context = context;
        mEGLEnv.surface = surface;
    }

    return success;
}

bool SkiaGpuDevice::initSkiaGL() {
    sk_sp<const GrGLInterface> glInterface(GrGLMakeNativeInterface());
    if (glInterface == nullptr) {
        ALOGE("Can not create SKia GL Interface");
        return false;
    }

    GrContextOptions options;
    options.fDisableDistanceFieldPaths = true;
    mGrContext = GrDirectContext::MakeGL(std::move(glInterface), options);
    return true;
}

bool SkiaGpuDevice::initWithTarget(SkBitmap &targetBitmap) {
    if (!mInitOk) {
        ALOGE("initWithTarget failed, since precondition init NG.");
        return false;
    }

    SkImageInfo info = SkImageInfo::MakeN32Premul(targetBitmap.info().width(),
                                            targetBitmap.info().height(),
                                            sk_sp<SkColorSpace>(targetBitmap.info().colorSpace()));
    mSurface = SkSurface::MakeRenderTarget(mGrContext.get(), skgpu::Budgeted::kYes,
                                           info,0, kTopLeft_GrSurfaceOrigin,nullptr);
    if (!mSurface) {
        ALOGE("Can not MakeRenderTarget");
        return false;
    }

    return true;
}

void SkiaGpuDevice::flush() {
    if (mGrContext) {
        mGrContext->flushAndSubmit();
    }
}

void SkiaGpuDevice::submit() {
    if (mGrContext) {
        mGrContext->submit();
    }
}

void SkiaGpuDevice::checkAsyncCompletion() {
    if (mGrContext) {
        mGrContext->checkAsyncWorkCompletion();
    }
}

}