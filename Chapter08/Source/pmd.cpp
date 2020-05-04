#include <tchar.h>
#include <iostream>
#include <Windows.h>
#include "pmd.h"

bool PMDMesh::LoadFromFile(LPCTSTR cpFileName)
{
	FILE* fp = nullptr;

	auto state = _tfopen_s(&fp, cpFileName, TEXT("rb"));
	if (state != 0) {
		return false;
	}

	std::fread(signature, sizeof(signature), 1, fp);
	std::fread(&header, sizeof(header), 1, fp);
	std::fread(&vertNum, sizeof(vertNum), 1, fp);

	auto dataSize = vertNum * VERTEX_SIZE;
	rawVertices.resize(dataSize);
	std::fread(rawVertices.data(), dataSize, 1, fp);
	std::fclose(fp);

	return true;

}