#ifndef DIRECTOR_H_9DFD929C
#define DIRECTOR_H_9DFD929C

#include <stdexcept>
#include <string>
#include <vector>
#include <Magick++.h>
#include <random>

class ffmpeg_error : public std::runtime_error {
public:

  ffmpeg_error(std::string msg) : std::runtime_error(msg)
  {
  }
};

class director {
public:

  director(std::string videosPath);

  Magick::Image generate(std::mt19937& rng) const;

private:

  std::string videosPath_;
  std::vector<std::string> videos_;
};

#endif /* end of include guard: DIRECTOR_H_9DFD929C */
