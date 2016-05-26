/*
 * The MIT License
 *
 * Copyright (c) 2016 Intel Corporation
 *
 * 	Permission is hereby granted, free of charge, to any person
 * 	obtaining a copy of this software and associated documentation
 * 	files (the "Software"), to deal in the Software without
 * 	restriction, including without limitation the rights to use,
 * 	copy, modify, merge, publish, distribute, sublicense, and/or
 * 	sell copies of the Software, and to permit persons to whom the
 * 	Software is furnished to do so, subject to the following
 * 	conditions:
 *
 * 	The above copyright notice and this permission notice shall be
 * 	included in all copies or substantial portions of the
 * 	Software.
 *
 * 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * 	KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * 	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * 	PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * 	COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * 	OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * 	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include "IntelDeflater.h"

extern "C" {
#include "igzip_lib.h"
}

#define DBG(M, ...)
//#define DBG(M, ...)  fprintf(stdout, "[DEBUG] (%s %s:%d) " M "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

jfieldID FID_lz_stream;
jfieldID FID_inputBuffer;
jfieldID FID_inputBufferLength;
jfieldID FID_endOfStream;
jfieldID FID_finished;

JNIEXPORT void JNICALL Java_com_intel_gkl_compression_IntelDeflater_init
(JNIEnv* env, jclass cls) {
  FID_lz_stream = env->GetFieldID(cls, "lz_stream", "J");
  FID_inputBuffer = env->GetFieldID(cls, "inputBuffer", "[B");
  FID_inputBufferLength = env->GetFieldID(cls, "inputBufferLength", "I");
  FID_endOfStream = env->GetFieldID(cls, "endOfStream", "Z");
  FID_finished = env->GetFieldID(cls, "finished", "Z");
}

JNIEXPORT void JNICALL Java_com_intel_gkl_compression_IntelDeflater_resetNative
(JNIEnv* env, jobject obj) {
  LZ_Stream2* lz_stream = (LZ_Stream2*)env->GetLongField(obj, FID_lz_stream);

  jclass Exception = env->FindClass("java/lang/Exception");
      if (lz_stream == 0) {
        lz_stream = (LZ_Stream2*)malloc(sizeof(LZ_Stream2));
        if ( lz_stream == NULL ) {
                env->ThrowNew(Exception,"Memory allocation error");
            }
    env->SetLongField(obj, FID_lz_stream, (jlong)lz_stream);
  }
  
  init_stream(lz_stream);
  lz_stream->end_of_stream = 0;
  DBG("lz_stream = 0x%lx", (long)lz_stream);
}

JNIEXPORT jint JNICALL Java_com_intel_gkl_compression_IntelDeflater_deflate
(JNIEnv * env, jobject obj, jbyteArray outputBuffer, jint outputBufferLength) {
  LZ_Stream2* lz_stream = (LZ_Stream2*)env->GetLongField(obj, FID_lz_stream);
  jbyteArray inputBuffer = (jbyteArray)env->GetObjectField(obj, FID_inputBuffer);
  jint inputBufferLength = env->GetIntField(obj, FID_inputBufferLength);
  jboolean endOfStream = env->GetBooleanField(obj, FID_endOfStream);
  
  DBG("lz_stream = 0x%lx", (long)lz_stream);

  jbyte* next_in = (jbyte*)env->GetPrimitiveArrayCritical(inputBuffer, 0);
  jbyte* next_out = (jbyte*)env->GetPrimitiveArrayCritical(outputBuffer, 0);

  lz_stream->next_in = (UINT8*)next_in;
  lz_stream->avail_in = inputBufferLength;
  lz_stream->end_of_stream = endOfStream;
  lz_stream->next_out = (UINT8*)next_out;
  lz_stream->avail_out = outputBufferLength;

  DBG("end_of_stream = %d", lz_stream->end_of_stream);
  DBG("avail_in = %d", lz_stream->avail_in);
  DBG("avail_out = %d", lz_stream->avail_out);
  DBG("total_out = %d", lz_stream->total_out);

  int bytes_in = inputBufferLength;

  DBG("Compressing");

  // compress and update lz_stream state
  fast_lz(lz_stream);

  int bytes_out = outputBufferLength - lz_stream->avail_out;

  DBG("Compression ratio = %2.2f", 100.0 - (100.0 * bytes_out / bytes_in));

  DBG("avail_in = %d", lz_stream->avail_in);
  DBG("avail_out = %d", lz_stream->avail_out);
  DBG("total_out = %d", lz_stream->total_out);

  // release buffers
  env->ReleasePrimitiveArrayCritical(inputBuffer, next_in, 0);
  env->ReleasePrimitiveArrayCritical(outputBuffer, next_out, 0);

  // set finished if endOfStream was set and all input processed
  if (endOfStream && lz_stream->avail_in == 0) {
    env->SetLongField(obj, FID_finished, true);
  }

  // return number of bytes written to output buffer
  return bytes_out;
}