#pragma once

#include <fstream>
#include <cstdlib>
#include <filesystem>

#include "LayerData.h"
#include "../vendor/stb_image_write.h"

/*
One of the worst things I have read in my life so far.
https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
*/

class PsdToPng
{
public:
	//returns 0 if success
	static int ParsePsd(const char* fileName, std::vector<LayerData> &ld);
	static void CreatePngs(const char* fileName, const std::vector<LayerData>& lds);
};

