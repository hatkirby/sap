#include "director.h"
#include <dirent.h>
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace ffmpeg {

  class format {
  public:

    format(std::string videoPath)
    {
      if (avformat_open_input(&ptr_, videoPath.c_str(), nullptr, nullptr))
      {
        throw ffmpeg_error("Could not open video " + videoPath);
      }

      if (avformat_find_stream_info(ptr_, nullptr))
      {
        throw ffmpeg_error("Could not read stream");
      }
    }

    ~format()
    {
      avformat_close_input(&ptr_);
    }

    format(const format& other) = delete;

    AVFormatContext* ptr()
    {
      return ptr_;
    }

  private:

    AVFormatContext* ptr_ = nullptr;
  };

  class codec {
  public:

    codec(
      format& fmt,
      AVStream* st)
    {
      AVCodec* dec = avcodec_find_decoder(st->codecpar->codec_id);
      if (!dec)
      {
        throw ffmpeg_error("Failed to find codec");
      }

      ptr_ = avcodec_alloc_context3(dec);
      if (!ptr_)
      {
        throw ffmpeg_error("Failed to allocate codec context");
      }

      if (avcodec_parameters_to_context(ptr_, st->codecpar) < 0)
      {
        throw ffmpeg_error("Failed to copy codec parameters to decoder");
      }

      // Init the decoders, with or without reference counting
      AVDictionary* opts = nullptr;
      av_dict_set(&opts, "refcounted_frames", "0", 0);

      if (avcodec_open2(ptr_, dec, &opts) < 0)
      {
        throw ffmpeg_error("Failed to open codec");
      }
    }

    codec(const codec& other) = delete;

    ~codec()
    {
      avcodec_free_context(&ptr_);
    }

    int getWidth() const
    {
      return ptr_->width;
    }

    int getHeight() const
    {
      return ptr_->height;
    }

    enum AVPixelFormat getPixelFormat() const
    {
      return ptr_->pix_fmt;
    }

    AVCodecContext* ptr()
    {
      return ptr_;
    }

  private:

    AVCodecContext* ptr_ = nullptr;
  };

  class packet {
  public:

    packet()
    {
      ptr_ = av_packet_alloc();
    }

    ~packet()
    {
      av_packet_free(&ptr_);
    }

    packet(const packet& other) = delete;

    int getStreamIndex() const
    {
      return ptr_->stream_index;
    }

    AVPacket* ptr()
    {
      return ptr_;
    }

    int getFlags() const
    {
      return ptr_->flags;
    }

  private:

    AVPacket* ptr_ = nullptr;
  };

  class frame {
  public:

    frame()
    {
      ptr_ = av_frame_alloc();
    }

    ~frame()
    {
      av_frame_free(&ptr_);
    }

    frame(const frame& other) = delete;

    uint8_t** getData()
    {
      return ptr_->data;
    }

    int* getLinesize()
    {
      return ptr_->linesize;
    }

    AVFrame* ptr()
    {
      return ptr_;
    }

  private:

    AVFrame* ptr_ = nullptr;
  };

  class sws {
  public:

    sws(
      int srcW,
      int srcH,
      enum AVPixelFormat srcFormat,
      int dstW,
      int dstH,
      enum AVPixelFormat dstFormat,
      int flags,
      SwsFilter* srcFilter,
      SwsFilter* dstFilter,
      const double* param)
    {
      ptr_ = sws_getContext(
        srcW,
        srcH,
        srcFormat,
        dstW,
        dstH,
        dstFormat,
        flags,
        srcFilter,
        dstFilter,
        param);

      if (ptr_ == NULL)
      {
        throw ffmpeg_error("Could not allocate sws context");
      }
    }

    ~sws()
    {
      sws_freeContext(ptr_);
    }

    sws(const sws& other) = delete;

    void scale(
      const uint8_t* const srcSlice[],
      const int srcStride[],
      int srcSliceY,
      int srcSliceH,
      uint8_t* const dst[],
      const int dstStride[])
    {
      sws_scale(
        ptr_,
        srcSlice,
        srcStride,
        srcSliceY,
        srcSliceH,
        dst,
        dstStride);
    }

  private:

    SwsContext* ptr_;
  };

}

director::director(std::string videosPath) : videosPath_(videosPath)
{
  DIR* videodir;
  struct dirent* ent;
  if ((videodir = opendir(videosPath.c_str())) == nullptr)
  {
    throw std::invalid_argument("Couldn't find videos");
  }

  while ((ent = readdir(videodir)) != nullptr)
  {
    std::string dname(ent->d_name);
    if (dname.find(".mp4") != std::string::npos)
    {
      videos_.push_back(dname);
    }
  }

  closedir(videodir);
}

Magick::Image director::generate(std::mt19937& rng) const
{
  std::uniform_int_distribution<size_t> videoDist(0, videos_.size() - 1);
  std::string video = videosPath_ + "/" + videos_[videoDist(rng)];

  ffmpeg::format fmt(video);

  int streamIdx =
    av_find_best_stream(fmt.ptr(), AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

  if (streamIdx < 0)
  {
    throw ffmpeg_error("Could not find stream");
  }

  AVStream* stream = fmt.ptr()->streams[streamIdx];

  ffmpeg::codec cdc(fmt, stream);

  size_t codecw = cdc.getWidth();
  size_t codech = cdc.getHeight();

  std::uniform_int_distribution<int64_t> frameDist(0, stream->duration - 1);
  int64_t seek = frameDist(rng);

  std::cout << seek << std::endl;

  if (av_seek_frame(fmt.ptr(), streamIdx, seek, 0))
  {
    throw ffmpeg_error("Could not seek");
  }

  ffmpeg::frame frame;
  ffmpeg::frame converted;

  av_image_alloc(
    converted.getData(),
    converted.getLinesize(),
    codecw,
    codech,
    AV_PIX_FMT_RGB24,
    1);

  ffmpeg::packet pkt;

  do
  {
    if (av_read_frame(fmt.ptr(), pkt.ptr()) < 0)
    {
      throw ffmpeg_error("Could not read frame");
    }
  } while ((pkt.getStreamIndex() != streamIdx)
      || !(pkt.getFlags() & AV_PKT_FLAG_KEY));

  int ret;
  do {
    if (avcodec_send_packet(cdc.ptr(), pkt.ptr()) < 0)
    {
      throw ffmpeg_error("Could not send packet");
    }

    ret = avcodec_receive_frame(cdc.ptr(), frame.ptr());
  } while (ret == AVERROR(EAGAIN));

  if (ret < 0)
  {
    throw ffmpeg_error("Could not decode frame");
  }

  ffmpeg::sws scaler(
    cdc.getWidth(),
    cdc.getHeight(),
    cdc.getPixelFormat(),
    cdc.getWidth(),
    cdc.getHeight(),
    AV_PIX_FMT_RGB24,
    0, nullptr, nullptr, 0);

  scaler.scale(
    frame.getData(),
    frame.getLinesize(),
    0,
    codech,
    converted.getData(),
    converted.getLinesize());

  size_t buffer_size = av_image_get_buffer_size(
    AV_PIX_FMT_RGB24, cdc.getWidth(), cdc.getHeight(), 1);

  std::vector<uint8_t> buffer(buffer_size);

  av_image_copy_to_buffer(
    buffer.data(),
    buffer_size,
    converted.getData(),
    converted.getLinesize(),
    AV_PIX_FMT_RGB24,
    cdc.getWidth(),
    cdc.getHeight(),
    1);

  size_t width = 1024;
  size_t height = codech * width / codecw;

  Magick::Image image;
  image.read(codecw, codech, "RGB", Magick::CharPixel, buffer.data());
  image.zoom(Magick::Geometry(width, height));

  return image;
}
