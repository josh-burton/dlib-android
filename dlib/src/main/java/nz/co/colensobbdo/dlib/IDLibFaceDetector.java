// Copyright (c) 2017-present boyw165
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
//    The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

package nz.co.colensobbdo.dlib;

import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Rect;

import com.google.protobuf.InvalidProtocolBufferException;

public interface IDLibFaceDetector {

    boolean isEnabled();

    void setEnabled(boolean enabled);

    boolean isFaceDetectorReady();

    boolean isFaceRecognitionDetectorReady();

    boolean isFaceLandmarksDetectorReady();

    /**
     * Prepare (deserialize the graph) the face detector.
     */
    void prepareFaceDetector();

    /**
     * Prepare the face recognition detector.
     *
     * @param path The model (serialized graph) file.
     */
    void prepareFaceRecognitionDetector(String path);


    /**
     * Prepare the face landmarks detector.
     *
     * @param path The model (serialized graph) file.
     */
    void prepareFaceLandmarksDetector(AssetManager assetManager, String path);

    /**
     * Detect the face landmarks in the given face bound (single face).
     *
     * @param bitmap The given photo.
     * @param bound  The boundary of the face.
     * @return A list of {@link DLibFace.Landmark}.
     * @throws InvalidProtocolBufferException Fired if the message cannot be
     *                                        recognized
     */
    DLibFace findLandmarksFromFace(Bitmap bitmap,
                                                  Rect bound)
            throws InvalidProtocolBufferException;
}
