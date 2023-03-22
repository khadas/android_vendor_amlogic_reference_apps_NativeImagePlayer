/* DO NOT EDIT THIS FILE - it is machine generated */
#include "ImageOperator.h"
#include "core_jni_helpers.h"
#include <jni/Bitmap.h>
#include <cutils/ashmem.h>
#include <media/stagefright/foundation/ADebug.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <math.h>
#include <am_gralloc_uvm_ext.h>
#include <am_gralloc_ext.h>
#include "android-base/parseint.h"
#include "android-base/properties.h"
#include "Bitmap.h"
#include "ImageEffector.h"

using namespace android;
//using namespace android::bitmap;
#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080
#define GRALLOC1_VIDEO 1ULL << 17
#define PROPERTY "vendor.display-size"
#define PROP_DUMP_IMAGE_PATH "debug.image_player.dump_path"
#define ODS_WIDTH_PROPERTY "ro.surface_flinger.max_graphics_width"
#define ODS_HEIGHT_PROPERTY "ro.surface_flinger.max_graphics_height"
sp<ANativeWindow> mNativeWindow = nullptr;
ImageOperator *imageplayer;
sp<ImageEffector> mEffector;
JNIEnv *jnienv;
int fenceFd = -1;
int mRotate = 0;
int mFrameWidth = 0;
int mFrameHeight = 0;
int mOsdWidth = 0;
int mOsdHeight = 0;
int mLastWidth = 0;
int mLastHeight = 0;
int mLastTransform = 0;
static bool isShown;
SharedMemoryProxy mMemory;
static jfieldID mImagePlayer_ScreenWidth;
static jfieldID mImagePlayer_ScreenHeight;
static jfieldID mImagePlayer_VideoScale;


static auto am_gralloc_get_omx_osd_producer_usage = []()->uint64_t {
    enum BufferUsage {
        GPU_RENDER_TARGET = 1 << 9,
        VIDEO_DECODER = 1 << 22,
    };

    return static_cast<uint64_t>(BufferUsage::VIDEO_DECODER | BufferUsage::GPU_RENDER_TARGET);
};

extern bool am_gralloc_set_uvm_buf_usage(const native_handle_t * hnd, int usage);
extern void rgbToYuv420(uint8_t* rgbBuf, size_t width, size_t height, uint8_t* yPlane,
        uint8_t* crPlane, uint8_t* cbPlane, size_t chromaStep, size_t yStride, size_t chromaStride, SkColorType colorType);

static bool setUvmUsage(ANativeWindowBuffer *buf, int usage) {
    return am_gralloc_set_uvm_buf_usage(buf->handle, usage);
}

static std::string& StringTrim(std::string &str)
{
    if (str.empty()) {
        return str;
    }
    str.erase(0, str.find_first_not_of(" "));
    str.erase(str.find_last_not_of(" ") + 1);
    return str;
}
static int StringSplit(std::vector<std::string>& dst, const std::string& src, const std::string& separator)
{
    if (src.empty() || separator.empty())
        return 0;

    int nCount = 0;
    std::string temp;
    size_t pos = 0, offset = 0;

    while ((pos = src.find_first_of(separator, offset)) != std::string::npos)
    {
        temp = src.substr(offset, pos - offset);
        if (temp.length() > 0) {
            dst.push_back(StringTrim(temp));
            nCount ++;
        }
        offset = pos + 1;
    }

    temp = src.substr(offset, src.length() - offset);
    if (temp.length() > 0) {
        dst.push_back(StringTrim(temp));
        nCount ++;
    }

    return nCount;
}
static int reRender(int32_t width, int32_t height, void *data, size_t inLen,  SkColorType colorType);
static int render(int32_t width, int32_t height, void *data, size_t inLen,  SkColorType colorType);

static int initVideoParam(JNIEnv *env, jobject entity) {
    ALOGE("initVideoParam");
    jclass imageplayer_class = FindClassOrDie(env,"com/droidlogic/imageplayer/decoder/ImagePlayer");
    jfieldID mImagePlayer_ScreenWidth = GetFieldIDOrDie(env,imageplayer_class,"mScreenWidth","I");
    jfieldID mImagePlayer_ScreenHeight = GetFieldIDOrDie(env,imageplayer_class,"mScreenHeight","I");
    int javaHeight = env->GetIntField(entity,mImagePlayer_ScreenHeight);
    int javaWidth = env->GetIntField(entity,mImagePlayer_ScreenWidth);
    ALOGE("last solution %d x%d",javaWidth,javaHeight);
    mFrameHeight = (int)env->GetIntField(entity, mImagePlayer_ScreenHeight);
    mFrameWidth = (int)env->GetIntField(entity,mImagePlayer_ScreenWidth);
    ALOGE("get width %dx%d",mFrameWidth,mFrameHeight);
    return 0;
}
static int initParam(JNIEnv *env, jobject entity) {
    ALOGE("imageplayer_initialParam");
    const std::string path(PROPERTY);
    const std::string v = android::base::GetProperty(path.c_str(), "1920x1080");
    if (!v.empty()) {
        std::vector<std::string> axis;
        int count=StringSplit(axis,v,"x");
        ALOGE("count= %d",count);
        if (count != 2) {
            mFrameWidth = DEFAULT_WIDTH;
            mFrameHeight = DEFAULT_HEIGHT;
        }else {
            ALOGE("read axis %s %s",axis[0].c_str(),axis[1].c_str());
            mFrameWidth = atoi(axis[0].c_str());
            mFrameHeight = atoi(axis[1].c_str());
        }
        ALOGE("read axis %s %d %d",v.c_str(),mFrameWidth,mFrameHeight);
    }

    jclass imageplayer_class = FindClassOrDie(env,"com/droidlogic/imageplayer/decoder/ImagePlayer");
    jfieldID mImagePlayer_ScreenWidth = GetFieldIDOrDie(env,imageplayer_class,"mScreenWidth","I");
    jfieldID mImagePlayer_ScreenHeight = GetFieldIDOrDie(env,imageplayer_class,"mScreenHeight","I");
    int lastHeight = env->GetIntField(entity,mImagePlayer_ScreenHeight);
    int lastWidth = env->GetIntField(entity,mImagePlayer_ScreenWidth);
    ALOGE("last solution %d x%d",lastWidth,lastHeight);
    bool ret = -1;
    if (lastHeight > 0 && lastWidth > 0 && (lastHeight != mFrameHeight || lastWidth != mFrameWidth)) {
        ret = 0;
    }
    env->SetIntField(entity, mImagePlayer_ScreenWidth, mFrameWidth);
    lastWidth = env->GetIntField(entity,mImagePlayer_ScreenWidth);
    ALOGE("get width %d",lastWidth);
    env->SetIntField(entity, mImagePlayer_ScreenHeight,mFrameHeight);
    ALOGE("after initial param");
    return ret;
}
void bindSurface(JNIEnv *env, jobject imageobj, jobject jsurface, jint surfaceWidth, jint surfaceHeight){
    sp<IGraphicBufferProducer> new_st = NULL;
    jnienv = env;
    mOsdWidth = surfaceWidth;
    mOsdHeight = surfaceHeight;
    if (jsurface) {
        sp<Surface> surface(android_view_Surface_getSurface(env, jsurface));
        if (surface != NULL) {
            new_st = surface->getIGraphicBufferProducer();

            if (new_st == NULL) {
                jniThrowException(env, "java/lang/IllegalArgumentException",
                                  "The surface does not have a binding SurfaceTexture!");
                return;
            }

        } else {
            jniThrowException(env, "java/lang/IllegalArgumentException",
                              "The surface has been released");
            return;
        }
    }

    if (new_st != NULL) {
        mNativeWindow = new Surface(new_st,/*controlledByApp*/false);
        if (mEffector) mEffector.clear();
        mEffector = new ImageEffector(surfaceWidth, surfaceHeight);
        ALOGI("before connect");
        native_window_api_connect(mNativeWindow.get(),NATIVE_WINDOW_API_MEDIA);
        ALOGI("set native window overlay %0x",GRALLOC1_PRODUCER_USAGE_CAMERA);
        CHECK_EQ(0,native_window_set_usage(mNativeWindow.get(), am_gralloc_get_omx_osd_producer_usage()));
        //CHECK_EQ(0,native_window_set_usage(mNativeWindow.get(), am_gralloc_get_video_decoder_full_buffer_usage()));
        //CHECK_EQ(0,native_window_set_usage(mNativeWindow.get(), GRALLOC1_PRODUCER_USAGE_CAMERA));
        native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
        //CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_FREEZE));
        native_window_set_buffers_format(mNativeWindow.get(), HAL_PIXEL_FORMAT_YCrCb_420_SP);
        native_window_set_auto_refresh(mNativeWindow.get(),true);
        imageplayer = new ImageOperator(env);
        imageplayer->setSurfaceSize(mFrameWidth,mFrameHeight);
        native_window_set_buffer_count(mNativeWindow.get(),4);
        isShown = true;
    }
}
void unbindSurface(){
    ALOGE("unbindSurface");
    isShown = false;
    mRotate = 0;
    mFrameWidth = 0;
    mFrameHeight = 0;
    mOsdWidth = 0;
    mOsdHeight = 0;
    mLastWidth = 0;
    mLastHeight = 0;
    mLastTransform = 0;
    mLastTransform = 0;
    imageplayer->stopShown();
    if (!mMemory.getIsUsed()) {
        mMemory.releaseMem();
    }
    if (mNativeWindow != NULL) {
       // CHECK_EQ(OK,native_window_api_disconnect(mNativeWindow.get(), NATIVE_WINDOW_API_EGL));
    }
    if (mEffector) {
        mEffector.clear();
    }
}
static int rotate(JNIEnv *env, jclass clz,jint rotation, jboolean redraw) {
    ALOGD("ImageEffector, %s", __FUNCTION__ );
    int ret = 0;
    if (mEffector->rotate(rotation, true, redraw)) {
        auto bitmap = mEffector->bitmap();
        uint8_t* pixelBuffer = (uint8_t*)bitmap.getPixels();
        ret = render(bitmap.width(),bitmap.height(),pixelBuffer,
                         bitmap.width()*bitmap.height(),bitmap.colorType());
    }
    return ret;

    /*ALOGD("rotate  %d rotation/90=%d",rotation,rotation/90);
    int transform = 0;
    switch ((rotation/90) % 4) {
        case 1: transform = HAL_TRANSFORM_ROT_90;break;
        case 2: transform = HAL_TRANSFORM_ROT_180;break;
        case 3: transform = HAL_TRANSFORM_ROT_270;break;
        default: transform = 0;break;
    }
    ALOGD("rotate  %d %d",transform,rotation);
    native_window_set_buffers_transform(mNativeWindow.get(), transform);
    mLastTransform = transform;
    if (redraw) {
        SkBitmap bmp;
        imageplayer->getSelf(bmp);
        uint8_t* pixelBuffer = (uint8_t*)bmp.getPixels();
        int ret = reRender(bmp.width(),bmp.height(),pixelBuffer,bmp.width()*bmp.height(),bmp.colorType());
        bmp.reset();
    }*/
}
static int reRender(int32_t width, int32_t height, void *data, size_t inLen, SkColorType colorType) {
    status_t err = NO_ERROR;
    int frame_width = ((width + 1) & ~1);
    int frame_height =((height + 1) & ~1);
    ALOGE("Repeat render for frame size (%d x %d)-->(%d x %d)",width,height,frame_width,frame_height);
    ANativeWindowBuffer *buf;
    native_window_set_buffers_dimensions(mNativeWindow.get(), frame_width, frame_height);
    //status_t res =  mNativeWindow->dequeueBuffer(mNativeWindow.get(),&buf,&fenceFd);
    err =  native_window_dequeue_buffer_and_wait(mNativeWindow.get(),&buf);
    if (err != OK) {
        ALOGE("%s:reRender Dequeue buffer failed: %s (%d)", __FUNCTION__, strerror(-err), err);
        switch (err) {
            case NO_INIT:
                jniThrowException(jnienv, "java/lang/IllegalStateException",
                    "Surface has been abandoned");
                break;
            default:
                jniThrowRuntimeException(jnienv, "dequeue buffer failed");
        }
        return err;
    }
    ALOGE("-->buf %d %d  %d %d %p",buf->format,buf->stride,buf->width, buf->height, buf);
    setUvmUsage(buf, 1);
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds(frame_width,frame_height);
    uint8_t* img = NULL;
    uint64_t usage = am_gralloc_get_omx_osd_producer_usage() |
                     GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN;
    err =  mapper.lock(buf->handle,usage,bounds,(void**)(&img));
    if (err != NO_ERROR) {
        ALOGE("%s: Error trying to lock output buffer fence: %s (%d)", __FUNCTION__,
                strerror(-err), err);
        return err;
    }
    if (img == NULL) return -1;
    if (mMemory.getSize() > 0) {
        ALOGE("recopy %d %d %d",mMemory.getSize(),buf->height, buf->stride);
        mMemory.setUsed(true);
        memcpy(img,mMemory.getmem(),mMemory.getSize());
        mMemory.setUsed(false);
    }else{
        memset(img, 128, buf->height * buf->stride*3/2);
       // memset(img, 0 , buf->stride *  buf->height);
        uint8_t* yPlane = img;
        uint8_t* uPlane = img + buf->stride*buf->height;
        uint8_t* vPlane = uPlane + 1;
        size_t chromaStep = 2;
        size_t yStride = buf->stride;
        size_t chromaStride =  buf->stride;
        uint8_t* pixelBuffer = (uint8_t*)data;
        imageplayer->rgbToYuv420(pixelBuffer, width, height, yPlane,
                        uPlane, vPlane, chromaStep, yStride, chromaStride, colorType);
        if (!mMemory.allocmem(buf->height * buf->stride*3/2)) {
            mMemory.setUsed(true);
            memcpy(mMemory.getmem(),img,buf->height * buf->stride*3/2);
            mMemory.setUsed(false);
        }
    }
    mapper.unlock(buf->handle);
    mNativeWindow->queueBuffer(mNativeWindow.get(), buf,  -1);

    return 0;
}
static int render(int32_t width, int32_t height, void *data, size_t inLen, SkColorType colorType ) {
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    status_t err = NO_ERROR;
    int frame_width = ((width + 1) & ~1);
    int frame_height =((height + 1) & ~1);
    ALOGE("render for frame size (%d x %d)-->(%d x %d)",width,height,frame_width,frame_height);
    ANativeWindowBuffer *buf;
    native_window_set_buffers_dimensions(mNativeWindow.get(), frame_width, frame_height);
    //status_t res =  mNativeWindow->dequeueBuffer(mNativeWindow.get(),&buf,&fenceFd);
    err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(),&buf);
    if (err != NO_ERROR) {
        ALOGE("%s: Dequeue buffer failed: %s (%d)", __FUNCTION__, strerror(-err), err);
        return err;
    }
    setUvmUsage(buf, 1);
    ALOGE("renderBuf %d %d  %d %d %d",buf->format,buf->stride,buf->width, buf->height, fenceFd);
    Rect bounds(frame_width,frame_height);
    uint8_t* img = NULL;
    uint64_t usage = am_gralloc_get_omx_osd_producer_usage() |
            GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN;
    err =  mapper.lock(buf->handle,usage, bounds,(void**)(&img));
    if (err != NO_ERROR) {
        ALOGE("%s: Error trying to lock output buffer fence: %s (%d)", __FUNCTION__,
                strerror(-err), err);
        return err;
    }
    if (img == NULL) return -1;
    memset(img, 128, buf->height * buf->stride*3/2);
    uint8_t* yPlane = img;
    uint8_t* uPlane = img + buf->stride*buf->height;
    uint8_t* vPlane = uPlane + 1;
    size_t chromaStep = 2;
    size_t yStride = buf->stride;
    size_t chromaStride =  buf->stride;
    uint8_t* pixelBuffer = (uint8_t*)data;
    if (!isShown) return -1;
    imageplayer->rgbToYuv420(pixelBuffer, width, height, yPlane,
                    uPlane, vPlane, chromaStep, yStride, chromaStride, colorType);

    const std::string dumpPath = android::base::GetProperty(std::string(PROP_DUMP_IMAGE_PATH), "");
    bool dump = !dumpPath.empty() && (access(dumpPath.c_str(), F_OK) == 0);
    ALOGI("Debug path: %s, dump: %d", dumpPath.c_str(), dump);
    if (dump) {
        imageplayer->saveBmp((const char*)img,dumpPath.c_str(), buf->height* buf->stride*3/2);
    }
//    mMemory.releaseMem();
//    if (isShown && !mMemory.allocmem(buf->height*buf->width *3)) {
//        ALOGE("----------allocate----------");
//        mMemory.setUsed(true);
//        memset(mMemory.getmem(),Y_PIXEL_BLACK,buf->height * buf->stride*3);
//        memcpy(mMemory.getmem(),img,buf->height * buf->stride*3);
//        mMemory.setUsed(false);
//    }
    mapper.unlock(buf->handle);

    mNativeWindow->queueBuffer(mNativeWindow.get(), buf,  -1);
    img = NULL;
    return NO_ERROR;
}

static int renderEmpty(int32_t width, int32_t height) {
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    status_t err = NO_ERROR;
    int frame_width = ((width + 1) & ~1);
    int frame_height =((height + 1) & ~1);
    ALOGE("render Empty for frame size (%d x %d)-->(%d x %d)",width,height,frame_width,frame_height);
    ANativeWindowBuffer *buf;
    native_window_set_buffers_dimensions(mNativeWindow.get(), frame_width, frame_height);
    err =  native_window_dequeue_buffer_and_wait(mNativeWindow.get(),&buf);
    if (err != OK) {
        ALOGE("%s: Dequeue buffer failed: %s (%d)", __FUNCTION__, strerror(-err), err);
        return err;
    }
    setUvmUsage(buf, 1);
    ALOGE("renderBuf %d %d  %d %d %d",buf->format,buf->stride,buf->width, buf->height, fenceFd);
    Rect bounds(frame_width,frame_height);
    uint8_t* img = NULL;
    uint64_t usage = am_gralloc_get_omx_osd_producer_usage() |
                     GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN;
    err = mapper.lock(buf->handle,usage,bounds,(void**)(&img));
    if (err != OK) {
        ALOGE("%s: Error trying to lock output buffer fence: %s (%d)", __FUNCTION__,
                strerror(-err), err);
        return err;
    }
    if (img == NULL) return -1;
    memset(img, 16, buf->height * buf->stride);
    memset(img+buf->height * buf->stride, 128, buf->height * buf->stride*1/2);
    if (!isShown) return -1;
    mapper.unlock(buf->handle);

    mNativeWindow->queueBuffer(mNativeWindow.get(), buf,  -1);
    img = NULL;
    return 0;
}

static jint nativeShow(JNIEnv *env, jclass clz, jlong jbitmap, int fit, jboolean movie){
    ALOGE("nativeShow %d %d %d",(mNativeWindow.get() == NULL),(imageplayer == NULL),(jbitmap ==0));

    if (mNativeWindow.get() == NULL || imageplayer == NULL || jbitmap ==0 ) {
        return -1;
    }

    SkBitmap skbitmap;
    imageplayer->init(jbitmap,0,true) ;
    VBitmap* bitmap = reinterpret_cast<VBitmap*>(jbitmap);

    bitmap->getSkBitmap(&skbitmap);
    ALOGE("skColorType %d %d %d",bitmap->colorType(),bitmap->width(),bitmap->height());

    if (mEffector->setImage(&skbitmap, fit, true, movie)) {
        auto bmp = mEffector->bitmap();
        uint8_t* pixelBuffer = (uint8_t*)mEffector->bitmap().getPixels();
        mLastWidth = 0;
        mLastHeight = 0;
        return render(bmp.width(),bmp.height(),pixelBuffer,bmp.width() * bmp.height(),bmp.colorType());
    }

    return 0;
}
static jint rotateScaleCrop(JNIEnv* env, jclass clz,jint rotation, jfloat sx,jfloat sy, jboolean redraw) {
    ALOGD("ImageEffector, %s", __FUNCTION__ );
    int ret = 0;
    if (mEffector->rotate(rotation, true, redraw)) {
        auto bitmap = mEffector->bitmap();
        uint8_t* pixelBuffer = (uint8_t*)bitmap.getPixels();
        ret = render(bitmap.width(),bitmap.height(),pixelBuffer,bitmap.width()*bitmap.height(),bitmap.colorType());
    }
/*
    int transform = 0;
    switch ((rotation/90) % 4) {
        case 1: transform = HAL_TRANSFORM_ROT_90;break;
        case 2: transform = HAL_TRANSFORM_ROT_180;break;
        case 3: transform = HAL_TRANSFORM_ROT_270;break;
        default: transform = 0;break;
    }
    native_window_set_buffers_transform(mNativeWindow.get(), transform);
    SkBitmap bmp;
    imageplayer->getSelf(bmp);

    int compWidth = mFrameWidth;
    int compHeight = mFrameHeight;
    int height = (int)ceil(sy*bmp.height());
    int width = (int)ceil(sx*bmp.width());
    ALOGD("rotateScaleCrop  %d %d pic:[%dx%d] lastpic attr:[%dx%d]",transform,rotation,width,height,mLastWidth,mLastHeight);
    if (bmp.width() <= mOsdWidth && bmp.height() <= mOsdHeight) {
        compWidth = mOsdWidth;
        compHeight = mOsdHeight;
    }
     ALOGD("compSize:  [%dx%d]",compWidth, compHeight);
    if (width < compWidth && height < compHeight) {
        CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_FREEZE));
        native_window_set_buffers_dimensions(mNativeWindow.get(), width, height);
        if (mLastWidth > compWidth || mLastHeight > compHeight) {
            CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));
            native_window_set_crop(mNativeWindow.get(), NULL);
            if (redraw) {
                uint8_t* pixelBuffer = (uint8_t*)bmp.getPixels();
                ret = reRender(bmp.width(),bmp.height(),pixelBuffer,bmp.width()*bmp.height(),bmp.colorType());
            }
        }else {
            if (mLastTransform != transform) {
                CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));
                if (redraw) {
                    uint8_t* pixelBuffer = (uint8_t*)bmp.getPixels();
                    ret = reRender(bmp.width(),bmp.height(),pixelBuffer,bmp.width()*bmp.height(),bmp.colorType());
                }
            }
        }
    } else {
        float scaleStep = 1.0f*compHeight/height > 1.0f*compWidth/width ? 1.0f*compHeight/height : 1.0f*compWidth/width;
        native_window_set_buffers_dimensions(mNativeWindow.get(), compHeight, compWidth);
        CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));
        int left = (int)ceil((compWidth-scaleStep*compWidth)/2);
        int right = (int)((compWidth+scaleStep*compWidth)/2);
        int top = (int)ceil((compHeight-scaleStep*compHeight)/2);
        int bottom = (int)ceil((compHeight+scaleStep*compHeight)/2);
        android_native_rect_t standard = {left,top,right,bottom};
        native_window_set_crop(mNativeWindow.get(), &standard);
        ALOGE("rotateScaleCrop  [%d %d]%f [%d %d %d %d]",bmp.width(),bmp.height(),scaleStep,left,top,right,bottom);
        if (redraw) {
            uint8_t* pixelBuffer = (uint8_t*)bmp.getPixels();
            ret =  reRender(bmp.width(),bmp.height(),pixelBuffer,bmp.width()*bmp.height(),bmp.colorType());
        }
    }
    bmp.reset();
    mLastWidth = width;
    mLastHeight = height;
    mLastTransform = transform;*/
    return ret;
}

void nativeReset(JNIEnv *env,jclass clz) {
    if (mNativeWindow != nullptr) {
        native_window_set_buffers_transform(mNativeWindow.get(), 0);
        //renderEmpty(1920,1080);
        ALOGD("nativeReset----->");
    }
}

void nativeRestore(JNIEnv *env,jclass clz) {
    if (mEffector) {
        mEffector->reset();
        auto bmp = mEffector->bitmap();
        render(bmp.width(),bmp.height(),(uint8_t*)bmp.getPixels(),bmp.width()*bmp.height(),bmp.colorType());
    }
}

void nativeRelease(JNIEnv *env,jclass clz) {
//    if (mEffector) {
//        mEffector.clear();
//    }
}

static jint nativeScale(JNIEnv *env, jclass clz,jfloat sx,jfloat sy,jboolean redraw) {
    /* SkBitmap bmp;
     imageplayer->getSelf(bmp);
     int compWidth = mFrameWidth;
     int compHeight = mFrameHeight;
     int height = (int)ceil(sy*bmp.height());
     int width = (int)ceil(sx*bmp.width());

     if (bmp.width() <= mOsdWidth && bmp.height() <= mOsdHeight) {
         compWidth = mOsdWidth;
         compHeight = mOsdHeight;
     }
     if (width < compWidth && height < compHeight) {
         CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_FREEZE));
         native_window_set_buffers_dimensions(mNativeWindow.get(), width, height);
         if (mLastWidth > compWidth || mLastHeight > compHeight) {
             CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));
             if (redraw) {
                 uint8_t* pixelBuffer = (uint8_t*)bmp.getPixels();
                 native_window_set_crop(mNativeWindow.get(), NULL);
                 ret = reRender(bmp.width(),bmp.height(),pixelBuffer,bmp.width()*bmp.height(),bmp.colorType());
             }
         }
     } else {
         float scaleStep = 1.0f*compHeight/height > 1.0f*compWidth/width ? 1.0f*compHeight/height : 1.0f*compWidth/width;
         native_window_set_buffers_dimensions(mNativeWindow.get(), compHeight, compWidth);
         CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));
         int left = (int)ceil((compWidth-scaleStep*compWidth)/2);
         int right = (int)((compWidth+scaleStep*compWidth)/2);
         int top = (int)ceil((compHeight-scaleStep*compHeight)/2);
         int bottom = (int)ceil((compHeight+scaleStep*compHeight)/2);
         android_native_rect_t standard = {left,top,right,bottom};
         native_window_set_crop(mNativeWindow.get(), &standard);
         ALOGE("nativeScale [%d %d %d %d] %d",scaleStep,left,top,right,bottom);
         if (redraw) {
             uint8_t* pixelBuffer = (uint8_t*)bmp.getPixels();
             ret =  reRender(bmp.width(),bmp.height(),pixelBuffer,bmp.width()*bmp.height(),bmp.colorType());
         }
     }
     bmp.reset();
     mLastWidth = width;
     mLastHeight = height;*/

    ALOGD("ImageEffector, %s", __FUNCTION__ );

    int ret = 0;
    if (mEffector->scale(sx, sy, redraw)) {
        auto bmp = mEffector->bitmap();
        ret =  render(bmp.width(),bmp.height(),(uint8_t*)bmp.getPixels(),bmp.width()*bmp.height(),bmp.colorType());
    }
    return ret;
}

static int nativeTranslate (JNIEnv *env, jclass clz, jfloat x, jfloat y, jboolean redraw) {
    int ret = 0;
    if (mEffector->translate(x, y, redraw)) {
        auto bmp = mEffector->bitmap();
        ret =  render(bmp.width(),bmp.height(),(uint8_t*)bmp.getPixels(),bmp.width()*bmp.height(),bmp.colorType());
    }
    return ret;
}

static int transform(JNIEnv *env, jclass clz, jint rotation, jfloat sx, jfloat sy,
                jint t,jint b, jint l,jint r,jint gap, jboolean redraw) {
    int ret = 0;

    //native_window_set_buffers_transform(mNativeWindow.get(), transform);
    SkBitmap bmp;
    imageplayer->getSelf(bmp);
    int height = (int)ceil(sy*bmp.height());
    int width = (int)ceil(sx*bmp.width());
    ALOGE("transform heightxwidth [%d x%d] %f",height,width,sy);

    int compWidth = mFrameWidth;
    int compHeight = mFrameHeight;
    if (bmp.width() <= mOsdWidth && bmp.height() <= mOsdHeight) {
        compWidth = mOsdWidth;
        compHeight = mOsdHeight;
    }
    if (height < compWidth && width < compHeight) {
        return -1;
    }

    ALOGE("transform Rotate %d [%d x%d]",rotation,compWidth,compHeight);
    float scaleStep = 1.0f*compHeight/height > 1.0f*compWidth/width ? 1.0f*compHeight/height : 1.0f*compWidth/width;
    native_window_set_buffers_dimensions(mNativeWindow.get(), compHeight, compWidth);
    CHECK_EQ(OK, native_window_set_scaling_mode(mNativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));
    int left = (int)ceil((compWidth-scaleStep*compWidth)/2);
    int right = (int)((compWidth+scaleStep*compWidth)/2);
    int top = (int)ceil((compHeight-scaleStep*compHeight)/2);
    int bottom = (int)ceil((compHeight+scaleStep*compHeight)/2);
    int widthCrop = right - left;
    int heightCrop = bottom - top;
    ALOGE("transform from %f [%d %d %d %d] axis[ %d %d %d %d]",scaleStep,top,bottom,left,right,t,b,l,r);
    left = (left+ gap*l);
    right = (right + gap*r);
    top = (top + gap*t);
    bottom = (bottom + gap*b);
    if (left < 0 && (left+gap) > 0) {
        left = 0;
        right = left+widthCrop;
        ret = -1;
    }
    if (top < 0 && (top+gap) > 0) {
        top = 0;
        bottom = top+heightCrop;
        ret = -1;
    }
    if (right > compWidth) {
        right = compWidth;
        left = right - widthCrop;
        ret = -1;
    }
    if (bottom > compHeight) {
        bottom = compHeight;
        top = bottom - heightCrop;
        ret = -1;
    }
    android_native_rect_t standard = {left,top,right,bottom};
    native_window_set_crop(mNativeWindow.get(), &standard);
    ALOGE("transform to [%d %d %d %d] %d ",top,bottom,left,right,ret);
    if (redraw) {
        uint8_t* pixelBuffer = (uint8_t*)bmp.getPixels();
        reRender(bmp.width(),bmp.height(),pixelBuffer,bmp.width()*bmp.height(),bmp.colorType());
    }
    bmp.reset();
    return ret;
}
static const JNINativeMethod gImagePlayerMethod[] = {
    {"initParam",           "()I",     (void*)initParam},
    {"bindSurface",           "(Landroid/view/Surface;II)V",          (void*)bindSurface },
    {"nativeUnbindSurface",           "()V",          (void*)unbindSurface },
    {"nativeShow",                "(JIZ)I",                               (void*)nativeShow },
    {"nativeScale",               "(FFZ)I",                             (void*)nativeScale},
    {"nativeRotate",              "(IZ)I",                               (void*)rotate},
    {"nativeRotateScaleCrop",           "(IFFZ)I",                      (void*)rotateScaleCrop},
    {"nativeTransform",          "(IFFIIIII)I",                               (void*)transform},
    {"nativeTranslate",          "(FFZ)I",                               (void*)nativeTranslate},
    {"nativeReset",                "()V",     (void*)nativeReset},
    {"nativeRelease",                "()V",     (void*)nativeRelease},
    {"nativeRestore",                "()V",     (void*)nativeRestore},
    {"initVideoParam",           "()I",     (void*)initVideoParam},
};

int register_com_droidlogic_imageplayer_decoder_ImagePlayer(JNIEnv* env) {
    return android::RegisterMethodsOrDie(env, "com/droidlogic/imageplayer/decoder/ImagePlayer", gImagePlayerMethod,
                                         NELEM(gImagePlayerMethod));
}
