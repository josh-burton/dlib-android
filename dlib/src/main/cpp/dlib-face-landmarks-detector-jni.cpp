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

#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_io.h>
#include "jni.h"
#include "profiler.h"
#include "messages.pb.h"
#include <dlib/dnn.h>
#include <dlib/clustering.h>
#include <android/asset_manager_jni.h>

using namespace dlib;

// ----------------------------------------------------------------------------------------

// The next bit of code defines a ResNet network.  It's basically copied
// and pasted from the dnn_imagenet_ex.cpp example, except we replaced the loss
// layer with loss_metric and made the network somewhat smaller.  Go read the introductory
// dlib DNN examples to learn what all this stuff means.
//
// Also, the dnn_metric_learning_on_images_ex.cpp example shows how to train this network.
// The dlib_face_recognition_resnet_model_v1 model used by this example was trained using
// essentially the code shown in dnn_metric_learning_on_images_ex.cpp except the
// mini-batches were made larger (35x15 instead of 5x5), the iterations without progress
// was set to 10000, the jittering you can see below in jitter_image() was used during
// training, and the training dataset consisted of about 3 million images instead of 55.
// Also, the input layer was locked to images of size 150.
template<template<int, template<typename> class, int, typename> class block, int N,
        template<typename> class BN, typename SUBNET>
using residual = add_prev1<block<N, BN, 1, tag1<SUBNET>>>;

template<template<int, template<typename> class, int, typename> class block, int N,
        template<typename> class BN, typename SUBNET>
using residual_down = add_prev2<avg_pool<2, 2, 2, 2, skip1<tag2<block<N, BN, 2, tag1<SUBNET>>>>>>;

template<int N, template<typename> class BN, int stride, typename SUBNET>
using block = BN<con<N, 3, 3, 1, 1, relu<BN<con<N, 3, 3, stride, stride, SUBNET>>>>>;

template<int N, typename SUBNET> using ares = relu<residual<block, N, affine, SUBNET>>;
template<int N, typename SUBNET> using ares_down = relu<residual_down<block, N, affine, SUBNET>>;

template<typename SUBNET> using alevel0 = ares_down<256, SUBNET>;
template<typename SUBNET> using alevel1 = ares<256, ares<256, ares_down<256, SUBNET>>>;
template<typename SUBNET> using alevel2 = ares<128, ares<128, ares_down<128, SUBNET>>>;
template<typename SUBNET> using alevel3 = ares<64, ares<64, ares<64, ares_down<64, SUBNET>>>>;
template<typename SUBNET> using alevel4 = ares<32, ares<32, ares<32, SUBNET>>>;

using anet_type = loss_metric<fc_no_bias<128, avg_pool_everything<alevel0<alevel1<alevel2<alevel3<alevel4<max_pool<3, 3, 2, 2, relu<affine<con<32, 7, 7, 2, 2, input_rgb_image_sized<150>>>>>>>>>>>>>;

// ----------------------------------------------------------------------------------------

#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, "dlib-jni:", __VA_ARGS__))

#define JNI_METHOD(NAME) \
    Java_nz_co_colensobbdo_dlib_DLibLandmarks68Detector_##NAME

using namespace ::nz::co::colensobbdo::dlib::proto;

// FIXME: Create a class inheriting from dlib::array2d<dlib::rgb_pixel>.
void convertBitmapToArray2d(JNIEnv *env, jobject bitmap, dlib::array2d<dlib::rgb_pixel> &out) {
    AndroidBitmapInfo bitmapInfo;
    void *pixels;
    int state;

    if (0 > (state = AndroidBitmap_getInfo(env, bitmap, &bitmapInfo))) {
        LOGI("L%d: AndroidBitmap_getInfo() failed! error=%d", __LINE__, state);
        throwException(env, "AndroidBitmap_getInfo() failed!");
        return;
    } else if (bitmapInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGI("L%d: Bitmap format is not RGB_565!", __LINE__);
        throwException(env, "Bitmap format is not RGB_565!");
    }

    // Lock the bitmap for copying the pixels safely.
    if (0 > (state = AndroidBitmap_lockPixels(env, bitmap, &pixels))) {
        LOGI("L%d: AndroidBitmap_lockPixels() failed! error=%d", __LINE__, state);
        throwException(env, "AndroidBitmap_lockPixels() failed!");
        return;
    }

    LOGI("L%d: info.width=%d, info.height=%d", __LINE__, bitmapInfo.width, bitmapInfo.height);
    out.set_size((long) bitmapInfo.height, (long) bitmapInfo.width);

    char *line = (char *) pixels;
    for (int h = 0; h < bitmapInfo.height; ++h) {
        for (int w = 0; w < bitmapInfo.width; ++w) {
            uint32_t *color = (uint32_t *) (line + 4 * w);

            out[h][w].red = (unsigned char) (0xFF & ((*color) >> 24));
            out[h][w].green = (unsigned char) (0xFF & ((*color) >> 16));
            out[h][w].blue = (unsigned char) (0xFF & ((*color) >> 8));
        }

        line = line + bitmapInfo.stride;
    }

    // Unlock the bitmap.
    AndroidBitmap_unlockPixels(env, bitmap);
}

struct membuf : std::streambuf {
    membuf(char *begin, char *end) {
        this->setg(begin, begin, end);
    }
};

// JNI ////////////////////////////////////////////////////////////////////////

dlib::shape_predictor sFaceLandmarksDetector;
anet_type sFaceRecognition;
dlib::frontal_face_detector sFaceDetector;

extern "C" JNIEXPORT jboolean JNICALL
JNI_METHOD(isFaceDetectorReady)(JNIEnv *env, jobject thiz) {
    if (sFaceDetector.num_detectors() > 0) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_METHOD(isFaceRecognitionDetectorReady)(JNIEnv *env, jobject thiz) {
    if (sFaceRecognition.num_layers > 0) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_METHOD(isFaceLandmarksDetectorReady)(JNIEnv *env, jobject thiz) {
    if (sFaceLandmarksDetector.num_parts() > 0) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
JNI_METHOD(prepareFaceDetector)(JNIEnv *env, jobject thiz) {
    // Profiler.
    Profiler profiler;
    profiler.start();

    // Prepare the detector.
    sFaceDetector = dlib::get_frontal_face_detector();

    double interval = profiler.stopAndGetInterval();

    LOGI("L%d: sFaceDetector is initialized (took %.3f ms)", __LINE__, interval);
    LOGI("L%d: sFaceDetector.num_detectors()=%lu", __LINE__, sFaceDetector.num_detectors());
}


extern "C" JNIEXPORT void JNICALL
JNI_METHOD(prepareFaceRecognitionDetector)(JNIEnv *env, jobject thiz, jstring recognitionPath) {
    const char *rPath = env->GetStringUTFChars(recognitionPath, JNI_FALSE);

    // Profiler.
    Profiler profiler;
    profiler.start();

    dlib::deserialize(rPath) >> sFaceRecognition;

    double interval = profiler.stopAndGetInterval();
    LOGI("L%d: sFaceRecognition is initialized (took %.3f ms)", __LINE__, interval);
    env->ReleaseStringUTFChars(recognitionPath, rPath);
}

extern "C" JNIEXPORT void JNICALL
JNI_METHOD(prepareFaceLandmarksDetector)(JNIEnv *env, jobject thiz, jobject assetManager,
                                         jstring fileName) {
    LOGI("L%d: init sFaceLandmarksDetector", __LINE__);
    // Profiler.
    Profiler profiler;
    profiler.start();

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    if (mgr == NULL) {
        return;
    }
    /*获取文件名并打开*/
    jboolean iscopy;
    const char *mfile = env->GetStringUTFChars(fileName, &iscopy);
    AAsset *asset = AAssetManager_open(mgr, mfile, AASSET_MODE_UNKNOWN);
    env->ReleaseStringUTFChars(fileName, mfile);
    if (asset == NULL) {
        return;
    }

    auto file_length = static_cast<size_t>(AAsset_getLength(asset));
    if (file_length == 0) {
        return;
    }
    char *model_buffer = (char *) malloc(file_length);
    //read file data
    AAsset_read(asset, model_buffer, file_length);
    //the data has been copied to model_buffer, so , close it
    AAsset_close(asset);

    //char* to istream
    membuf mem_buf(model_buffer, model_buffer + file_length);
    std::istream in(&mem_buf);

    //load shape_predictor_68_face_landmarks.dat from memory
    dlib::deserialize(sFaceLandmarksDetector, in);
    //free malloc
    free(model_buffer);

//    const char *file_name = env->GetStringUTFChars(fileName, nullptr);
//    env->ReleaseStringUTFChars(fileName, file_name);
//
//    //get AAssetManager
//    AAssetManager *native_asset = AAssetManager_fromJava(env, assetManager);
//
//    //open file
//    AAsset *assetFile = AAssetManager_open(native_asset, file_name, AASSET_MODE_BUFFER);
//    //get file length
//    size_t file_length = static_cast<size_t>(AAsset_getLength(assetFile));
//    char *model_buffer = (char *) malloc(file_length);
//    //read file data
//    AAsset_read(assetFile, model_buffer, file_length);
//    //the data has been copied to model_buffer, so , close it
//    AAsset_close(assetFile);
//
//    //LOGI("asset file length %d", file_length);
//
//    //char* to istream
//    membuf mem_buf(model_buffer, model_buffer + file_length);
//    std::istream in(&mem_buf);
//
////    const char *path = env->GetStringUTFChars(detectorPath, JNI_FALSE);
//
//    // We need a shape_predictor. This is the tool that will predict face
//    // landmark positions given an image and face bounding box.  Here we are just
//    // loading the model from the shape_predictor_68_face_landmarks.dat file you gave
//    // as a command line argument.
//    // Deserialize the shape detector.
//    //load shape_predictor_68_face_landmarks.dat from memory
//    dlib::deserialize(sFaceLandmarksDetector,in);


    //free malloc
//    free(model_buffer);

    double interval = profiler.stopAndGetInterval();

    LOGI("L%d: sFaceLandmarksDetector is initialized (took %.3f ms)", __LINE__, interval);
    LOGI("L%d: sFaceLandmarksDetector.num_parts()=%lu", __LINE__,
         sFaceLandmarksDetector.num_parts());

//    env->ReleaseStringUTFChars(fileName, file_name);

//    if (sFaceLandmarksDetector.num_parts() != 68) {
//        throwException(env, "It's not a 68 landmarks detector!");
//    }
}

extern "C" JNIEXPORT jbyteArray JNICALL
JNI_METHOD(detectFaces)(JNIEnv *env, jobject thiz, jobject bitmap) {
    // Profiler.
    Profiler profiler;
    profiler.start();

    // Convert bitmap to dlib::array2d.
    dlib::array2d<dlib::rgb_pixel> img;
    convertBitmapToArray2d(env, bitmap, img);

    double interval = profiler.stopAndGetInterval();

    const long width = img.nc();
    const long height = img.nr();
    LOGI("L%d: input image (w=%ld, h=%ld) is read (took %.3f ms)",
         __LINE__, width, height, interval);

    profiler.start();

    // Now tell the face detector to give us a list of bounding boxes
    // around all the faces in the image.
    std::vector<dlib::rectangle> dets = sFaceDetector(img);
    interval = profiler.stopAndGetInterval();
    LOGI("L%d: Number of faces detected: %u (took %.3f ms)",
         __LINE__, (unsigned int) dets.size(), interval);

    // To protobuf message.
    FaceList faces;
    for (unsigned long i = 0; i < dets.size(); ++i) {
        // Profiler.
        profiler.start();

        dlib::rectangle &det = dets.at(i);

        Face *face = faces.add_faces();
        RectF *bound = face->mutable_bound();

        bound->set_left((float) det.left() / width);
        bound->set_top((float) det.top() / height);
        bound->set_right((float) det.right() / width);
        bound->set_bottom((float) det.bottom() / height);

        interval = profiler.stopAndGetInterval();
        LOGI("L%d: Convert face #%lu to protobuf message (took %.3f ms)",
             __LINE__, i, interval);
    }

    // Profiler.
    profiler.start();

    // Prepare the return message.
    int outSize = faces.ByteSize();
    jbyteArray out = env->NewByteArray(outSize);
    jbyte *buffer = new jbyte[outSize];

    faces.SerializeToArray(buffer, outSize);
    env->SetByteArrayRegion(out, 0, outSize, buffer);
    delete[] buffer;

    interval = profiler.stopAndGetInterval();
    LOGI("L%d: Convert faces to protobuf message (took %.3f ms)",
         __LINE__, interval);

    return out;
}

extern "C" JNIEXPORT jbyteArray JNICALL
JNI_METHOD(detectLandmarksFromFace)(JNIEnv *env, jobject thiz, jobject bitmap, jlong left,
                                    jlong top, jlong right, jlong bottom) {
    // Profiler.
    Profiler profiler;
    profiler.start();

    // Convert bitmap to dlib::array2d.
    dlib::array2d<dlib::rgb_pixel> img;
    convertBitmapToArray2d(env, bitmap, img);

    double interval = profiler.stopAndGetInterval();

    const long width = img.nc();
    const long height = img.nr();
    LOGI("L%d: input image (w=%ld, h=%ld) is read (took %.3f ms)",
         __LINE__, width, height, interval);

    profiler.start();

    // Detect landmarks.
    dlib::rectangle bound(left, top, right, bottom);
    dlib::full_object_detection shape = sFaceLandmarksDetector(img, bound);
    interval = profiler.stopAndGetInterval();
    LOGI("L%d: %lu landmarks detected (took %.3f ms)",
         __LINE__, shape.num_parts(), interval);

    profiler.start();
    // Protobuf message.
    LandmarkList landmarks;
    // You get the idea, you can get all the face part locations if
    // you want them.  Here we just store them in shapes so we can
    // put them on the screen.
    for (unsigned long i = 0; i < shape.num_parts(); ++i) {
        dlib::point &pt = shape.part(i);

        Landmark *landmark = landmarks.add_landmarks();
        landmark->set_x((float) pt.x() / width);
        landmark->set_y((float) pt.y() / height);
    }
    interval = profiler.stopAndGetInterval();
    LOGI("L%d: Convert #%lu landmarks to protobuf message (took %.3f ms)",
         __LINE__, shape.num_parts(), interval);

    // Profiler.
    profiler.start();

    // TODO: Make a JNI function to convert a message to byte[] living in
    // TODO: lib-protobuf project.
    // Prepare the return message.
    int outSize = landmarks.ByteSize();
    jbyteArray out = env->NewByteArray(outSize);
    jbyte *buffer = new jbyte[outSize];

    landmarks.SerializeToArray(buffer, outSize);
    env->SetByteArrayRegion(out, 0, outSize, buffer);
    delete[] buffer;

    interval = profiler.stopAndGetInterval();
    LOGI("L%d: Convert faces to protobuf message (took %.3f ms)",
         __LINE__, interval);

    return out;
}

extern "C" JNIEXPORT jbyteArray JNICALL
JNI_METHOD(detectLandmarksFromFaces)(JNIEnv *env, jobject thiz, jobject bitmap,
                                     jbyteArray faceRects) {
    // Profiler.
    Profiler profiler;
    profiler.start();

    // Convert bitmap to dlib::array2d.
    dlib::array2d<dlib::rgb_pixel> img;
    convertBitmapToArray2d(env, bitmap, img);

    const long width = img.nc();
    const long height = img.nr();
    LOGI("L%d: input image (w=%ld, h=%ld) is read (took %.3f ms)",
         __LINE__, width, height,
         profiler.stopAndGetInterval());

    profiler.start();

    // Translate the input face-rects message into something we recognize here.
    jbyte *pFaceRects = env->GetByteArrayElements(faceRects, NULL);
    jsize pFaceRectsLen = env->GetArrayLength(faceRects);
    RectFList msgBounds;
    msgBounds.ParseFromArray(pFaceRects, pFaceRectsLen);
    env->ReleaseByteArrayElements(faceRects, pFaceRects, 0);
    std::vector<dlib::rectangle> bounds;
    for (int i = 0; i < msgBounds.rects().size(); ++i) {
        const RectF &msgBound = msgBounds.rects().Get(i);
        bounds.push_back(dlib::rectangle((long) msgBound.left(),
                                         (long) msgBound.top(),
                                         (long) msgBound.right(),
                                         (long) msgBound.bottom()));
    }
    LOGI("L%d: input face rects (size=%d) is read (took %.3f ms)",
         __LINE__, msgBounds.rects().size(),
         profiler.stopAndGetInterval());

    // Detect landmarks and return protobuf message.
    FaceList faces;
    for (unsigned long j = 0; j < bounds.size(); ++j) {
        profiler.start();
        dlib::full_object_detection shape = sFaceLandmarksDetector(img, bounds[j]);
        LOGI("L%d: #%lu face, %lu landmarks detected (took %.3f ms)",
             __LINE__, j, shape.num_parts(),
             profiler.stopAndGetInterval());

        profiler.start();

        // To protobuf message.
        Face *face = faces.add_faces();
        // Transfer face boundary.
        RectF *bound = face->mutable_bound();
        bound->set_left((float) bounds[j].left() / width);
        bound->set_top((float) bounds[j].top() / height);
        bound->set_right((float) bounds[j].right() / width);
        bound->set_bottom((float) bounds[j].bottom() / height);
        // Transfer face landmarks.
        for (u_long i = 0; i < shape.num_parts(); ++i) {
            dlib::point &pt = shape.part(i);

            Landmark *landmark = face->add_landmarks();
            landmark->set_x((float) pt.x() / width);
            landmark->set_y((float) pt.y() / height);
        }
        LOGI("L%d: Convert #%lu face to protobuf message (took %.3f ms)",
             __LINE__, j,
             profiler.stopAndGetInterval());
    }

    profiler.start();

    // Prepare the return message.
    int outSize = faces.ByteSize();
    jbyteArray out = env->NewByteArray(outSize);
    jbyte *buffer = new jbyte[outSize];

    faces.SerializeToArray(buffer, outSize);
    env->SetByteArrayRegion(out, 0, outSize, buffer);
    delete[] buffer;

    LOGI("L%d: Convert faces to protobuf message (took %.3f ms)",
         __LINE__, profiler.stopAndGetInterval());

    return out;
}

extern "C" JNIEXPORT jbyteArray JNICALL
JNI_METHOD(detectFacesAndLandmarks)(JNIEnv *env, jobject thiz, jobject bitmap) {
    if (sFaceDetector.num_detectors() == 0) {
        LOGI("L%d: sFaceDetector is not initialized!", __LINE__);
        throwException(env, "sFaceDetector is not initialized!");
        return NULL;
    }
    if (sFaceLandmarksDetector.num_parts() == 0) {
        LOGI("L%d: sFaceLandmarksDetector is not initialized!", __LINE__);
        throwException(env, "sFaceLandmarksDetector is not initialized!");
        return NULL;
    }

    // Profiler.
    Profiler profiler;
    profiler.start();

    // Convert bitmap to dlib::array2d.
    dlib::array2d<dlib::rgb_pixel> img;
    convertBitmapToArray2d(env, bitmap, img);

    double interval = profiler.stopAndGetInterval();

    const float width = (float) img.nc();
    const float height = (float) img.nr();
    LOGI("L%d: input image (w=%f, h=%f) is read (took %.3f ms)",
         __LINE__, width, height, interval);

//    // Make the image larger so we can detect small faces.
//    dlib::pyramid_up(img);
//    LOGI("L%d: pyramid_up the input image (w=%lu, h=%lu).", __LINE__, img.nc(), img.nr());

    profiler.start();

    // Now tell the face detector to give us a list of bounding boxes
    // around all the faces in the image.
    std::vector<dlib::rectangle> dets = sFaceDetector(img);
    interval = profiler.stopAndGetInterval();
    LOGI("L%d: Number of faces detected: %u (took %.3f ms)",
         __LINE__, (unsigned int) dets.size(), interval);

    // Protobuf message.
    FaceList faces;
    // Now we will go ask the shape_predictor to tell us the pose of
    // each face we detected.
    for (unsigned long j = 0; j < dets.size(); ++j) {
        profiler.start();
        dlib::full_object_detection shape = sFaceLandmarksDetector(img, dets[j]);
        interval = profiler.stopAndGetInterval();
        LOGI("L%d: #%lu face, %lu landmarks detected (took %.3f ms)",
             __LINE__, j, shape.num_parts(), interval);

        profiler.start();

        // To protobuf message.
        Face *face = faces.add_faces();
        // Transfer face boundary.
        RectF *bound = face->mutable_bound();
        bound->set_left((float) dets[j].left() / width);
        bound->set_top((float) dets[j].top() / height);
        bound->set_right((float) dets[j].right() / width);
        bound->set_bottom((float) dets[j].bottom() / height);
        // Transfer face landmarks.
        for (u_long i = 0; i < shape.num_parts(); ++i) {
            dlib::point &pt = shape.part(i);

            Landmark *landmark = face->add_landmarks();
            landmark->set_x((float) pt.x() / width);
            landmark->set_y((float) pt.y() / height);
        }
        interval = profiler.stopAndGetInterval();
        LOGI("L%d: Convert #%lu face to protobuf message (took %.3f ms)",
             __LINE__, j, interval);
    }

    profiler.start();

    // Prepare the return message.
    int outSize = faces.ByteSize();
    jbyteArray out = env->NewByteArray(outSize);
    jbyte *buffer = new jbyte[outSize];

    faces.SerializeToArray(buffer, outSize);
    env->SetByteArrayRegion(out, 0, outSize, buffer);
    delete[] buffer;

    interval = profiler.stopAndGetInterval();
    LOGI("L%d: Convert faces to protobuf message (took %.3f ms)",
         __LINE__, interval);

    return out;
}
