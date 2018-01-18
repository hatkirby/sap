#ifndef SAP_H_11D8D668
#define SAP_H_11D8D668

#include <random>
#include <string>
#include <memory>
#include <Magick++.h>
#include <twitter.h>
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

  void sendTweet(Magick::Image image) const;

  std::mt19937& rng_;
  std::unique_ptr<twitter::client> client_;
  rawr kgramstats_;
  std::unique_ptr<designer> layout_;
  std::unique_ptr<director> director_;

};

#endif /* end of include guard: SAP_H_11D8D668 */
