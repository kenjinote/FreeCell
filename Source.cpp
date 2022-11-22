#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib,"d3d11")
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dxgi")

#include <list>

#include <windows.h>
#include <windowsx.h>
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

class card {
public:
	~card() {
		SafeRelease(m_svgDocument);
	}
	ID2D1SvgDocument* m_svgDocument;
	BOOL bVisible = TRUE;
	float x = 0;
	float y = 0;
	float width = 224.22508f;
	float height = 312.80777f;
	float scale = 0.5f;
	void Draw(ID2D1DeviceContext6* d2dDeviceContext) {
		if (!d2dDeviceContext) return;
		if (!m_svgDocument) return;
		if (!bVisible) return;
		D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Identity();
		transform = transform * D2D1::Matrix3x2F::Scale(scale, scale);
		transform = transform * D2D1::Matrix3x2F::Translation(x, y);
		d2dDeviceContext->SetTransform(transform);
		d2dDeviceContext->DrawSvgDocument(m_svgDocument);
	}
	BOOL CreateSvgDocumentFromResource(_In_opt_ HMODULE hModule,
		_In_ LPCWSTR lpName,
		_In_ LPCWSTR lpType,
		ID2D1DeviceContext6* d2dContext) {
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
		}
		LPVOID ptr = GlobalLock(hMem);
		if (!ptr) {
			return FALSE;
		}
		memcpy(ptr, LockResource(hResLoad), nSize);
		GlobalUnlock(hMem);
		if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pIStream))) {
			return FALSE;
		}
		if (FAILED(d2dContext->CreateSvgDocument(
			pIStream,
			D2D1::SizeF(width, height), // Create the document at a size of 500x500 DIPs.
			&m_svgDocument
		))) {
			return FALSE;
		}
		return TRUE;
	}
	BOOL HitTest(int _x, int _y) {
		if (
			bVisible &&
			_x >= x &&
			_x <= x + scale * width &&
			_y >= y &&
			_y <= y + scale * height
			)
		{
			return TRUE;
		}
		return FALSE;
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
	static std::list<card*> pcard;
	static card* pdragcard = 0;
	static float offset_x = 0;
	static float offset_y = 0;
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		{
			pdragcard = 0;
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			for (auto it = pcard.rbegin(), end = pcard.rend(); it != end; ++it) {
				if ((*it)->HitTest(x, y)) {
					card* p = *it;					
					pcard.erase((++it).base());
					pcard.push_back(p);
					pdragcard = p;
					offset_x = x - p->x;
					offset_y = y - p->y;
					SetCapture(hWnd);
					break;
				}
			}
		}
		break;
	case WM_MOUSEMOVE:
		if (pdragcard) {
			pdragcard->x = GET_X_LPARAM(lParam) - offset_x;
			pdragcard->y = GET_Y_LPARAM(lParam) - offset_y;
			InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_LBUTTONUP:
		if (pdragcard) {
			ReleaseCapture();
			pdragcard = 0;
			offset_x = 0.0f;
			offset_y = 0.0f;			
		}
		break;
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
			const int ids[]={
				IDR_SVG1,IDR_SVG2,IDR_SVG3,IDR_SVG4,IDR_SVG5,IDR_SVG6,IDR_SVG7,IDR_SVG8,IDR_SVG9,IDR_SVG10,IDR_SVG11,IDR_SVG12,IDR_SVG13,
				IDR_SVG14,IDR_SVG15,IDR_SVG16,IDR_SVG17,IDR_SVG18,IDR_SVG19,IDR_SVG20,IDR_SVG21,IDR_SVG22,IDR_SVG23,IDR_SVG24,IDR_SVG25,IDR_SVG26,
				IDR_SVG27,IDR_SVG28,IDR_SVG29,IDR_SVG30,IDR_SVG31,IDR_SVG32,IDR_SVG33,IDR_SVG34,IDR_SVG35,IDR_SVG36,IDR_SVG37,IDR_SVG38,IDR_SVG39,
				IDR_SVG40,IDR_SVG41,IDR_SVG42,IDR_SVG43,IDR_SVG44,IDR_SVG45,IDR_SVG46,IDR_SVG47,IDR_SVG48,IDR_SVG49,IDR_SVG50,IDR_SVG51,IDR_SVG52,
				IDR_SVG53,IDR_SVG54,IDR_SVG55,IDR_SVG56,IDR_SVG57,IDR_SVG58,
				};
			for (int i = 0; i < 58; i++) {
				card* p = new card;
				p->x = i * 20.0f;
				p->y = i * 20.0f;
				p->CreateSvgDocumentFromResource(GetModuleHandle(0), MAKEINTRESOURCE(ids[i]), L"SVG", d2dDeviceContext);
				pcard.push_back(p);
			}
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
			for(auto& i : pcard) {
				i->Draw(d2dDeviceContext);
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
		for (auto& i : pcard) {
			delete i;
			i = 0;
		}
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