package com.droidlogic.imageplayer.decoder;

import android.util.Log;
import android.webkit.MimeTypeMap;

import java.io.File;

public class BmpInfoFactory {
    public static final String TYPE_GIF = "gif";
    public static final String TYPE_APP = "application";
    public static final int BMP_SMALL_W = 1280;
    public static final int BMP_SMALL_H = 720;
    private static final String TAG = "BmpInfoFactory";

    public static BmpInfo getBmpInfo(String filePath) {
        String mimeType = getMimeType(filePath);
        Log.d(TAG, "getBmpInfo, mimeType= " + mimeType);
        if (mimeType.contains("application")) {
            return null;
        } else if (mimeType.contains(TYPE_GIF)) {
            GifBmpInfo info = new GifBmpInfo();
            if (!info.setDataSource(filePath)) {
                info.release();
                return new BmpInfo();
            }
            return info;
        }
        return new BmpInfo();
    }

    public static BmpInfo getStaticBmpInfo() {
        return new BmpInfo();
    }

    public static String getMimeType(String name) {
        final int lastDot = name.lastIndexOf('.');
        if (lastDot >= 0) {
            final String extension = name.substring(lastDot + 1);
            Log.d(TAG, "getMimeType, name= " + name + ", extension= " + extension);
            final String mimeType =
                    MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension.toLowerCase());
            if (mimeType != null) {
                return mimeType;
            }
        }
        return "application/octet-stream";
    }
}
