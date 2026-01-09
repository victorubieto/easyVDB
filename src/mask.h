#pragma once

#include <vector>
#include <string>
#include <stdint.h>

namespace easyVDB
{

	// Forward declaration so the compiler knows InternalNode class
	class InternalNode;

	class Mask {
	public:

		InternalNode* targetNode = nullptr;

		int size = 0;
		int onCache = 0;
		int offCache = 0;

		std::vector<std::string> words;
		std::vector<uint8_t> onIndexCache; // this is a bool list

		Mask();
		Mask(const int _size);

		void read(InternalNode* node);
		int countOn();
		int countOff();
		/*void forEachOn();
		void forEachOff();*/
		bool isOn(int offset);
		bool isOff(int offset);
	};

}
