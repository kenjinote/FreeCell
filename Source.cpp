#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib,"d3d11")
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dxgi")

#include <windows.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <d2d1svg.h>
#include <d2d1_3.h>
#include <d2d1_1helper.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>
#include "resource.h"

TCHAR szClassName[] = TEXT("FreeCell");

D3D_FEATURE_LEVEL featureLevels[] = {
	D3D_FEATURE_LEVEL_11_1,
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
	D3D_FEATURE_LEVEL_9_2,
	D3D_FEATURE_LEVEL_9_1,
};

template <typename T>
inline void SafeRelease(T*& p)
{
	if (NULL != p)
	{
		p->Release();
		p = NULL;
	}
}

BOOL CreateSvgDocumentFromResource(
	_In_opt_ HMODULE hModule,
	_In_ LPCWSTR lpName,
	_In_ LPCWSTR lpType,
	ID2D1DeviceContext6* d2dContext,
	ID2D1SvgDocument** m_svgDocument)
{
	HRSRC hRes = FindResource(hModule, lpName, lpType);
	if (hRes == NULL)
	{
		return FALSE;
	}
	HGLOBAL hResLoad = LoadResource(hModule, hRes);
	if (hResLoad == NULL)
	{
		return FALSE;
	}
	IStream* pIStream = 0;
	int nSize = SizeofResource(hModule, hRes);
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nSize);
	if (!hMem) {
		return FALSE;
	} else {
		LPVOID ptr = GlobalLock(hMem);
		if (ptr) {
			memcpy(ptr, LockResource(hResLoad), nSize);
			GlobalUnlock(hMem);
			HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &pIStream);
			if (SUCCEEDED(hr)) {
				d2dContext->CreateSvgDocument(
					pIStream,
					D2D1::SizeF(50, 50), // Create the document at a size of 500x500 DIPs.
					m_svgDocument
				);
				return TRUE;
			}
		}
	}
	return FALSE;
}

class card {
	ID2D1SvgDocument* m_svgDocument;
	float x = 0;
	float y = 0;
	void Draw(ID2D1DeviceContext6* d2dDeviceContext) {
		if (!d2dDeviceContext) return;
		if (!m_svgDocument) return;
		D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Identity();
		transform = transform * D2D1::Matrix3x2F::Translation(x, y);
		d2dDeviceContext->SetTransform(transform);
		d2dDeviceContext->DrawSvgDocument(m_svgDocument);
	}
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static ID3D11Device* d3dDevice = NULL;
	static IDXGIDevice* dxgiDevice = NULL;
	static ID2D1Device6* d2dDevice = NULL;
	static ID2D1DeviceContext6* d2dDeviceContext = NULL;
	static IDXGIFactory2* dxgiFactory = NULL;
	static IDXGISwapChain1* dxgiSwapChain = NULL;
	static IDXGISurface* dxgiBackBufferSurface = NULL; // Back buffer 
	static ID2D1Bitmap1* bmpTarget = NULL;
	static ID2D1SolidColorBrush* shapeBr = NULL;
	static ID2D1SvgDocument* m_svgDocument[13 * 4] = {};
	switch (msg)
	{
	case WM_CREATE:
	{
		HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels
			, 7, D3D11_SDK_VERSION, &d3dDevice, NULL, NULL);
		if (SUCCEEDED(hr)) {
			hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
		}
		DXGI_SWAP_CHAIN_DESC1 dscd;
		dscd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		dscd.BufferCount = 2;
		dscd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		dscd.Flags = 0;
		dscd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		dscd.Height = 480;
		dscd.SampleDesc.Count = 1;
		dscd.SampleDesc.Quality = 0;
		dscd.Scaling = DXGI_SCALING_NONE;
		dscd.Stereo = FALSE;
		dscd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		dscd.Width = 640;
		if (SUCCEEDED(hr)) {
			hr = D2D1CreateDevice(dxgiDevice, D2D1::CreationProperties(D2D1_THREADING_MODE_SINGLE_THREADED, D2D1_DEBUG_LEVEL_NONE, D2D1_DEVICE_CONTEXT_OPTIONS_NONE), (ID2D1Device**)&d2dDevice);
		}
		if (SUCCEEDED(hr)) {
			hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, (ID2D1DeviceContext**)&d2dDeviceContext);
		}
		if (SUCCEEDED(hr)) {
			hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory2), (void**)&dxgiFactory);
		}
		if (SUCCEEDED(hr)) {
			hr = dxgiFactory->CreateSwapChainForHwnd(d3dDevice, hWnd, &dscd, NULL, NULL, &dxgiSwapChain);
		}
		if (SUCCEEDED(hr)) {
			hr = dxgiSwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&dxgiBackBufferSurface);
		}
		if (SUCCEEDED(hr)) {
			hr = d2dDeviceContext->CreateBitmapFromDxgiSurface(dxgiBackBufferSurface, D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 0, 0), &bmpTarget);
		}
		if (SUCCEEDED(hr)) {
			d2dDeviceContext->SetTarget(bmpTarget);
			hr = d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &shapeBr);
		}
		if (SUCCEEDED(hr)) {
			d2dDeviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
			d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
			d2dDeviceContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE::D2D1_ANTIALIAS_MODE_ALIASED);
		}
		if (SUCCEEDED(hr)) {
			CreateSvgDocumentFromResource(GetModuleHandle(0), MAKEINTRESOURCE(IDR_SVG1), L"SVG", d2dDeviceContext, &m_svgDocument[0]);
			CreateSvgDocumentFromResource(GetModuleHandle(0), MAKEINTRESOURCE(IDR_SVG2), L"SVG", d2dDeviceContext, &m_svgDocument[1]);
		}
		break;
	}
	case WM_PAINT:
	{
		HRESULT hr = S_OK;
		PAINTSTRUCT ps;
		if (BeginPaint(hWnd, &ps))
		{
			d2dDeviceContext->BeginDraw();
			d2dDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::ForestGreen));
			//d2dDeviceContext->DrawLine(D2D1::Point2F(400, 0), D2D1::Point2F(400, 400), shapeBr);
			//d2dDeviceContext->DrawLine(D2D1::Point2F(200.0f, 320.0f), D2D1::Point2F(400.0f, 320.0f), shapeBr);
			//d2dDeviceContext->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(300.0f, 300.0f), 10.0f, 10.0f), shapeBr);
			D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Identity();
			d2dDeviceContext->SetTransform(transform);
			transform = transform * D2D1::Matrix3x2F::Translation(
				100.0f,
				100.0f
			);
			d2dDeviceContext->SetTransform(transform);
			if (m_svgDocument[0]) {
				d2dDeviceContext->DrawSvgDocument(m_svgDocument[0]);
			}
			transform = transform * D2D1::Matrix3x2F::Translation(
				200.0f,
				200.0f
			);
			d2dDeviceContext->SetTransform(transform);
			if (m_svgDocument[1]) {
				d2dDeviceContext->DrawSvgDocument(m_svgDocument[1]);
			}
			d2dDeviceContext->EndDraw();
			dxgiSwapChain->Present(1, 0);
			EndPaint(hWnd, &ps);
		}
		return SUCCEEDED(hr) ? 0 : 1;
	}
	case WM_DESTROY:
		SafeRelease(d3dDevice);
		SafeRelease(dxgiBackBufferSurface);
		SafeRelease(dxgiDevice);
		SafeRelease(dxgiFactory);
		SafeRelease(dxgiSwapChain);
		SafeRelease(d2dDevice);
		SafeRelease(d2dDeviceContext);
		SafeRelease(bmpTarget);
		SafeRelease(shapeBr);
		SafeRelease(m_svgDocument[0]);
		SafeRelease(m_svgDocument[1]);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MSG msg;
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		0,
		LoadCursor(0,IDC_ARROW),
		0,
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	HWND hWnd = CreateWindow(
		szClassName,
		TEXT("FreeCell"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	CoUninitialize();
	return (int)msg.wParam;
}