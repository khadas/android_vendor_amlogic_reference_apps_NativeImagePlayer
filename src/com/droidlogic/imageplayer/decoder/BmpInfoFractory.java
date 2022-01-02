package com.droidlogic.imageplayer.decoder;

import android.util.Log;
import android.webkit.MimeTypeMap;

import java.io.File;

public class BmpInfoFractory {
    public static final String TYPE_GIF = "gif";
    public static final String TYPE_APP = "application";

    public static BmpInfo getBmpInfo(String filePath) {
        String mimeType = getMimeType(filePath);
        if (mimeType.contains("application")) {
            return null;
        } else if (mimeType.contains(TYPE_GIF)) {
            return new GifBmpInfo();
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
            final String mimeType =
                    MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension.toLowerCase());
            if (mimeType != null) {
                return mimeType;
            }
        }
        return "application/octet-stream";
    }
}
