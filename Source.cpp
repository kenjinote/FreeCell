#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib,"d3d11")
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dxgi")
#pragma comment(lib,"dxguid")
#pragma comment(lib,"gdiplus")

#include <list>
#include <vector>
#include <random>

#include <windows.h>
#include <windowsx.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <d2d1svg.h>
#include <d2d1_3.h>
#include <d2d1_1helper.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <gdiplus.h>
#include "resource.h"

#define CLIENT_WIDTH (960.0f)
#define CLIENT_HEIGHT (731.0f)
#define CARD_WIDTH (224.22508f)
#define CARD_HEIGHT (312.80777f)
#define CARD_SCALE (0.5f)
#define CARD_OFFSET (30.0f)
#define BOARD_OFFSET (10.0f)
#define NOT_FOUND (-1)
#define ANIMATION_TIME (250)

using namespace Gdiplus;

#undef min 

TCHAR szClassName[] = TEXT("FreeCell");
HHOOK g_hHook;

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

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	switch (nCode) {
	case HCBT_ACTIVATE:
	{
		UnhookWindowsHookEx(g_hHook);
		HWND hMes = (HWND)wParam;
		HWND hParent = GetParent(hMes);
		RECT m, w;
		GetWindowRect(hMes, &m);
		GetWindowRect(hParent, &w);
		SetWindowPos(hMes, hParent, (w.right + w.left - m.right + m.left) / 2, (w.bottom + w.top - m.bottom + m.top) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	break;
	}
	return 0;
}

double easeOutExpo(double t, double b, double c, double d)
{
	return (t == d) ? b + c : c * (-pow(2, -10 * t / d) + 1) + b;
}

class Card {
public:
	~Card() {
		SafeRelease(m_svgDocument);
	}
	ID2D1SvgDocument* m_svgDocument;
	int no = 0;
	BOOL bVisible = TRUE;
	BOOL bSelected = FALSE;
	BOOL bCanDrag = FALSE;
	BOOL bDrag = FALSE;
	float animation_from_x = 0;
	float animation_from_y = 0;
	float x = 0;
	float y = 0;
	float width = CARD_WIDTH;
	float height = CARD_HEIGHT;
	float scale = CARD_SCALE;
	float offset_x = 0;
	float offset_y = 0;
	ULONGLONG animation_start_time = 0;
	void Draw(ID2D1DeviceContext6* d2dDeviceContext, ID2D1Brush* brush) {
		if (!d2dDeviceContext) return;
		if (!bVisible) return;
		float xx;
		float yy;
		ULONGLONG now = GetTickCount64();
		if (now < animation_start_time) {
			xx = animation_from_x;
			yy = animation_from_y;
		}
		else if (now > animation_start_time + ANIMATION_TIME) {
			animation_start_time = 0;
			xx = x;
			yy = y;
		}
		else {
			xx = (float)(easeOutExpo((double)(now - animation_start_time), animation_from_x, x - animation_from_x, (double)(ANIMATION_TIME)));
			yy = (float)(easeOutExpo((double)(now - animation_start_time), animation_from_y, y - animation_from_y, (double)(ANIMATION_TIME)));
		}
		D2D1_MATRIX_3X2_F currentTransform;
		d2dDeviceContext->GetTransform(&currentTransform);
		if (bSelected) {
			d2dDeviceContext->SetTransform(currentTransform);
			D2D1_RECT_F rect;
			rect.left = xx;
			rect.top = yy;
			rect.right = xx + scale * width;
			rect.bottom = yy + scale * height;
			d2dDeviceContext->DrawRectangle(rect, brush, 10.0f);
		}
		D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(xx, yy) * currentTransform;
		d2dDeviceContext->SetTransform(transform);
		d2dDeviceContext->DrawSvgDocument(m_svgDocument);
		d2dDeviceContext->SetTransform(currentTransform);
	}
	BOOL CreateSvgDocumentFromResource(_In_opt_ HMODULE hModule,
		_In_ LPCWSTR lpName,
		_In_ LPCWSTR lpType,
		ID2D1DeviceContext6* d2dContext) {
		HRSRC hRes = FindResource(hModule, lpName, lpType);
		if (hRes == NULL) {
			return FALSE;
		}
		HGLOBAL hResLoad = LoadResource(hModule, hRes);
		if (hResLoad == NULL) {
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
			GlobalFree(hMem);
			return FALSE;
		}
		memcpy(ptr, LockResource(hResLoad), nSize);
		GlobalUnlock(hMem);
		if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pIStream))) {
			GlobalFree(hMem);
			return FALSE;
		}
		if (FAILED(d2dContext->CreateSvgDocument(
			pIStream,
			D2D1::SizeF(width, height),
			&m_svgDocument
		))) {
			SafeRelease(pIStream);
			return FALSE;
		}
		SafeRelease(pIStream);
		return TRUE;
	}
	BOOL HitTest(float _x, float _y) {
		if (
			bVisible &&
			_x >= x &&
			_x <= x + scale * width &&
			_y >= y &&
			_y <= y + scale * height
			) {
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

class Board {
public:
	float x = 0;
	float y = 0;
	enum TYPE {
		freecell,
		homecell,
		tablecell
	};
	TYPE type = tablecell;
	bool active = false;
	void push_back(Card* c, ULONGLONG delay = 0ULL) {
		c->animation_start_time = GetTickCount64() + delay;
		c->animation_from_x = c->x;
		c->animation_from_y = c->y;
		if (type == freecell) {
			c->x = x;
			c->y = y;
		}
		else if (type == homecell) {
			c->x = x;
			c->y = y;
		}
		else {
			float cardOffset = CARD_OFFSET;
			c->x = x;
			c->y = y + (pcard_list.size()) * cardOffset;
		}
		pcard_list.push_back(c);
	}
	bool canpush(int card_no) {
		if (type == freecell) {
			return (pcard_list.size() == 0);
		}
		else if (type == homecell) {
			if (pcard_list.size() == 0) {
				return (card_no % 100 == 1);
			}
			else {
				Card* back = pcard_list.back();
				return ((back->no / 100 == card_no / 100) && (back->no % 100 + 1 == card_no % 100));
			}
		}
		else {
			if (pcard_list.size() == 0) {
				return true;
			}
			else {
				Card* back = pcard_list.back();
				return (((back->no / 100 + card_no / 100) % 2 == 1) && (back->no % 100 == card_no % 100 + 1));
			}
		}
		return false;
	}
	void clear() {
		pcard_list.clear();
	}
	void SetCanDragCard() {
		for (auto& i : pcard_list) {
			i->bCanDrag = FALSE;
		}
		if (type == freecell || type == homecell) {
			if (pcard_list.size() > 0) {
				Card* back = pcard_list.back();
				back->bCanDrag = TRUE;
			}
		}
		else if (type == tablecell) {
			bool first = true;
			bool second = false;
			int prev = 0;
			for (auto i = pcard_list.rbegin(), e = pcard_list.rend(); i != e; ++i) {
				if (first) {
					(*i)->bCanDrag = TRUE;
					first = false;
					second = true;
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
	void draw(ID2D1DeviceContext6* d2dDeviceContext, ID2D1SolidColorBrush* selectBrush, ID2D1SolidColorBrush* emptyBrush) {
		D2D1_RECT_F rect;
		rect.left = x;
		rect.top = y;
		rect.right = x + CARD_SCALE * CARD_WIDTH;
		rect.bottom = y + CARD_SCALE * CARD_HEIGHT;
		d2dDeviceContext->FillRectangle(rect, emptyBrush);
		for (auto ii = pcard_list.begin(), e = pcard_list.end(); ii != e; ++ii) {
			if (!(*ii)->bDrag) {
				(*ii)->Draw(d2dDeviceContext, selectBrush);
			}
		}
	}
	void NormalizationPos() {
		if (type == freecell || type == homecell) {
			for (auto p : pcard_list) {
				p->animation_start_time = GetTickCount64();
				p->animation_from_x = p->x;
				p->animation_from_y = p->y;
				p->x = x;
				p->y = y;
			}
		}
		else {
			int i = 0;
			float cardOffset = CARD_OFFSET;
			for (auto p : pcard_list) {
				p->animation_start_time = GetTickCount64();
				p->animation_from_x = p->x;
				p->animation_from_y = p->y;
				p->x = x;
				p->y = y + i * cardOffset;
				i++;
			}
		}
	}
	size_t size() {
		return pcard_list.size();
	}
	void pop_back() {
		pcard_list.pop_back();
	}
	Card* back() {
		if (pcard_list.size() == 0) return 0;
		return pcard_list.back();
	}
	void GetCardListFromPos(float _x, float _y, std::vector<Card*>& dragcard) {
		dragcard.clear();
		for (auto it = pcard_list.rbegin(), end = pcard_list.rend(); it != end; ++it) {
			if ((*it)->HitTest(_x, _y) && (*it)->CanDrag()) {
				dragcard.push_back(*it);
				break;
			}
		}
		if (dragcard.size() > 0) {
			bool first = false;
			for (auto it = pcard_list.begin(), end = pcard_list.end(); it != end; ++it) {
				if (*it == dragcard[0]) {
					first = true;
					continue;
				}
				if (first) {
					dragcard.push_back(*it);
				}
			}
		}
	}
	void GetCardListFromCount(byte count, std::vector<Card*>& dragcard) {
		dragcard.clear();
		int i = 0;
		for (auto it = pcard_list.begin(), end = pcard_list.end(); it != end; ++it) {
			if (pcard_list.size() - count <= i) {
				dragcard.push_back(*it);
			}
			i++;
		}
	}
	void resize(size_t size) {
		pcard_list.resize(size);
	}
private:
	std::vector<Card*> pcard_list; // ● cardsからpcard_listに名前変更
};

struct operation {
	byte from_board_no;
	byte to_board_no;
	byte card_count;
};

class Game {
public:
	ID2D1SolidColorBrush* selectBrush = NULL;
	ID2D1SolidColorBrush* emptyBrush = NULL;
	ID2D1Bitmap* m_pClearImage = NULL; // ★追加：クリア画像用ビットマップ
	BOOL m_bGameClear = FALSE;         // ★追加：ゲームクリアフラグ
	std::list<Card*> pcard;
	std::vector<Card*> dragcard;
	Board board[16];
	int from_board_no = 0;
	HWND hWnd;
	ULONGLONG animation_start_time = 0LL;
	std::vector<operation> buffer;
	int generation = 0;
	float m_scale = 1.0f;
	D2D1_POINT_2F m_offset = { 0.0f, 0.0f };
	Game::Game(HWND hWnd, ID2D1DeviceContext6* d2dDeviceContext) {
		this->hWnd = hWnd;
		HRESULT hr = S_OK;
		if (SUCCEEDED(hr)) {
			hr = d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.5F), &selectBrush);
		}
		if (SUCCEEDED(hr)) {
			hr = d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.5F), &emptyBrush);
		}
		if (SUCCEEDED(hr)) {
			LoadResourcePng(d2dDeviceContext, MAKEINTRESOURCE(IDB_PNG2), &m_pClearImage);
		}
		if (SUCCEEDED(hr)) {
			const int ids[] = {
				IDR_SVG301, IDR_SVG302, IDR_SVG303, IDR_SVG304, IDR_SVG305, IDR_SVG306, IDR_SVG307, IDR_SVG308, IDR_SVG309, IDR_SVG310, IDR_SVG311, IDR_SVG312, IDR_SVG313,
				IDR_SVG401, IDR_SVG402, IDR_SVG403, IDR_SVG404, IDR_SVG405, IDR_SVG406, IDR_SVG407, IDR_SVG408, IDR_SVG409, IDR_SVG410, IDR_SVG411, IDR_SVG412, IDR_SVG413,
				IDR_SVG201, IDR_SVG202, IDR_SVG203, IDR_SVG204, IDR_SVG205, IDR_SVG206, IDR_SVG207, IDR_SVG208, IDR_SVG209, IDR_SVG210, IDR_SVG211, IDR_SVG212, IDR_SVG213,
				IDR_SVG101, IDR_SVG102, IDR_SVG103, IDR_SVG104, IDR_SVG105, IDR_SVG106, IDR_SVG107, IDR_SVG108, IDR_SVG109, IDR_SVG110, IDR_SVG111, IDR_SVG112, IDR_SVG113
			};
			for (int i = 0; i < _countof(ids); i++) {
				Card* p = new Card;
				p->no = ids[i];
				p->CreateSvgDocumentFromResource(GetModuleHandle(0), MAKEINTRESOURCE(ids[i]), L"SVG", d2dDeviceContext);
				pcard.push_back(p);
			}
		}
		float cardWidthScaled = CARD_SCALE * CARD_WIDTH;
		float totalCardsWidth = 8.0f * cardWidthScaled;
		float totalEmptySpace = CLIENT_WIDTH - totalCardsWidth;
		float gap = totalEmptySpace / 9.0f;
		float sideMargin = gap;
		float columnPitch = cardWidthScaled + gap;
		for (int i = 0; i < _countof(board); i++) {
			if (i < 8) { // 上段 (フリーセル, ホームセル)
				board[i].x = sideMargin + columnPitch * i;
				board[i].y = BOARD_OFFSET;
				board[i].type = (i < 4) ? Board::freecell : Board::homecell;
			}
			else { // 下段 (テーブル)
				board[i].x = sideMargin + columnPitch * (i - 8);
				board[i].y = CARD_SCALE * CARD_HEIGHT + 2.0f * BOARD_OFFSET;
				board[i].type = Board::tablecell;
			}
		}
	}
	~Game() {
		for (auto& i : pcard) {
			delete i;
			i = 0;
		}
		SafeRelease(selectBrush);
		SafeRelease(emptyBrush);
		SafeRelease(m_pClearImage);
	}
	void Game::UpdateLayout(D2D1_SIZE_F physicalSize) {
		float scaleX = physicalSize.width / CLIENT_WIDTH;
		float scaleY = physicalSize.height / CLIENT_HEIGHT;
		m_scale = std::min(scaleX, scaleY);
		float scaledWidth = CLIENT_WIDTH * m_scale;
		float scaledHeight = CLIENT_HEIGHT * m_scale;
		m_offset.x = (physicalSize.width - scaledWidth) / 2.0f;
		m_offset.y = (physicalSize.height - scaledHeight) / 2.0f;
	}
	void TransformMouse(float& x, float& y) {
		x = (x - m_offset.x) / m_scale;
		y = (y - m_offset.y) / m_scale;
	}
	int Game::GetBoardIndexFromPoint(float x, float y) {
		float cardWidthScaled = CARD_SCALE * CARD_WIDTH;
		float totalCardsWidth = 8.0f * cardWidthScaled;
		float totalEmptySpace = CLIENT_WIDTH - totalCardsWidth;
		float gap = totalEmptySpace / 9.0f;
		float sideMargin = gap;
		float columnPitch = cardWidthScaled + gap;
		if (x < sideMargin || x >= CLIENT_WIDTH - sideMargin) {
			return -1;
		}
		int columnIndex = static_cast<int>((x - sideMargin) / columnPitch);
		if (columnIndex < 0) columnIndex = 0;
		if (columnIndex > 7) columnIndex = 7;
		bool isBottomRow = (y > CARD_SCALE * CARD_HEIGHT + 1.5 * BOARD_OFFSET);
		return columnIndex + (isBottomRow ? 8 : 0);
	}
	void OnNewGame(long gameNumber) {
		{
			WCHAR szTitle[256];
			WCHAR szAppName[256];
			LoadString(NULL, IDS_STRING430, szAppName, _countof(szAppName));
			wsprintf(szTitle, TEXT("%s #%ld"), szAppName, gameNumber);
			SetWindowTextW(hWnd, szTitle);
		}
		m_bGameClear = FALSE;
		UnSelectAll();
		UnDragAll();
		for (auto& i : board) {
			i.clear();
		}
		generation = 0;
		buffer.clear();
		RECT rc;
		GetClientRect(hWnd, &rc);
		UpdateLayout(D2D1::SizeF(static_cast<float>(rc.right - rc.left), static_cast<float>(rc.bottom - rc.top)));
		for (auto& c : pcard) {
			c->x = CLIENT_WIDTH / 2.0f - (CARD_SCALE * c->width) / 2.0f;
			c->y = CLIENT_HEIGHT;
		}
		InvalidateRect(hWnd, 0, 0);
		UpdateWindow(hWnd);
		ULONGLONG total_delay = 0;
		if (gameNumber == -1 || gameNumber == -2) {
			std::vector<Card*> base_deck(pcard.begin(), pcard.end());
			ULONGLONG delay = 0;
			if (gameNumber == -1) {
				const int odd_ranks[] = { 0, 2, 4, 6, 8, 10, 12 };
				const int even_ranks[] = { 11, 9, 7, 5, 3, 1 };
				for (int i = 0; i < 4; ++i) {
					for (int rank_idx : odd_ranks) { board[8 + i].push_back(base_deck[i * 13 + rank_idx], delay); delay += 20; }
				}
				for (int i = 0; i < 4; ++i) {
					for (int rank_idx : even_ranks) { board[12 + i].push_back(base_deck[i * 13 + rank_idx], delay); delay += 20; }
				}
			}
			else {
				for (int suit = 0; suit < 4; ++suit) {
					for (int rank = 12; rank >= 0; --rank) { board[8 + suit].push_back(base_deck[suit * 13 + rank], delay); delay += 20; }
				}
			}
			total_delay = (delay > 0) ? (delay - 20) : 0;
		}
		else {
			long holdrand = (long)gameNumber;
			std::vector<int> card_indices(52);
			for (int i = 0; i < 52; i++) card_indices[i] = 51 - i;
			for (int i = 0; i < 51; i++) {
				holdrand = holdrand * 214013L + 2531011L;
				int r = (holdrand >> 16) & 0x7fff;
				int j = 51 - r % (52 - i);
				std::swap(card_indices[i], card_indices[j]);
			}
			std::vector<Card*> base_deck(pcard.begin(), pcard.end());
			std::vector<Card*> sorted_base_deck(52);
			for (int rank = 0; rank < 13; ++rank) {
				for (int suit = 0; suit < 4; ++suit) { sorted_base_deck[rank * 4 + suit] = base_deck[suit * 13 + rank]; }
			}
			std::vector<Card*> deck(52);
			for (int i = 0; i < 52; i++) deck[i] = sorted_base_deck[card_indices[i]];
			ULONGLONG delay = 0;
			for (int i = 0; i < 52; i++) {
				board[8 + i % 8].push_back(deck[i], delay);
				delay += 20;
			}
			total_delay = (delay > 0) ? (delay - 20) : 0;
		}
		AnimationStart(total_delay);
		SetCanDragCard();
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
	}
	void SetCanDragCard() {
		for (auto& m : board) {
			m.SetCanDragCard();
		}
	}
	BOOL CanDrop(int card_no, int board_no) {
		if (board_no < 0 || _countof(board) <= board_no) return FALSE;
		return board[board_no].canpush(card_no);
	}
	void DrawBoard(ID2D1DeviceContext6* d2dDeviceContext) {
		D2D1_SIZE_F physicalSize = d2dDeviceContext->GetSize();
		UpdateLayout(physicalSize);
		d2dDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::ForestGreen));
		D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(m_scale, m_scale) * D2D1::Matrix3x2F::Translation(m_offset.x, m_offset.y);
		d2dDeviceContext->SetTransform(transform);
		for (int i = _countof(board) - 1; i >= 0; i--) {
			if (board[i].active == false) {
				board[i].draw(d2dDeviceContext, selectBrush, emptyBrush);
			}
		}
		for (int i = _countof(board) - 1; i >= 0; i--) {
			if (board[i].active == true) {
				board[i].draw(d2dDeviceContext, selectBrush, emptyBrush);
			}
		}
		for (auto& i : dragcard) {
			i->Draw(d2dDeviceContext, selectBrush);
		}
		d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
		if (m_bGameClear && m_pClearImage)
		{
			D2D1_SIZE_F imageSize = m_pClearImage->GetSize();
			float imgScale = 0.3f * m_scale;
			float scaledWidth = imageSize.width * imgScale;
			float scaledHeight = imageSize.height * imgScale;
			D2D1_RECT_F destinationRect = D2D1::RectF(
				m_offset.x + BOARD_OFFSET * m_scale,
				m_offset.y + CLIENT_HEIGHT * m_scale - scaledHeight - BOARD_OFFSET * m_scale,
				m_offset.x + BOARD_OFFSET * m_scale + scaledWidth,
				m_offset.y + CLIENT_HEIGHT * m_scale - BOARD_OFFSET * m_scale
			);
			d2dDeviceContext->DrawBitmap(m_pClearImage, &destinationRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		}
	}
	int CanHome(int card_no) {
		for (int i = 4; i < 8; i++) {
			if (board[i].canpush(card_no)) {
				return i;
			}
		}
		return NOT_FOUND;
	}
	int CanFreeCell(int card_no) {
		for (int i = 0; i < 4; i++) {
			if (board[i].canpush(card_no)) {
				return i;
			}
		}
		return NOT_FOUND;
	}
	BOOL IsGameClear() {
		for (int i = 4; i < 8; i++)
		{
			if (board[i].size() != 13) return FALSE;
		}
		return TRUE;
	}
	void AskNewGame() {
		WCHAR szPrompt[256];
		LoadString(GetModuleHandle(0), IDS_STRING431, szPrompt, _countof(szPrompt));
		WCHAR szTitle[128];
		LoadString(GetModuleHandle(0), IDS_STRING430, szTitle, _countof(szTitle));
		g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, NULL, GetCurrentThreadId());
		if (MessageBox(hWnd, szPrompt, szTitle, MB_YESNOCANCEL) == IDYES) {
			SendMessage(hWnd, WM_COMMAND, ID_NEW_GAME, 0);
		}
	}
	void OnLButtonDoubleClick(float x, float y) {
		UnSelectAll();
		UnDragAll();
		int from_board_no = GetBoardIndexFromPoint(x, y);
		if (from_board_no < 0) return; // 余白クリックは無視
		if (4 <= from_board_no && from_board_no < 8) return; // freecellのクリックは無視
		if (board[from_board_no].size() > 0) {
			Card* p = board[from_board_no].back();
			int to_board_no = CanHome(p->no);
			if (to_board_no == NOT_FOUND && from_board_no >= 4) to_board_no = CanFreeCell(p->no);
			if (to_board_no != NOT_FOUND) {
				SetActiveBoard(to_board_no);
				AnimationStart();
				board[to_board_no].push_back(p);
				board[from_board_no].pop_back();
				operation op = { (byte)from_board_no, (byte)to_board_no, (byte)1 };
				Operation(op);
				SetCanDragCard();
				InvalidateRect(hWnd, 0, 0);
				if (IsGameClear()) {
					m_bGameClear = TRUE; // ★追加
					AskNewGame();
				}
				else {
					AutoMove(ANIMATION_TIME);
				}
			}
		}
	}
	void OnLButtonDown(float x, float y) {
		UnSelectAll();
		UnDragAll();
		int board_no = GetBoardIndexFromPoint(x, y);
		if (board_no < 0) return;
		board[board_no].GetCardListFromPos(x, y, dragcard);
		if (dragcard.size() > 0) {
			for (auto card : dragcard) {
				card->bSelected = TRUE;
				card->bDrag = TRUE;
				card->offset_x = x - card->x;
				card->offset_y = y - card->y;
			}
			from_board_no = board_no;
			SetCapture(hWnd);
		}
		InvalidateRect(hWnd, 0, 0);
	}
	void OnMouseMove(float x, float y) {
		if (dragcard.size() > 0) {
			for (auto card : dragcard) {
				card->x = x - card->offset_x;
				card->y = y - card->offset_y;
			}
			InvalidateRect(hWnd, 0, 0);
		}
	}
	void SetActiveBoard(int board_no) {
		for (int i = 0; i < _countof(board); i++) {
			if (i == board_no) {
				board[i].active = true;
			}
			else {
				board[i].active = false;
			}
		}
	}
	void OnLButtonUP(float x, float y) {
		if (dragcard.size() > 0) {
			ReleaseCapture();
			Card* back = dragcard.back();
			int card_center_x = static_cast<int>(back->x + CARD_SCALE * back->width / 2.0f);
			int card_center_y = static_cast<int>(back->y + CARD_SCALE * back->height / 2.0f);
			int to_board_no = GetBoardIndexFromPoint(static_cast<float>(card_center_x), static_cast<float>(card_center_y));
			if (
				to_board_no != from_board_no &&
				to_board_no >= 0 &&
				(to_board_no >= 8 || dragcard.size() == 1) &&
				CanDrop(dragcard[0]->no, to_board_no) &&
				dragcard.size() <= GetSpaceCount() + 1)
			{
				SetActiveBoard(to_board_no);
				for (auto card : dragcard) {
					AnimationStart();
					board[to_board_no].push_back(card);
				}
				board[from_board_no].resize(board[from_board_no].size() - dragcard.size()); //元の列から要素消す
				operation op = { (byte)from_board_no, (byte)to_board_no, (byte)dragcard.size() };
				Operation(op);
				UnSelectAll();
				SetCanDragCard();
				if (IsGameClear()) {
					m_bGameClear = TRUE;
					AskNewGame();
				}
				else {
					AutoMove();
				}
			}
			else {
				SetActiveBoard(from_board_no);
				AnimationStart();
				board[from_board_no].NormalizationPos();
			}
			UnDragAll();
			InvalidateRect(hWnd, 0, 0);
		}
	}
	void AnimationStart(ULONGLONG delay = 0ULL) {
		animation_start_time = GetTickCount64() + delay;
		SetTimer(hWnd, 0x1234, 1, NULL);
	}
	void OnTimer() {
		ULONGLONG now = GetTickCount64();
		if (now > animation_start_time + ANIMATION_TIME * 2) {
			KillTimer(hWnd, 0x1234);
			SetActiveBoard(-1);
		}
		InvalidateRect(hWnd, 0, 0);
	}
	int GetSpaceCount() {
		int nCount = 0;
		for (auto b : board) {
			if (b.type != Board::homecell && b.size() == 0) {
				nCount++;
			}
		}
		return nCount;
	}
	void Operation(operation& op) {
		if (generation == buffer.size()) {
			buffer.push_back(op);
		}
		else {
			buffer.resize(generation);
			buffer.push_back(op);
		}
		generation++;
	}
	void OnUndo() {
		if (generation <= 0) return;
		byte from_board_no = buffer[generation - 1].from_board_no;
		byte to_board_no = buffer[generation - 1].to_board_no;
		byte card_count = buffer[generation - 1].card_count;
		board[to_board_no].GetCardListFromCount(card_count, dragcard);
		SetActiveBoard(from_board_no);
		for (auto card : dragcard) {
			AnimationStart();
			board[from_board_no].push_back(card);
		}
		board[to_board_no].resize(board[to_board_no].size() - dragcard.size()); //元の列から要素消す
		dragcard.clear();
		generation--;
	}
	void OnRedo() {
		if (generation == buffer.size()) return;
		byte from_board_no = buffer[generation].from_board_no;
		byte to_board_no = buffer[generation].to_board_no;
		byte card_count = buffer[generation].card_count;
		board[from_board_no].GetCardListFromCount(card_count, dragcard);
		SetActiveBoard(to_board_no);
		for (auto card : dragcard) {
			AnimationStart();
			board[to_board_no].push_back(card);
		}
		board[from_board_no].resize(board[from_board_no].size() - dragcard.size()); //元の列から要素消す
		dragcard.clear();
		generation++;
	}
	void AutoMove(ULONGLONG delay = 0ULL) {
		for (int nMoveCount = 0;; nMoveCount++) {
			bool bMoved = false;
			for (int from_board_no = 0; from_board_no < 16; from_board_no++) {
				if (board[from_board_no].size() > 0 && (board[from_board_no].type == Board::freecell || board[from_board_no].type == Board::tablecell)) {
					Card* card = board[from_board_no].back();
					for (int to_board_no = 4; to_board_no < 8; to_board_no++) {
						if (CanDrop(card->no, to_board_no)) {
							bMoved = true;
							SetActiveBoard(to_board_no);
							AnimationStart(ANIMATION_TIME * nMoveCount + delay);
							board[to_board_no].push_back(card, ANIMATION_TIME * nMoveCount + delay);
							board[from_board_no].resize(board[from_board_no].size() - 1);
							operation op = { (byte)from_board_no, (byte)to_board_no, (byte)1 };
							Operation(op);
							UnSelectAll();
							SetCanDragCard();
							if (IsGameClear()) {
								m_bGameClear = TRUE;
								AskNewGame();
								return;
							}
							break;
						}
					}
				}
				if (bMoved) {
					break;
				}
			}
			if (!bMoved) {
				break;
			}
		}
	}
private:
	HRESULT LoadResourcePng(
		ID2D1DeviceContext* pRenderTarget,
		PCWSTR resourceName,
		ID2D1Bitmap** ppBitmap
	)
	{
		HRESULT hr = S_OK;
		IWICImagingFactory* pIWICFactory = NULL;
		IWICBitmapDecoder* pDecoder = NULL;
		IWICBitmapFrameDecode* pSource = NULL;
		IWICStream* pStream = NULL;
		IWICFormatConverter* pConverter = NULL;
		HRSRC imageResHandle = NULL;
		HGLOBAL imageResDataHandle = NULL;
		void* pImageFile = NULL;
		DWORD imageFileSize = 0;
		hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory));
		if (FAILED(hr)) return hr;
		imageResHandle = FindResource(NULL, resourceName, L"PNG");
		hr = imageResHandle ? S_OK : E_FAIL;
		if (SUCCEEDED(hr)) {
			imageResDataHandle = LoadResource(NULL, imageResHandle);
			hr = imageResDataHandle ? S_OK : E_FAIL;
		}
		if (SUCCEEDED(hr)) {
			pImageFile = LockResource(imageResDataHandle);
			hr = pImageFile ? S_OK : E_FAIL;
		}
		if (SUCCEEDED(hr)) {
			imageFileSize = SizeofResource(NULL, imageResHandle);
			hr = imageFileSize ? S_OK : E_FAIL;
		}
		if (SUCCEEDED(hr)) {
			hr = pIWICFactory->CreateStream(&pStream);
		}
		if (SUCCEEDED(hr)) {
			hr = pStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pImageFile), imageFileSize);
		}
		if (SUCCEEDED(hr)) {
			hr = pIWICFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnLoad, &pDecoder);
		}
		if (SUCCEEDED(hr)) {
			hr = pDecoder->GetFrame(0, &pSource);
		}
		if (SUCCEEDED(hr)) {
			hr = pIWICFactory->CreateFormatConverter(&pConverter);
		}
		if (SUCCEEDED(hr)) {
			hr = pConverter->Initialize(pSource, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut);
		}
		if (SUCCEEDED(hr)) {
			hr = pRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, ppBitmap);
		}
		SafeRelease(pIWICFactory);
		SafeRelease(pDecoder);
		SafeRelease(pSource);
		SafeRelease(pStream);
		SafeRelease(pConverter);
		return hr;
	}
};

void CenterWindow(HWND hWnd)
{
	RECT rc, rc2;
	int	x, y;
	HWND hParent = GetParent(hWnd);
	if (hParent) {
		GetWindowRect(hParent, &rc);
	}
	else {
		SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
	}
	GetWindowRect(hWnd, &rc2);
	x = ((rc.right - rc.left) - (rc2.right - rc2.left)) / 2 + rc.left;
	y = ((rc.bottom - rc.top) - (rc2.bottom - rc2.top)) / 2 + rc.top;
	SetWindowPos(hWnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
}

Bitmap* LoadBitmapFromResource(LPCTSTR pName, LPCTSTR pType, HMODULE hInst)
{
	Bitmap* pImage = NULL;
	const HRSRC hResource = FindResource(hInst, pName, pType);
	if (!hResource)
		return NULL;
	const DWORD imageSize = SizeofResource(hInst, hResource);
	if (!imageSize)
		return NULL;
	const void* pResourceData = LockResource(LoadResource(hInst, hResource));
	if (!pResourceData)
		return NULL;
	const HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, imageSize);
	if (hBuffer) {
		void* pBuffer = GlobalLock(hBuffer);
		if (pBuffer) {
			CopyMemory(pBuffer, pResourceData, imageSize);
			IStream* pStream = NULL;
			if (CreateStreamOnHGlobal(hBuffer, TRUE, &pStream) == S_OK) {
				pImage = new Bitmap(pStream);
				pStream->Release();
				if (pImage) {
					if (pImage->GetLastStatus() != Gdiplus::Ok) {
						delete pImage;
						pImage = NULL;
					}
				}
			}
			GlobalUnlock(hBuffer);
		}
	}
	return pImage;
}

INT_PTR CALLBACK SelectGameDialogProc(HWND hDlg, unsigned msg, WPARAM wParam, LPARAM lParam)
{
	static Game* g;
	switch (msg) {
	case WM_INITDIALOG:
	{
		WCHAR szTitle[256];
		LoadString(GetModuleHandle(0), IDS_STRING425, szTitle, _countof(szTitle));
		SetWindowText(hDlg, szTitle);

		WCHAR szGameNumber[256];
		LoadString(GetModuleHandle(0), IDS_STRING426, szGameNumber, _countof(szGameNumber));
		SetDlgItemText(hDlg, IDC_STATIC_GAMENUMBER, szGameNumber);

		WCHAR szCancel[256];
		LoadString(GetModuleHandle(0), IDS_STRING427, szCancel, _countof(szCancel));
		SetDlgItemText(hDlg, IDCANCEL, szCancel);
	}
	g = (Game*)lParam;
	CenterWindow(hDlg);
	SetDlgItemInt(hDlg, IDC_EDIT_SEED, (UINT)(rand() % 32000), FALSE);
	return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			EndDialog(hDlg, LOWORD(wParam));
			int seed = GetDlgItemInt(hDlg, IDC_EDIT_SEED, NULL, TRUE);
			g->OnNewGame(seed);
			return TRUE;
		}
		else if (LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

INT_PTR CALLBACK VersionDialogProc(HWND hDlg, unsigned msg, WPARAM wParam, LPARAM lParam)
{
	static Game* g;
	switch (msg) {
	case WM_INITDIALOG:
	{
		WCHAR szVersionInfomation[256];
		LoadString(GetModuleHandle(0), IDS_STRING428, szVersionInfomation, _countof(szVersionInfomation));
		SetWindowText(hDlg, szVersionInfomation);
		WCHAR szAppName[256];
		LoadString(GetModuleHandle(0), IDS_STRING430, szAppName, _countof(szAppName));
		SetDlgItemTextW(hDlg, IDC_STATIC_APP_NAME, szAppName);
	}
	g = (Game*)lParam;
	CenterWindow(hDlg);
	return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

INT_PTR CALLBACK HelpDialogProc(HWND hDlg, unsigned msg, WPARAM wParam, LPARAM lParam)
{
	static Game* g;
	static Bitmap* pBitmap;
	switch (msg) {
	case WM_INITDIALOG:
	{
		WCHAR szTitle[256];
		LoadString(GetModuleHandle(0), IDS_STRING424, szTitle, _countof(szTitle));
		SetWindowText(hDlg, szTitle);
	}
	g = (Game*)lParam;
	CenterWindow(hDlg);
	{
		WCHAR szHelp[1024];
		LoadString(GetModuleHandle(0), IDS_STRING429, szHelp, _countof(szHelp));
		SetDlgItemText(hDlg, IDC_EDIT_HELP, szHelp);
	}
	return TRUE;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hDlg, &ps);
		if (!pBitmap) {
			pBitmap = LoadBitmapFromResource(MAKEINTRESOURCE(IDB_PNG1), L"PNG", GetModuleHandle(0));
		}
		if (pBitmap) {
			RECT rc;
			GetClientRect(hDlg, &rc);
			int w = pBitmap->GetWidth() * rc.bottom / pBitmap->GetHeight() / 2;
			int h = rc.bottom / 2;
			int x = 16;
			int y = 16;
			{
				Gdiplus::Graphics g(hdc);
				g.DrawImage(pBitmap, x, y, w, h);
			}
		}
		EndPaint(hDlg, &ps);
	}
	break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	case WM_DESTROY:
		if (pBitmap) {
			delete pBitmap;
			pBitmap = 0;
		}
		break;
	}
	return FALSE;
}

HRESULT ResizeSwapChain(
	IDXGISwapChain1* dxgiSwapChain,
	ID2D1DeviceContext6* d2dDeviceContext,
	UINT width,
	UINT height,
	IDXGISurface*& dxgiBackBufferSurface,
	ID2D1Bitmap1*& bmpTarget) {
	HRESULT hr = S_OK;
	d2dDeviceContext->SetTarget(nullptr);
	SafeRelease(bmpTarget);
	SafeRelease(dxgiBackBufferSurface);
	hr = dxgiSwapChain->ResizeBuffers(
		2,
		width,
		height,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		0
	);
	if (FAILED(hr)) return hr;
	hr = dxgiSwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&dxgiBackBufferSurface);
	if (FAILED(hr)) return hr;
	D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
	);
	hr = d2dDeviceContext->CreateBitmapFromDxgiSurface(dxgiBackBufferSurface, &bp, &bmpTarget);
	if (FAILED(hr)) return hr;
	d2dDeviceContext->SetTarget(bmpTarget);
	return hr;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static ID3D11Device* d3dDevice;
	static IDXGIDevice* dxgiDevice;
	static ID2D1Device6* d2dDevice;
	static ID2D1DeviceContext6* d2dDeviceContext;
	static IDXGIFactory2* dxgiFactory;
	static IDXGISwapChain1* dxgiSwapChain;
	static IDXGISurface* dxgiBackBufferSurface;
	static ID2D1Bitmap1* bmpTarget;
	static Game* g;
	switch (msg) {
	case WM_DPICHANGED:
	{
		RECT* const prcNewWindow = (RECT*)lParam;
		if (prcNewWindow) {
			SetWindowPos(hWnd,
				NULL,
				prcNewWindow->left,
				prcNewWindow->top,
				prcNewWindow->right - prcNewWindow->left,
				prcNewWindow->bottom - prcNewWindow->top,
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
		break;
	}
	case WM_SIZE:
	{
		UINT width = LOWORD(lParam);
		UINT height = HIWORD(lParam);
		if (dxgiSwapChain && width > 0 && height > 0) {
			ResizeSwapChain(dxgiSwapChain, d2dDeviceContext, width, height, dxgiBackBufferSurface, bmpTarget);
			InvalidateRect(hWnd, NULL, FALSE);
		}
		break;
	}
	case WM_TIMER:
	{
		if (g) {
			g->OnTimer();
		}
		break;
	}
	case WM_LBUTTONDBLCLK:
	{
		if (g) {
			float x = static_cast<float>(GET_X_LPARAM(lParam));
			float y = static_cast<float>(GET_Y_LPARAM(lParam));
			g->TransformMouse(x, y);
			g->OnLButtonDoubleClick(x, y);
		}
		break;
	}
	case WM_LBUTTONDOWN:
	{
		if (g) {
			float x = static_cast<float>(GET_X_LPARAM(lParam));
			float y = static_cast<float>(GET_Y_LPARAM(lParam));
			g->TransformMouse(x, y);
			g->OnLButtonDown(x, y);
		}
		break;
	}
	case WM_MOUSEMOVE:
	{
		if (g) {
			float x = static_cast<float>(GET_X_LPARAM(lParam));
			float y = static_cast<float>(GET_Y_LPARAM(lParam));
			g->TransformMouse(x, y);
			g->OnMouseMove(x, y);
		}
		break;
	}
	case WM_LBUTTONUP:
	{
		if (g) {
			float x = static_cast<float>(GET_X_LPARAM(lParam));
			float y = static_cast<float>(GET_Y_LPARAM(lParam));
			g->TransformMouse(x, y);
			g->OnLButtonUP(x, y);
		}
		break;
	}
	case WM_CREATE:
	{
		HRESULT hr = S_OK;
		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_1, // Windows 8 以降で有効。ダメなら下にフォールバック
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};
		UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // ← D2D 連携には必須
#ifdef _DEBUG
		deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			deviceFlags,
			featureLevels,
			_countof(featureLevels),
			D3D11_SDK_VERSION,
			&d3dDevice,
			&obtained,
			NULL
		);
		if (FAILED(hr)) return -1;
		hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
		if (FAILED(hr)) return -1;
		UINT factoryFlags = 0;
#ifdef _DEBUG
		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG; // リリース環境では 0 に
#endif
		hr = CreateDXGIFactory2(factoryFlags, __uuidof(IDXGIFactory2), (void**)&dxgiFactory);
		if (FAILED(hr)) return -1;
		if (dxgiFactory) {
			IDXGIFactory* oldFactory = nullptr;
			hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory), (void**)&oldFactory);
			if (SUCCEEDED(hr) && oldFactory) {
				oldFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
				oldFactory->Release();
			}
		}
		RECT rc;
		GetClientRect(hWnd, &rc);
		UINT width = rc.right - rc.left;
		UINT height = rc.bottom - rc.top;
		DXGI_SWAP_CHAIN_DESC1 scd = {};
		scd.Width = width;
		scd.Height = height;
		scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;                            // D2D と相性良し
		scd.Stereo = FALSE;
		scd.SampleDesc.Count = 1;
		scd.SampleDesc.Quality = 0;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.BufferCount = 2;                                             // ダブルバッファ
		scd.Scaling = DXGI_SCALING_NONE;
		scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;                   // 推奨（Win8+）
		scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;                              // HWND の場合は IGNORE
		hr = dxgiFactory->CreateSwapChainForHwnd(
			d3dDevice,   // IUnknown* (ID3D11Device*)
			hWnd,
			&scd,
			nullptr,       // フルスクリーン記述（未使用）
			nullptr,       // 出力ターゲット（未指定）
			&dxgiSwapChain
		);
		if (FAILED(hr)) return -1;
		D2D1_CREATION_PROPERTIES cp = D2D1::CreationProperties(
			D2D1_THREADING_MODE_SINGLE_THREADED,
#ifdef _DEBUG
			D2D1_DEBUG_LEVEL_INFORMATION,
#else
			D2D1_DEBUG_LEVEL_NONE,
#endif
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE
		);
		hr = D2D1CreateDevice(dxgiDevice, cp, (ID2D1Device**)&d2dDevice);
		if (FAILED(hr)) return -1;
		hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, (ID2D1DeviceContext**)&d2dDeviceContext);
		if (FAILED(hr)) return -1;
		hr = dxgiSwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&dxgiBackBufferSurface);
		if (FAILED(hr)) return -1;
		D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			0.0f,
			0.0f
		);
		hr = d2dDeviceContext->CreateBitmapFromDxgiSurface(
			dxgiBackBufferSurface,
			&bp,
			&bmpTarget
		);
		if (FAILED(hr)) return -1;
		d2dDeviceContext->SetTarget(bmpTarget);
		d2dDeviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
		d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
		d2dDeviceContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE::D2D1_ANTIALIAS_MODE_ALIASED);
		g = new Game(hWnd, d2dDeviceContext);
		if (!g) return -1;
		PostMessage(hWnd, WM_COMMAND, ID_NEW_GAME, 0);
		break;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_NEW_GAME:
		{
			long randomGameNumber = rand() % 32000 + 1;
			g->OnNewGame(randomGameNumber);
		}
		break;
		case ID_SELECT_GAME:
			DialogBoxParam(GetModuleHandle(0), MAKEINTRESOURCE(IDD_SELECT_GAME), hWnd, SelectGameDialogProc, (LPARAM)g);
			break;
		case ID_VERSION:
			DialogBoxParam(GetModuleHandle(0), MAKEINTRESOURCE(IDD_VERSION), hWnd, VersionDialogProc, (LPARAM)g);
			break;
		case ID_EXIT:
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		case ID_UNDO:
			g->OnUndo();
			break;
		case ID_REDO:
			g->OnRedo();
			break;
		case ID_HELP:
			DialogBoxParam(GetModuleHandle(0), MAKEINTRESOURCE(IDD_HELP), hWnd, HelpDialogProc, (LPARAM)g);
			break;
		}
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		if (BeginPaint(hWnd, &ps)) {
			if (d2dDeviceContext && g) {
				d2dDeviceContext->BeginDraw();
				g->DrawBoard(d2dDeviceContext);
				HRESULT hr = d2dDeviceContext->EndDraw();
				if (hr != D2DERR_RECREATE_TARGET) {
					dxgiSwapChain->Present(1, 0);
				}
			}
			EndPaint(hWnd, &ps);
		}
		break;
	}
	case WM_DESTROY:
		KillTimer(hWnd, 0x1234);
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
	// 英語言語で起動
	//SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL));
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	ULONG_PTR gdiToken;
	GdiplusStartupInput gdiSI;
	GdiplusStartup(&gdiToken, &gdiSI, NULL);
	srand((unsigned int)time(NULL));
	MSG msg = {};
	WNDCLASS wndclass = { CS_DBLCLKS, WndProc, 0, 0, hInstance, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)), LoadCursor(0,IDC_ARROW), 0, MAKEINTRESOURCE(IDR_MENU1), szClassName };
	RegisterClass(&wndclass);
	UINT dpi = GetDpiForSystem(); // 最初のウィンドウはシステムのDPIに合わせる
	RECT rect = { 0, 0, (LONG)CLIENT_WIDTH, (LONG)CLIENT_HEIGHT };
	DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	AdjustWindowRectExForDpi(&rect, dwStyle, TRUE, 0, dpi);
	HWND hWnd = CreateWindow(szClassName, 0, dwStyle, CW_USEDEFAULT, 0, rect.right - rect.left, rect.bottom - rect.top, 0, 0, hInstance, 0);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
	while (GetMessage(&msg, 0, 0, 0)) {
		if (!TranslateAccelerator(hWnd, hAccel, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	DestroyAcceleratorTable(hAccel);
	GdiplusShutdown(gdiToken);
	CoUninitialize();
	return (int)msg.wParam;
}