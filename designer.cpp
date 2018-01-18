#include "designer.h"
#include <dirent.h>
#include <stdexcept>
#include <iostream>
#include "util.h"

designer::designer(std::string fontsDir)
{
  DIR* fontdir;
  struct dirent* ent;
  if ((fontdir = opendir(fontsDir.c_str())) == nullptr)
  {
    throw std::invalid_argument("Couldn't find fonts");
  }

  while ((ent = readdir(fontdir)) != nullptr)
  {
    std::string dname(ent->d_name);
    if ((dname.find(".otf") != std::string::npos)
      || (dname.find(".ttf") != std::string::npos))
    {
      fonts.push_back(fontsDir + "/" + dname);
    }
  }

  closedir(fontdir);
}

Magick::Image designer::generate(
  size_t width,
  size_t height,
  const std::string& text,
  std::mt19937& rng) const
{
  // Initialize two layers: the text, and a shadow to increase contrast.
  Magick::Image textimage(Magick::Geometry(width, height), "transparent");
  Magick::Image shadowimage(Magick::Geometry(width, height), "transparent");

  textimage.fillColor(Magick::Color(MaxRGB, MaxRGB, MaxRGB, MaxRGB * 0.0));
  shadowimage.fillColor(Magick::Color(0, 0, 0, 0));
  shadowimage.strokeColor("black");

  bool hasFont = false;
  std::uniform_int_distribution<size_t> fontDist(0, fonts.size() - 1);

  std::vector<std::string> words = split<std::vector<std::string>>(text, " ");
  std::vector<std::string>::const_iterator curWord = std::begin(words);

  size_t top = V_PADDING;
  size_t minWords = 1;
  while (curWord != std::end(words))
  {
    // There is a 1 in 10 chance of randomly changing the font. We also have to
    // set the font if this is the beginning of generation.
    if (!hasFont || (std::bernoulli_distribution(0.1)(rng)))
    {
      std::string font = fonts[fontDist(rng)];

      textimage.font(font);
      shadowimage.font(font);

      hasFont = true;
    }

    // Choose a font size for the current line.
    std::uniform_int_distribution<size_t> sizeDist(
      MIN_FONT_SIZE, MAX_FONT_SIZE);

    size_t size = sizeDist(rng);
    textimage.fontPointsize(size);

    // Decide what words to put on this line.
    size_t maxWords = maxWordsInLine(curWord, std::end(words), textimage);

    size_t lineLen;
    if (minWords > maxWords)
    {
      lineLen = maxWords;
    } else {
      std::uniform_int_distribution<size_t> lenDist(minWords, maxWords);
      lineLen = lenDist(rng);
    }

    std::string prefixText = implode(curWord, curWord + lineLen, " ");

    // Determine if the choice of font, font size, and number of words, would
    // prevent the algorithm from being about to layout the entire string even
    // if the rest of it were rendered in the smallest font size.
    Magick::TypeMetric metric;
    textimage.fontTypeMetrics(prefixText, &metric);

    textimage.fontPointsize(MIN_FONT_SIZE);

    // This is how much vertical space would be required to render the rest of
    // the string at the minimum font size.
    int lowpadding = minHeightRequired(
      curWord + lineLen,
      std::end(words),
      textimage);

    // This is the amount of space that would be left over if the rest of the
    // string were rendered at the minimum font size.
    int freespace =
      static_cast<int>(height) - static_cast<int>(V_PADDING)
        - static_cast<int>(top) - lowpadding - metric.textHeight();

    // Some debug text.
    std::cout << "top of " << top << " with lowpad of " << lowpadding
      << " and textheight of " << metric.textHeight() << " with freespace="
      << freespace << std::endl;

    // If there wouldn't be enough room to render the rest of the string at the
    // minimum font size, go back to the top of the loop and choose a different
    // font, font size, and number of words.
    if (freespace < 0)
    {
      minWords = lineLen;

      continue;
    }

    minWords = 1;

    // Determine how much space to leave between this line and the previous one.
    size_t toppadding;
    if (std::bernoulli_distribution(0.5)(rng))
    {
      // Exponential distribution, biased toward top
      std::uniform_int_distribution<size_t> expDist(
        1, static_cast<size_t>(exp(freespace + 1)));

      toppadding = log(expDist(rng));
    } else {
      // Linear distribution, biased toward bottom
      std::uniform_int_distribution<size_t> linDist(0, freespace);

      toppadding = linDist(rng);
    }

    // Determine the x-coordinate of this line.
    std::uniform_int_distribution<size_t> leftDist(
      H_PADDING,
      width - H_PADDING - static_cast<size_t>(metric.textWidth()) - 1);

    size_t leftx = leftDist(rng);

    // Render this line.
    size_t ycor = top + toppadding + metric.ascent();

    std::cout << "printing at " << leftx << "," << ycor << std::endl;

    textimage.fontPointsize(size);
    textimage.annotate(prefixText, Magick::Geometry(0, 0, leftx, ycor));

    shadowimage.fontPointsize(size);
    shadowimage.strokeWidth(size / 10);
    shadowimage.annotate(prefixText, Magick::Geometry(0, 0, leftx, ycor));

    top += toppadding + metric.textHeight();

    std::advance(curWord, lineLen);
  }

  // Make the shadow layer semi-transparent.
  Magick::PixelPacket* shadpixels = shadowimage.getPixels(0, 0, width, height);
  Magick::PixelPacket* textpixels = textimage.getPixels(0, 0, width, height);
  for (size_t j = 0; j < height; j++)
  {
    for (size_t i = 0; i < width; i++)
    {
      size_t ind = j * width + i;

      if (shadpixels[ind].opacity != MaxRGB)
      {
        shadpixels[ind].opacity = MaxRGB * 0.25;
      }
    }
  }

  shadowimage.syncPixels();
  textimage.syncPixels();

  // Add some blur to the text and shadow.
  shadowimage.blur(10.0, 20.0);
  textimage.blur(0.0, 0.5);

  // Put the text layer on top of the shadow.
  shadowimage.composite(textimage, 0, 0, Magick::OverCompositeOp);

  return shadowimage;
}

size_t designer::maxWordsInLine(
  std::vector<std::string>::const_iterator first,
  std::vector<std::string>::const_iterator last,
  Magick::Image& textimage) const
{
  size_t result = 0;

  std::string curline = "";
  for (; first != last; first++)
  {
    curline += " " + *first;

    Magick::TypeMetric metric;
    textimage.fontTypeMetrics(curline, &metric);

    if (metric.textWidth() > ((textimage.columns() / 10) * 9))
    {
      break;
    } else {
      result++;
    }
  }

  return result;
}

size_t designer::minHeightRequired(
  std::vector<std::string>::const_iterator first,
  std::vector<std::string>::const_iterator last,
  Magick::Image& textimage) const
{
  if (first == last)
  {
    return 0;
  } else {
    size_t result = 0;

    while (first != last)
    {
      size_t prefixlen = maxWordsInLine(first, last, textimage);
      std::string prefixText = implode(first, first + prefixlen, " ");

      Magick::TypeMetric metric;
      textimage.fontTypeMetrics(prefixText, &metric);
      result += metric.textHeight() + V_PADDING;

      std::advance(first, prefixlen);
    }

    return result - V_PADDING;
  }
}
