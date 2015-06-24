

#include <iostream>
#include <map>
#include "H264RawPixelsSink.h"
#include <boost/thread.hpp>
#include <GroupsockHelper.hh>

#include "CubemapSource.hpp"

namespace bc = boost::chrono;

H264RawPixelsSink* H264RawPixelsSink::createNew(UsageEnvironment& env,
	unsigned int bufferSize)
{
    avcodec_register_all();
    avformat_network_init();
	return new H264RawPixelsSink(env, bufferSize);
}

void H264RawPixelsSink::setOnDroppedNALU(std::function<void (H264RawPixelsSink*, u_int8_t type)>& callback)
{
    onDroppedNALU = callback;
}

void H264RawPixelsSink::setOnAddedNALU(std::function<void (H264RawPixelsSink*, u_int8_t type)>& callback)
{
    onAddedNALU = callback;
}

H264RawPixelsSink::H264RawPixelsSink(UsageEnvironment& env,
	unsigned int bufferSize)
	: MediaSink(env), bufferSize(bufferSize), buffer(new unsigned char[bufferSize]),
    img_convert_ctx(NULL), lastIFramePkt(nullptr), gotFirstIFrame(false),
    counter(0), sumRelativePresentationTimeMicroSec(0), maxRelativePresentationTimeMicroSec(0)
{
	for (int i = 0; i < 1; i++)
	{
		AVFrame* frame = av_frame_alloc();
		if (!frame)
		{
			fprintf(stderr, "Could not allocate video frame\n");
			exit(1);
		}
		//frame->format = AV_PIX_FMT_RGBA;

		framePool.push(frame);
        
        AVPacket* pkt = new AVPacket;
        pktPool.push(pkt);
	}


	// Initialize codec and decoder
	AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec)
	{
		fprintf(stderr, "Codec not found\n");
		return;
	}

	codecContext = avcodec_alloc_context3(codec);

	if (!codecContext)
	{
		fprintf(stderr, "could not allocate video codec context\n");
		return;
	}

	/* open it */
	if (avcodec_open2(codecContext, codec, NULL) < 0)
	{
		fprintf(stderr, "could not open codec\n");
		return;
	}

	decodeFrameThread = boost::thread(boost::bind(&H264RawPixelsSink::decodeFrameLoop, this));
}

void H264RawPixelsSink::packageData(AVPacket* pkt, unsigned int frameSize, timeval presentationTime)
{
    unsigned char const start_code[4] = { 0x00, 0x00, 0x00, 0x01 };
    
    av_init_packet(pkt);
    
    AVRational microSecBase = { 1, 1000000 };
    
    pkt->size = frameSize + sizeof(start_code);
    pkt->data = (uint8_t*)new char[frameSize + sizeof(start_code)];
    pkt->pts = av_rescale_q(presentationTime.tv_sec * 1000000 + presentationTime.tv_usec,
                            microSecBase,
                            codecContext->time_base);
    
    memcpy(pkt->data, start_code, sizeof(start_code));
    memcpy(pkt->data + sizeof(start_code), buffer, frameSize);
}

void H264RawPixelsSink::afterGettingFrame(unsigned frameSize,
	unsigned numTruncatedBytes,
	timeval presentationTime)
{
    u_int8_t nal_unit_type = buffer[0] & 0x1F;
    AVPacket* pkt = (pktPool.try_pop(pkt)) ? pkt : nullptr;
    
    if (pkt)
    {
        // We currently have the capacities to decode the received frame
        
        if (lastIFramePkt)
        {
            // We still have an I frame to process -> do it now
            // Don't care about the received frame
            pktBuffer.push(lastIFramePkt);
            lastIFramePkt = nullptr;
            gotFirstIFrame = true;
            if (onDroppedNALU) onDroppedNALU(this, nal_unit_type);
        }
        else if (nal_unit_type != 7 && gotFirstIFrame)
        {
            // We received a B/P frame
            // and have the capacities to decode it -> do it
            
            packageData(pkt, frameSize, presentationTime);
            pktBuffer.push(pkt);
            if (onAddedNALU) onAddedNALU(this, nal_unit_type);
        }
        else if (nal_unit_type == 7)
        {
            packageData(pkt, frameSize, presentationTime);
            pktBuffer.push(pkt);
            gotFirstIFrame = true;
            if (onAddedNALU) onAddedNALU(this, nal_unit_type);
        }
        else
        {
            pktPool.push(pkt);
            if (onDroppedNALU) onDroppedNALU(this, nal_unit_type);
        }
    }
    else
    {
        // We currently don't have the capacities to decode the received frame
        
        if (nal_unit_type == 7)
        {
            // We received an I frame but don't have the capacities
            // to encode it right now -> safe it for later
            
            if (!lastIFramePkt)
            {
                lastIFramePkt = new AVPacket;
                packageData(lastIFramePkt, frameSize, presentationTime);
            }
        }
        else if (nal_unit_type != 7 && !pkt)
        {
            // We received a B/P frame but don't have the capacities
            // to encode it right now.
            // We can safely skip it.
        }
        
        if (onDroppedNALU) onDroppedNALU(this, nal_unit_type);
    }

	// Then try getting the next frame:
	continuePlaying();
}

Boolean H264RawPixelsSink::continuePlaying()
{
	fSource->getNextFrame(buffer, bufferSize,
		afterGettingFrame, this,
		onSourceClosure, this);

	return True;
}

void H264RawPixelsSink::afterGettingFrame(void*clientData,
                                          unsigned frameSize,
                                          unsigned numTruncatedBytes,
                                          timeval presentationTime,
                                          unsigned durationInMicroseconds)
{
	H264RawPixelsSink* sink = (H264RawPixelsSink*)clientData;
	sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime);
}

void H264RawPixelsSink::decodeFrameLoop()
{
	while (true)
	{
		// Pop frame ptr from buffer
		AVFrame* frame;
		AVPacket* pkt;

		if (!pktBuffer.wait_and_pop(pkt))
		{
			// queue did close
			return;
		}

		if (!framePool.wait_and_pop(frame))
		{
			// queue did close
			return;
		}

		int got_frame;
		int len = avcodec_decode_video2(codecContext, frame, &got_frame, pkt);

        if (got_frame == 1)
		{
            // We have decoded a frame :) ->
            // Make the frame available to the application
            frame->pts = pkt->pts;
            frameBuffer.push(frame);
		}
        else
        {
            // No frame could be decoded :( ->
            // Put frame back to the pool so that the next packet will be read
            framePool.push(frame);
            
            if (len < 0)
            {
                // error decoding frame
            }
            else if (len == 0)
            {
                // package contained no frame
            }

        }
        

		bc::microseconds nowSinceEpoch =
			bc::duration_cast<bc::microseconds>(bc::system_clock::now().time_since_epoch());

		AVRational microSecBase = { 1, 1000000 };
		bc::microseconds presentationTimeSinceEpoch =
			bc::microseconds(av_rescale_q(pkt->pts, codecContext->time_base, microSecBase));


		bc::microseconds relativePresentationTime = presentationTimeSinceEpoch - nowSinceEpoch;

		sumRelativePresentationTimeMicroSec += relativePresentationTime.count();
		if (maxRelativePresentationTimeMicroSec > relativePresentationTime.count())
		{
			maxRelativePresentationTimeMicroSec = relativePresentationTime.count();
		}

		const long frequency = 100;
		if (counter % frequency == 0)
		{
			//std::cout << this << " delay: avg " << -sumRelativePresentationTimeMicroSec / 1000.0 / frequency << " ms; max " << -maxRelativePresentationTimeMicroSec / 1000.0 << " ms" << std::endl;
			sumRelativePresentationTimeMicroSec = 0;
			maxRelativePresentationTimeMicroSec = 0;
            
            //std::cout << stats.summary(bc::milliseconds(1001)) << std::endl;
		}

		counter++;
		
        pktPool.push(pkt);
	}
}

AVFrame* H264RawPixelsSink::getNextFrame()
{
    AVFrame* frame;
    
    if (frameBuffer.wait_and_pop(frame))
    {
        AVFrame* clone = av_frame_clone(frame);
        // av_frame_clone only copies properties and still references the sources data.
        // Make full copy instead
        av_frame_copy(clone, frame);
        framePool.push(frame);
        return clone;
    }
    else
    {
        return nullptr;
    }
}
