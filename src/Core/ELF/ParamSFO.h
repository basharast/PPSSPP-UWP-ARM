// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <string>
#include <map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Log.h"

class ParamSFOData
{
public:
	void SetValue(std::string key, unsigned int value, int max_size);
	void SetValue(std::string key, std::string value, int max_size);
	void SetValue(std::string key, const u8 *value, unsigned int size, int max_size);

	int GetValueInt(std::string key) const;
	std::string GetValueString(std::string key) const;
	const u8 *GetValueData(std::string key, unsigned int *size) const;

	std::vector<std::string> GetKeys() const;
	std::string GenerateFakeID(std::string filename = "") const;

	std::string GetDiscID() {
		const std::string discID = GetValueString("DISC_ID");
		if (discID.empty()) {
			std::string fakeID = GenerateFakeID();
			WARN_LOG(LOADER, "No DiscID found - generating a fake one: '%s'", fakeID.c_str());
			ValueData data;
			data.type = VT_UTF8;
			data.s_value = fakeID;
			values["DISC_ID"] = data;
			return fakeID;
		}
		return discID;
	}

	bool ReadSFO(const u8 *paramsfo, size_t size);
	bool WriteSFO(u8 **paramsfo, size_t *size) const;

	bool ReadSFO(const std::vector<u8> &paramsfo) {
		if (!paramsfo.empty()) {
			return ReadSFO(&paramsfo[0], paramsfo.size());
		} else {
			return false;
		}
	}

	int GetDataOffset(const u8 *paramsfo, std::string dataName);

	void Clear();

private:
	enum ValueType
	{
		VT_INT,
		VT_UTF8,
		VT_UTF8_SPE	// raw data in u8
	};

	class ValueData
	{
	public:
		ValueType type = VT_INT;
		int max_size = 0;
		std::string s_value;
		int i_value = 0;

		u8* u_value = nullptr;
		unsigned int u_size = 0;

		void SetData(const u8* data, int size);

		~ValueData() {
			if (u_value)
				delete[] u_value;
		}
	};

	std::map<std::string,ValueData> values;
};

