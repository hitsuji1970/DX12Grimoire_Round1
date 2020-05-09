#pragma once

#include <string>
#include <vector>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

/**
 * Direct3D12�̕`���
 */
class D3D12Environment
{
private:
	/** Microsoft::WRL::ComPtr�̌^����Z�k */
	template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	/** �R���X�g���N�^�[ */
	D3D12Environment();

	/** �f�X�g���N�^�[ */
	virtual ~D3D12Environment();

	/** ������ */
	HRESULT Initialize(HWND hWnd, UINT windowWidth, UINT windowHeight);

	/** �R�}���h���X�g�̐��� */
	HRESULT CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineState* pinitialState, REFIID riid, void** ppCommandList);

	/** �R�}���h���X�g�̎��s */
	void ExecuteCommandLists(UINT numCommandLists, ID3D12CommandList* const* ppCommandLists);

	/** DirectX�C���^�[�t�F�C�X�̎擾 */
	const ComPtr<ID3D12Device>& GetDevice() const
	{
		return _device;
	}

	const ComPtr<IDXGISwapChain4> GetSwapChain() const
	{
		return _swapChain;
	}

	const ComPtr<ID3D12CommandAllocator> GetCommandAllocator() const
	{
		return _cmdAllocator;
	}

	const ComPtr<ID3D12DescriptorHeap> GetRenderTargetViewHeaps() const
	{
		return _rtvHeaps;
	}

	const ComPtr<ID3D12DescriptorHeap> GetDepthStencilViewHeap() const
	{
		return _dsvHeap;
	}

	ID3D12Resource* GetBackBuffer(size_t index)
	{
		return _backBuffers[index];
	}

private:
	/** �f�B�X�v���C�A�_�v�^�[�̃C���^�[�t�F�C�X */
	ComPtr<IDXGIFactory6> _dxgiFactory;

	/** DirectX�f�o�C�X�C���^�[�t�F�C�X */
	ComPtr<ID3D12Device> _device;

	/** �R�}���h�A���P�[�^�[: �R�}���h�̎��̂��i�[���郁�����[�̃C���^�[�t�F�C�X */
	ComPtr<ID3D12CommandAllocator> _cmdAllocator;

	/** �R�}���h�L���[ */
	ComPtr<ID3D12CommandQueue> _cmdQueue;

	/** �����_�[�^�[�Q�b�g */
	ComPtr<IDXGISwapChain4> _swapChain;
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	std::vector<ID3D12Resource*> _backBuffers;
	ComPtr<ID3D12Resource> _depthBuffer = nullptr;

	/** �t�F���X */
	ComPtr<ID3D12Fence> _fence;
	UINT64 _fenceVal;

private:
	/** �X���b�v�`�F�C���ƃo�b�N�o�b�t�@�[�̐��� */
	HRESULT CreateBackBuffers(HWND hWnd, UINT bufferWidth, UINT bufferHight);

	/** �[�x�o�b�t�@�[�̐��� */
	HRESULT CreateDepthBuffer(UINT bufferWidth, UINT bufferHeight);

	/** �f�o�b�O���C���[�̗L���� */
	void EnableDebugLayer();

	/** �����̃O���t�B�b�N�X�J�[�h�𓋍ڂ���V�X�e���œ���̃A�_�v�^�[���擾 */
	ComPtr<IDXGIAdapter> FindDXGIAdapter(const std::wstring& key);
};
