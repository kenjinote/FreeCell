#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib,"d3d11")
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dxgi")
#pragma comment(lib,"dxguid")

#include <list>
#include <vector>
#include <random>
#include <algorithm>

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

#define CLIENT_WIDTH (960)
#define CLIENT_HEIGHT (600)

#define CARD_WIDTH (224.22508f)
#define CARD_HEIGHT (312.80777f)
#define CARD_SCALE (0.5f)
#define CARD_OFFSET (30.0f) /* カードが重なるときのオフセット */
#define BOARD_OFFSET (10.0f) /* カード同士の隙間の余白 */

#define NOT_FOUND (-1)

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
	int no;
	BOOL bVisible = TRUE;
	BOOL bSelected = FALSE;
	BOOL bCanDrag = FALSE;
	BOOL bDrag = FALSE; // ドラッグ中はtrue
	float x = 0;
	float y = 0;
	float width = CARD_WIDTH;
	float height = CARD_HEIGHT;
	float scale = CARD_SCALE;
	float offset_x = 0;
	float offset_y = 0;
	void Draw(ID2D1DeviceContext6* d2dDeviceContext, ID2D1Brush * brush) {
		if (!d2dDeviceContext) return;
		if (!bVisible) return;
		if (bSelected) {
			d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
			D2D1_RECT_F rect;
			rect.left = x;
			rect.top = y;
			rect.right = x + scale * width;
			rect.bottom = y + scale * height;
			d2dDeviceContext->DrawRectangle(rect, brush, 10.0f);
		}
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
	BOOL CanDrag() {
		if (bVisible && bCanDrag) {
			return TRUE;
		}
		return FALSE;
	}
};

class game {
public:
	ID2D1SolidColorBrush* selectBrush = NULL;
	ID2D1SolidColorBrush* emptyBrush = NULL;
	std::list<card*> pcard;
	std::vector<card*> dragcard;
	card* freecell[4] = {};
	card* homecell[4] = {};
	std::vector<card*> board[8];
	int old_board_no = -1;
	game(ID2D1DeviceContext6* d2dDeviceContext) {
		HRESULT hr = S_OK;
		if (SUCCEEDED(hr)) {
			hr = d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.5F), &selectBrush);
		}
		if (SUCCEEDED(hr)) {
			hr = d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.5F), &emptyBrush);
		}
		if (SUCCEEDED(hr)) {
			const int ids[] = {
				IDR_SVG101,IDR_SVG102,IDR_SVG103,IDR_SVG104,IDR_SVG105,IDR_SVG106,IDR_SVG107,IDR_SVG108,IDR_SVG109,IDR_SVG110,IDR_SVG111,IDR_SVG112,IDR_SVG113,
				IDR_SVG201,IDR_SVG202,IDR_SVG203,IDR_SVG204,IDR_SVG205,IDR_SVG206,IDR_SVG207,IDR_SVG208,IDR_SVG209,IDR_SVG210,IDR_SVG211,IDR_SVG212,IDR_SVG213,
				IDR_SVG301,IDR_SVG302,IDR_SVG303,IDR_SVG304,IDR_SVG305,IDR_SVG306,IDR_SVG307,IDR_SVG308,IDR_SVG309,IDR_SVG310,IDR_SVG311,IDR_SVG312,IDR_SVG313,
				IDR_SVG401,IDR_SVG402,IDR_SVG403,IDR_SVG404,IDR_SVG405,IDR_SVG406,IDR_SVG407,IDR_SVG408,IDR_SVG409,IDR_SVG410,IDR_SVG411,IDR_SVG412,IDR_SVG413,
			};
			for (int i = 0; i < _countof(ids); i++) {
				card* p = new card;
				p->no = ids[i];
				p->CreateSvgDocumentFromResource(GetModuleHandle(0), MAKEINTRESOURCE(ids[i]), L"SVG", d2dDeviceContext);
				pcard.push_back(p);
			}
		}
	}
	~game() {
		for (auto& i : pcard) {
			delete i;
			i = 0;
		}
		SafeRelease(selectBrush);
		SafeRelease(emptyBrush);
	}
	void start() {
		UnSelectAll();
		UnDragAll();
		for (auto& i : freecell) {
			i = 0;
		};
		for (auto& i : homecell) {
			i = 0;
		};
		for (auto& i : board) {
			i.clear();
		};
		old_board_no = -1;
		{
			// シャッフル
			std::vector<card*> temp(pcard.begin(), pcard.end());
			std::random_device rd;
			std::mt19937 generator(rd());
			std::shuffle(temp.begin(), temp.end(), generator);
			int count[] = { 7, 7, 7, 7, 6, 6, 6, 6 };
			int board_index = 0;
			int card_index = 0;
			float width = CARD_WIDTH;
			float height = CARD_HEIGHT;
			float scale = CARD_SCALE;
			for (auto c : count) {
				for (int i = 0; i < c; i++) {
					temp[card_index]->x = CLIENT_WIDTH / 8.0f * board_index;
					temp[card_index]->y = i * CARD_OFFSET + scale * height + 2.0f * BOARD_OFFSET;
					board[board_index].push_back(temp[card_index]);
					card_index++;
				}
				board_index++;
			}
			std::copy(temp.begin(), temp.end(), pcard.begin());
			SetCanDragCard();
		}
	}
	void UnSelectAll() {
		for (auto& i : pcard) {
			i->bSelected = FALSE;
		}
	}
	void UnDragAll() {
		for (auto& i : pcard) {
			i->bDrag = FALSE;
			i->offset_x = 0.0f;
			i->offset_y = 0.0f;
		}
		dragcard.clear();
		old_board_no = -1;
	}
	void SetCanDragCard() {
		for (auto& i : pcard) {
			i->bCanDrag = FALSE;
		}
		for (auto& i : freecell) {
			if (i) {
				i->bCanDrag = TRUE;
			}
		}
		for (auto& i : pcard) {
			if (i) {
				i->bCanDrag = TRUE;
			}
		}
		for (auto& m : board) {
			bool first = true;
			bool second = false;
			int prev = 0;
			for (auto i = m.rbegin(), e = m.rend(); i != e; ++i) {
				if (first) {
					first = false;
					second = true;
					(*i)->bCanDrag = TRUE;
					prev = (*i)->no;
					continue;
				}
				if (second) {
					if (prev % 100 + 1 == (*i)->no % 100 && (prev / 100 + (*i)->no / 100) % 2 == 1) {
						(*i)->bCanDrag = TRUE;
						prev = (*i)->no;
					}
					else {
						break;
					}
				}
			}
		}
	}
	BOOL CanDrop(int card_no, int board_no) {
		if (board_no < 0 || board_no > 7) return FALSE;
		if (board[board_no].size() == 0) {
			if (card_no % 100 == 13) {
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
		else {
			card * p = board[board_no].back();
			int back = p->no;
			if (card_no % 100 + 1 == back % 100 && (card_no / 100 + back / 100) % 2 == 1) {
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
	}
	void DrawBoard(ID2D1DeviceContext6* d2dDeviceContext) {
		for (int i = 0; i < 8; i++)
		{
			D2D1_RECT_F rect;
			float x = CLIENT_WIDTH / 8.0f * i;
			float y = BOARD_OFFSET;
			rect.left = x;
			rect.top = y;
			rect.right = x + CARD_SCALE * CARD_WIDTH;
			rect.bottom = y + CARD_SCALE * CARD_HEIGHT;
			d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
			d2dDeviceContext->FillRectangle(rect, emptyBrush);
			if (i < 4) {
				if (freecell[i]) {
					freecell[i]->Draw(d2dDeviceContext, selectBrush);
				}
			}
			else {
				if (homecell[i - 4]) {
					homecell[i - 4]->Draw(d2dDeviceContext, selectBrush);
				}
			}
		}
		for (auto& m : board) {
			for (auto ii = m.begin(), e = m.end(); ii != e; ++ii) {
				if (!(*ii)->bDrag) {
					(*ii)->Draw(d2dDeviceContext, selectBrush);
				}
			}
		}
		for (auto& i : dragcard) {
			i->Draw(d2dDeviceContext, selectBrush);
		}
	}
	int CanHome(int card_no) {
		int index = 0;
		for (auto i : homecell) {
			if (i) {
				if ((i->no / 100 == card_no / 100) && (i->no % 100 + 1 == card_no % 100)) {
					return index;
				}
			} else if (card_no % 100 == 1) {				
				return index;
			}
			index++;
		}
		return NOT_FOUND;
	}
	int CanFreeCell(int card_no) {
		int index = 0;
		for (auto i : freecell) {
			if (i == 0) {
				return index;
			}
			index++;
		}
		return NOT_FOUND;
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
	static IDXGISurface* dxgiBackBufferSurface = NULL;
	static ID2D1Bitmap1* bmpTarget = NULL;
	static game* g;
	switch (msg)
	{
	case WM_LBUTTONDBLCLK:
	{
		g->UnSelectAll();
		g->UnDragAll();
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);
		int board_no = x / (CLIENT_WIDTH / 8);
		if (g->board[board_no].size() > 0) {
			card* p = g->board[board_no].back();
			int home_no = g->CanHome(p->no);
			if (home_no != NOT_FOUND) {
				if (g->homecell[home_no]) {
					g->homecell[home_no]->bVisible = FALSE; // すでにあるものは非表示にする
				}
				p->x = CLIENT_WIDTH / 8.0f * (4 + home_no);
				p->y = BOARD_OFFSET;
				p->bCanDrag = FALSE;
				p->bDrag = FALSE;
				p->bSelected = FALSE;
				g->homecell[home_no] = p;
				g->board[board_no].pop_back();
				g->SetCanDragCard();
				InvalidateRect(hWnd, 0, 0);
			}
			else {
				int freecell_no = g->CanFreeCell(p->no);
				if (freecell_no != NOT_FOUND) {
					p->x = CLIENT_WIDTH / 8.0f * freecell_no;
					p->y = BOARD_OFFSET;
					p->bCanDrag = TRUE;
					p->bDrag = FALSE;
					p->bSelected = FALSE;
					g->freecell[freecell_no] = p;
					g->board[board_no].pop_back();
					g->SetCanDragCard();
					InvalidateRect(hWnd, 0, 0);
				}
			}
		}
		break;
	}
	case WM_SIZE:
		break;
	case WM_LBUTTONDOWN:
		{
			g->UnSelectAll();
			g->UnDragAll();
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			int board_no = x / (CLIENT_WIDTH / 8);
			if (y < CARD_SCALE * CARD_HEIGHT + BOARD_OFFSET * 1.5f) {
				for (auto i : g->freecell) {
					if (i && i->HitTest(x, y) && i->CanDrag()) {
						i->bDrag = TRUE;
						g->dragcard.push_back(i);
						break;
					}
				}
				if (g->dragcard.size() == 0) {
					for (auto i : g->homecell) {
						if (i && i->HitTest(x, y) && i->CanDrag()) {
							i->bDrag = TRUE;
							g->dragcard.push_back(i);
							break;
						}
					}
				}
			}
			else {
				for (auto it = g->board[board_no].rbegin(), end = g->board[board_no].rend(); it != end; ++it) {
					if ((*it)->HitTest(x, y) && (*it)->CanDrag()) {
						g->dragcard.push_back(*it);
						break;
					}
				}
				if (g->dragcard.size() > 0) {
					bool first = false;
					for (auto it = g->board[board_no].begin(), end = g->board[board_no].end(); it != end; ++it) {
						if (first) {
							g->dragcard.push_back(*it);
						}
						if (*it == g->dragcard[0]) {
							first = true;
						}
					}
					for (auto it = g->dragcard.begin(), end = g->dragcard.end(); it != end; ++it) {
						card* p = *it;
						p->bSelected = TRUE;
						p->bDrag = TRUE;
						p->offset_x = x - p->x;
						p->offset_y = y - p->y;
					}
					g->old_board_no = board_no;
					SetCapture(hWnd);
				}
			}
			InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_MOUSEMOVE:
		if (g->dragcard.size() > 0) {
			for (auto& i : g->dragcard) {
				i->x = GET_X_LPARAM(lParam) - i->offset_x;
				i->y = GET_Y_LPARAM(lParam) - i->offset_y;
			}
			InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_LBUTTONUP:
		if (g->dragcard.size() > 0) {
			ReleaseCapture();
			BOOL bCanDrop = FALSE;
			card* p0 = g->dragcard[0];
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			int card_no = p0->no;
			int board_no = x / (CLIENT_WIDTH / 8);
			if (y < CARD_SCALE * CARD_HEIGHT + BOARD_OFFSET * 1.5f) {
				if (g->dragcard.size() == 1) {
					if (board_no < 4) {
						if (!g->freecell[board_no]) {
							bCanDrop = TRUE;
							p0->x = CLIENT_WIDTH / 8.0f * board_no;
							p0->y = BOARD_OFFSET;
							g->freecell[board_no] = p0;
							if (g->old_board_no == -1) {
								// TODO: ヘッダーのドラッグ元を削除する処理を書く
							}
							else {
								g->board[g->old_board_no].pop_back();
							}
						}
						else {
							const card* p1 = g->homecell[board_no - 4];
							if ((!p1 && card_no % 100 == 1) || 
							(p1 && (p1->no /100 == p0->no / 100) && (p1->no%100 +1 == p0->no % 100) )) {
								bCanDrop = TRUE;
								p0->x = CLIENT_WIDTH / 8.0f * board_no;
								p0->y = BOARD_OFFSET;
								g->homecell[board_no - 4] = p0;
								if (g->old_board_no == -1) {
									// TODO: ヘッダーのドラッグ元を削除する処理を書く
								}
								else {
									g->board[g->old_board_no].pop_back();
								}
							}
						}
					}
				}
				//for (auto i : g->freecell) {
				//	if (i && i->HitTest(x, y) && i->CanDrag()) {
				//		i->bDrag = TRUE;
				//		g->dragcard.push_back(i);
				//		break;
				//	}
				//}
				//if (g->dragcard.size() == 0) {
				//	for (auto i : g->homecell) {
				//		if (i && i->HitTest(x, y) && i->CanDrag()) {
				//			i->bDrag = TRUE;
				//			g->dragcard.push_back(i);
				//			break;
				//		}
				//	}
				//}
			}
			else {
				if (board_no != g->old_board_no && g->CanDrop(card_no, board_no)) {
					bCanDrop = TRUE;
					const card* last = g->board[board_no].back();
					for (auto p : g->dragcard) {
						p->x = last->x;
						p->y = last->y + 30.0f;
						g->board[board_no].push_back(p);
					}
					//元の列から要素消す
					if (g->old_board_no != -1) {
						g->board[g->old_board_no].resize(g->board[g->old_board_no].size() - g->dragcard.size());
					}
					g->UnSelectAll();
					g->SetCanDragCard();
				}
			}
			if (bCanDrop == FALSE) {
				// もどす
				int i = 0;
				if (g->old_board_no == -1) {
					for (auto p : g->freecell) {
						if (p) {
							p->x = CLIENT_WIDTH / 8.0f * i;
							p->y = BOARD_OFFSET;
						}
						i++;
					}
					for (auto p : g->homecell) {
						if (p) {
							p->x = CLIENT_WIDTH / 8.0f * i;
							p->y = BOARD_OFFSET;
						}
						i++;
					}
				}
				else {
					for (auto p : g->board[g->old_board_no]) {
						p->x = CLIENT_WIDTH / 8.0f * g->old_board_no;
						p->y = i * CARD_OFFSET + CARD_SCALE * CARD_HEIGHT + 20.0f;
						i++;
					}
				}
			}
			g->UnDragAll();
			InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_CREATE:
	{
		HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels
			, 7, D3D11_SDK_VERSION, &d3dDevice, NULL, NULL);
		if (SUCCEEDED(hr)) {
			hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
		}
		DXGI_SWAP_CHAIN_DESC1 dscd = {};
		dscd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		dscd.BufferCount = 2;
		dscd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		dscd.Flags = 0;
		dscd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		dscd.Height = CLIENT_HEIGHT;
		dscd.SampleDesc.Count = 1;
		dscd.SampleDesc.Quality = 0;
		dscd.Scaling = DXGI_SCALING_NONE;
		dscd.Stereo = FALSE;
		dscd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		dscd.Width = CLIENT_WIDTH;
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
			hr = S_OK;
		}
		if (SUCCEEDED(hr)) {
			d2dDeviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
			d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
			d2dDeviceContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE::D2D1_ANTIALIAS_MODE_ALIASED);
		}
		g = new game(d2dDeviceContext);
		if (!g) return -1;
		g->start();
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
			g->DrawBoard(d2dDeviceContext);
			d2dDeviceContext->EndDraw();
			dxgiSwapChain->Present(1, 0);
			EndPaint(hWnd, &ps);
		}
		break;
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
		delete g;
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
		CS_DBLCLKS,
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
	RECT rect = {0, 0, CLIENT_WIDTH, CLIENT_HEIGHT};
	DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	AdjustWindowRect(&rect, dwStyle, FALSE);
	HWND hWnd = CreateWindow(
		szClassName,
		TEXT("FreeCell"),
		dwStyle,
		CW_USEDEFAULT,
		0,
		rect.right - rect.left,
		rect.bottom - rect.top,
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