#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <iostream>
#include "../src/PsdToPng.h"

void printData(const std::vector<LayerData>& ld)
{
	int depth = 0;
	for (int i = (int)ld.size() - 1; i >= 0; i--)
	{
		for (int j = 0; j < depth; j++)
			std::cout << '\t';
		std::cout << ld[i].layerName << std::endl;

		if (ld[i].layerType == LayerData::LayerType::folder)
			depth++;
		else if (ld[i].layerType == LayerData::LayerType::divider)
			depth--;
	}
}

int main()
{
	auto fileName = "testing.psd";
	std::vector<LayerData> ld;

	int error = PsdToPng::ParsePsd(fileName, ld);
	if (error == 0)
	{
		printData(ld);
		PsdToPng::CreatePngs(fileName, ld);
	}

	return 0;
}