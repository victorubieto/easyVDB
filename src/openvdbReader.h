#pragma once

#include <vector>
#include <string>

#include "grid.h"

struct FileVersion
{
	unsigned int version;
	unsigned int minor;
	unsigned int major;
};

class OpenVDBReader {
public:
	SharedContext sharedContext;
	BufferIterator* bufferIterator;

	// header data
	FileVersion fileVersion;
	bool hasGridOffsets;
	Compression compression;
	std::vector<Metadata> metadata; // list of metadata per grid

	// grid data
	Grid* grids;
	unsigned int gridsSize;

	OpenVDBReader();
	void read(std::vector<uint8_t> source);
	bool isValidFile();
	bool readFileVersion();
	bool readHeader();
	bool readGrids();
};