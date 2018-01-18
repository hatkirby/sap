#include "sap.h"
#include <yaml-cpp/yaml.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>

/* - random frames from Spongebob (using ffmpeg)
 * with strange text overlaid, possibly rawr'd from
 * spongebob transcripts combined with drug trip
 * experiences? either that or something with verb
 * frames
 */

sap::sap(
  std::string configFile,
  std::mt19937& rng) :
    rng_(rng)
{
  // Load the config file.
  YAML::Node config = YAML::LoadFile(configFile);

  // Set up the Twitter client.
  twitter::auth auth;
  auth.setConsumerKey(config["consumer_key"].as<std::string>());
  auth.setConsumerSecret(config["consumer_secret"].as<std::string>());
  auth.setAccessKey(config["access_key"].as<std::string>());
  auth.setAccessSecret(config["access_secret"].as<std::string>());

  client_ = std::unique_ptr<twitter::client>(new twitter::client(auth));

  // Set up the text generator.
  for (const YAML::Node& corpusname : config["corpuses"])
  {
    std::ifstream infile(corpusname.as<std::string>());
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

    kgramstats_.addCorpus(corpus);
  }

  kgramstats_.compile(5);
  kgramstats_.setMinCorpora(config["corpuses"].size());

  // Set up the layout designer.
  layout_ = std::unique_ptr<designer>(
    new designer(config["fonts"].as<std::string>()));

  // Set up the frame picker.
  director_ = std::unique_ptr<director>(
    new director(config["videos"].as<std::string>()));
}

void sap::run() const
{
  for (;;)
  {
    std::cout << "Generating tweet..." << std::endl;

    try
    {
      // Pick the video frame.
      Magick::Image image = director_->generate(rng_);

      // Generate the text.
      std::uniform_int_distribution<size_t> lenDist(5, 19);
      std::string action = kgramstats_.randomSentence(lenDist(rng_));

      // Lay the text on the video frame.
      Magick::Image textimage =
        layout_->generate(image.columns(), image.rows(), action, rng_);
      image.composite(textimage, 0, 0, Magick::OverCompositeOp);

      // Send the tweet.
      std::cout << "Sending tweet..." << std::endl;

      sendTweet(std::move(image));

      std::cout << "Tweeted!" << std::endl;

      // Wait.
      std::this_thread::sleep_for(std::chrono::hours(1));
    } catch (const Magick::ErrorImage& ex)
    {
      std::cout << "Image error: " << ex.what() << std::endl;
    } catch (const twitter::twitter_error& ex)
    {
      std::cout << "Twitter error: " << ex.what() << std::endl;

      std::this_thread::sleep_for(std::chrono::hours(1));
    }

    std::cout << std::endl;
  }
}

void sap::sendTweet(Magick::Image image) const
{
  Magick::Blob outputimg;
  image.magick("jpeg");
  image.write(&outputimg);

  long media_id = client_->uploadMedia("image/jpeg",
    static_cast<const char*>(outputimg.data()), outputimg.length());

  client_->updateStatus("", {media_id});
}
