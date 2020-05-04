#include <d3d12.h>
#include <DirectXMath.h>
#include <Windows.h>
#include <string>

namespace pmd
{
	// PMDヘッダー構造体
	struct PMDHeader {
		float version;			// 例 : 00 00 80 3F == 1.00
		char model_name[20];	// モデル名
		char comment[256];		// モデルコメント
	};

	// PMD頂点構造体
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		unsigned short boneNo[2];
		unsigned char boneWeight;
		unsigned char endflg;
	};

	// PMDマテリアル構造体
#pragma pack(1)
	struct SerializedMaterial
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		DirectX::XMFLOAT3 specular;
		DirectX::XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char edgeFlg;
		// パディング2bytesが入る
		unsigned int indicesNum;
		char texFilePath[20];
	};
#pragma pack()

	struct BasicMatrial
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	struct AdditionalMaterial
	{
		std::wstring texPath;
		int toonIdx;
		bool edgeFlg;
	};

	struct Material
	{
		unsigned int indicesNum;
		BasicMatrial basicMaterial;
		AdditionalMaterial additionalMaterial;
		ID3D12Resource* pTextureResource;
	};

	class PMDMesh
	{
	public:
		// 頂点データのサイズ
		static const size_t VERTEX_SIZE = 38;

		// 頂点レイアウト
		static const std::vector<D3D12_INPUT_ELEMENT_DESC> INPUT_LAYOUT;

		PMDMesh();
		virtual ~PMDMesh();

		HRESULT LoadFromFile(ID3D12Device* const pD3D12Device, const std::wstring& filename);

		unsigned int GetNumberOfVertex()
		{
			return m_numberOfVertex;
		}

		unsigned int GetNumberOfIndex()
		{
			return m_numberOfIndex;
		}

		const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const
		{
			return m_vertexBufferView;
		}

		const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const
		{
			return m_indexBufferView;
		}

		ID3D12DescriptorHeap* GetMaterialDescriptorHeap()
		{
			return m_pMaterialDescHeap;
		}

		const std::vector<Material>& GetMaterials() const
		{
			return m_materials;
		}

	private:
		// ロードしたファイル名
		std::wstring m_loadedModelPath;

		// シグネチャー情報
		char m_signature[3];

		// ヘッダー情報
		PMDHeader m_header;

		// 頂点数
		unsigned int m_numberOfVertex;

		// 頂点バッファー
		ID3D12Resource* m_pVertexBuffer;

		// 頂点バッファービュー
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

		// インデックスの数
		unsigned int m_numberOfIndex;

		// インデックスバッファー
		ID3D12Resource* m_pIndexBuffer;

		// インデックスバッファービュー
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

		// マテリアルの数
		unsigned int m_numberOfMaterial;

		// マテリアルバッファー
		ID3D12Resource* m_pMaterialBuffer;

		// マテリアルディスクリプターヒープ
		ID3D12DescriptorHeap* m_pMaterialDescHeap;

		// マテリアル実体
		std::vector<Material> m_materials;

	private:
		std::wstring GetExtension(const std::wstring& path);
		std::pair<std::wstring, std::wstring> SplitFileName(const std::wstring& src, const wchar_t separator = L'*');
		ID3D12Resource* LoadTextureFromFile(ID3D12Device* const pD3D12Device, const std::wstring& filename);
		ID3D12Resource* CreateWhiteTexture(ID3D12Device* const pD3D12Device);
		void ClearResources();
	};

} // namespace pmd
