#include "Rad/Window.h"
#include "Rad/Windowxx.h"
#include "Rad/Log.h"

#include "Utils.h"

#include <tchar.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <stack>
#include <PdhMsg.h>

#include "tinyexpr/tinyexpr.h"
#include "Rad/Format.h"
#include "Rad/MemoryPlus.h"
#include "resource.h"

#define AUTO_VAR(NAME, EXPR) decltype(EXPR) NAME { EXPR }

double te_interp_var(const char* expression, const te_variable* variables, int var_count, int* error) {
    te_expr* n = te_compile(expression, variables, var_count, error);
    double ret;
    if (n) {
        ret = te_eval(n);
        te_free(n);
    }
    else {
        ret = NAN;
    }
    return ret;
}

const UINT WM_SHELLHOOKMESSAGE = RegisterWindowMessage(TEXT("SHELLHOOK"));

enum TIMER
{
    TIMER_UPDATE,
    TIMER_HIDE,
};

inline void ChangeWindowLong(HWND hWnd, int nIndex, LONG add, LONG remove)
{
    LONG exstyle = GetWindowLong(hWnd, nIndex);
    exstyle |= add;
    exstyle &= ~remove;
    SetWindowLong(hWnd, nIndex, exstyle);
}

template <class T> inline T K(T v) { return v * 1024; }
template <class T> inline T M(T v) { return K(v) * 1024; }
template <class T> inline T G(T v) { return M(v) * 1024; }

class Widget : public Window
{
protected:
    struct Class
    {
        static LPCTSTR ClassName() { return TEXT("RadWidget"); }
        static void GetWndClass(WNDCLASS& wc)
        {
            //MainClass::GetWndClass(wc);
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            //wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
            wc.hbrBackground = CreateSolidBrush(RGB(24, 24, 24));
            wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_RADMETER));
        }
        static void GetCreateWindow(CREATESTRUCT& cs)
        {
            //MainClass::GetCreateWindow(cs);
            cs.style = WS_POPUPWINDOW /*| WS_THICKFRAME*/;
            cs.dwExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
        }
    };

public:
    static ATOM Register() { return ::Register<Class>(); }

protected:
    void OnMouseMove(int x, int y, UINT keyFlags);
    void OnLButtonDown(int x, int y, UINT keyFlags);
    void OnRButtonDown(int x, int y, UINT keyFlags);
    void OnTimer(UINT id);

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_MOUSEMOVE, OnMouseMove);
            HANDLE_MSG(WM_LBUTTONDOWN, OnLButtonDown);
            HANDLE_MSG(WM_RBUTTONDOWN, OnRButtonDown);
            HANDLE_MSG(WM_TIMER, OnTimer);
        }

        if (!IsHandled())
            ret = Window::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

private:
    bool m_bHidden = false;
};

void Widget::OnMouseMove(int x, int y, UINT keyFlags)
{
    if (!m_bHidden && !(GetKeyState(VK_CONTROL) & 0x8000))
    {
        m_bHidden = true;
        AnimateWindow(*this, 300, AW_HIDE | AW_HOR_POSITIVE | AW_BLEND);
        SetTimer(*this, TIMER_HIDE, 500, nullptr);
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

void Widget::OnTimer(UINT id)
{
    switch (id)
    {
    case TIMER_HIDE:
    {
        RECT r;
        GetWindowRect(*this, &r);

        POINT pt;
        GetCursorPos(&pt);

        if (!PtInRect(&r, pt))
        {
            m_bHidden = false;
            AnimateWindow(*this, 300, AW_HOR_NEGATIVE | AW_BLEND);
            KillTimer(*this, TIMER_HIDE);
        }
    }
    break;

    default:
        SetHandled(false);
        break;
    }
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
    static RadMeter* Create() { return WindowManager<RadMeter>::Create(NULL, TEXT("RadMeter")); }

protected:
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnTimer(UINT id);
    void OnDisplayChange(UINT bitsPerPixel, UINT cxScreen, UINT cyScreen);

    virtual void OnDraw(const PAINTSTRUCT* pps) const override;

    void DoPosition();

    int m_nFullScreenCount = 0;

    HFONT m_hFont;
    LONG m_Margin;

    AUTO_VAR(m_hQuery, MakeUniqueHandle<PDH_HQUERY>(NULL, PdhCloseQuery));
    std::vector<CounterInstance> m_hCounters;
};

BOOL RadMeter::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    try
    {
        auto hKey = MakeUniqueHandle<HKEY>(NULL, RegCloseKey);
        CHECK_HR(RegOpenKey(HKEY_CURRENT_USER, TEXT("SOFTWARE\\RadSoft\\RadMeter"), out_ptr(hKey)));

        CHECK_LE(RegisterShellHookWindow(*this));

        HDC hDC = GetDC(*this);
        LOGFONT lf = {};
        _tcscpy_s(lf.lfFaceName, TEXT("Segoe UI"));
        RegGetSZ(hKey.get(), TEXT("FontFace"), lf.lfFaceName, ARRAYSIZE(lf.lfFaceName));
        lf.lfHeight = -MulDiv(RegGetDWORD(hKey.get(), TEXT("FontSize"), 9), GetDeviceCaps(hDC, LOGPIXELSY), 72);
        lf.lfWeight = RegGetDWORD(hKey.get(), TEXT("FontWeight"), FW_NORMAL);
        m_hFont = CreateFontIndirect(&lf);
        ReleaseDC(*this, hDC);
        hDC = NULL;

        m_Margin = RegGetDWORD(hKey.get(), TEXT("Margin"), 5);

        CHECK_HR_PDH_THROW(PdhOpenQuery(nullptr, 0, out_ptr(m_hQuery)));

        auto hKeyMeter = MakeUniqueHandle<HKEY>(NULL, RegCloseKey);
        CHECK_HR(RegOpenKey(hKey.get(), TEXT("Meters"), out_ptr(hKeyMeter)));

        TCHAR Order[1024] = TEXT("CPU\0");
        RegGetMULTISZ(hKeyMeter.get(), TEXT("Order"), Order, ARRAYSIZE(Order));

        for (TCHAR* lpMeter = Order; *lpMeter != TEXT('\0'); lpMeter += _tcslen(lpMeter) + 1)
        {
            OutputDebugString(lpMeter);
            auto hKeyCurrentMeter = MakeUniqueHandle<HKEY>(NULL, RegCloseKey);
            CHECK_HR(RegOpenKey(hKeyMeter.get(), lpMeter, out_ptr(hKeyCurrentMeter)));

            Counter counter = { TEXT(""), TEXT("\\Processor(_Total)\\% Processor Time"), TEXT(""), PDH_FMT_DOUBLE, RGB(17, 125, 187), {}, 100 };

            _tcscpy_s(counter.szName, lpMeter);
            RegGetSZ(hKeyCurrentMeter.get(), TEXT("Name"), counter.szName, ARRAYSIZE(counter.szName));
            RegGetSZ(hKeyCurrentMeter.get(), TEXT("Counter"), counter.szCounter, ARRAYSIZE(counter.szCounter));
            RegGetSZ(hKeyCurrentMeter.get(), TEXT("Unit"), counter.szUnit, ARRAYSIZE(counter.szUnit));
            TCHAR Format[100] = TEXT("double");
            RegGetSZ(hKeyCurrentMeter.get(), TEXT("Format"), Format, ARRAYSIZE(Format));
            if (_tcsicmp(Format, TEXT("long")) == 0)          counter.dwFormat = PDH_FMT_LONG;
            else if (_tcsicmp(Format, TEXT("double")) == 0)   counter.dwFormat = PDH_FMT_DOUBLE;
            else                                            counter.dwFormat = PDH_FMT_DOUBLE;
            counter.m_PenColor = RegGetDWORD(hKeyCurrentMeter.get(), TEXT("Color"), counter.m_PenColor);
            counter.m_BrushColor = RegGetDWORD(hKeyCurrentMeter.get(), TEXT("Color2"), counter.m_BrushColor);
            counter.MaxValue = static_cast<double>(RegGetQWORD(hKeyCurrentMeter.get(), TEXT("Max"), static_cast<QWORD>(counter.MaxValue)));
            counter.AutoScale = RegGetDWORD(hKeyCurrentMeter.get(), TEXT("AutoScale"), counter.AutoScale);

            if (counter.m_BrushColor == COLORREF{})
                counter.m_BrushColor = MakeDarker(counter.m_PenColor, 0.5f);

            PDH_HCOUNTER hCounter = NULL;
            CHECK_HR_PDH_THROW(PdhAddEnglishCounter(m_hQuery.get(), counter.szCounter, 0, &hCounter));

            m_hCounters.push_back(CounterInstance({ counter, hCounter }));
        }

        if (m_hCounters.empty())
        {
            const Counter counters[] = {
                { TEXT("CPU"),       TEXT("\\Processor(_Total)\\% Processor Time"),        TEXT(""),  PDH_FMT_DOUBLE, RGB(17, 125, 187) },
                { TEXT("Processes"), TEXT("\\System\\Processes"),                          TEXT(""),  PDH_FMT_LONG,   MakeLighter(RGB(17, 125, 187), 0.3f), {}, 500 },
                { TEXT("Memory"),    TEXT("\\Memory\\Available Bytes"),                    TEXT("B"), PDH_FMT_DOUBLE, RGB(139, 18, 174),  {}, G(16.0) },
                { TEXT("Disk"),      TEXT("\\PhysicalDisk(_Total)\\% Disk Time"),          TEXT(""),  PDH_FMT_DOUBLE, RGB(77, 166, 12) },
                { TEXT("Net D"),     TEXT("\\Network Interface(*)\\Bytes Received/sec"),   TEXT("B"), PDH_FMT_DOUBLE, RGB(167, 79, 1),    {}, 2000, true },
                { TEXT("Net U"),     TEXT("\\Network Interface(*)\\Bytes Sent/sec"),       TEXT("B"), PDH_FMT_DOUBLE, RGB(237, 165, 130), {}, 2000, true },
                { TEXT("GPU"),       TEXT("\\GPU Engine(*)\\Utilization Percentage"),      TEXT(""),  PDH_FMT_DOUBLE, RGB(172, 57, 49) },
            };
            for (Counter c : counters)
            {
                if (c.m_BrushColor == COLORREF{})
                    c.m_BrushColor = MakeDarker(c.m_PenColor, 0.5f);
                if (c.MaxValue == 0)
                    c.MaxValue = 100;

                PDH_HCOUNTER hCounter = NULL;
                CHECK_HR_PDH_THROW(PdhAddEnglishCounter(m_hQuery.get(), c.szCounter, 0, &hCounter));

                m_hCounters.push_back(CounterInstance({ c, hCounter }));
            }
        }

        CHECK_HR_PDH(PdhCollectQueryData(m_hQuery.get()));

        DoPosition();

        SetTimer(*this, TIMER_UPDATE, 1000, nullptr);
    }
    catch (const WinError& e)
    {
        MessageBox(*this, e.getMessage().c_str(), TEXT("RadMeter"), MB_ICONERROR | MB_OK);
    }

    return TRUE;
}

void RadMeter::OnDestroy()
{
    CHECK_LE(DeregisterShellHookWindow(*this));
    DeleteFont(m_hFont);
    m_hFont = NULL;
    PostQuitMessage(0);
}

void RadMeter::OnTimer(UINT id)
{
    switch (id)
    {
    case TIMER_UPDATE:
    {
        CHECK_HR_PDH(PdhCollectQueryData(m_hQuery.get()));

        for (CounterInstance& counter : m_hCounters)
        {
            Measure m = {};
            CHECK_HR_MSG_PDH(Ignore(PdhGetFormattedCounterValue(counter.m_hCounter, counter.style.dwFormat, &m.dwType, &m.Value), { PDH_STATUS(PDH_CALC_NEGATIVE_VALUE) }), counter.style.szCounter);

            DWORD dwBufferSize = 0;
            DWORD dwItemCount = 0;
            CHECK_HR_MSG_PDH(Ignore(PdhGetFormattedCounterArray(counter.m_hCounter, counter.style.dwFormat, &dwBufferSize, &dwItemCount, nullptr), { PDH_STATUS(PDH_MORE_DATA) }), counter.style.szCounter);
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
    break;

    default:
        SetHandled(false);
        break;
    }
}

void RadMeter::OnDisplayChange(UINT bitsPerPixel, UINT cxScreen, UINT cyScreen)
{
    DoPosition();
}

void RadMeter::OnDraw(const PAINTSTRUCT* pps) const
{
    HDC hDC = pps->hdc;

    HPEN hOldPen = SelectPen(hDC, GetStockPen(DC_PEN));
    HBRUSH hOldBrush = SelectBrush(hDC, GetStockBrush(DC_BRUSH));

    HFONT hOldFont = SelectFont(hDC, m_hFont);
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
        SelectBrush(hDC, GetStockBrush(NULL_BRUSH));
        Rectangle(hDC, rchart.left, rchart.top, rchart.right, rchart.bottom);

        SelectBrush(hDC, GetStockBrush(DC_BRUSH));
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
        text = Format(TEXT("%s\n"), counter.style.szName);
        if (!counter.m_Data.empty())
        {
            const Measure& m = counter.m_Data.back();
            switch (counter.style.dwFormat)
            {
            case PDH_FMT_DOUBLE:
            {
                TCHAR prefix = TEXT(' ');
                //double mag = log10(m.Value.doubleValue);
                double value = m.Value.doubleValue;
                if (value > (1024 * 90 / 100))
                {
                    const TCHAR PrefixSet[] = TEXT(" KMGTPE");
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
                case PERF_DISPLAY_PER_SEC:  text += Format(TEXT("%.1f %c%s/s"), value, prefix, counter.style.szUnit); break;
                case PERF_DISPLAY_PERCENT:  text += Format(TEXT("%.0f%%"), m.Value.doubleValue); break;
                case PERF_DISPLAY_SECONDS:  text += Format(TEXT("%.1f %c%s s"), value, prefix, counter.style.szUnit); break;
                default:                    text += Format(TEXT("%.1f %c%s"), value, prefix, counter.style.szUnit); break;
                }
            }
            break;
            case PDH_FMT_LONG:
            {
                switch (m.dwType & 0xF0000000)
                {
                case PERF_DISPLAY_PER_SEC:  text += Format(TEXT("%d %s/s"), m.Value.longValue, counter.style.szUnit); break;
                case PERF_DISPLAY_PERCENT:  text += Format(TEXT("%d%%"), m.Value.longValue); break;
                case PERF_DISPLAY_SECONDS:  text += Format(TEXT("%d %s s"), m.Value.longValue, counter.style.szUnit); break;
                default:                    text += Format(TEXT("%d %s"), m.Value.longValue, counter.style.szUnit); break;
                }
            }
            break;
            }
        }
        DrawText(hDC, text.c_str(), static_cast<int>(text.length()), &rchart, DT_TOP | DT_LEFT);

        InflateRect(&rchart, 1, 0);

        OffsetRect(&rchart, 0, height);
    }

    SelectPen(hDC, hOldPen);
    SelectBrush(hDC, hOldBrush);
    SelectFont(hDC, hOldFont);
}

LRESULT RadMeter::HandleMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    LRESULT ret = 0;
    switch (uMsg)
    {
        HANDLE_MSG(WM_CREATE, OnCreate);
        HANDLE_MSG(WM_DESTROY, OnDestroy);
        //HANDLE_MSG(WM_SIZE, OnSize);
        HANDLE_MSG(WM_TIMER, OnTimer);
        HANDLE_MSG(WM_DISPLAYCHANGE, OnDisplayChange);
    }

    if (uMsg == WM_SHELLHOOKMESSAGE)
    {
        switch (wParam)
        {
        case 53:    // Undocumented on fullscreen enter
            ++m_nFullScreenCount;
            if (m_nFullScreenCount > 0)
                CHECK_LE(SetWindowPos(*this, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
            break;
        case 54:    // Undocumented on fullscreen exit
            --m_nFullScreenCount;
            if (m_nFullScreenCount <= 0)
                CHECK_LE(SetWindowPos(*this, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
            break;
        }
    }

    if (!IsHandled())
        ret = Widget::HandleMessage(uMsg, wParam, lParam);

    return ret;
}

void RadMeter::DoPosition()
{
    auto hKey = MakeUniqueHandle<HKEY>(NULL, RegCloseKey);
    CHECK_HR(RegOpenKey(HKEY_CURRENT_USER, TEXT("SOFTWARE\\RadSoft\\RadMeter"), out_ptr(hKey)));

    auto hKeyMeter = MakeUniqueHandle<HKEY>(NULL, RegCloseKey);
    CHECK_HR(RegOpenKey(hKey.get(), TEXT("Meters"), out_ptr(hKeyMeter)));

    const LONG style = GetWindowLong(*this, GWL_STYLE);
    const LONG exstyle = GetWindowLong(*this, GWL_EXSTYLE);

    const LONG width = RegGetDWORD(hKeyMeter.get(), TEXT("Width"), 100);
    const LONG height = RegGetDWORD(hKeyMeter.get(), TEXT("Height"), 50);

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
            MessageBox(*this, Format(TEXT("Error in x expression: %d"), errorx).c_str(), TEXT("RadMeter"), MB_ICONERROR | MB_OK);
        if (errory)
            MessageBox(*this, Format(TEXT("Error in y expression: %d"), errory).c_str(), TEXT("RadMeter"), MB_ICONERROR | MB_OK);
    }

    SetWindowPos(*this, NULL, r.left, r.top, Width(r), Height(r), SWP_NOOWNERZORDER);
}

bool Run(_In_ const LPCTSTR lpCmdLine, _In_ const int nShowCmd)
{
    RadLogInitWnd(NULL, "RadMeter", L"RadMeter");

    CHECK_LE_RET(Widget::Register(), false);

    RadMeter* prw = RadMeter::Create();
    CHECK_LE_RET(prw != nullptr, false);

    RadLogInitWnd(*prw, nullptr, nullptr);
    ShowWindow(*prw, nShowCmd);
    return true;
}
