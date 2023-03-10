#pragma once

#include <vector>
#include <string>

class LayerData
{
public:
	enum class LayerType : int { image = 0, folder = 1, divider = 2 };

	bool visible = false;
	int w = 0, h = 0;
	std::string layerName = "";
	LayerType layerType = LayerType::image;
	std::vector<unsigned char> imageData;

	LayerData() {}
	~LayerData() {}
};

