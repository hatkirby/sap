#ifndef SAP_H_11D8D668
#define SAP_H_11D8D668

#include <random>
#include <string>
#include <memory>
#include <Magick++.h>
#include <mastodonpp.hpp>
#include <rawr.h>
#include "designer.h"
#include "director.h"

class sap {
public:

  sap(
    std::string configFile,
    std::mt19937& rng);

  void run() const;

private:

  void sendTweet(Magick::Image image, const std::string& text) const;

  std::mt19937& rng_;
  std::unique_ptr<mastodonpp::Instance> instance_;
  std::unique_ptr<mastodonpp::Connection> connection_;
  rawr kgramstats_;
  std::unique_ptr<designer> layout_;
  std::unique_ptr<director> director_;
  std::string tempfile_;

};

#endif /* end of include guard: SAP_H_11D8D668 */
