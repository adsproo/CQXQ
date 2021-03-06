#include "GUI.h"
#define WIN32_LEAN_AND_MEAN
#include <cassert>
#include <map>
#include <string>
#include <Windows.h>
#include <Windowsx.h>
#include <CommCtrl.h>
#include <shellapi.h>
#include <type_traits>
#include <memory>
#include <utility>

#include "GlobalVar.h"
#include "native.h"

using namespace std;

#pragma comment(lib, "comctl32.lib")

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define ID_MASTER_BUTTONENABLE 1001
#define ID_MASTER_BUTTONRELOAD 1002
#define ID_MASTER_BUTTONMENU 1003
#define ID_MASTER_STATICDESC 1004
#define ID_MASTER_LVPLUGIN 1005

template <typename T>
class BaseWindow
{
public:
	virtual ~BaseWindow() = default;

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		T* pThis;

		if (uMsg == WM_NCCREATE)
		{
			auto pCreate = reinterpret_cast<LPCREATESTRUCTA>(lParam);
			pThis = static_cast<T*>(pCreate->lpCreateParams);
			SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));

			pThis->m_hwnd = hwnd;
		}
		else
		{
			pThis = reinterpret_cast<T*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
		}

		if (pThis)
		{
			return pThis->HandleMessage(uMsg, wParam, lParam);
		}
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);
	}

	BaseWindow() : m_hwnd(nullptr)
	{
	}

	BOOL Create(
		PCSTR lpWindowName,
		DWORD dwStyle,
		DWORD dwExStyle = 0,
		int x = CW_USEDEFAULT,
		int y = CW_USEDEFAULT,
		int nWidth = CW_USEDEFAULT,
		int nHeight = CW_USEDEFAULT,
		HWND hWndParent = nullptr,
		HMENU hMenu = nullptr
	)
	{
		WNDCLASS wc{};

		wc.lpfnWndProc = T::WindowProc;
		wc.hInstance = hDllModule;
		wc.lpszClassName = ClassName();

		RegisterClassA(&wc);

		m_hwnd = CreateWindowExA(
			dwExStyle, ClassName(), lpWindowName, dwStyle, x, y,
			nWidth, nHeight, hWndParent, hMenu, hDllModule, this
		);

		return (m_hwnd ? TRUE : FALSE);
	}

	[[nodiscard]] HWND Window() const { return m_hwnd; }

protected:

	[[nodiscard]] virtual PCSTR ClassName() const = 0;
	virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;

	HWND m_hwnd;
};

// 基础ListView类
// 使用之前必须先调用InitCommonControl/InitCommonControlEx
class BasicListView
{
public:
	BasicListView() : hwnd(nullptr)
	{
	}

	[[nodiscard]] HWND Window() const { return hwnd; }

	BOOL Create(
		PCSTR lpWindowName,
		DWORD dwStyle,
		DWORD dwExStyle = 0,
		int x = CW_USEDEFAULT,
		int y = CW_USEDEFAULT,
		int nWidth = CW_USEDEFAULT,
		int nHeight = CW_USEDEFAULT,
		HWND hWndParent = nullptr,
		HMENU hMenu = nullptr
	)
	{
		hwnd = CreateWindowExA(
			dwExStyle, WC_LISTVIEWA, lpWindowName, dwStyle, x, y,
			nWidth, nHeight, hWndParent, hMenu, hDllModule, this
		);

		return (hwnd ? TRUE : FALSE);
	}

	int AddTextColumn(const char* pszText, int width = 150, int fmt = LVCFMT_LEFT, int isubItem = -1)
	{
		LVCOLUMNA lvC;
		lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvC.pszText = const_cast<char*>(pszText);
		lvC.cx = width;
		lvC.fmt = fmt;
		if (isubItem == -1)
		{
			HWND header = ListView_GetHeader(hwnd);
			isubItem = Header_GetItemCount(header);
		}
		return ListView_InsertColumn(hwnd, isubItem, &lvC);
	}

	// 带宽度
	void AddAllTextColumn(const std::vector<std::pair<std::string, int>>& texts)
	{
		for (const auto& item : texts)
		{
			AddTextColumn(item.first.c_str(), item.second);
		}
	}

	void AddAllTextColumn(const std::initializer_list<std::string>& texts)
	{
		for (const auto& item : texts)
		{
			AddTextColumn(item.c_str());
		}
	}

	void AddTextRow(const std::initializer_list<std::string>& texts, int index = -1)
	{
		if (texts.size() == 0) return;
		LVITEMA lvI;
		lvI.mask = LVIF_TEXT;
		lvI.pszText = const_cast<char*>(texts.begin()->c_str());
		if (index == -1)
		{
			index = ListView_GetItemCount(hwnd);
		}
		lvI.iItem = index;
		lvI.iSubItem = 0;
		ListView_InsertItem(hwnd, &lvI);
		int curr = 1;
		for (auto s = texts.begin() + 1; s != texts.end(); s++)
		{
			ListView_SetItemText(hwnd, index, curr, const_cast<char*>(s->c_str()));
			curr++;
		}
	}

	DWORD SetExtendedListViewStyle(DWORD style)
	{
		return ListView_SetExtendedListViewStyle(hwnd, style);
	}

	[[nodiscard]] int GetItemIndexByText(const std::string& text, int iStart = -1)
	{
		LVFINDINFOA info;
		info.flags = LVFI_STRING;
		info.psz = const_cast<char*>(text.c_str());
		return ListView_FindItem(hwnd, iStart, &info);
	}

	void SetItemText(const string& text, int index, int subindex = 0)
	{
		if (index < 0)return;
		ListView_SetItemText(hwnd, index, subindex, const_cast<char*>(text.c_str()));
	}

	// 长度最长为1000
	[[nodiscard]] std::string GetItemText(int index, int subindex = 0)
	{
		char buffer[1000];
		ListView_GetItemText(hwnd, index, subindex, buffer, 1000);
		return buffer;
	}

	BOOL DeleteItemByIndex(int index)
	{
		return ListView_DeleteItem(hwnd, index);
	}

	BOOL DeleteAllItems()
	{
		return ListView_DeleteAllItems(hwnd);
	}

	BOOL DeleteColumn(int iCol = 0)
	{
		return ListView_DeleteColumn(hwnd, iCol);
	}

	BOOL Show(bool show = true)
	{
		return ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
	}

protected:
	HWND hwnd;
};

// 基础Edit类
class BasicEdit
{
public:
	BasicEdit() : hwnd(nullptr)
	{
	}

	[[nodiscard]] HWND Window() const { return hwnd; }

	BOOL Create(
		PCSTR lpWindowName,
		DWORD dwStyle,
		DWORD dwExStyle = 0,
		int x = CW_USEDEFAULT,
		int y = CW_USEDEFAULT,
		int nWidth = CW_USEDEFAULT,
		int nHeight = CW_USEDEFAULT,
		HWND hWndParent = nullptr,
		HMENU hMenu = nullptr
	)
	{
		hwnd = CreateWindowExA(
			dwExStyle, "EDIT", lpWindowName, dwStyle, x, y,
			nWidth, nHeight, hWndParent, hMenu, hDllModule, this
		);

		return (hwnd ? TRUE : FALSE);
	}

	LRESULT SetFont(HFONT hFont)
	{
		return SendMessageA(hwnd, WM_SETFONT, (WPARAM)hFont, 1);
	}

	void SetText(const std::string& text)
	{
		Edit_SetText(hwnd, text.c_str());
	}

	[[nodiscard]] std::string GetText()
	{
		const int length = Edit_GetTextLength(hwnd) + 1;
		const std::unique_ptr<char[]> uptr = std::make_unique<char[]>(length);
		if (uptr)
		{
			Edit_GetText(hwnd, uptr.get(), length);
			return uptr.get();
		}
		return "";
	}

	BOOL Show(bool show = true)
	{
		return ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
	}

protected:
	HWND hwnd;
};

// 基础Button类
class BasicButton
{
public:
	BasicButton() : hwnd(nullptr)
	{
	}

	[[nodiscard]] HWND Window() const { return hwnd; }

	BOOL Create(
		PCSTR lpWindowName,
		DWORD dwStyle,
		DWORD dwExStyle = 0,
		int x = CW_USEDEFAULT,
		int y = CW_USEDEFAULT,
		int nWidth = CW_USEDEFAULT,
		int nHeight = CW_USEDEFAULT,
		HWND hWndParent = nullptr,
		HMENU hMenu = nullptr
	)
	{
		hwnd = CreateWindowExA(
			dwExStyle, "BUTTON", lpWindowName, dwStyle, x, y,
			nWidth, nHeight, hWndParent, hMenu, hDllModule, this
		);

		return (hwnd ? TRUE : FALSE);
	}

	LRESULT SetFont(HFONT hFont)
	{
		return SendMessageA(hwnd, WM_SETFONT, (WPARAM)hFont, 1);
	}

	BOOL Show(bool show = true)
	{
		return ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
	}

	void SetText(const std::string& text)
	{
		Button_SetText(hwnd, text.c_str());
	}

protected:
	HWND hwnd;
};

// 基础Static类
class BasicStatic
{
public:
	BasicStatic() : hwnd(nullptr)
	{
	}

	[[nodiscard]] HWND Window() const { return hwnd; }

	BOOL Create(
		PCSTR lpWindowName,
		DWORD dwStyle,
		DWORD dwExStyle = 0,
		int x = CW_USEDEFAULT,
		int y = CW_USEDEFAULT,
		int nWidth = CW_USEDEFAULT,
		int nHeight = CW_USEDEFAULT,
		HWND hWndParent = nullptr,
		HMENU hMenu = nullptr
	)
	{
		hwnd = CreateWindowExA(
			dwExStyle, "STATIC", lpWindowName, dwStyle, x, y,
			nWidth, nHeight, hWndParent, hMenu, hDllModule, this
		);

		return (hwnd ? TRUE : FALSE);
	}

	LRESULT SetFont(HFONT hFont)
	{
		return SendMessageA(hwnd, WM_SETFONT, (WPARAM)hFont, 1);
	}

	BOOL Show(bool show = true)
	{
		return ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
	}

	void SetText(const std::string& text)
	{
		Static_SetText(hwnd, text.c_str());
	}

	[[nodiscard]] std::string GetText()
	{
		const int length = Static_GetTextLength(hwnd) + 1;
		const std::unique_ptr<char[]> uptr = std::make_unique<char[]>(length);
		if (uptr)
		{
			Static_GetText(hwnd, uptr.get(), length);
			return uptr.get();
		}
		return "";
	}

	void SetBitmap(HBITMAP hBitmap)
	{
		SendMessageA(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
	}

protected:
	HWND hwnd;
};

class GUI final : public BaseWindow<GUI>
{
public:
	// Master

	int SelectedIndex = -1;

	BasicListView ListViewPlugin;
	BasicStatic StaticDesc;
	BasicButton ButtonEnable;
	BasicButton ButtonReload;
	BasicButton ButtonMenu;

	std::map<std::string, HFONT> Fonts;
	LRESULT CreateMainPage();

	[[nodiscard]] PCSTR ClassName() const override { return "GUI"; }
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	GUI() = default;
};


LRESULT GUI::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		RECT rcClient; // The parent window's client area.
		GetClientRect(m_hwnd, &rcClient);

		// 添加字体/图片等
		Fonts["Yahei14"] = CreateFontA(14, 0, 0, 0, FW_DONTCARE, FALSE,
			FALSE, FALSE, GB2312_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "微软雅黑");
		Fonts["Yahei18"] = CreateFontA(18, 0, 0, 0, FW_DONTCARE, FALSE,
			FALSE, FALSE, GB2312_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "微软雅黑");
		Fonts["Yahei22"] = CreateFontA(22, 0, 0, 0, FW_DONTCARE, FALSE,
			FALSE, FALSE, GB2312_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "微软雅黑");

		SendMessage(m_hwnd, WM_SETFONT, (WPARAM)Fonts["Yahei14"], 1);
		CreateMainPage();

		return 0;
	}
	case WM_CLOSE:
		DestroyWindow(m_hwnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		for (const auto& font : Fonts)
		{
			DeleteObject(font.second);
		}
		Fonts.clear();
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(m_hwnd, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
		EndPaint(m_hwnd, &ps);
	}
	return 0;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		//SetTextColor(hdcStatic, RGB(255, 255, 255));
		SetBkMode(hdcStatic, TRANSPARENT);
		return (INT_PTR)static_cast<HBRUSH>(GetStockObject(COLOR_WINDOW + 1));
	}
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case ID_MASTER_BUTTONENABLE:
		{
			if (SelectedIndex == -1)
			{
				MessageBoxA(m_hwnd, "请先双击左侧列表选择一个插件!", "CQXQ", MB_OK);
				return 0;
			}
			if (plugins[SelectedIndex].enabled)
			{
				plugins[SelectedIndex].enabled = false;
				ButtonEnable.SetText("启用");
				if (!plugins[SelectedIndex].events.count(CQ_eventDisable)) return 0;
				const auto disable = IntMethod(plugins[SelectedIndex].events.at(CQ_eventDisable));
				if (disable)
				{
					disable();
				}
			}
			else
			{
				plugins[SelectedIndex].enabled = true;
				ButtonEnable.SetText("停用");
				if (!plugins[SelectedIndex].events.count(CQ_eventEnable)) return 0;
				const auto enable = IntMethod(plugins[SelectedIndex].events.at(CQ_eventEnable));
				if (enable)
				{
					enable();
				}
			}
		}
		return 0;
		case ID_MASTER_BUTTONMENU:
		{
			if (SelectedIndex == -1)
			{
				MessageBoxA(m_hwnd, "请先双击左侧列表选择一个插件!", "CQXQ", MB_OK);
				return 0;
			}
			if (plugins[SelectedIndex].menus.empty()) return 0;
			HMENU hMenu = CreatePopupMenu();
			POINT curpos;
			GetCursorPos(&curpos);
			int count = 1;
			for (const auto& menu : plugins[SelectedIndex].menus)
			{
				AppendMenuA(hMenu, MF_STRING, count, menu.first.c_str());
				count++;
			}
			BOOL ret = TrackPopupMenuEx(
				hMenu,
				TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_NOANIMATION,
				curpos.x,
				curpos.y,
				m_hwnd,
				nullptr
			);
			if (ret)
			{
				const auto m = IntMethod(plugins[SelectedIndex].menus[ret - 1].second);
				if (m)
				{
					m();
				}
			}
			DestroyMenu(hMenu);
		}
		return 0;
		case ID_MASTER_BUTTONRELOAD:
		{
			MessageBoxA(m_hwnd, "暂未实现，敬请期待", "CQXQ", MB_OK);
		}
		return 0;
		default:
			return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
		}
	}
	case WM_NOTIFY:
	{
		switch (reinterpret_cast<LPNMHDR>(lParam)->code)
		{
		case LVN_ITEMACTIVATE:
		{
			if (reinterpret_cast<LPNMHDR>(lParam)->idFrom == ID_MASTER_LVPLUGIN)
			{
				auto lpnmitem = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
				if (lpnmitem->iItem != -1)
				{
					std::string text = ListViewPlugin.GetItemText(lpnmitem->iItem);
					SelectedIndex = std::stoi(text);
					StaticDesc.SetText(plugins[SelectedIndex].description);
					if (plugins[SelectedIndex].enabled)
					{
						ButtonEnable.SetText("停用");
					}
					else
					{
						ButtonEnable.SetText("启用");
					}
				}
				return 0;
			}
		}
		return 0;
		default:
			return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
		}
	}
	default:
		return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
	}
}

LRESULT GUI::CreateMainPage()
{
	RECT rcClient;
	GetClientRect(m_hwnd, &rcClient);

	ButtonEnable.Create("启用", WS_CHILD | WS_VISIBLE, 0,
		400, 280, 70, 30, m_hwnd, reinterpret_cast<HMENU>(ID_MASTER_BUTTONENABLE));
	ButtonReload.Create("重载", WS_CHILD | WS_VISIBLE, 0,
		500, 280, 70, 30, m_hwnd, reinterpret_cast<HMENU>(ID_MASTER_BUTTONRELOAD));
	ButtonMenu.Create("菜单", WS_CHILD | WS_VISIBLE, 0,
		600, 280, 70, 30, m_hwnd, reinterpret_cast<HMENU>(ID_MASTER_BUTTONMENU));

	StaticDesc.Create("双击左侧列表选择一个插件",
		WS_CHILD | WS_VISIBLE, 0,
		400, 30, 300, 200, m_hwnd, reinterpret_cast<HMENU>(ID_MASTER_STATICDESC));

	ListViewPlugin.Create("",
		WS_CHILD | LVS_REPORT | WS_VISIBLE | WS_BORDER | LVS_SINGLESEL,
		0,
		12, 12,
		355, 426,
		m_hwnd,
		reinterpret_cast<HMENU>(ID_MASTER_LVPLUGIN));
	ListViewPlugin.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_AUTOSIZECOLUMNS | LVS_EX_TWOCLICKACTIVATE | LVS_EX_FULLROWSELECT);

	ListViewPlugin.AddAllTextColumn(std::vector<std::pair<std::string, int>>{ {"ID", 50}, { "名称", 300 }, { "作者", 200 }, { "版本", 200 }});
	int index = 0;
	for (const auto& item : plugins)
	{
		ListViewPlugin.AddTextRow({ std::to_string(item.id), item.name, item.author, item.version }, index);
		index++;
	}

	HFONT Yahei18 = Fonts["Yahei18"];
	ButtonEnable.SetFont(Yahei18);
	ButtonReload.SetFont(Yahei18);
	ButtonMenu.SetFont(Yahei18);
	StaticDesc.SetFont(Yahei18);
	return 0;
}


int WINAPI GUIMain()
{
	// hDllModule不应为空
	assert(hDllModule);

	// 初始化CommonControl
	INITCOMMONCONTROLSEX icex; // Structure for control initialization.
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	// 主GUI
	GUI MainWindow;

	if (!MainWindow.Create("CQXQ GUI", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_BORDER | WS_CLIPSIBLINGS, 0,
		CW_USEDEFAULT, CW_USEDEFAULT, 780, 500))
	{
		return 0;
	}

	ShowWindow(MainWindow.Window(), SW_SHOWDEFAULT);

	// Run the message loop.

	MSG msg = {};
	while (GetMessageA(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return 0;
}
