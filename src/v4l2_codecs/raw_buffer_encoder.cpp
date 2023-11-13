#include "v4l2_codecs/raw_buffer_encoder.h"
#include "v4l2_codecs/raw_buffer.h"

#include <iostream>

RawBufferEncoder::RawBufferEncoder(const webrtc::SdpVideoFormat &format, Args args)
    : src_width_(args.width),
      src_height_(args.height),
      callback_(nullptr),
      bitrate_adjuster_(.85, 1) {}

RawBufferEncoder::~RawBufferEncoder() {
    Release();
}

int32_t RawBufferEncoder::InitEncode(
    const webrtc::VideoCodec *codec_settings,
    const VideoEncoder::Settings &settings) {
    codec_ = *codec_settings;
    dst_width_ = codec_settings->width;
    dst_height_ = codec_settings->height;
    std::cout << "[RawEncoder]: width, " << dst_width_ << std::endl;
    std::cout << "[RawEncoder]: height, " << dst_height_ << std::endl;

    if (codec_.codecType != webrtc::kVideoCodecH264) {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    encoded_image_.timing_.flags = webrtc::VideoSendTiming::TimingFrameFlags::kInvalid;
    encoded_image_.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

    auto e = std::async(std::launch::async, [&]() {
        encoder_ = std::make_unique<V4l2Encoder>();
        encoder_->Configure(dst_width_, dst_height_, true);
    });

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RawBufferEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback *callback) {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RawBufferEncoder::Release() {
    std::lock_guard<std::mutex> lock(mtx_);
    encoder_.reset();
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RawBufferEncoder::Encode(
    const webrtc::VideoFrame &frame,
    const std::vector<webrtc::VideoFrameType> *frame_types) {
    std::lock_guard<std::mutex> lock(mtx_);
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
        frame.video_frame_buffer();

    if (frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    RawBuffer *raw_buffer = static_cast<RawBuffer *>(frame_buffer.get());
    Buffer buffer = raw_buffer->GetBuffer();

    // skip sending task if output_result is false
    bool is_output = encoder_->EmplaceBuffer(buffer,
        [&](Buffer encoded_buffer) {
            SendFrame(frame, encoded_buffer);
        });
    
    return is_output? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

void RawBufferEncoder::SendFrame(const webrtc::VideoFrame &frame, Buffer &encoded_buffer) {
    auto encoded_image_buffer =
        webrtc::EncodedImageBuffer::Create((uint8_t *)encoded_buffer.start, encoded_buffer.length);

    webrtc::CodecSpecificInfo codec_specific;
    codec_specific.codecType = webrtc::kVideoCodecH264;
    codec_specific.codecSpecific.H264.packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;

    encoded_image_.SetEncodedData(encoded_image_buffer);
    encoded_image_.SetTimestamp(frame.timestamp());
    encoded_image_.SetColorSpace(frame.color_space());
    encoded_image_._encodedWidth = dst_width_;
    encoded_image_._encodedHeight = dst_height_;
    encoded_image_.capture_time_ms_ = frame.render_time_ms();
    encoded_image_.ntp_time_ms_ = frame.ntp_time_ms();
    encoded_image_.rotation_ = frame.rotation();
    encoded_image_._frameType = webrtc::VideoFrameType::kVideoFrameDelta;

    if (encoded_buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
        encoded_image_._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    }

    auto result = callback_->OnEncodedImage(encoded_image_, &codec_specific);
    if (result.error != webrtc::EncodedImageCallback::Result::OK) {
        std::cout << "OnEncodedImage failed error:" << result.error << std::endl;
    }
}

void RawBufferEncoder::SetRates(const RateControlParameters &parameters) {
    std::lock_guard<std::mutex> lock(mtx_);
    encoder_->SetFps(parameters, bitrate_adjuster_);
}

webrtc::VideoEncoder::EncoderInfo RawBufferEncoder::GetEncoderInfo() const {
    EncoderInfo info;
    info.supports_native_handle = true;
    info.implementation_name = "Raw H264";
    return info;
}