package com.droidlogic.imageplayer.decoder;

import android.graphics.ImageDecoder;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.HandlerThread;
import android.text.TextUtils;
import android.util.Log;
import android.util.Size;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.nio.ByteBuffer;

public class NetImageLoader {
    private static final String TAG = "NetImageLoader";
    private Handler mHandler;

    public static class ImageInfo {
        public boolean isAnimated;
        public Size size;
        public String memType;
    }

    public enum LoadPhase {
        INIT_URL(0, "Init URL"),
        CONNECTING(1, "Connecting url"),
        DOWNLOADING(2, "Downloading image"),
        LOADING(3, "Image is loading"),
        SUCCESS(4, "Image is loading OK"),
        FAILED(5, "Image is loading failed");


        LoadPhase(int phase, String description) {
            phaseInt = phase;
            phaseDescription = description;
        }

        @Override
        public String toString() {
            return name() + "(" + phaseInt + ")" + ": " + phaseDescription;
        }

        public final int phaseInt;
        public String phaseDescription;
    }

    public interface LoadCallback {
        void onPhase(LoadPhase phase, Drawable drawable, ImageInfo info);
    }

    public void start() {
        synchronized (NetImageLoader.class) {
            if (mHandler == null) {
                HandlerThread handlerThread = new HandlerThread("NetImageLoader.load");
                handlerThread.start();
                mHandler = new Handler(handlerThread.getLooper());
            }
        }
    }

    public void end() {
        synchronized (NetImageLoader.class) {
            if (mHandler != null) {
                mHandler.getLooper().quitSafely();
                mHandler = null;
            }
        }
    }

    public static boolean isNetImage(String url) {
        return url.startsWith("http") || url.startsWith("https");
    }

    public void load(final String urlStr, final LoadCallback callback) {
        if (TextUtils.isEmpty(urlStr) || callback == null || !isNetImage(urlStr)) {
            return;
        }

        try {
            callback.onPhase(LoadPhase.INIT_URL, null, null);
            URL url = new URL(urlStr);
            load(url, callback);
        } catch (MalformedURLException e) {
            Log.e(TAG, "Failed to create URL using path: " + urlStr, e);
            LoadPhase.FAILED.phaseDescription = e.getMessage();
            callback.onPhase(LoadPhase.FAILED, null, null);
        }
    }

    public void load(final URL url, final LoadCallback callback) {
        if (url == null || callback == null) {
            return;
        }

        mHandler.post(() -> {
            try {
                callback.onPhase(LoadPhase.CONNECTING, null, null);

                URLConnection urlConnection = url.openConnection();
                if (urlConnection != null) {
                    urlConnection.setDoInput(true);
                    urlConnection.setConnectTimeout(5000);
                    urlConnection.setReadTimeout(10000);
                    urlConnection.connect();

                    int contentLength = urlConnection.getContentLength();
                    LoadPhase.DOWNLOADING.phaseDescription = "0%";
                    callback.onPhase(LoadPhase.DOWNLOADING, null, null);

                    InputStream inputStream = urlConnection.getInputStream();
                    if (inputStream != null) {
                        ByteArrayOutputStream baos = new ByteArrayOutputStream();
                        byte[] buffer = new byte[1024];
                        int len = 0;
                        int progress = 0;
                        int totalRead = 0;
                        while ((len = inputStream.read(buffer)) >= 0) {
                            baos.write(buffer, 0, len);

                            totalRead += len;
                            int curProgress = (int) ((float) totalRead / contentLength * 100);
                            if (progress != curProgress) {
                                progress = curProgress;
                                LoadPhase.DOWNLOADING.phaseDescription =
                                        String.valueOf(progress).concat("%");
                                callback.onPhase(LoadPhase.DOWNLOADING, null, null);
                            }
                        }

                        byte[] bytes = baos.toByteArray();
                        if (bytes.length < contentLength) {
                            LoadPhase.FAILED.phaseDescription = "Download incomplete";
                            callback.onPhase(LoadPhase.FAILED, null, null);
                            inputStream.close();
                            baos.close();
                            return;
                        }

                        callback.onPhase(LoadPhase.LOADING, null, null);
                        final ImageInfo info = new ImageInfo();
                        Drawable decodeDrawable = ImageDecoder.decodeDrawable(
                                ImageDecoder.createSource(ByteBuffer.wrap(bytes)),
                                (imageDecoder, imageInfo, source) -> {
                                    info.isAnimated = imageInfo.isAnimated();
                                    info.size = Size.parseSize(imageInfo.getSize().toString());
                                    info.memType = imageInfo.getMimeType();
                                });

                        if (decodeDrawable != null) {
                            callback.onPhase(LoadPhase.SUCCESS, decodeDrawable, info);
                        } else {
                            LoadPhase.FAILED.phaseDescription = "Can not decode this image";
                            callback.onPhase(LoadPhase.FAILED, null, null);
                        }

                        inputStream.close();
                        baos.close();
                    } else {
                        LoadPhase.FAILED.phaseDescription = "Can not build download stream";
                        callback.onPhase(LoadPhase.FAILED, null, null);
                    }
                    return;
                }
            } catch (IOException e) {
                LoadPhase.FAILED.phaseDescription = e.getMessage();
            }

            callback.onPhase(LoadPhase.FAILED, null, null);
        });
    }
}
