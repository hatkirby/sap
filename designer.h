#ifndef DESIGNER_H_CCE34BEB
#define DESIGNER_H_CCE34BEB

#include <random>
#include <string>
#include <Magick++.h>
#include <vector>

class designer {
public:

  designer(std::string fontsPath);

  Magick::Image generate(
    size_t width,
    size_t height,
    const std::string& text,
    std::mt19937& rng) const;

private:

  const size_t MIN_FONT_SIZE = 48;
  const size_t MAX_FONT_SIZE = 96;
  const size_t V_PADDING = 5;
  const size_t H_PADDING = 5;

  size_t maxWordsInLine(
    std::vector<std::string>::const_iterator first,
    std::vector<std::string>::const_iterator last,
    Magick::Image& textimage) const;

  size_t minHeightRequired(
    std::vector<std::string>::const_iterator first,
    std::vector<std::string>::const_iterator last,
    Magick::Image& textimage) const;

  std::vector<std::string> fonts;
};

#endif /* end of include guard: DESIGNER_H_CCE34BEB */
