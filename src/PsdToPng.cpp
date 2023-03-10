#include "PsdToPng.h"

/*
may contain some unnecessary read/seekg
mostly there for people who want to get data not used in the example
*/
int PsdToPng::ParsePsd(const char* fileName, std::vector<LayerData> &ld)
{
	//check if file successfully opened
	std::ifstream pf(fileName, std::ios::binary);
	if (!pf.is_open())
		return 1;

	char buffer[16];
	pf.seekg(0, std::ios::beg);

#pragma region File Header Section
	//check signature
	pf.read(buffer, 4);
	if (std::strncmp(buffer, "8BPS", 4) != 0)
		return 2;

	//check version
	pf.read(buffer, 2);
	if (buffer[0] != 0 || buffer[1] != 1)
		return 3;

	//skip over junk
	pf.seekg(8, std::ios::cur);

	//get width and height of psd file
	pf.read(buffer, 4);
	pf.read(buffer, 4);

	//skip over junk
	pf.seekg(4, std::ios::cur);
#pragma endregion

#pragma region Color Mode Data Section
	//not important, assume user isn't on index or duotone mode
	pf.seekg(4, std::ios::cur);
#pragma endregion

#pragma region Image Resources Section
	//get image resource length
	pf.read(buffer, 4);

	//skip over junk
	pf.seekg((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF), std::ios::cur);

#pragma endregion

#pragma region Layer and Mask Information Section, also make texture atlas
	//layer info
	//skip junk data
	pf.seekg(8, std::ios::cur);

	//layers count as 1, folders count as 2, don't ask me why cause that's how psd works
	//negative layer counts may exists
	pf.read(buffer, 2);
	int layerCount = std::abs(((buffer[0] & 0xFF) << 24) | ((buffer[1] & 0xFF) << 16));
	layerCount = layerCount >> 16;

	ld.resize(layerCount);

	//read in layer data
	for (int layerNum = 0; layerNum < layerCount; layerNum++)
	{
		pf.read(buffer, 16);
		int posY = (buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF);
		int posX = (buffer[4] << 24) | ((buffer[5] & 0xFF) << 16) | ((buffer[6] & 0xFF) << 8) | (buffer[7] & 0xFF);
		ld[layerNum].h = ((buffer[8] << 24) | ((buffer[9] & 0xFF) << 16) | ((buffer[10] & 0xFF) << 8) | (buffer[11] & 0xFF)) - posY;
		ld[layerNum].w = ((buffer[12] << 24) | ((buffer[13] & 0xFF) << 16) | ((buffer[14] & 0xFF) << 8) | (buffer[15] & 0xFF)) - posX;

		//number of channels
		pf.read(buffer, 2);
		int channels = std::abs(((buffer[0] & 0xFF) << 8) | (buffer[1] & 0xFF));

		//skip over a junk
		pf.seekg(channels * 6 + 10, std::ios::cur);

		//get visibility
		pf.read(buffer, 1);
		ld[layerNum].visible = !(buffer[0] & 2);
		pf.seekg(5, std::ios::cur);

		//layer mask data, junk
		pf.read(buffer, 4);
		pf.seekg((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF), std::ios::cur);

		//layer blending ranges, also junk
		pf.read(buffer, 4);
		pf.seekg((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF), std::ios::cur);

		//read in layer name
		//i have no idea how this part works
		//if the number of characters is (3 + 4x), for example "arm" or "chicken", the null character does not exist at the end
		bool layerNameBad = false;

		//skip a byte cause idk
		pf.seekg(1, std::ios::cur);

		//read until null or signature
		pf.read(buffer, 1);
		while (buffer[0] != '\0')
		{
			//check if the signature exists "8BIM", might also be "8B64", add later if problems exist
			if (buffer[0] == '8')
			{
				pf.read(&buffer[1], 3);
				if (std::strncmp(&buffer[1], "BIM", 3) == 0)
				{
					layerNameBad = true;
					break;
				}
				else
					pf.seekg(-3, std::ios::cur);
			}

			ld[layerNum].layerName += buffer[0];
			pf.read(buffer, 1);
		}

		//skip through null padding and signature if name is bad
		if (!layerNameBad)
		{
			while (buffer[0] == '\0')
			{
				pf.read(buffer, 1);
			}
			pf.seekg(3, std::ios::cur);
		}

		//TODO: add a better way of detecting if image or folder, probably through the signature
		//folder divider names hopefully start with this, Clip Studio Paint is "</Layer set", Krita is "</Layer group>"
		if (ld[layerNum].layerName.compare(0, 7, "</Layer") == 0)
		{
			ld[layerNum].layerType = LayerData::LayerType::divider;

			pf.read(buffer, 4);
			int code = (buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF);

			switch (code)
			{
			//lsct, divider signature for Clip Studio Paint
			case 1819501428:
				//also skip a bunch of extra stuff, number of bytes are wrong on documentation but who cares
				pf.seekg(44, std::ios::cur);

				//skip unicode name
				pf.read(buffer, 4);
				pf.seekg(2 * ((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF)), std::ios::cur);

				//some junk I think, hope nothing breaks
				pf.seekg(16, std::ios::cur);
				break;
			//luni, divider signature for Krita
			case 1819635305:
				//skip unicode name
				pf.read(buffer, 4);
				pf.seekg(((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF)), std::ios::cur);

				//should say 8BIM lsct, skip section divider setting
				pf.seekg(24, std::ios::cur);
				break;
			}

		}
		//if not divider
		else
		{
			//documentation is horrible why is Additional Layer Information section in a random place

			//distinguish if folder or image layer
			pf.read(buffer, 4);

			int code = (buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF);
			switch (code)
			{
			//lsct, divider end for Clip Studio Paint
			case 1819501428:
				ld[layerNum].layerType = LayerData::LayerType::folder;

				//skip a few things
				pf.seekg(44, std::ios::cur);

				//skip unicode name
				pf.read(buffer, 4);
				pf.seekg(2 * ((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF)), std::ios::cur);

				//junk
				pf.seekg(16, std::ios::cur);
				break;

			//lspf, Clip Studio Paint raster layers start wit this
			case 1819504742:
				//skip a bunch of junk
				pf.read(buffer, 4);
				pf.seekg((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF), std::ios::cur);

				//skip over junk
				pf.seekg(12, std::ios::cur);

				//skip unicode name
				pf.read(buffer, 4);
				pf.seekg(2 * ((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF)), std::ios::cur);

				//skip more junk
				pf.seekg(16, std::ios::cur);
				break;

			//luni, both Krita raster and divider end starts with this
			case 1819635305:
				//skip unicode name
				pf.read(buffer, 4);
				pf.seekg((buffer[0] << 24) | ((buffer[1] & 0xFF) << 16) | ((buffer[2] & 0xFF) << 8) | (buffer[3] & 0xFF), std::ios::cur);

				//check to see if divider end for Krita
				pf.read(buffer, 4);
				//if folder
				if (std::strncmp(buffer, "8BIM", 4) == 0)
				{
					ld[layerNum].layerType = LayerData::LayerType::folder;
					pf.seekg(20, std::ios::cur);
				}
				//if raster layer
				else
				{
					pf.seekg(-4, std::ios::cur);
				}
				break;
			default:
				//Unsupported signature
				return 4;
			}

		}
	}

	//Channel Image Data

	//get rects for each layer
	for (int layerNum = 0; layerNum < layerCount; layerNum++)
	{
		//figure out compression method
		pf.read(buffer, 2);
		int compression = ((buffer[0] & 0xFF) << 8) | (buffer[1] & 0xFF);

		int channelBytesLeft = 0, channelOffset = 0, pixelsRead = 0;

		switch (compression)
		{
		//Raw Compression
		//I'm guessing this only happens in folders so if not... good luck
		case 0:
			pf.seekg(6, std::ios::cur);

			break;
		//RLE Compression
		//only happens in image layers, I hope
		case 1:
			ld[layerNum].imageData.resize(ld[layerNum].w * ld[layerNum].h * 4);

			//channel order is ARGB in psd, RGBA in png
			for (int channel = 0; channel < 4; channel++)
			{
				if (channel == 0)
					channelOffset = 3;
				else
					channelOffset = channel - 1;

				//read each row's byte count
				for (int i = 0; i < ld[layerNum].h; i++)
				{
					pf.read(buffer, 2);
					channelBytesLeft += ((buffer[0] & 0xFF) << 8) | (buffer[1] & 0xFF);
				}

				while (channelBytesLeft > 1)
				{
					//read the count
					pf.read(buffer, 1);
					channelBytesLeft--;

					//RLE compressed part
					if ((int)buffer[0] < 0)
					{
						pf.read(buffer + 1, 1);
						channelBytesLeft--;
						for (int i = 0; i < -buffer[0] + 1; i++)
						{
							ld[layerNum].imageData[(pixelsRead % ld[layerNum].w
								+ (pixelsRead / ld[layerNum].w) * ld[layerNum].w)
								* 4 + channelOffset] = buffer[1];
							pixelsRead++;
						}
					}
					//not RLE compressed part
					else
					{
						for (int i = 0; i < buffer[0] + 1; i++)
						{
							pf.read(buffer + 1, 1);
							channelBytesLeft--;
							ld[layerNum].imageData[(pixelsRead % ld[layerNum].w
								+ (pixelsRead / ld[layerNum].w) * ld[layerNum].w)
								* 4 + channelOffset] = buffer[1];
							pixelsRead++;
						}
					}
				}

				//skip over the compression bits, why does it have it for each channel except for the last one? idk
				if (channel != 3)
					pf.seekg(2, std::ios::cur);

				channelBytesLeft = 0;
				pixelsRead = 0;
			}

			break;
		default:
			return 5;
		}
	}

#pragma endregion

	return 0;
}

void PsdToPng::CreatePngs(const char* fileName, const std::vector<LayerData>& ld)
{
	std::string curFolder = std::string("outputs/") + fileName;
	//remove .psd from the end
	curFolder.erase(curFolder.size() - 4);

	//TODO delete existing folder before creating

	std::filesystem::create_directories(curFolder);

	//go in reverse because folder name appears after divider
	for (int i = (int)ld.size() - 1; i >= 0; i--)
	{
		if (ld[i].layerType == LayerData::LayerType::image)
		{
			if (ld[i].visible)
				stbi_write_png((curFolder + '/' + ld[i].layerName + ".png").c_str(), ld[i].w, ld[i].h, 4, &ld[i].imageData[0], ld[i].w * 4);
		}
		else if (ld[i].layerType == LayerData::LayerType::folder)
		{
			if (ld[i].visible)
			{
				curFolder += '/' + ld[i].layerName;
				std::filesystem::create_directory(curFolder);
			}
			//skip folder
			else
			{
				int depth = 0;
				do
				{
					if (ld[i].layerType == LayerData::LayerType::folder)
						depth++;
					else if (ld[i].layerType == LayerData::LayerType::divider)
						depth--;
					i--;
				} while (depth > 0);
				i++;
			}
		}
		//divider
		else
			curFolder.erase(curFolder.find_last_of('/'));
	}
}
