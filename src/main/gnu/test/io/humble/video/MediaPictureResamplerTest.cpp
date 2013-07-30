/*******************************************************************************
 * Copyright (c) 2013, Art Clarke.  All rights reserved.
 *  
 * This file is part of Humble-Video.
 *
 * Humble-Video is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Humble-Video is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Humble-Video.  If not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************/
/*
 * MediaPictureResamplerTest.cpp
 *
 *  Created on: Jul 30, 2013
 *      Author: aclarke
 */

#include "MediaPictureResamplerTest.h"
#include <io/humble/ferry/RefPointer.h>
#include <io/humble/video/MediaPicture.h>
#include <io/humble/video/MediaPictureResampler.h>
#include <io/humble/video/Source.h>
#include <io/humble/video/Decoder.h>
#include "lodepng.h"

using namespace io::humble::ferry;
using namespace io::humble::video;

MediaPictureResamplerTest::MediaPictureResamplerTest() {
}

MediaPictureResamplerTest::~MediaPictureResamplerTest() {
}


void
MediaPictureResamplerTest::writePicture(const char* prefix, int32_t* frameNo,
    MediaPicture* picture,
    MediaPictureResampler* resampler,
    MediaPicture* resampled)
{
  char filename[2048];

  // resample the image from YUV to RGBA
  resampler->resample(resampled, picture);

  // write data as PGM file.
  snprintf(filename, sizeof(filename), "%s-%06d.png", prefix, *frameNo);
  // only write every n'th frame to save disk space
  RefPointer<Buffer> buf = resampled->getData(0);
  uint8_t* data = (uint8_t*)buf->getBytes(0, buf->getBufferSize());
  if (!((*frameNo) % 30)) {
    lodepng_encode32_file(filename, data, resampled->getWidth(), resampled->getHeight());
  }
  (*frameNo)++;
}

void
MediaPictureResamplerTest::testRescale() {

  TestData::Fixture* fixture=mFixtures.getFixture("testfile_h264_mp4a_tmcd.mov");
  TS_ASSERT(fixture);
  char filepath[2048];
  mFixtures.fillPath(fixture, filepath, sizeof(filepath));

  RefPointer<Source> source = Source::make();

  int32_t retval=-1;
  retval = source->open(filepath, 0, false, true, 0, 0);
  TS_ASSERT(retval >= 0);

  int32_t numStreams = source->getNumStreams();
  TS_ASSERT_EQUALS(fixture->num_streams, numStreams);

  int32_t streamToDecode = 0;
  RefPointer<SourceStream> stream = source->getSourceStream(streamToDecode);
  TS_ASSERT(stream);
  RefPointer<Decoder> decoder = stream->getDecoder();
  TS_ASSERT(decoder);
  RefPointer<Codec> codec = decoder->getCodec();
  TS_ASSERT(codec);
  TS_ASSERT_EQUALS(Codec::CODEC_ID_H264, codec->getID());

  decoder->open(0, 0);

  TS_ASSERT_EQUALS(PixelFormat::PIX_FMT_YUV420P, decoder->getPixelFormat());

  // now, let's start a decoding loop.
  RefPointer<MediaPacket> packet = MediaPacket::make();

  RefPointer<MediaPicture> picture = MediaPicture::make(
      decoder->getWidth(),
      decoder->getHeight(),
      decoder->getPixelFormat());

  int32_t rescaleW = decoder->getWidth()/4;
  int32_t rescaleH = (int32_t)(decoder->getHeight()*5.22);
  PixelFormat::Type rescaleFmt = PixelFormat::PIX_FMT_RGBA;
  RefPointer<MediaPicture> rescaled = MediaPicture::make(
      rescaleW, rescaleH, rescaleFmt);
  RefPointer<MediaPictureResampler> resampler =
      MediaPictureResampler::make(rescaled->getWidth(),
          rescaled->getHeight(),
          rescaled->getFormat(),
          picture->getWidth(),
          picture->getHeight(),
          picture->getFormat(),
          0);

  int32_t frameNo = 0;
  while(source->read(packet.value()) >= 0) {
    // got a packet; now we try to decode it.
    if (packet->getStreamIndex() == streamToDecode &&
        packet->isComplete()) {
      int32_t bytesRead = 0;
      int32_t byteOffset=0;
      do {
        bytesRead = decoder->decodeVideo(picture.value(), packet.value(), byteOffset);
        if (picture->isComplete()) {
          writePicture("MediaPictureResamplerTest_testRescaleVideo",
              &frameNo, picture.value(), resampler.value(), rescaled.value());
        }
        byteOffset += bytesRead;
      } while(byteOffset < packet->getSize());
      // now, handle the case where bytesRead is 0; we need to flush any
      // cached packets
      do {
        decoder->decodeVideo(picture.value(), 0, 0);
        if (picture->isComplete()) {
          writePicture("MediaPictureResamplerTest_testRescaleVideo", &frameNo, picture.value(),
              resampler.value(), rescaled.value());
        }
      } while (picture->isComplete());
    }
    if ((int32_t)(frameNo/30) > 10)
      // 20 pictures should be enough to see if it's working.
      break;
  }
  source->close();
}