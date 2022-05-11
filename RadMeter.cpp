#include "Window.h"
#include "Windowxx.h"
//#include <tchar.h>

#include "Utils.h"

#include <vector>
#include <algorithm>
#include <numeric>
#include <PdhMsg.h>

#include "tinyexpr/tinyexpr.h"
#include "Format.h"
#include "resource.h"

template <class T> inline T K(T v) { return v * 1024; }
template <class T> inline T M(T v) { return K(v) * 1024; }
template <class T> inline T G(T v) { return M(v) * 1024; }


struct Message
{
    UINT        m_message;
    WPARAM      m_wParam;
    LPARAM      m_lParam;
};

class Widget : public Window
{
    friend WindowManager<Widget>;

public:
    static ATOM Register() { return WindowManager<Widget>::Register(); }

protected:
    static void GetCreateWindow(CREATESTRUCT& cs)
    {
        Window::GetCreateWindow(cs);
        cs.style = WS_POPUPWINDOW /*| WS_THICKFRAME*/;
        cs.dwExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW /*| WS_EX_LAYERED*/;
    }
    static void GetWndClass(WNDCLASS& wc)
    {
        Window::GetWndClass(wc);
        //wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
        wc.hbrBackground = CreateSolidBrush(RGB(24, 24, 24));
        wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_RADMETER));
    }

    void OnPaint()
    {
#if 1
        PAINTSTRUCT ps;
        BeginPaint(*this, &ps);
        OnDraw(&ps);
        EndPaint(*this, &ps);
#else
        RECT r;
        GetClientRect(*this, &r);
        // TODO Need to get size
        const int margin = 5;
        r.right = 100 + 2 * margin;
        r.bottom = /*m_hCounters.size()*/ 7 * (50 + margin) + margin;
        HDC hdcScreen = GetDC(NULL);
        HDC hdcBackBuffer = CreateCompatibleDC(hdcScreen);
        HBITMAP hbmBackBuffer = CreateCompatibleBitmap(hdcScreen, Width(r), Height(r));
        HGDIOBJ hbmOld = SelectObject(hdcBackBuffer, hbmBackBuffer);

        PAINTSTRUCT ps = {};
        //BeginPaint(*this, &ps);
        ps.hdc = hdcBackBuffer;
        OnDraw(&ps);
        //EndPaint(*this, &ps);

        // inform Windows that we have new graphics data available.
        POINT ptSrc;
        ptSrc.x = 0;
        ptSrc.y = 0;
        SIZE size;
        size.cx = Width(r);
        size.cy = Height(r);
        BLENDFUNCTION bf;
        bf.AlphaFormat = AC_SRC_ALPHA;
        bf.SourceConstantAlpha = 255;
        bf.BlendFlags = 0;
        bf.BlendOp = AC_SRC_OVER;
        UpdateLayeredWindow(*this, NULL, NULL, &size, hdcBackBuffer, &ptSrc, 0, &bf, ULW_ALPHA);

        SelectObject(hdcBackBuffer, hbmOld);
        DeleteDC(hdcBackBuffer);
#endif
    }
    void OnTimer(UINT id);
    void OnMouseMove(int x, int y, UINT keyFlags);
    void OnLButtonDown(int x, int y, UINT keyFlags);
    void OnRButtonDown(int x, int y, UINT keyFlags);
    void OnSysCommand(UINT cmd, int x, int y)
    {
        Window::HandleMessage(m_msg.m_message, m_msg.m_wParam, m_msg.m_lParam);
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        m_msg.m_message = uMsg;
        m_msg.m_wParam = wParam;
        m_msg.m_lParam = lParam;

        switch (uMsg)
        {
            //HANDLE_MSG(WM_PAINT, OnPaint);
        case (WM_PAINT):
            return HANDLEX_WM_PAINT((wParam), (lParam), (OnPaint));
            HANDLE_MSG(WM_TIMER, OnTimer);
            HANDLE_MSG(WM_MOUSEMOVE, OnMouseMove);
            HANDLE_MSG(WM_LBUTTONDOWN, OnLButtonDown);
            HANDLE_MSG(WM_RBUTTONDOWN, OnRButtonDown);
            HANDLE_MSG(WM_SYSCOMMAND, OnSysCommand);
            HANDLE_DEF(Window::HandleMessage);
        }
    }

    static LPCTSTR ClassName() { return TEXT("RadWidget"); }

private:
    Message m_msg;
};

void Widget::OnTimer(UINT id)
{
    //Window::OnTimer(id);
    if (id == 5421)
    {
        RECT r;
        GetWindowRect(*this, &r);

        POINT pt;
        GetCursorPos(&pt);

        if (!PtInRect(&r, pt))
        {
            ShowWindow(*this, SW_SHOW);
            KillTimer(*this, id);
        }
    }
}

void Widget::OnMouseMove(int x, int y, UINT keyFlags)
{
    if (!(GetKeyState(VK_CONTROL) & 0x8000))
    {
        ShowWindow(*this, SW_HIDE);
        SetTimer(*this, 5421, 1000, nullptr);
    }
}

void Widget::OnLButtonDown(int x, int y, UINT keyFlags)
{
    if (GetKeyState(VK_CONTROL) & 0x8000)
        SendMessage(*this, WM_SYSCOMMAND, SC_MOVE | 0x0002, 0);
}

void Widget::OnRButtonDown(int x, int y, UINT keyFlags)
{
    if (GetKeyState(VK_CONTROL) & 0x8000)
#if 1
    {
        HMENU hSysMenu = (HMENU) GetSystemMenu(*this, FALSE);
        if (hSysMenu != NULL)
        {
            LONG style = GetWindowLong(*this, GWL_STYLE);
            EnableMenuItem(hSysMenu, SC_RESTORE, MF_BYCOMMAND | ((IsIconic(*this) | IsZoomed(*this)) ? MF_ENABLED : MF_DISABLED));
            EnableMenuItem(hSysMenu, SC_MINIMIZE, MF_BYCOMMAND | ((style & WS_MINIMIZEBOX) && !IsIconic(*this) ? MF_ENABLED : MF_DISABLED));
            EnableMenuItem(hSysMenu, SC_MAXIMIZE, MF_BYCOMMAND | ((style & WS_MAXIMIZEBOX) && !IsZoomed(*this) ? MF_ENABLED : MF_DISABLED));
            EnableMenuItem(hSysMenu, SC_SIZE, MF_BYCOMMAND | (style & WS_THICKFRAME ? MF_ENABLED : MF_DISABLED));

            POINT pt{ x, y };
            ClientToScreen(*this, &pt);
            int flag = TrackPopupMenu(hSysMenu,
                TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                pt.x, pt.y,
                NULL, *this, 0);
            if (flag > 0)
                SendMessage(*this, WM_SYSCOMMAND, flag, 0);
        }
    }
#else
        SendMessage(*this, WM_SYSCOMMAND, SC_MOUSEMENU, 0);
#endif
}

struct Measure
{
    DWORD dwType;
    PDH_FMT_COUNTERVALUE Value;
};

struct Counter
{
    TCHAR szName[100];
    TCHAR szCounter[200];
    TCHAR szUnit[10];
    DWORD   dwFormat;
    COLORREF m_PenColor;
    COLORREF m_BrushColor;
    double MaxValue;
    bool AutoScale;
};

struct CounterInstance
{
    Counter style;
    PDH_HCOUNTER m_hCounter;
    std::vector<Measure> m_Data;
};

class RadMeter : public Widget
{
    friend WindowManager<RadMeter>;

public:
    static RadMeter* Create() { return WindowManager<RadMeter>::Create(); }

protected:
    static void GetCreateWindow(CREATESTRUCT& cs);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnTimer(UINT id);
    void OnDisplayChange(UINT bitsPerPixel, UINT cxScreen, UINT cyScreen);
    void OnSysCommand(UINT cmd, int x, int y);

    virtual void OnDraw(const PAINTSTRUCT* pps) const override;

    void DoPosition();

    HFONT m_hFont;
    LONG m_Margin;

    std::unique_ptr<PDH_HQUERY, QueryDeleter> m_hQuery;
    std::vector<CounterInstance> m_hCounters;
};

void RadMeter::GetCreateWindow(CREATESTRUCT& cs)
{
    Widget::GetCreateWindow(cs);
    cs.lpszName = TEXT("RadMeter");
}

BOOL RadMeter::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    try
    {
        std::unique_ptr<HKEY, KeyDeleter> hKey;
        CheckLog(RegOpenKey(HKEY_CURRENT_USER, _T("SOFTWARE\\RadSoft\\RadMeter"), ptr(hKey)), _T("RegOpenKey"));

        HDC hDC = GetDC(*this);
        LOGFONT lf = {};
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        RegGetSZ(hKey.get(), _T("FontFace"), lf.lfFaceName, ARRAYSIZE(lf.lfFaceName));
        lf.lfHeight = -MulDiv(RegGetDWORD(hKey.get(), _T("FontSize"), 9), GetDeviceCaps(hDC, LOGPIXELSY), 72);
        lf.lfWeight = RegGetDWORD(hKey.get(), _T("FontWeight"), FW_NORMAL);
        m_hFont = CreateFontIndirect(&lf);
        ReleaseDC(*this, hDC);
        hDC = NULL;

        m_Margin = RegGetDWORD(hKey.get(), _T("Margin"), 5);

        CheckPdhThrow(PdhOpenQuery(nullptr, 0, ptr(m_hQuery)));

        std::unique_ptr<HKEY, KeyDeleter> hKeyMeter;
        CheckLog(RegOpenKey(hKey.get(), _T("Meters"), ptr(hKeyMeter)), _T("RegOpenKey"));

        TCHAR Order[1024] = _T("CPU\0");
        RegGetMULTISZ(hKeyMeter.get(), _T("Order"), Order, ARRAYSIZE(Order));

        for (TCHAR* lpMeter = Order; *lpMeter != _T('\0'); lpMeter += _tcslen(lpMeter) + 1)
        {
            OutputDebugString(lpMeter);
            std::unique_ptr<HKEY, KeyDeleter> hKeyCurrentMeter;
            CheckLog(RegOpenKey(hKeyMeter.get(), lpMeter, ptr(hKeyCurrentMeter)), _T("RegOpenKey"));

            Counter counter = { _T(""), _T("\\Processor(_Total)\\% Processor Time"), _T(""), PDH_FMT_DOUBLE, RGB(17, 125, 187), {}, 100 };

            _tcscpy_s(counter.szName, lpMeter);
            RegGetSZ(hKeyCurrentMeter.get(), _T("Name"), counter.szName, ARRAYSIZE(counter.szName));
            RegGetSZ(hKeyCurrentMeter.get(), _T("Counter"), counter.szCounter, ARRAYSIZE(counter.szCounter));
            RegGetSZ(hKeyCurrentMeter.get(), _T("Unit"), counter.szUnit, ARRAYSIZE(counter.szUnit));
            TCHAR Format[100] = _T("double");
            RegGetSZ(hKeyCurrentMeter.get(), _T("Format"), Format, ARRAYSIZE(Format));
            if (_tcsicmp(Format, _T("long")) == 0)          counter.dwFormat = PDH_FMT_LONG;
            else if (_tcsicmp(Format, _T("double")) == 0)   counter.dwFormat = PDH_FMT_DOUBLE;
            else                                            counter.dwFormat = PDH_FMT_DOUBLE;
            counter.m_PenColor = RegGetDWORD(hKeyCurrentMeter.get(), _T("Color"), counter.m_PenColor);
            counter.m_BrushColor = RegGetDWORD(hKeyCurrentMeter.get(), _T("Color2"), counter.m_BrushColor);
            counter.MaxValue = static_cast<double>(RegGetQWORD(hKeyCurrentMeter.get(), _T("Max"), static_cast<QWORD>(counter.MaxValue)));
            counter.AutoScale = RegGetDWORD(hKeyCurrentMeter.get(), _T("AutoScale"), counter.AutoScale);

            if (counter.m_BrushColor == COLORREF{})
                counter.m_BrushColor = MakeDarker(counter.m_PenColor, 0.5f);

            PDH_HCOUNTER hCounter = NULL;
            CheckPdhThrow(PdhAddEnglishCounter(m_hQuery.get(), counter.szCounter, 0, &hCounter));

            m_hCounters.push_back(CounterInstance({ counter, hCounter }));
        }

#if 0
        const Counter counters[] = {
            { _T("CPU"),       _T("\\Processor(_Total)\\% Processor Time"),        _T(""),  PDH_FMT_DOUBLE, RGB(17, 125, 187) },
            { _T("Processes"), _T("\\System\\Processes"),                          _T(""),  PDH_FMT_LONG,   MakeLighter(RGB(17, 125, 187), 0.3f), {}, 500 },
            { _T("Memory"),    _T("\\Memory\\Available Bytes"),                    _T("B"), PDH_FMT_DOUBLE, RGB(139, 18, 174),  {}, G(16.0) },
            { _T("Disk"),      _T("\\PhysicalDisk(_Total)\\% Disk Time"),          _T(""),  PDH_FMT_DOUBLE, RGB(77, 166, 12) },
            { _T("Net D"),     _T("\\Network Interface(*)\\Bytes Received/sec"),   _T("B"), PDH_FMT_DOUBLE, RGB(167, 79, 1),    {}, 2000, true },
            { _T("Net U"),     _T("\\Network Interface(*)\\Bytes Sent/sec"),       _T("B"), PDH_FMT_DOUBLE, RGB(237, 165, 130), {}, 2000, true },
            { _T("GPU"),       _T("\\GPU Engine(*)\\Utilization Percentage"),      _T(""),  PDH_FMT_DOUBLE, RGB(172, 57, 49) },
        };
        for (Counter c : counters)
        {
            if (c.m_BrushColor == COLORREF{})
                c.m_BrushColor = MakeDarker(c.m_PenColor, 0.5f);
            if (c.MaxValue == 0)
                c.MaxValue = 100;

            PDH_HCOUNTER hCounter = NULL;
            CheckPdhThrow(PdhAddEnglishCounter(m_hQuery.get(), c.szCounter, 0, &hCounter));

            m_hCounters.push_back(CounterInstance({ c, hCounter }));
        }
#endif

        CheckPdhLog(PdhCollectQueryData(m_hQuery.get()), _T("PdhCollectQueryData"));

        DoPosition();

        SetTimer(*this, 1, 1000, nullptr);
    }
    catch (const WinError& e)
    {
        MessageBox(*this, e.getMessage().c_str(), TEXT("RadMeter"), MB_ICONERROR | MB_OK);
    }

    return TRUE;
}

void RadMeter::OnDestroy()
{
    PostQuitMessage(0);
}

void RadMeter::OnTimer(UINT id)
{
    Widget::OnTimer(id);

    if (id == 1)
    {
        CheckPdhLog(PdhCollectQueryData(m_hQuery.get()), _T("PdhCollectQueryData"));

        for (CounterInstance& counter : m_hCounters)
        {
            Measure m = {};
            CheckPdhLog(PdhGetFormattedCounterValue(counter.m_hCounter, counter.style.dwFormat, &m.dwType, &m.Value), counter.style.szCounter);

            DWORD dwBufferSize = 0;
            DWORD dwItemCount = 0;
            CheckPdhLog(Ignore(PdhGetFormattedCounterArray(counter.m_hCounter, counter.style.dwFormat, &dwBufferSize, &dwItemCount, nullptr), { PDH_STATUS(PDH_MORE_DATA) }), counter.style.szCounter);
            //assert(dwItemCount * sizeof(PDH_FMT_COUNTERVALUE) == dwBufferSize);

            if (dwItemCount > 1)
            {
                std::vector<BYTE> values(dwBufferSize);
                PPDH_FMT_COUNTERVALUE_ITEM data = reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM>(values.data());
                PDH_STATUS status = PdhGetFormattedCounterArray(counter.m_hCounter, counter.style.dwFormat, &dwBufferSize, &dwItemCount, data);

                if (status != PDH_CSTATUS_INVALID_DATA)
                    switch (counter.style.dwFormat)
                    {
                    case PDH_FMT_DOUBLE: m.Value.doubleValue = std::accumulate(data, data + dwItemCount, 0.0, [](double v, const PDH_FMT_COUNTERVALUE_ITEM& item) { return v + item.FmtValue.doubleValue; }); break;
                    case PDH_FMT_LONG:   m.Value.longValue = std::accumulate(data, data + dwItemCount, LONG(0), [](LONG v, const PDH_FMT_COUNTERVALUE_ITEM& item) { return v + item.FmtValue.longValue; }); break;
                    }
            }
            counter.m_Data.push_back(m);
            if (counter.m_Data.size() > 1000)
                counter.m_Data.erase(counter.m_Data.begin(), counter.m_Data.end() - 1000);

            if (counter.style.AutoScale && !counter.m_Data.empty())
            {
                std::vector<Measure>::const_iterator max = counter.m_Data.cend();
                switch (counter.style.dwFormat)
                {
                case PDH_FMT_DOUBLE:
                    max = std::max_element(counter.m_Data.cbegin(), counter.m_Data.cend(), [](const Measure& c1, const Measure& c2) { return c1.Value.doubleValue < c2.Value.doubleValue; });
                    if (max->Value.doubleValue > 0)
                        counter.style.MaxValue = ceil_to_n_digits(max->Value.doubleValue, 2);
                    break;
                case PDH_FMT_LONG:
                    max = std::max_element(counter.m_Data.cbegin(), counter.m_Data.cend(), [](const Measure& c1, const Measure& c2) { return c1.Value.longValue < c2.Value.longValue; });
                    if (max->Value.longValue > 0)
                        counter.style.MaxValue = ceil_to_n_digits(max->Value.longValue, 2);
                    break;
                }
            }
        }

        InvalidateRect(*this, nullptr, TRUE);
    }
}

void RadMeter::OnDisplayChange(UINT bitsPerPixel, UINT cxScreen, UINT cyScreen)
{
    DoPosition();
}

void RadMeter::OnDraw(const PAINTSTRUCT* pps) const
{
    HDC hDC = pps->hdc;

    HPEN hOldPen = (HPEN) SelectObject(hDC, GetStockObject(DC_PEN));
    HBRUSH hOldBrush = (HBRUSH) SelectObject(hDC, GetStockObject(DC_BRUSH));

    HFONT hOldFont = (HFONT) SelectObject(hDC, m_hFont);
    SetBkMode(hDC, TRANSPARENT);
    SetTextColor(hDC, RGB(255, 255, 255));

    RECT client;
    GetClientRect(*this, &client);
    InflateRect(&client, -m_Margin, -m_Margin);

    const LONG width = Width(client);
    const LONG height = (Height(client) + m_Margin) / static_cast<LONG>(m_hCounters.size());

    RECT rchart(client);
    rchart.bottom = rchart.top + height - m_Margin;
    for (const CounterInstance& counter : m_hCounters)
    {
        SetDCPenColor(hDC, counter.style.m_PenColor);
        SelectObject(hDC, GetStockObject(NULL_BRUSH));
        Rectangle(hDC, rchart.left, rchart.top, rchart.right, rchart.bottom);

        SelectObject(hDC, GetStockObject(DC_BRUSH));
        SetDCBrushColor(hDC, counter.style.m_BrushColor);

        std::vector<POINT> points;

        const int length = std::min<int>(width - 1, static_cast<int>(counter.m_Data.size()));
        LONG x = rchart.right - length - 1;
        //if (length < width)
        if (x > rchart.left)
            points.push_back(POINT({ x, rchart.bottom }));
        for (size_t i = counter.m_Data.size() - length; i < counter.m_Data.size(); ++i)
        {
            ++x;
            const Measure& m = counter.m_Data[i];
            switch (counter.style.dwFormat)
            {
            case PDH_FMT_DOUBLE:
            {
                const LONG y = lerp(rchart.bottom, rchart.top, std::min(static_cast<float>(m.Value.doubleValue / counter.style.MaxValue), 1.0f));
                points.push_back(POINT({ x, y }));
            }
            break;
            case PDH_FMT_LONG:
            {
                const LONG y = lerp(rchart.bottom, rchart.top, std::min(static_cast<float>(static_cast<double>(m.Value.longValue) / counter.style.MaxValue), 1.0f));
                points.push_back(POINT({ x, y }));
            }
            break;
            }
        }
        points.push_back(POINT({ x, rchart.bottom }));
        points.push_back(POINT({ rchart.left, rchart.bottom }));

        Polygon(hDC, points.data(), static_cast<int>(points.size()));

        InflateRect(&rchart, -1, 0);

        std::wstring text;
        text = Format(_T("%s\n"), counter.style.szName);
        if (!counter.m_Data.empty())
        {
            const Measure& m = counter.m_Data.back();
            switch (counter.style.dwFormat)
            {
            case PDH_FMT_DOUBLE:
            {
                TCHAR prefix = _T(' ');
                //double mag = log10(m.Value.doubleValue);
                double value = m.Value.doubleValue;
                if (value > (1024 * 90 / 100))
                {
                    const TCHAR PrefixSet[] = _T(" KMGTPE");
                    const TCHAR* CurrentPrefix = PrefixSet;
                    while (value > (1024 * 90 / 100))
                    {
                        value /= 1024;
                        ++CurrentPrefix;
                    }
                    prefix = *CurrentPrefix;
                }
                switch (m.dwType & 0xF0000000)
                {
                case PERF_DISPLAY_PER_SEC:  text += Format(_T("%.1f %c%s/s"), value, prefix, counter.style.szUnit); break;
                case PERF_DISPLAY_PERCENT:  text += Format(_T("%.0f%%"), m.Value.doubleValue); break;
                case PERF_DISPLAY_SECONDS:  text += Format(_T("%.1f %c%s s"), value, prefix, counter.style.szUnit); break;
                default:                    text += Format(_T("%.1f %c%s"), value, prefix, counter.style.szUnit); break;
                }
            }
            break;
            case PDH_FMT_LONG:
            {
                switch (m.dwType & 0xF0000000)
                {
                case PERF_DISPLAY_PER_SEC:  text += Format(_T("%d %s/s"), m.Value.longValue, counter.style.szUnit); break;
                case PERF_DISPLAY_PERCENT:  text += Format(_T("%d%%"), m.Value.longValue); break;
                case PERF_DISPLAY_SECONDS:  text += Format(_T("%d %s s"), m.Value.longValue, counter.style.szUnit); break;
                default:                    text += Format(_T("%d %s"), m.Value.longValue, counter.style.szUnit); break;
                }
            }
            break;
            }
        }
        DrawText(hDC, text.c_str(), static_cast<int>(text.length()), &rchart, DT_TOP | DT_LEFT);

        InflateRect(&rchart, 1, 0);

        OffsetRect(&rchart, 0, height);
    }

    SelectObject(hDC, hOldPen);
    SelectObject(hDC, hOldBrush);
    SelectObject(hDC, hOldFont);
}

LRESULT RadMeter::HandleMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(WM_CREATE, OnCreate);
        HANDLE_MSG(WM_DESTROY, OnDestroy);
        //HANDLE_MSG(WM_SIZE, OnSize);
        HANDLE_MSG(WM_TIMER, OnTimer);
        HANDLE_MSG(WM_DISPLAYCHANGE, OnDisplayChange);
        HANDLE_DEF(Widget::HandleMessage);
    }
}

void RadMeter::DoPosition()
{
    std::unique_ptr<HKEY, KeyDeleter> hKey;
    CheckLog(RegOpenKey(HKEY_CURRENT_USER, _T("SOFTWARE\\RadSoft\\RadMeter"), ptr(hKey)), _T("RegOpenKey"));

    std::unique_ptr<HKEY, KeyDeleter> hKeyMeter;
    CheckLog(RegOpenKey(hKey.get(), _T("Meters"), ptr(hKeyMeter)), _T("RegOpenKey"));

    const LONG style = GetWindowLong(*this, GWL_STYLE);
    const LONG exstyle = GetWindowLong(*this, GWL_EXSTYLE);

    const LONG width = RegGetDWORD(hKeyMeter.get(), _T("Width"), 100);
    const LONG height = RegGetDWORD(hKeyMeter.get(), _T("Height"), 50);

    RECT r{ 0, 0, width + 2 * m_Margin, static_cast<LONG>(m_hCounters.size()) * (height + m_Margin) + m_Margin };
    AdjustWindowRectEx(&r, style, FALSE, exstyle);
    //OffsetRect(&r, GetSystemMetrics(SM_CXSCREEN) - r.right, GetSystemMetrics(SM_CYSCREEN)/2 - r.top);
    {
        double dwidth = Width(r);
        double dheight = Height(r);
        double dCXSCREEN = GetSystemMetrics(SM_CXSCREEN);
        double dCYSCREEN = GetSystemMetrics(SM_CYSCREEN);

        te_variable vars[] = {
            { "width", &dwidth },
            { "height", &dheight },
            { "cxscreen", &dCXSCREEN },
            { "cyscreen", &dCYSCREEN},
        };

        char exprx[100] = "0";
        char expry[100] = "0";
        RegGetSZ(hKey.get(), "x", exprx, ARRAYSIZE(exprx));
        RegGetSZ(hKey.get(), "y", expry, ARRAYSIZE(expry));

        int errorx = 0;
        int errory = 0;
        OffsetRect(&r,
            lround(te_interp_var(exprx, vars, ARRAYSIZE(vars), &errorx)) - r.left,
            lround(te_interp_var(expry, vars, ARRAYSIZE(vars), &errorx)) - r.top);
        if (errorx)
            MessageBox(*this, Format(_T("Error in x expression: %d"), errorx).c_str(), TEXT("RadMeter"), MB_ICONERROR | MB_OK);
        if (errory)
            MessageBox(*this, Format(_T("Error in y expression: %d"), errory).c_str(), TEXT("RadMeter"), MB_ICONERROR | MB_OK);
    }

    SetWindowPos(*this, NULL, r.left, r.top, Width(r), Height(r), SWP_NOOWNERZORDER);
}

bool Run(_In_ const LPCTSTR lpCmdLine, _In_ const int nShowCmd)
{
    if (RadMeter::Register() == 0)
    {
        MessageBox(NULL, TEXT("Error registering window class"), TEXT("RadMeter"), MB_ICONERROR | MB_OK);
        return false;
    }
    RadMeter* prw = RadMeter::Create();
    if (prw == nullptr)
    {
        MessageBox(NULL, TEXT("Error creating root window"), TEXT("RadMeter"), MB_ICONERROR | MB_OK);
        return false;
    }

    ShowWindow(*prw, nShowCmd);
    return true;
}
