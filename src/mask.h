#pragma once

#include <vector>
#include <string>

// Forward declaration so the compiler knows InternalNode class
class InternalNode;

class Mask {
public:

	InternalNode* targetNode;
	
	int size;
	int onCache;
	int offCache;

	std::vector<std::string> words;
	std::vector<uint8_t> onIndexCache; // this is a bool list

	Mask();

	void read(InternalNode* node);
	int countOn();
	int countOff();
	/*void forEachOn();
	void forEachOff();*/
	bool isOn(int offset);
	bool isOff(int offset);
};