#include "sap.h"
#include <yaml-cpp/yaml.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <json.hpp>

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
  tempfile_ = config["tempfile"].as<std::string>();

  // Set up the Mastodon client.
  instance_ = std::make_unique<mastodonpp::Instance>(
    config["mastodon_instance"].as<std::string>(),
    config["mastodon_token"].as<std::string>());
  connection_ = std::make_unique<mastodonpp::Connection>(*instance_);

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
    std::cout << "Generating post..." << std::endl;

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

      sendTweet(std::move(image), action);

      std::cout << "Tweeted!" << std::endl;

      // Wait.
      std::this_thread::sleep_for(std::chrono::hours(1));
    } catch (const Magick::ErrorImage& ex)
    {
      std::cout << "Image error: " << ex.what() << std::endl;
    }

    std::cout << std::endl;
  }
}

void sap::sendTweet(Magick::Image image, const std::string& text) const
{
  image.magick("jpeg");
  image.write(tempfile_);

  auto answer{connection_->post(mastodonpp::API::v1::media,
    {{"file", std::string("@file:") + tempfile_}, {"description", text}})};
  if (!answer)
  {
    if (answer.curl_error_code == 0)
    {
      std::cout << "HTTP status: " << answer.http_status << std::endl;
    }
    else
    {
      std::cout << "libcurl error " << std::to_string(answer.curl_error_code)
           << ": " << answer.error_message << std::endl;
    }
    return;
  }

  nlohmann::json response_json = nlohmann::json::parse(answer.body);
  answer = connection_->post(mastodonpp::API::v1::statuses,
    {{"status", ""}, {"media_ids",
      std::vector<std::string_view>{response_json["id"].get<std::string>()}}});

  if (!answer)
  {
    if (answer.curl_error_code == 0)
    {
      std::cout << "HTTP status: " << answer.http_status << std::endl;
    }
    else
    {
      std::cout << "libcurl error " << std::to_string(answer.curl_error_code)
           << ": " << answer.error_message << std::endl;
    }
  }
}
