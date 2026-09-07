#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define APP_CLASS L"CatalystLongevityNative"
#define CHART_CLASS L"CatalystLongevityChart"
#define APP_TITLE L"Catalyst Longevity Research"
#define ID_OPEN 1001
#define ID_DEMO 1002
#define ID_LIST 1101
#define ID_CHART 1102
#define MAX_RECORDS 10000
#define MAX_CATALYSTS 256
#define MAX_FIELDS 64
#define MAX_FIELD 1024
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    wchar_t catalyst[128];
    double time_h;
    double performance;
} Record;

typedef struct {
    wchar_t catalyst[128];
    int count;
    double initial_t;
    double initial_p;
    double latest_t;
    double latest_p;
    double retention;
} Summary;

typedef struct {
    double t;
    double p;
} Point;

static HINSTANCE g_hinst;
static HWND g_hwnd;
static HWND g_list;
static HWND g_chart;
static HWND g_status;
static HWND g_m1, g_m2, g_m3, g_m4;
static HFONT g_font, g_title_font, g_metric_font;
static Record g_records[MAX_RECORDS];
static int g_record_count = 0;
static Summary g_summaries[MAX_CATALYSTS];
static int g_summary_count = 0;
static double g_shared_time = -1.0;
static wchar_t g_shared_leader[128] = L"—";
static wchar_t g_source[MAX_PATH] = L"内置示例数据";

static const COLORREF SERIES[] = {
    RGB(31,119,180), RGB(214,39,40), RGB(44,160,44), RGB(148,103,189),
    RGB(255,127,14), RGB(23,190,207), RGB(140,86,75), RGB(227,119,194)
};

static void set_font(HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE); }
static void set_text(HWND h, const wchar_t *s) { SetWindowTextW(h, s ? s : L""); }

static HWND make_static(HWND parent, const wchar_t *text, int id, DWORD extra) {
    HWND h = CreateWindowExW(0, L"STATIC", text, WS_CHILD|WS_VISIBLE|extra,
        0,0,10,10,parent,(HMENU)(INT_PTR)id,g_hinst,NULL);
    set_font(h, g_font);
    return h;
}

static HWND make_button(HWND parent, const wchar_t *text, int id) {
    HWND h = CreateWindowExW(0, L"BUTTON", text, WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,10,10,parent,(HMENU)(INT_PTR)id,g_hinst,NULL);
    set_font(h, g_font);
    return h;
}

static void trim_w(wchar_t *s) {
    wchar_t *p = s;
    size_t n;
    while (*p==L' ' || *p==L'\t' || *p==L'\r' || *p==L'\n') p++;
    if (p != s) memmove(s,p,(wcslen(p)+1)*sizeof(wchar_t));
    n = wcslen(s);
    while (n && (s[n-1]==L' ' || s[n-1]==L'\t' || s[n-1]==L'\r' || s[n-1]==L'\n')) s[--n]=0;
}

static int to_wide(const char *src, wchar_t *dst, int cap) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, dst, cap);
    if (!n) n = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, cap);
    if (!n) { dst[0]=0; return 0; }
    trim_w(dst);
    return 1;
}

static int parse_csv(const char *line, char out[MAX_FIELDS][MAX_FIELD]) {
    int f=0, p=0, quoted=0;
    const char *s=line;
    memset(out,0,MAX_FIELDS*MAX_FIELD);
    while (*s && f<MAX_FIELDS) {
        char c=*s++;
        if (quoted) {
            if (c=='"') {
                if (*s=='"') { if (p<MAX_FIELD-1) out[f][p++]='"'; s++; }
                else quoted=0;
            } else if (p<MAX_FIELD-1) out[f][p++]=c;
        } else {
            if (c=='"' && p==0) quoted=1;
            else if (c==',') { out[f][p]=0; f++; p=0; }
            else if (c=='\r' || c=='\n') break;
            else if (p<MAX_FIELD-1) out[f][p++]=c;
        }
    }
    if (f<MAX_FIELDS) { out[f][p]=0; f++; }
    return f;
}

static int match_any(const wchar_t *s, const wchar_t *const *choices, int count) {
    int i;
    for (i=0;i<count;i++) if (_wcsicmp(s,choices[i])==0) return 1;
    return 0;
}

static int load_csv_file(const wchar_t *path, wchar_t *err, int errcap) {
    FILE *fp;
    char line[32768];
    char fields[MAX_FIELDS][MAX_FIELD];
    wchar_t head[128];
    int first=1, ccol=-1, tcol=-1, pcol=-1;
    const wchar_t *cn[]={L"催化剂",L"样品",L"catalyst",L"catalyst_id",L"sample"};
    const wchar_t *tn[]={L"时间",L"TOS",L"time",L"time_h",L"tos_h"};
    const wchar_t *pn[]={L"性能",L"转化率",L"活性",L"performance",L"conversion",L"activity"};

    fp=_wfopen(path,L"rb");
    if (!fp) { swprintf_s(err,errcap,L"无法打开文件：%ls",path); return 0; }
    g_record_count=0;
    while (fgets(line,sizeof(line),fp)) {
        int n=parse_csv(line,fields), i;
        if (first) {
            if ((unsigned char)fields[0][0]==0xEF && (unsigned char)fields[0][1]==0xBB && (unsigned char)fields[0][2]==0xBF)
                memmove(fields[0],fields[0]+3,strlen(fields[0]+3)+1);
            for (i=0;i<n;i++) {
                to_wide(fields[i],head,(int)ARRAY_LEN(head));
                if (match_any(head,cn,(int)ARRAY_LEN(cn))) ccol=i;
                if (match_any(head,tn,(int)ARRAY_LEN(tn))) tcol=i;
                if (match_any(head,pn,(int)ARRAY_LEN(pn))) pcol=i;
            }
            first=0;
            if (ccol<0 || tcol<0 || pcol<0) {
                fclose(fp);
                swprintf_s(err,errcap,L"CSV 缺少必要列。至少需要：催化剂、时间、性能。");
                return 0;
            }
            continue;
        }
        if (n<=ccol || n<=tcol || n<=pcol || g_record_count>=MAX_RECORDS) continue;
        if (!fields[ccol][0] || !fields[tcol][0] || !fields[pcol][0]) continue;
        {
            wchar_t cat[128];
            char *e1=NULL,*e2=NULL;
            double t=strtod(fields[tcol],&e1), p=strtod(fields[pcol],&e2);
            if (e1==fields[tcol] || e2==fields[pcol]) continue;
            if (!to_wide(fields[ccol],cat,(int)ARRAY_LEN(cat)) || !cat[0]) continue;
            wcsncpy_s(g_records[g_record_count].catalyst,ARRAY_LEN(g_records[g_record_count].catalyst),cat,_TRUNCATE);
            g_records[g_record_count].time_h=t;
            g_records[g_record_count].performance=p;
            g_record_count++;
        }
    }
    fclose(fp);
    if (!g_record_count) { swprintf_s(err,errcap,L"没有读取到有效数据行。"); return 0; }
    wcsncpy_s(g_source,ARRAY_LEN(g_source),path,_TRUNCATE);
    return 1;
}

static void load_demo(void) {
    const wchar_t *cats[]={L"Catalyst A",L"Catalyst A",L"Catalyst A",L"Catalyst B",L"Catalyst B",L"Catalyst B"};
    const double ts[]={0,20,50,0,20,50};
    const double ps[]={82,70,58,76,72,69};
    int i;
    g_record_count=6;
    for (i=0;i<6;i++) {
        wcsncpy_s(g_records[i].catalyst,ARRAY_LEN(g_records[i].catalyst),cats[i],_TRUNCATE);
        g_records[i].time_h=ts[i]; g_records[i].performance=ps[i];
    }
    wcscpy_s(g_source,ARRAY_LEN(g_source),L"内置示例数据");
}

static int summary_index(const wchar_t *cat) {
    int i;
    for (i=0;i<g_summary_count;i++) if (_wcsicmp(g_summaries[i].catalyst,cat)==0) return i;
    return -1;
}

static int value_at(const wchar_t *cat,double t,double *p) {
    int i;
    for (i=0;i<g_record_count;i++) {
        if (_wcsicmp(g_records[i].catalyst,cat)==0 && fabs(g_records[i].time_h-t)<1e-9) {
            if (p) *p=g_records[i].performance;
            return 1;
        }
    }
    return 0;
}

static void analyze(void) {
    int i,j;
    g_summary_count=0;
    for (i=0;i<g_record_count;i++) {
        Record *r=&g_records[i];
        int idx=summary_index(r->catalyst);
        Summary *s;
        if (idx<0) {
            if (g_summary_count>=MAX_CATALYSTS) continue;
            s=&g_summaries[g_summary_count++];
            ZeroMemory(s,sizeof(*s));
            wcsncpy_s(s->catalyst,ARRAY_LEN(s->catalyst),r->catalyst,_TRUNCATE);
            s->count=1; s->initial_t=s->latest_t=r->time_h; s->initial_p=s->latest_p=r->performance;
        } else {
            s=&g_summaries[idx]; s->count++;
            if (r->time_h<s->initial_t) { s->initial_t=r->time_h; s->initial_p=r->performance; }
            if (r->time_h>s->latest_t) { s->latest_t=r->time_h; s->latest_p=r->performance; }
        }
    }
    for (i=0;i<g_summary_count;i++)
        g_summaries[i].retention = g_summaries[i].initial_p!=0 ? 100.0*g_summaries[i].latest_p/g_summaries[i].initial_p : 0.0;

    g_shared_time=-1.0;
    wcscpy_s(g_shared_leader,ARRAY_LEN(g_shared_leader),L"—");
    if (g_summary_count) {
        for (i=0;i<g_record_count;i++) {
            double t;
            int all=1;
            if (_wcsicmp(g_records[i].catalyst,g_summaries[0].catalyst)!=0) continue;
            t=g_records[i].time_h;
            for (j=1;j<g_summary_count;j++) if (!value_at(g_summaries[j].catalyst,t,NULL)) { all=0; break; }
            if (all && t>g_shared_time) g_shared_time=t;
        }
        if (g_shared_time>=0) {
            double best=-1e300;
            for (i=0;i<g_summary_count;i++) {
                double p;
                if (value_at(g_summaries[i].catalyst,g_shared_time,&p) && p>best) {
                    best=p;
                    wcsncpy_s(g_shared_leader,ARRAY_LEN(g_shared_leader),g_summaries[i].catalyst,_TRUNCATE);
                }
            }
        }
    }
}

static void init_columns(void) {
    const wchar_t *names[]={L"催化剂",L"点数",L"初始性能",L"最新性能",L"最新时间(h)",L"保持率(%)",L"T90 证据"};
    const int widths[]={160,70,100,100,110,105,220};
    int i;
    ListView_DeleteAllItems(g_list);
    while (Header_GetItemCount(ListView_GetHeader(g_list))>0) ListView_DeleteColumn(g_list,0);
    for (i=0;i<(int)ARRAY_LEN(names);i++) {
        LVCOLUMNW c; ZeroMemory(&c,sizeof(c));
        c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT; c.pszText=(LPWSTR)names[i]; c.cx=widths[i];
        c.fmt=(i==0 || i==6)?LVCFMT_LEFT:LVCFMT_RIGHT;
        ListView_InsertColumn(g_list,i,&c);
    }
}

static void refresh_ui(void) {
    int i,j;
    double longest=0;
    wchar_t b[256];
    init_columns();
    for (i=0;i<g_summary_count;i++) {
        Summary *s=&g_summaries[i];
        LVITEMW it; ZeroMemory(&it,sizeof(it)); it.mask=LVIF_TEXT; it.iItem=i; it.pszText=s->catalyst;
        ListView_InsertItem(g_list,&it);
        swprintf_s(b,ARRAY_LEN(b),L"%d",s->count); ListView_SetItemText(g_list,i,1,b);
        swprintf_s(b,ARRAY_LEN(b),L"%.3g",s->initial_p); ListView_SetItemText(g_list,i,2,b);
        swprintf_s(b,ARRAY_LEN(b),L"%.3g",s->latest_p); ListView_SetItemText(g_list,i,3,b);
        swprintf_s(b,ARRAY_LEN(b),L"%.3g",s->latest_t); ListView_SetItemText(g_list,i,4,b);
        swprintf_s(b,ARRAY_LEN(b),L"%.1f",s->retention); ListView_SetItemText(g_list,i,5,b);
        {
            int crossed=0; double cross=0, threshold=s->initial_p*0.9;
            for (j=0;j<g_record_count;j++) if (_wcsicmp(g_records[j].catalyst,s->catalyst)==0 && g_records[j].performance<=threshold) {
                if (!crossed || g_records[j].time_h<cross) { crossed=1; cross=g_records[j].time_h; }
            }
            if (crossed) swprintf_s(b,ARRAY_LEN(b),L"首次观测 ≤90%%：%.3g h",cross);
            else swprintf_s(b,ARRAY_LEN(b),L"截至 %.3g h 尚未观测 ≤90%%",s->latest_t);
            ListView_SetItemText(g_list,i,6,b);
        }
        if (s->latest_t>longest) longest=s->latest_t;
    }
    swprintf_s(b,ARRAY_LEN(b),L"%d",g_summary_count); set_text(g_m1,b);
    swprintf_s(b,ARRAY_LEN(b),L"%d",g_record_count); set_text(g_m2,b);
    swprintf_s(b,ARRAY_LEN(b),L"%.3g h",longest); set_text(g_m3,b);
    if (g_shared_time>=0) swprintf_s(b,ARRAY_LEN(b),L"%ls @ %.3g h",g_shared_leader,g_shared_time);
    else wcscpy_s(b,ARRAY_LEN(b),L"无共同观测点");
    set_text(g_m4,b);
    swprintf_s(b,ARRAY_LEN(b),L"已分析 %d 个数据点 / %d 个催化剂。数据源：%ls",g_record_count,g_summary_count,g_source);
    set_text(g_status,b);
    InvalidateRect(g_chart,NULL,TRUE);
}

static void open_csv(void) {
    OPENFILENAMEW ofn; wchar_t path[MAX_PATH]=L""; wchar_t err[512];
    ZeroMemory(&ofn,sizeof(ofn)); ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=g_hwnd;
    ofn.lpstrFilter=L"CSV 文件 (*.csv)\0*.csv\0所有文件 (*.*)\0*.*\0"; ofn.lpstrFile=path; ofn.nMaxFile=ARRAY_LEN(path);
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) return;
    if (!load_csv_file(path,err,(int)ARRAY_LEN(err))) { MessageBoxW(g_hwnd,err,L"导入失败",MB_OK|MB_ICONERROR); return; }
    analyze(); refresh_ui();
}

static int cmp_point(const void *a,const void *b) {
    const Point *pa=(const Point*)a,*pb=(const Point*)b;
    return pa->t<pb->t?-1:(pa->t>pb->t?1:0);
}

static LRESULT CALLBACK ChartProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    if (m==WM_PAINT) {
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT r; HBRUSH bg; int i,s;
        GetClientRect(h,&r); bg=CreateSolidBrush(RGB(250,251,253)); FillRect(dc,&r,bg); DeleteObject(bg);
        SetBkMode(dc,TRANSPARENT); SelectObject(dc,g_font); SetTextColor(dc,RGB(70,78,90));
        if (!g_record_count || !g_summary_count) { DrawTextW(dc,L"导入数据后显示原生趋势图",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE); EndPaint(h,&ps); return 0; }
        {
            double mint=g_records[0].time_h,maxt=mint,minp=g_records[0].performance,maxp=minp;
            int left=55,top=38,right=r.right-22,bottom=r.bottom-42;
            for (i=1;i<g_record_count;i++) { if(g_records[i].time_h<mint)mint=g_records[i].time_h; if(g_records[i].time_h>maxt)maxt=g_records[i].time_h; if(g_records[i].performance<minp)minp=g_records[i].performance; if(g_records[i].performance>maxp)maxp=g_records[i].performance; }
            if (fabs(maxt-mint)<1e-9) maxt=mint+1; if (fabs(maxp-minp)<1e-9) maxp=minp+1;
            { HPEN axis=CreatePen(PS_SOLID,1,RGB(170,178,190)); SelectObject(dc,axis); MoveToEx(dc,left,top,NULL); LineTo(dc,left,bottom); LineTo(dc,right,bottom); DeleteObject(axis); }
            for (s=0;s<g_summary_count;s++) {
                Point pts[MAX_RECORDS]; int n=0,j; HPEN pen;
                for (i=0;i<g_record_count;i++) if (_wcsicmp(g_records[i].catalyst,g_summaries[s].catalyst)==0) { pts[n].t=g_records[i].time_h; pts[n].p=g_records[i].performance; n++; }
                qsort(pts,n,sizeof(Point),cmp_point); pen=CreatePen(PS_SOLID,2,SERIES[s%ARRAY_LEN(SERIES)]); SelectObject(dc,pen);
                for (j=0;j<n;j++) { int x=left+(int)((pts[j].t-mint)/(maxt-mint)*(right-left)); int y=bottom-(int)((pts[j].p-minp)/(maxp-minp)*(bottom-top)); if(!j)MoveToEx(dc,x,y,NULL); else LineTo(dc,x,y); Ellipse(dc,x-3,y-3,x+4,y+4); }
                DeleteObject(pen);
            }
            {
                wchar_t b[64]; RECT t={0,top-8,left-7,top+18}; swprintf_s(b,ARRAY_LEN(b),L"%.3g",maxp); DrawTextW(dc,b,-1,&t,DT_RIGHT|DT_SINGLELINE);
                t.top=bottom-10;t.bottom=bottom+16;swprintf_s(b,ARRAY_LEN(b),L"%.3g",minp);DrawTextW(dc,b,-1,&t,DT_RIGHT|DT_SINGLELINE);
                t.left=left;t.right=right;t.top=bottom+8;t.bottom=bottom+30;swprintf_s(b,ARRAY_LEN(b),L"%.3g h  →  %.3g h",mint,maxt);DrawTextW(dc,b,-1,&t,DT_CENTER|DT_SINGLELINE);
            }
        }
        EndPaint(h,&ps); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

static void layout(HWND h) {
    RECT r; int w,hgt,margin=22,top=18,summary=142,content;
    GetClientRect(h,&r); w=r.right; hgt=r.bottom;
    MoveWindow(GetDlgItem(h,2001),margin,top,w-2*margin,38,TRUE);
    MoveWindow(GetDlgItem(h,2002),margin,top+39,w-2*margin,24,TRUE);
    MoveWindow(GetDlgItem(h,ID_OPEN),margin,top+72,150,36,TRUE);
    MoveWindow(GetDlgItem(h,ID_DEMO),margin+162,top+72,150,36,TRUE);
    MoveWindow(g_status,margin+328,top+72,w-margin-(margin+328),36,TRUE);
    MoveWindow(GetDlgItem(h,2101),margin,summary,w-2*margin,105,TRUE);
    { int cell=(w-2*margin-24)/4,i; HWND labs[]={GetDlgItem(h,2201),GetDlgItem(h,2202),GetDlgItem(h,2203),GetDlgItem(h,2204)}; HWND vals[]={g_m1,g_m2,g_m3,g_m4};
      for(i=0;i<4;i++){int x=margin+12+i*cell;MoveWindow(labs[i],x,summary+18,cell-6,20,TRUE);MoveWindow(vals[i],x,summary+40,cell-6,48,TRUE);} }
    content=summary+118;
    MoveWindow(GetDlgItem(h,2301),margin,content,(w-3*margin)/2,hgt-content-margin,TRUE);
    MoveWindow(GetDlgItem(h,2302),2*margin+(w-3*margin)/2,content,(w-3*margin)/2,hgt-content-margin,TRUE);
    MoveWindow(g_list,margin+10,content+30,(w-3*margin)/2-20,hgt-content-margin-40,TRUE);
    MoveWindow(g_chart,2*margin+(w-3*margin)/2+10,content+30,(w-3*margin)/2-20,hgt-content-margin-40,TRUE);
}

static LRESULT CALLBACK MainProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    switch(m) {
    case WM_CREATE: {
        HWND x;
        x=make_static(h,APP_TITLE,2001,SS_LEFT); set_font(x,g_title_font);
        make_static(h,L"原生 Windows 桌面版 · C / Win32 · 不启动浏览器 · 不依赖 Python",2002,SS_LEFT);
        make_button(h,L"导入 CSV",ID_OPEN); make_button(h,L"加载示例数据",ID_DEMO);
        g_status=make_static(h,L"启动完成。可以直接测试示例数据。",2003,SS_LEFT|SS_CENTERIMAGE);
        x=CreateWindowExW(0,L"BUTTON",L"结果总览",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,0,10,10,h,(HMENU)2101,g_hinst,NULL);set_font(x,g_font);
        make_static(h,L"催化剂数量",2201,SS_CENTER); make_static(h,L"数据点",2202,SS_CENTER); make_static(h,L"最长测试",2203,SS_CENTER); make_static(h,L"共同时间点领先",2204,SS_CENTER);
        g_m1=make_static(h,L"—",2211,SS_CENTER|SS_CENTERIMAGE);set_font(g_m1,g_metric_font);
        g_m2=make_static(h,L"—",2212,SS_CENTER|SS_CENTERIMAGE);set_font(g_m2,g_metric_font);
        g_m3=make_static(h,L"—",2213,SS_CENTER|SS_CENTERIMAGE);set_font(g_m3,g_metric_font);
        g_m4=make_static(h,L"—",2214,SS_CENTER|SS_CENTERIMAGE);set_font(g_m4,g_metric_font);
        x=CreateWindowExW(0,L"BUTTON",L"寿命分析结果",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,0,10,10,h,(HMENU)2301,g_hinst,NULL);set_font(x,g_font);
        x=CreateWindowExW(0,L"BUTTON",L"性能随时间变化",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,0,0,10,10,h,(HMENU)2302,g_hinst,NULL);set_font(x,g_font);
        g_list=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL,0,0,10,10,h,(HMENU)ID_LIST,g_hinst,NULL);set_font(g_list,g_font);ListView_SetExtendedListViewStyle(g_list,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);init_columns();
        g_chart=CreateWindowExW(WS_EX_CLIENTEDGE,CHART_CLASS,L"",WS_CHILD|WS_VISIBLE,0,0,10,10,h,(HMENU)ID_CHART,g_hinst,NULL);
        load_demo(); analyze(); refresh_ui();
        return 0;
    }
    case WM_SIZE: layout(h); return 0;
    case WM_COMMAND:
        if (LOWORD(w)==ID_OPEN) { open_csv(); return 0; }
        if (LOWORD(w)==ID_DEMO) { load_demo(); analyze(); refresh_ui(); return 0; }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE hp,PWSTR cmd,int show) {
    WNDCLASSEXW wc,cc; MSG msg; LOGFONTW lf;
    (void)hp;(void)cmd; g_hinst=hi;
    ZeroMemory(&lf,sizeof(lf)); wcscpy_s(lf.lfFaceName,ARRAY_LEN(lf.lfFaceName),L"Segoe UI"); lf.lfHeight=-16; g_font=CreateFontIndirectW(&lf);
    lf.lfHeight=-28; lf.lfWeight=FW_SEMIBOLD; g_title_font=CreateFontIndirectW(&lf);
    lf.lfHeight=-22; g_metric_font=CreateFontIndirectW(&lf);
    { INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic); }
    ZeroMemory(&cc,sizeof(cc));cc.cbSize=sizeof(cc);cc.hInstance=hi;cc.lpfnWndProc=ChartProc;cc.lpszClassName=CHART_CLASS;cc.hCursor=LoadCursorW(NULL,IDC_ARROW);cc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassExW(&cc);
    ZeroMemory(&wc,sizeof(wc));wc.cbSize=sizeof(wc);wc.hInstance=hi;wc.lpfnWndProc=MainProc;wc.lpszClassName=APP_CLASS;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hIcon=LoadIconW(NULL,IDI_APPLICATION);wc.hIconSm=wc.hIcon;wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassExW(&wc);
    g_hwnd=CreateWindowExW(0,APP_CLASS,APP_TITLE,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,820,NULL,NULL,hi,NULL);
    if(!g_hwnd)return 1;ShowWindow(g_hwnd,show);UpdateWindow(g_hwnd);
    while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}DeleteObject(g_font);DeleteObject(g_title_font);DeleteObject(g_metric_font);return (int)msg.wParam;
}
