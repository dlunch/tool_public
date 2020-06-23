#pragma once

#include <string>

//we're in windows, utf16 world. utf32 is not concerned..
inline std::wstring fromUtf8(const std::string &input)
{
	std::wstring result;

	for(size_t i = 0; i < input.length();)
	{
		if(!((input[i] & 0xF0) == 0xF0))
		{
			//1 utf-16 character
			if((input[i] & 0xE0) == 0xE0)
			{
				result.push_back(((input[i] & 0x0f) << 12) | (input[i + 1] & 0x3f) << 6 | (input[i + 2] & 0x3f));
				i += 2;
			}
			else if((input[i] & 0xC0) == 0xC0)
			{
				result.push_back(((input[i] & 0x1f) << 6) | (input[i + 1] & 0x3f));
				i ++;
			}
			else
			{
				result.push_back(input[i]);
			}
		}
		else
		{
			size_t temp = ((input[i] & 0x07) << 18) | ((input[i + 1] & 0x3f) << 12) | ((input[i + 2] & 0x3f) << 6) | (input[i + 3] & 0x3f);
			i += 3;
			temp -= 0x10000;
			result.push_back(static_cast<wchar_t>(0xD800 | (temp & 0xFFC00)));
			result.push_back(static_cast<wchar_t>(0xDC00 | (temp & 0x3FF)));
		}
		i ++;
	}

	return result;
}

inline std::string fromUtf16(const std::wstring &input)
{
	std::string result;
	for(size_t i = 0; i < input.length();)
	{
		if(input[i] < 0xD800 || input[i] > 0xDFFF)
		{ //no surrogate pair
			if(input[i] < 0x80) //1 byte
			{
				result.push_back((char)input[i]);
			}
			else if(input[i] < 0x800)
			{
				result.push_back((char)(0xC0 | ((input[i] & 0x7C0) >> 6)));
				result.push_back((char)(0x80 | (input[i] & 0x3F)));
			}
			else
			{
				result.push_back((char)(0xE0 | ((input[i] & 0xF000) >> 12)));
				result.push_back((char)(0x80 | ((input[i] & 0xFC0) >> 6)));
				result.push_back((char)(0x80 | (input[i] & 0x3F)));
			}
		}
		else
		{
			size_t temp;
			temp = (input[i] & 0x3FF << 10) | (input[i + 1] & 0x3FF);
			temp += 0x10000;
			if(temp < 0x80) //1 byte
			{
				result.push_back((char)temp);
			}
			else if(temp < 0x800)
			{
				result.push_back((char)(0xC0 | ((temp & 0x7C0) >> 6)));
				result.push_back((char)(0x80 | (temp & 0x3F)));
			}
			else if(temp < 0x10000)
			{
				result.push_back((char)(0xE0 | ((temp & 0xF000) >> 12)));
				result.push_back((char)(0x80 | ((temp & 0xFC0) >> 6)));
				result.push_back((char)(0x80 | (temp & 0x3F)));
			}
			else
			{
				result.push_back((char)(0xF0 | ((temp & 0x1C0000) >> 18)));
				result.push_back((char)(0x80 | ((temp & 0x3F000) >> 12)));
				result.push_back((char)(0x80 | ((temp & 0xFC0) >> 6)));
				result.push_back((char)(0x80 | (temp & 0x3F)));
			}
			i ++;
		}
		i ++;
	}

	return result;
}