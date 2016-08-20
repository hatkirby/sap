extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <Magick++.h>
#include <iostream>
#include <rawr.h>
#include <vector>
#include <list>
#include <fstream>
#include <dirent.h>
#include <sstream>
#include <twitter.h>
#include <yaml-cpp/yaml.h>
#include <thread>
#include <chrono>

/* - random frames from Spongebob (using ffmpeg)
 * with strange text overlaid, possibly rawr'd from
 * spongebob transcripts combined with drug trip
 * experiences? either that or something with verb
 * frames
 */

template <class Container>
Container split(std::string input, std::string delimiter)
{
  Container result;
  
  while (!input.empty())
  {
    int divider = input.find(delimiter);
    if (divider == std::string::npos)
    {
      result.push_back(input);
      
      input = "";
    } else {
      result.push_back(input.substr(0, divider));
      
      input = input.substr(divider+delimiter.length());
    }
  }
  
  return result;
}

template <class InputIterator>
std::string implode(InputIterator first, InputIterator last, std::string delimiter)
{
  std::stringstream result;
  
  for (InputIterator it = first; it != last; it++)
  {
    if (it != first)
    {
      result << delimiter;
    }
    
    result << *it;
  }
  
  return result.str();
}

int maxWordsInLine(std::vector<std::string> words, Magick::Image& textimage)
{
  int result = 0;
  
  std::string curline = "";
  Magick::TypeMetric metric;
  for (auto word : words)
  {
    curline += " " + word;
    
    textimage.fontTypeMetrics(curline, &metric);
    if (metric.textWidth() > ((textimage.columns()/10)*9))
    {
      break;
    } else {
      result++;
    }
  }
  
  return result;
}

int minHeightRequired(std::vector<std::string> words, Magick::Image& textimage)
{
  int result = 0;
  while (!words.empty())
  {
    int prefixlen = maxWordsInLine(words, textimage);
    std::string prefixText = implode(std::begin(words), std::begin(words) + prefixlen, " ");
    std::vector<std::string> suffix(std::begin(words) + prefixlen, std::end(words));
    Magick::TypeMetric metric;
    textimage.fontTypeMetrics(prefixText, &metric);
    result += metric.textHeight() + 5;
    
    words = suffix;
  }
  
  return result - 5;
}

void layoutText(Magick::Image& textimage, Magick::Image& shadowimage, int width, int height, std::string text)
{
  DIR* fontdir;
  struct dirent* ent;
  if ((fontdir = opendir("fonts")) == nullptr)
  {
    std::cout << "Couldn't find fonts." << std::endl;
    return;
  }

  std::vector<std::string> fonts;
  while ((ent = readdir(fontdir)) != nullptr)
  {
    std::string dname(ent->d_name);
    if ((dname.find(".otf") != std::string::npos) || (dname.find(".ttf") != std::string::npos))
    {
      fonts.push_back(dname);
    }
  }

  closedir(fontdir);

  textimage.fillColor(Magick::Color(MaxRGB, MaxRGB, MaxRGB, MaxRGB * 0.0));
  shadowimage.fillColor(Magick::Color(0, 0, 0, 0));
  shadowimage.strokeColor("black");
  
  int minSize = 48;
  int realMaxSize = 96;
  int maxSize = realMaxSize;
  Magick::TypeMetric metric;
  std::string font;
  auto words = split<std::vector<std::string>>(text, " ");
  int top = 5;
  int minWords = 1;
  while (!words.empty())
  {
    if (font.empty() || (rand() % 10 == 0))
    {
      font = fonts[rand() % fonts.size()];
      textimage.font("fonts/" + font);
      shadowimage.font("fonts/" + font);
    }
    
    int size = rand() % (maxSize - minSize + 1) + minSize;
    textimage.fontPointsize(size);
    int maxWords = maxWordsInLine(words, textimage);
    int touse;
    if (minWords > maxWords)
    {
      touse = maxWords;
    } else {
      touse = rand() % (maxWords - minWords + 1) + minWords;
    }
    std::string prefixText = implode(std::begin(words), std::begin(words) + touse, " ");
    std::vector<std::string> suffix(std::begin(words) + touse, std::end(words));
    textimage.fontTypeMetrics(prefixText, &metric);
    
    textimage.fontPointsize(minSize);
    int lowpadding = minHeightRequired(suffix, textimage);
    int freespace = height - 5 - top - lowpadding - metric.textHeight();
    std::cout << "top of " << top << " with lowpad of " << lowpadding << " and textheight of " << metric.textHeight() << " with freespace=" << freespace << std::endl;
    if (freespace < 0)
    {
      minWords = touse;

      continue;
    }
    
    maxSize = realMaxSize;
    minWords = 1;
    
    int toppadding;
    if (rand() % 2 == 0)
    {
      // Exponential distribution, biased toward top
      toppadding = log(rand() % (int)exp(freespace + 1) + 1);
    } else {
      // Linear distribution, biased toward bottom
      toppadding = rand() % (freespace + 1);
    }
    
    int leftx = rand() % (width - 10 - (int)metric.textWidth()) + 5;
    std::cout << "printing at " << leftx << "," << (top + toppadding + metric.ascent()) << std::endl;
    textimage.fontPointsize(size);
    textimage.annotate(prefixText, Magick::Geometry(0, 0, leftx, top + toppadding + metric.ascent()));
    
    shadowimage.fontPointsize(size);
    shadowimage.strokeWidth(size / 10);
    shadowimage.annotate(prefixText, Magick::Geometry(0, 0, leftx, top + toppadding + metric.ascent()));
    //shadowimage.draw(Magick::DrawableRectangle(leftx - 5, top + toppadding, leftx + metric.textWidth() + 5, top + toppadding + metric.textHeight() + 10 + metric.descent()));
    
    words = suffix;
    top += toppadding + metric.textHeight();
  }
  
  Magick::PixelPacket* shadowpixels = shadowimage.getPixels(0, 0, width, height);
  Magick::PixelPacket* textpixels = textimage.getPixels(0, 0, width, height);
  for (int j=0; j<height; j++)
  {
    for (int i=0; i<width; i++)
    {
      int ind = j*width+i;
      if (shadowpixels[ind].opacity != MaxRGB)
      {
        shadowpixels[ind].opacity = MaxRGB * 0.25;
      }
      
      if (textpixels[ind].opacity != MaxRGB)
      {
        //textpixels[ind].opacity = MaxRGB * 0.05;
      }
    }
  }
  
  shadowimage.syncPixels();
  textimage.syncPixels();
  
  shadowimage.blur(10.0, 20.0);
  textimage.blur(0.0, 0.5);
}

static int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
  int ret, stream_index;
  AVStream *st;
  AVCodecContext *dec_ctx = NULL;
  AVCodec *dec = NULL;
  AVDictionary *opts = NULL;
  ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
  if (ret < 0)
  {
    //fprintf(stderr, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(type), src_filename);
    return ret;
  } else {
    stream_index = ret;
    st = fmt_ctx->streams[stream_index];
    
    // find decoder for the stream
    dec_ctx = st->codec;
    dec = avcodec_find_decoder(dec_ctx->codec_id);
    if (!dec)
    {
      fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
      return AVERROR(EINVAL);
    }
    
    // Init the decoders, with or without reference counting
    av_dict_set(&opts, "refcounted_frames", "0", 0);
    if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0)
    {
      fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
      return ret;
    }
    
    *stream_idx = stream_index;
  }
  
  return 0;
}

int main(int argc, char** argv)
{
  srand(time(NULL));
  rand(); rand(); rand(); rand();
  
  Magick::InitializeMagick(nullptr);
  av_register_all();
  
  YAML::Node config = YAML::LoadFile("config.yml");
  
  twitter::auth auth;
  auth.setConsumerKey(config["consumer_key"].as<std::string>());
  auth.setConsumerSecret(config["consumer_secret"].as<std::string>());
  auth.setAccessKey(config["access_key"].as<std::string>());
  auth.setAccessSecret(config["access_secret"].as<std::string>());
  
  twitter::client client(auth);
  
  std::ifstream infile("corpus1.txt");
  std::string corpus;
  std::string line;
  while (getline(infile, line))
  {
    if (line.back() == '\r')
    {
      line.pop_back();
    }
    
    corpus += line + " ";
  }
  
  infile.close();
  
  std::ifstream infile2("corpus2.txt");
  std::string corpus2;
  while (getline(infile2, line))
  {
    if (line.back() == '\r')
    {
      line.pop_back();
    }
    
    corpus2 += line + " ";
  }
  
  infile2.close();
  
  rawr kgramstats;
  kgramstats.addCorpus(corpus);
  kgramstats.addCorpus(corpus2);
  kgramstats.compile(5);
  kgramstats.setMinCorpora(2);
  
  DIR* videodir;
  struct dirent* ent;
  if ((videodir = opendir("videos")) == nullptr)
  {
    std::cout << "Couldn't find videos." << std::endl;
    return -1;
  }

  std::vector<std::string> videos;
  while ((ent = readdir(videodir)) != nullptr)
  {
    std::string dname(ent->d_name);
    if (dname.find(".mp4") != std::string::npos)
    {
      videos.push_back(dname);
    }
  }

  closedir(videodir);
  
  for (;;)
  {
    std::string video = "videos/" + videos[rand() % videos.size()];
    std::cout << "Opening " << video << std::endl;
  
    AVFormatContext* format = nullptr;
    if (avformat_open_input(&format, video.c_str(), nullptr, nullptr))
    {
      std::cout << "could not open file" << std::endl;
      return 1;
    }
  
    if (avformat_find_stream_info(format, nullptr))
    {
      std::cout << "could not read stream" << std::endl;
      return 5;
    }
  
    int video_stream_idx = -1;
    if (open_codec_context(&video_stream_idx, format, AVMEDIA_TYPE_VIDEO))
    {
      std::cout << "could not open codec" << std::endl;
      return 6;
    }
  
    AVStream* stream = format->streams[video_stream_idx];
    AVCodecContext* codec = stream->codec;
    int codecw = codec->width;
    int codech = codec->height;

    int64_t seek = (rand() % format->duration) * codec->time_base.num / codec->time_base.den;
    std::cout << seek << std::endl;
    if (av_seek_frame(format, video_stream_idx, seek, 0))
    {
      std::cout << "could not seek" << std::endl;
      return 4;
    }
  
    AVPacket packet;
    av_init_packet(&packet);
  
    AVFrame* frame = av_frame_alloc();
    AVFrame* converted = av_frame_alloc();
  
    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecw, codech, 1);
    uint8_t* buffer = new uint8_t[buffer_size];

    av_image_alloc(converted->data, converted->linesize, codecw, codech, AV_PIX_FMT_RGB24, 1);

    for (;;)
    {
      if (av_read_frame(format, &packet))
      {
        std::cout << "could not read frame" << std::endl;
        return 2;
      }
    
      if (packet.stream_index != video_stream_idx)
      {
        continue;
      }
    
      int got_pic;
      if (avcodec_decode_video2(codec, frame, &got_pic, &packet) < 0)
      {
        std::cout << "could not decode frame" << std::endl;
        return 7;
      }
  
      if (!got_pic)
      {
        continue;
      }
    
      if (packet.flags && AV_PKT_FLAG_KEY)
      {
        SwsContext* sws = sws_getContext(codecw, codech, codec->pix_fmt, codecw, codech, AV_PIX_FMT_RGB24, 0, nullptr, nullptr, 0);
        sws_scale(sws, frame->data, frame->linesize, 0, codech, converted->data, converted->linesize);
        sws_freeContext(sws);
      
        av_image_copy_to_buffer(buffer, buffer_size, converted->data, converted->linesize, AV_PIX_FMT_RGB24, codecw, codech, 1);
        av_frame_free(&frame);
        av_frame_free(&converted);
        av_packet_unref(&packet);
        avcodec_close(codec);
        avformat_close_input(&format);
      
        int width = 1024;
        int height = codech * width / codecw;
      
        Magick::Image image;
        image.read(codecw, codech, "RGB", Magick::CharPixel, buffer);
        image.zoom(Magick::Geometry(width, height));
      
        std::string action = kgramstats.randomSentence(rand() % 15 + 5);
        Magick::Image textimage(Magick::Geometry(width, height), "transparent");
        Magick::Image shadowimage(Magick::Geometry(width, height), "transparent");
        layoutText(textimage, shadowimage, width, height, action);
        image.composite(shadowimage, 0, 0, Magick::OverCompositeOp);
        image.composite(textimage, 0, 0, Magick::OverCompositeOp);
      
        image.magick("jpeg");
        
        Magick::Blob outputimg;
        image.write(&outputimg);
      
        delete[] buffer;
        
        std::cout << "Generated image." << std::endl << "Tweeting..." << std::endl;
        
        try
        {
          long media_id = client.uploadMedia("image/jpeg", (const char*) outputimg.data(), outputimg.length());
          client.updateStatus("", {media_id});
    
          std::cout << "Done!" << std::endl << "Waiting..." << std::endl << std::endl;
        } catch (const twitter::twitter_error& error)
        {
          std::cout << "Twitter error: " << error.what() << std::endl;
        }
      
        break;
      }
    }
    
    std::this_thread::sleep_for(std::chrono::hours(1));
  }
  
  return 0;
}
