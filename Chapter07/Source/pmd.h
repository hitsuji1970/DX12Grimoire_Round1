#include <DirectXMath.h>
#include <vector>

// PMD�w�b�_�[�\����
struct PMDHeader {
	float version;			// �� : 00 00 80 3F == 1.00
	char model_name[20];	// ���f����
	char comment[256];		// ���f���R�����g
};

// PMD���_�\����
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
	// ���_�f�[�^�̃T�C�Y
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
