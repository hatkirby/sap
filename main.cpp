#include "sap.h"
#include <iostream>
#include <ctime>
#include <cstdlib>

extern "C" {
#include <libavformat/avformat.h>
}

int main(int argc, char** argv)
{
  Magick::InitializeMagick(nullptr);
  av_register_all();

  std::random_device randomDevice;
  std::mt19937 rng(randomDevice());

  // We also have to seed the C-style RNG because librawr uses it.
  srand(time(NULL));
  rand(); rand(); rand(); rand();

  if (argc != 2)
  {
    std::cout << "usage: sap [configfile]" << std::endl;
    return -1;
  }

  std::string configfile(argv[1]);

  try
  {
    sap bot(configfile, rng);

    try
    {
      bot.run();
    } catch (const std::exception& ex)
    {
      std::cout << "Error running bot: " << ex.what() << std::endl;
    }
  } catch (const std::exception& ex)
  {
    std::cout << "Error initializing bot: " << ex.what() << std::endl;
  }
}
