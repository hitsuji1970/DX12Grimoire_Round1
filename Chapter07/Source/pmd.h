#include <DirectXMath.h>
#include <vector>

// PMDヘッダー構造体
struct PMDHeader {
	float version;			// 例 : 00 00 80 3F == 1.00
	char model_name[20];	// モデル名
	char comment[256];		// モデルコメント
};

// PMD頂点構造体
struct PMDVertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
	unsigned short boneNo[2];
	unsigned char boneWeight;
	unsigned char endflg;
};

class PMDMesh
{
public:
	// 頂点データのサイズ
	static const size_t VERTEX_SIZE = 38;

private:
	char signature[3];
	PMDHeader header;
	unsigned int vertNum;
	std::vector<unsigned char> rawVertices;

public:
	bool LoadFromFile(LPCTSTR filename);

	const std::vector<unsigned char>& GetRawVertices() const
	{
		return rawVertices;
	}

	unsigned int GetNumberOfVertex()
	{
		return vertNum;
	}
};
