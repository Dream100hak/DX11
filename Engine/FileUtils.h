#pragma once

enum FileMode : uint8
{
	Write,
	Read,
};

class FileUtils
{
public:
	FileUtils();
	~FileUtils();

	void Open(wstring filePath, FileMode mode);

	template<typename T>
	void Write(const T& data)
	{
		DWORD numOfBytes = 0;
		assert(::WriteFile(_handle, &data, sizeof(T), (LPDWORD)&numOfBytes, nullptr));
	}

	template<>
	void Write<string>(const string& data)
	{
		return Write(data);
	}

	void Write(void* data, uint32 dataSize);
	void Write(const string& data);

	template<typename T>
	void Read(OUT T& data)
	{
		DWORD numOfBytes = 0;
		assert(::ReadFile(_handle, &data, sizeof(T), (LPDWORD)&numOfBytes, nullptr));
	}

	template<typename T>
	T Read()
	{
		T data;
		Read(data);
		return data;
	}

	// EOF ?덉슜 ?쎄린 ???щ㎎ ?뺤옣 ?꾨뱶??(援щ쾭???뚯씪?대㈃ false)
	template<typename T>
	bool TryRead(OUT T& data)
	{
		DWORD numOfBytes = 0;
		if (!::ReadFile(_handle, &data, sizeof(T), (LPDWORD)&numOfBytes, nullptr))
			return false;
		return numOfBytes == sizeof(T);
	}

	void Read(void** data, uint32 dataSize);
	void Read(OUT string& data);

private:
	HANDLE _handle = INVALID_HANDLE_VALUE;
};

