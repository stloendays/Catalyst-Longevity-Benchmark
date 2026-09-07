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
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

#define APP_CLASS L"CatalystLongevityNativeWindow"
#define CHART_CLASS L"CatalystLongevityChartWindow"
#define APP_TITLE L"Catalyst Longevity Research"

#define ID_OPEN 1001
#define ID_DEMO 1002
#define ID_ANALYZE 1003
#define ID_LIST 1101
#define ID_CHART 1102

#define MAX_FIELDS 64
#define MAX_FIELD_BYTES 1024
#define LINE_BYTES 32768

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    wchar_t catalyst[128];
    double time_h;
    double performance;
} Record;

typedef struct {
    wchar_t catalyst[128];
    int count;
    double initial_time;
    double initial_performance;
    double latest_time;
    double latest_performance;
    double retention_percent;
    int t90_crossed;
    double t90_time;
} Summary;

typedef struct {
    double time_h;
    double performance;
} PlotPoint;

static HINSTANCE g_instance;
static HWND g_main;
static HWND g_list;
static HWND g_chart;
static HWND g_status;
static HWND g_metric_catalysts;
static HWND g_metric_points;
static HWND g_metric_longest;
static HWND g_metric_leader;
static HFONT g_font;
static HFONT g_title_font;
static HFONT g_metric_font;
static Record *g_records = NULL;
static size_t g_record_count = 0;
static size_t g_record_capacity = 0;
static Summary *g_summaries = NULL;
static size_t g_summary_count = 0;
static wchar_t g_source_file[MAX_PATH] = L"内置示例数据";
static double g_shared_time = -1.0;
static wchar_t g_shared_leader[128] = L"—";

static const COLORREF g_series_colors[] = {
    RGB(31, 119, 180), RGB(214, 39, 40), RGB(44, 160, 44), RGB(148, 103, 189),
    RGB(255, 127, 14), RGB(23, 190, 207), RGB(140, 86, 75), RGB(227, 119, 194)
};

static void set_text(HWND hwnd, const wchar_t *text) {
    SetWindowTextW(hwnd, text ? text : L"");
}

static void set_status(const wchar_t *text) {
    set_text(g_status, text);
}

static void free_data(void) {
    free(g_records);
    g_records = NULL;
    g_record_count = 0;
    g_record_capacity = 0;
    free(g_summaries);
    g_summaries = NULL;
    g_summary_count = 0;
    g_shared_time = -1.0;
    wcscpy_s(g_shared_leader, ARRAY_COUNT(g_shared_leader), L"—");
}

static int ensure_record_capacity(size_t wanted) {
    Record *next;
    size_t cap;
    if (wanted <= g_record_capacity) return 1;
    cap = g_record_capacity ? g_record_capacity * 2 : 128;
    while (cap < wanted) cap *= 2;
    next = (Record *)realloc(g_records, cap * sizeof(Record));
    if (!next) return 0;
    g_records = next;
    g_record_capacity = cap;
    return 1;
}

static void trim_wide(wchar_t *text) {
    wchar_t *start;
    size_t len;
    if (!text) return;
    start = text;
    while (*start == L' ' || *start == L'\t' || *start == L'\r' || *start == L'\n') start++;
    if (start != text) memmove(text, start, (wcslen(start) + 1) * sizeof(wchar_t));
    len = wcslen(text);
    while (len > 0) {
        wchar_t ch = text[len - 1];
        if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n') break;
        text[--len] = L'\0';
    }
}

static int utf8_to_wide(const char *src, wchar_t *dst, int dst_count) {
    int n;
    if (!src || !dst || dst_count <= 0) return 0;
    dst[0] = L'\0';
    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, dst, dst_count);
    if (n == 0) n = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, dst_count);
    if (n == 0) return 0;
    trim_wide(dst);
    return 1;
}

static int header_matches(const wchar_t *value, const wchar_t *const *choices, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (_wcsicmp(value, choices[i]) == 0) return 1;
    }
    return 0;
}

static int parse_csv_line(const char *line, char fields[MAX_FIELDS][MAX_FIELD_BYTES]) {
    int field = 0;
    int pos = 0;
    int quoted = 0;
    const char *p = line;
    memset(fields, 0, MAX_FIELDS * MAX_FIELD_BYTES);
    while (*p && field < MAX_FIELDS) {
        char ch = *p++;
        if (quoted) {
            if (ch == '"') {
                if (*p == '"') {
                    if (pos < MAX_FIELD_BYTES - 1) fields[field][pos++] = '"';
                    p++;
                } else {
                    quoted = 0;
                }
            } else {
                if (pos < MAX_FIELD_BYTES - 1) fields[field][pos++] = ch;
            }
        } else {
            if (ch == '"' && pos == 0) {
                quoted = 1;
            } else if (ch == ',') {
                fields[field][pos] = '\0';
                field++;
                pos = 0;
            } else if (ch == '\r' || ch == '\n') {
                break;
            } else {
                if (pos < MAX_FIELD_BYTES - 1) fields[field][pos++] = ch;
            }
        }
    }
    if (field < MAX_FIELDS) {
        fields[field][pos] = '\0';
        field++;
    }
    return field;
}

static int add_record(const wchar_t *catalyst, double time_h, double performance) {
    Record *r;
    if (!ensure_record_capacity(g_record_count + 1)) return 0;
    r = &g_records[g_record_count++];
    wcsncpy_s(r->catalyst, ARRAY_COUNT(r->catalyst), catalyst, _TRUNCATE);
    r->time_h = time_h;
    r->performance = performance;
    return 1;
}

static int load_csv_file(const wchar_t *path, wchar_t *error, size_t error_count) {
    FILE *fp;
    char line[LINE_BYTES];
    char fields[MAX_FIELDS][MAX_FIELD_BYTES];
    wchar_t header[MAX_FIELDS][128];
    int field_count;
    int catalyst_col = -1, time_col = -1, performance_col = -1;
    int first = 1;
    size_t valid_rows = 0;
    const wchar_t *catalyst_names[] = {L"催化剂", L"样品", L"catalyst", L"catalyst_id", L"sample"};
    const wchar_t *time_names[] = {L"时间", L"TOS", L"time", L"time_h", L"time (h)", L"tos_h"};
    const wchar_t *performance_names[] = {L"性能", L"转化率", L"活性", L"performance", L"conversion", L"activity"};

    fp = _wfopen(path, L"rb");
    if (!fp) {
        swprintf_s(error, error_count, L"无法打开文件：%ls", path);
        return 0;
    }

    free_data();
    while (fgets(line, sizeof(line), fp)) {
        if (first) {
            int i;
            if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
                memmove(line, line + 3, strlen(line + 3) + 1);
            }
            field_count = parse_csv_line(line, fields);
            for (i = 0; i < field_count; ++i) {
                utf8_to_wide(fields[i], header[i], ARRAY_COUNT(header[i]));
                if (header_matches(header[i], catalyst_names, ARRAY_COUNT(catalyst_names))) catalyst_col = i;
                if (header_matches(header[i], time_names, ARRAY_COUNT(time_names))) time_col = i;
                if (header_matches(header[i], performance_names, ARRAY_COUNT(performance_names))) performance_col = i;
            }
            first = 0;
            if (catalyst_col < 0 || time_col < 0 || performance_col < 0) {
                fclose(fp);
                swprintf_s(error, error_count,
                    L"CSV 缺少必要列。至少需要：催化剂、时间、性能。\n当前识别结果：催化剂列=%d，时间列=%d，性能列=%d",
                    catalyst_col, time_col, performance_col);
                free_data();
                return 0;
            }
            continue;
        }

        field_count = parse_csv_line(line, fields);
        if (field_count <= catalyst_col || field_count <= time_col || field_count <= performance_col) continue;
        if (fields[catalyst_col][0] == '\0' || fields[time_col][0] == '\0' || fields[performance_col][0] == '\0') continue;
        {
            wchar_t catalyst[128];
            char *end_time = NULL;
            char *end_perf = NULL;
            double time_h = strtod(fields[time_col], &end_time);
            double performance = strtod(fields[performance_col], &end_perf);
            if (end_time == fields[time_col] || end_perf == fields[performance_col]) continue;
            if (!utf8_to_wide(fields[catalyst_col], catalyst, ARRAY_COUNT(catalyst)) || catalyst[0] == L'\0') continue;
            if (!add_record(catalyst, time_h, performance)) {
                fclose(fp);
                swprintf_s(error, error_count, L"内存不足，无法继续读取数据。");
                free_data();
                return 0;
            }
            valid_rows++;
        }
    }
    fclose(fp);

    if (valid_rows == 0) {
        swprintf_s(error, error_count, L"没有读取到有效数据行。请检查 CSV 编码和数值列。");
        free_data();
        return 0;
    }
    wcsncpy_s(g_source_file, ARRAY_COUNT(g_source_file), path, _TRUNCATE);
    return 1;
}

static void load_demo_data(void) {
    free_data();
    add_record(L"Catalyst A", 0.0, 82.0);
    add_record(L"Catalyst A", 20.0, 70.0);
    add_record(L"Catalyst A", 50.0, 58.0);
    add_record(L"Catalyst B", 0.0, 76.0);
    add_record(L"Catalyst B", 20.0, 72.0);
    add_record(L"Catalyst B", 50.0, 69.0);
    wcscpy_s(g_source_file, ARRAY_COUNT(g_source_file), L"内置示例数据");
}

static int find_summary(const wchar_t *name) {
    size_t i;
    for (i = 0; i < g_summary_count; ++i) {
        if (_wcsicmp(g_summaries[i].catalyst, name) == 0) return (int)i;
    }
    return -1;
}

static int compare_summary_retention(const void *a, const void *b) {
    const Summary *sa = (const Summary *)a;
    const Summary *sb = (const Summary *)b;
    if (sa->retention_percent < sb->retention_percent) return 1;
    if (sa->retention_percent > sb->retention_percent) return -1;
    return _wcsicmp(sa->catalyst, sb->catalyst);
}

static int compare_plot_point(const void *a, const void *b) {
    const PlotPoint *pa = (const PlotPoint *)a;
    const PlotPoint *pb = (const PlotPoint *)b;
    if (pa->time_h < pb->time_h) return -1;
    if (pa->time_h > pb->time_h) return 1;
    return 0;
}

static int has_value_at_time(const wchar_t *catalyst, double time_h, double *performance) {
    size_t i;
    for (i = 0; i < g_record_count; ++i) {
        if (_wcsicmp(g_records[i].catalyst, catalyst) == 0 && fabs(g_records[i].time_h - time_h) < 1e-9) {
            if (performance) *performance = g_records[i].performance;
            return 1;
        }
    }
    return 0;
}

static void compute_shared_leader(void) {
    size_t i, j;
    double best_time = -1.0;
    wchar_t leader[128] = L"—";
    double leader_perf = 0.0;
    if (g_summary_count == 0) return;

    for (i = 0; i < g_record_count; ++i) {
        double t;
        int all_present = 1;
        if (_wcsicmp(g_records[i].catalyst, g_summaries[0].catalyst) != 0) continue;
        t = g_records[i].time_h;
        for (j = 1; j < g_summary_count; ++j) {
            if (!has_value_at_time(g_summaries[j].catalyst, t, NULL)) {
                all_present = 0;
                break;
            }
        }
        if (all_present && t > best_time) best_time = t;
    }

    if (best_time >= 0.0) {
        int first = 1;
        for (j = 0; j < g_summary_count; ++j) {
            double p = 0.0;
            if (has_value_at_time(g_summaries[j].catalyst, best_time, &p)) {
                if (first || p > leader_perf) {
                    first = 0;
                    leader_perf = p;
                    wcsncpy_s(leader, ARRAY_COUNT(leader), g_summaries[j].catalyst, _TRUNCATE);
                }
            }
        }
    }
    g_shared_time = best_time;
    wcsncpy_s(g_shared_leader, ARRAY_COUNT(g_shared_leader), leader, _TRUNCATE);
}

static int analyze_data(void) {
    size_t i;
    if (g_record_count == 0) return 0;
    free(g_summaries);
    g_summaries = (Summary *)calloc(g_record_count, sizeof(Summary));
    if (!g_summaries) return 0;
    g_summary_count = 0;

    for (i = 0; i < g_record_count; ++i) {
        Record *r = &g_records[i];
        int idx = find_summary(r->catalyst);
        Summary *s;
        if (idx < 0) {
            s = &g_summaries[g_summary_count++];
            wcsncpy_s(s->catalyst, ARRAY_COUNT(s->catalyst), r->catalyst, _TRUNCATE);
            s->count = 1;
            s->initial_time = s->latest_time = r->time_h;
            s->initial_performance = s->latest_performance = r->performance;
        } else {
            s = &g_summaries[idx];
            s->count++;
            if (r->time_h < s->initial_time) {
                s->initial_time = r->time_h;
                s->initial_performance = r->performance;
            }
            if (r->time_h > s->latest_time) {
                s->latest_time = r->time_h;
                s->latest_performance = r->performance;
            }
        }
    }

    for (i = 0; i < g_summary_count; ++i) {
        Summary *s = &g_summaries[i];
        size_t j;
        double threshold;
        s->retention_percent = s->initial_performance != 0.0 ? (s->latest_performance / s->initial_performance) * 100.0 : 0.0;
        threshold = s->initial_performance * 0.90;
        s->t90_crossed = 0;
        s->t90_time = 0.0;
        for (j = 0; j < g_record_count; ++j) {
            Record *r = &g_records[j];
            if (_wcsicmp(r->catalyst, s->catalyst) == 0 && r->performance <= threshold) {
                if (!s->t90_crossed || r->time_h < s->t90_time) {
                    s->t90_crossed = 1;
                    s->t90_time = r->time_h;
                }
            }
        }
    }

    qsort(g_summaries, g_summary_count, sizeof(Summary), compare_summary_retention);
    compute_shared_leader();
    return 1;
}

static void init_list_columns(void) {
    const wchar_t *titles[] = {L"催化剂", L"观测点", L"初始性能", L"最新性能", L"最新时间 (h)", L"保持率 (%)", L"T90 证据"};
    int widths[] = {170, 80, 110, 110, 120, 115, 220};
    int i;
    ListView_DeleteAllItems(g_list);
    while (Header_GetItemCount(ListView_GetHeader(g_list)) > 0) ListView_DeleteColumn(g_list, 0);
    for (i = 0; i < (int)ARRAY_COUNT(titles); ++i) {
        LVCOLUMNW col;
        ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.pszText = (LPWSTR)titles[i];
        col.cx = widths[i];
        col.fmt = i == 0 || i == 6 ? LVCFMT_LEFT : LVCFMT_RIGHT;
        ListView_InsertColumn(g_list, i, &col);
    }
}

static void refresh_results(void) {
    size_t i;
    wchar_t buffer[256];
    double longest = 0.0;
    init_list_columns();
    for (i = 0; i < g_summary_count; ++i) {
        Summary *s = &g_summaries[i];
        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = (int)i;
        item.pszText = s->catalyst;
        ListView_InsertItem(g_list, &item);

        swprintf_s(buffer, ARRAY_COUNT(buffer), L"%d", s->count);
        ListView_SetItemText(g_list, (int)i, 1, buffer);
        swprintf_s(buffer, ARRAY_COUNT(buffer), L"%.3g", s->initial_performance);
        ListView_SetItemText(g_list, (int)i, 2, buffer);
        swprintf_s(buffer, ARRAY_COUNT(buffer), L"%.3g", s->latest_performance);
        ListView_SetItemText(g_list, (int)i, 3, buffer);
        swprintf_s(buffer, ARRAY_COUNT(buffer), L"%.3g", s->latest_time);
        ListView_SetItemText(g_list, (int)i, 4, buffer);
        swprintf_s(buffer, ARRAY_COUNT(buffer), L"%.1f", s->retention_percent);
        ListView_SetItemText(g_list, (int)i, 5, buffer);
        if (s->t90_crossed) {
            swprintf_s(buffer, ARRAY_COUNT(buffer), L"首次观测到 ≤90%%：%.3g h", s->t90_time);
        } else {
            swprintf_s(buffer, ARRAY_COUNT(buffer), L"截至 %.3g h 尚未观测到 ≤90%%", s->latest_time);
        }
        ListView_SetItemText(g_list, (int)i, 6, buffer);
        if (s->latest_time > longest) longest = s->latest_time;
    }

    swprintf_s(buffer, ARRAY_COUNT(buffer), L"%zu", g_summary_count);
    set_text(g_metric_catalysts, buffer);
    swprintf_s(buffer, ARRAY_COUNT(buffer), L"%zu", g_record_count);
    set_text(g_metric_points, buffer);
    swprintf_s(buffer, ARRAY_COUNT(buffer), L"%.3g h", longest);
    set_text(g_metric_longest, buffer);
    if (g_shared_time >= 0.0) {
        swprintf_s(buffer, ARRAY_COUNT(buffer), L"%ls\n@ %.3g h", g_shared_leader, g_shared_time);
        set_text(g_metric_leader, buffer);
    } else {
        set_text(g_metric_leader, L"无共同观测点");
    }
    InvalidateRect(g_chart, NULL, TRUE);
}

static void run_analysis_and_refresh(void) {
    wchar_t status[512];
    if (g_record_count == 0) {
        MessageBoxW(g_main, L"请先导入 CSV，或者加载示例数据。", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!analyze_data()) {
        MessageBoxW(g_main, L"分析失败：内存不足或数据状态异常。", APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }
    refresh_results();
    swprintf_s(status, ARRAY_COUNT(status), L"已分析 %zu 个数据点，%zu 个催化剂。数据源：%ls", g_record_count, g_summary_count, g_source_file);
    set_status(status);
}

static void choose_and_load_csv(void) {
    OPENFILENAMEW ofn;
    wchar_t path[MAX_PATH] = L"";
    wchar_t error[512];
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"CSV 文件 (*.csv)\0*.csv\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = ARRAY_COUNT(path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) return;
    if (!load_csv_file(path, error, ARRAY_COUNT(error))) {
        MessageBoxW(g_main, error, L"导入失败", MB_OK | MB_ICONERROR);
        set_status(L"导入失败。CSV 至少需要催化剂、时间、性能三列。");
        return;
    }
    run_analysis_and_refresh();
}

static void draw_text_center(HDC hdc, RECT rc, const wchar_t *text) {
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static LRESULT CALLBACK ChartProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        HBRUSH bg;
        HPEN axis_pen;
        double min_t = 0.0, max_t = 1.0, min_p = 0.0, max_p = 1.0;
        size_t i, sidx;
        int left, top, right, bottom, width, height;
        GetClientRect(hwnd, &rc);
        bg = CreateSolidBrush(RGB(250, 251, 253));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_font);
        SetTextColor(hdc, RGB(60, 68, 80));

        if (g_record_count == 0 || g_summary_count == 0) {
            draw_text_center(hdc, rc, L"导入数据后，这里会显示原生 GDI 趋势图");
            EndPaint(hwnd, &ps);
            return 0;
        }

        min_t = max_t = g_records[0].time_h;
        min_p = max_p = g_records[0].performance;
        for (i = 1; i < g_record_count; ++i) {
            if (g_records[i].time_h < min_t) min_t = g_records[i].time_h;
            if (g_records[i].time_h > max_t) max_t = g_records[i].time_h;
            if (g_records[i].performance < min_p) min_p = g_records[i].performance;
            if (g_records[i].performance > max_p) max_p = g_records[i].performance;
        }
        if (fabs(max_t - min_t) < 1e-12) max_t = min_t + 1.0;
        if (fabs(max_p - min_p) < 1e-12) max_p = min_p + 1.0;
        {
            double pad = (max_p - min_p) * 0.08;
            min_p -= pad;
            max_p += pad;
        }

        left = 58; top = 32; right = rc.right - 24; bottom = rc.bottom - 48;
        width = right - left; height = bottom - top;
        if (width < 100 || height < 100) {
            EndPaint(hwnd, &ps);
            return 0;
        }

        axis_pen = CreatePen(PS_SOLID, 1, RGB(170, 178, 190));
        SelectObject(hdc, axis_pen);
        MoveToEx(hdc, left, top, NULL); LineTo(hdc, left, bottom); LineTo(hdc, right, bottom);
        DeleteObject(axis_pen);

        {
            wchar_t label[64];
            RECT tr;
            SetTextColor(hdc, RGB(100, 108, 120));
            swprintf_s(label, ARRAY_COUNT(label), L"%.3g", max_p);
            tr.left = 0; tr.top = top - 8; tr.right = left - 8; tr.bottom = top + 18;
            DrawTextW(hdc, label, -1, &tr, DT_RIGHT | DT_SINGLELINE);
            swprintf_s(label, ARRAY_COUNT(label), L"%.3g", min_p);
            tr.top = bottom - 10; tr.bottom = bottom + 16;
            DrawTextW(hdc, label, -1, &tr, DT_RIGHT | DT_SINGLELINE);
            swprintf_s(label, ARRAY_COUNT(label), L"%.3g h", min_t);
            tr.left = left; tr.top = bottom + 8; tr.right = left + 100; tr.bottom = bottom + 30;
            DrawTextW(hdc, label, -1, &tr, DT_LEFT | DT_SINGLELINE);
            swprintf_s(label, ARRAY_COUNT(label), L"%.3g h", max_t);
            tr.left = right - 100; tr.right = right; DrawTextW(hdc, label, -1, &tr, DT_RIGHT | DT_SINGLELINE);
        }

        for (sidx = 0; sidx < g_summary_count; ++sidx) {
            PlotPoint *points;
            size_t count = 0, j;
            HPEN pen;
            COLORREF color = g_series_colors[sidx % ARRAY_COUNT(g_series_colors)];
            for (i = 0; i < g_record_count; ++i) if (_wcsicmp(g_records[i].catalyst, g_summaries[sidx].catalyst) == 0) count++;
            if (count == 0) continue;
            points = (PlotPoint *)malloc(count * sizeof(PlotPoint));
            if (!points) continue;
            count = 0;
            for (i = 0; i < g_record_count; ++i) {
                if (_wcsicmp(g_records[i].catalyst, g_summaries[sidx].catalyst) == 0) {
                    points[count].time_h = g_records[i].time_h;
                    points[count].performance = g_records[i].performance;
                    count++;
                }
            }
            qsort(points, count, sizeof(PlotPoint), compare_plot_point);
            pen = CreatePen(PS_SOLID, 2, color);
            SelectObject(hdc, pen);
            for (j = 0; j < count; ++j) {
                int x = left + (int)(((points[j].time_h - min_t) / (max_t - min_t)) * width);
                int y = bottom - (int)(((points[j].performance - min_p) / (max_p - min_p)) * height);
                if (j == 0) MoveToEx(hdc, x, y, NULL); else LineTo(hdc, x, y);
                Ellipse(hdc, x - 3, y - 3, x + 4, y + 4);
            }
            DeleteObject(pen);
            free(points);
        }

        {
            int x = left;
            int y = 7;
            for (sidx = 0; sidx < g_summary_count && sidx < 6; ++sidx) {
                HBRUSH b = CreateSolidBrush(g_series_colors[sidx % ARRAY_COUNT(g_series_colors)]);
                RECT box = {x, y + 3, x + 12, y + 15};
                FillRect(hdc, &box, b);
                DeleteObject(b);
                TextOutW(hdc, x + 18, y, g_summaries[sidx].catalyst, (int)wcslen(g_summaries[sidx].catalyst));
                x += 140;
                if (x > right - 120) { x = left; y += 18; }
            }
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HWND create_static(HWND parent, const wchar_t *text, DWORD style) {
    HWND h = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 10, 10, parent, NULL, g_instance, NULL);
    SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE);
    return h;
}

static HWND create_metric_value(HWND parent) {
    HWND h = create_static(parent, L"—", SS_CENTER | SS_CENTERIMAGE);
    SendMessageW(h, WM_SETFONT, (WPARAM)g_metric_font, TRUE);
    return h;
}

static void layout_controls(HWND hwnd) {
    RECT rc;
    int w, h;
    int margin = 24;
    int top = 22;
    int summary_top;
    int content_top;
    GetClientRect(hwnd, &rc);
    w = rc.right;
    h = rc.bottom;

    MoveWindow(GetDlgItem(hwnd, 2001), margin, top, w - 2 * margin, 38, TRUE);
    MoveWindow(GetDlgItem(hwnd, 2002), margin, top + 40, w - 2 * margin, 26, TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_OPEN), margin, top + 78, 150, 36, TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_DEMO), margin + 162, top + 78, 150, 36, TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_ANALYZE), margin + 324, top + 78, 150, 36, TRUE);
    MoveWindow(g_status, margin + 490, top + 80, w - margin - (margin + 490), 32, TRUE);

    summary_top = top + 130;
    MoveWindow(GetDlgItem(hwnd, 2101), margin, summary_top, w - 2 * margin, 112, TRUE);
    {
        int inner_w = w - 2 * margin - 28;
        int cell = inner_w / 4;
        HWND labels[] = {GetDlgItem(hwnd, 2201), GetDlgItem(hwnd, 2202), GetDlgItem(hwnd, 2203), GetDlgItem(hwnd, 2204)};
        HWND values[] = {g_metric_catalysts, g_metric_points, g_metric_longest, g_metric_leader};
        int i;
        for (i = 0; i < 4; ++i) {
            int x = margin + 14 + i * cell;
            MoveWindow(labels[i], x, summary_top + 20, cell - 8, 22, TRUE);
            MoveWindow(values[i], x, summary_top + 43, cell - 8, 50, TRUE);
        }
    }

    content_top = summary_top + 128;
    MoveWindow(GetDlgItem(hwnd, 2301), margin, content_top, (w - 3 * margin) / 2, h - content_top - margin, TRUE);
    MoveWindow(GetDlgItem(hwnd, 2302), margin * 2 + (w - 3 * margin) / 2, content_top, (w - 3 * margin) / 2, h - content_top - margin, TRUE);
    MoveWindow(g_list, margin + 12, content_top + 34, (w - 3 * margin) / 2 - 24, h - content_top - margin - 46, TRUE);
    MoveWindow(g_chart, margin * 2 + (w - 3 * margin) / 2 + 12, content_top + 34, (w - 3 * margin) / 2 - 24, h - content_top - margin - 46, TRUE);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icc;
        HWND title;
        HWND subtitle;
        HWND btn;
        HWND group;
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icc);

        title = create_static(hwnd, APP_TITLE, SS_LEFT);
        SetWindowLongPtrW(title, GWLP_ID, 2001);
        SendMessageW(title, WM_SETFONT, (WPARAM)g_title_font, TRUE);
        subtitle = create_static(hwnd, L"原生 Windows 桌面版 · 不启动浏览器 · 不依赖 Python · C / Win32", SS_LEFT);
        SetWindowLongPtrW(subtitle, GWLP_ID, 2002);

        btn = CreateWindowExW(0, L"BUTTON", L"导入 CSV", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 10, 10, hwnd, (HMENU)ID_OPEN, g_instance, NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)g_font, TRUE);
        btn = CreateWindowExW(0, L"BUTTON", L"加载示例数据", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 10, 10, hwnd, (HMENU)ID_DEMO, g_instance, NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)g_font, TRUE);
        btn = CreateWindowExW(0, L"BUTTON", L"重新分析", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            0, 0, 10, 10, hwnd, (HMENU)ID_ANALYZE, g_instance, NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)g_font, TRUE);

        g_status = create_static(hwnd, L"可直接加载示例数据，也可以导入 UTF-8 CSV。", SS_LEFT | SS_CENTERIMAGE);

        group = CreateWindowExW(0, L"BUTTON", L"结果总览", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            0, 0, 10, 10, hwnd, (HMENU)2101, g_instance, NULL);
        SendMessageW(group, WM_SETFONT, (WPARAM)g_font, TRUE);
        create_static(hwnd, L"催化剂数量", SS_CENTER)->unused;
        break;
    }
    case WM_SIZE:
        layout_controls(hwnd);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_OPEN:
            choose_and_load_csv();
            return 0;
        case ID_DEMO:
            load_demo_data();
            run_analysis_and_refresh();
            return 0;
        case ID_ANALYZE:
            run_analysis_and_refresh();
            return 0;
        }
        break;
    case WM_DESTROY:
        free_data();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int create_main_controls(HWND hwnd) {
    HWND label;
    HWND group;
    DWORD list_style = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;

    group = CreateWindowExW(0, L"BUTTON", L"结果总览", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        0, 0, 10, 10, hwnd, (HMENU)2101, g_instance, NULL);
    SendMessageW(group, WM_SETFONT, (WPARAM)g_font, TRUE);

    label = create_static(hwnd, L"催化剂数量", SS_CENTER); SetWindowLongPtrW(label, GWLP_ID, 2201);
    label = create_static(hwnd, L"数据点", SS_CENTER); SetWindowLongPtrW(label, GWLP_ID, 2202);
    label = create_static(hwnd, L"最长测试", SS_CENTER); SetWindowLongPtrW(label, GWLP_ID, 2203);
    label = create_static(hwnd, L"共同时间点领先", SS_CENTER); SetWindowLongPtrW(label, GWLP_ID, 2204);
    g_metric_catalysts = create_metric_value(hwnd);
    g_metric_points = create_metric_value(hwnd);
    g_metric_longest = create_metric_value(hwnd);
    g_metric_leader = create_metric_value(hwnd);

    group = CreateWindowExW(0, L"BUTTON", L"寿命分析结果", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        0, 0, 10, 10, hwnd, (HMENU)2301, g_instance, NULL);
    SendMessageW(group, WM_SETFONT, (WPARAM)g_font, TRUE);
    group = CreateWindowExW(0, L"BUTTON", L"性能随时间变化", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        0, 0, 10, 10, hwnd, (HMENU)2302, g_instance, NULL);
    SendMessageW(group, WM_SETFONT, (WPARAM)g_font, TRUE);

    g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", list_style,
        0, 0, 10, 10, hwnd, (HMENU)ID_LIST, g_instance, NULL);
    if (!g_list) return 0;
    SendMessageW(g_list, WM_SETFONT, (WPARAM)g_font, TRUE);
    ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    init_list_columns();

    g_chart = CreateWindowExW(WS_EX_CLIENTEDGE, CHART_CLASS, L"", WS_CHILD | WS_VISIBLE,
        0, 0, 10, 10, hwnd, (HMENU)ID_CHART, g_instance, NULL);
    return g_chart != NULL;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev, PWSTR cmdline, int show) {
    WNDCLASSEXW wc;
    WNDCLASSEXW chart_wc;
    MSG msg;
    NONCLIENTMETRICSW ncm;
    LOGFONTW lf;
    (void)prev; (void)cmdline;
    g_instance = instance;

    ZeroMemory(&ncm, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    lf = ncm.lfMessageFont;
    wcscpy_s(lf.lfFaceName, ARRAY_COUNT(lf.lfFaceName), L"Segoe UI");
    lf.lfHeight = -16;
    g_font = CreateFontIndirectW(&lf);
    lf.lfHeight = -28;
    lf.lfWeight = FW_SEMIBOLD;
    g_title_font = CreateFontIndirectW(&lf);
    lf.lfHeight = -24;
    lf.lfWeight = FW_SEMIBOLD;
    g_metric_font = CreateFontIndirectW(&lf);

    ZeroMemory(&chart_wc, sizeof(chart_wc));
    chart_wc.cbSize = sizeof(chart_wc);
    chart_wc.hInstance = instance;
    chart_wc.lpfnWndProc = ChartProc;
    chart_wc.lpszClassName = CHART_CLASS;
    chart_wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    chart_wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&chart_wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = MainProc;
    wc.lpszClassName = APP_CLASS;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    g_main = CreateWindowExW(0, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 820,
        NULL, NULL, instance, NULL);
    if (!g_main) return 1;

    {
        HWND title = create_static(g_main, APP_TITLE, SS_LEFT);
        SetWindowLongPtrW(title, GWLP_ID, 2001);
        SendMessageW(title, WM_SETFONT, (WPARAM)g_title_font, TRUE);
        {
            HWND subtitle = create_static(g_main, L"原生 Windows 桌面版 · 不启动浏览器 · 不依赖 Python · C / Win32", SS_LEFT);
            SetWindowLongPtrW(subtitle, GWLP_ID, 2002);
        }
        {
            HWND btn = CreateWindowExW(0, L"BUTTON", L"导入 CSV", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10, g_main, (HMENU)ID_OPEN, instance, NULL);
            SendMessageW(btn, WM_SETFONT, (WPARAM)g_font, TRUE);
            btn = CreateWindowExW(0, L"BUTTON", L"加载示例数据", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10, g_main, (HMENU)ID_DEMO, instance, NULL);
            SendMessageW(btn, WM_SETFONT, (WPARAM)g_font, TRUE);
            btn = CreateWindowExW(0, L"BUTTON", L"重新分析", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, g_main, (HMENU)ID_ANALYZE, instance, NULL);
            SendMessageW(btn, WM_SETFONT, (WPARAM)g_font, TRUE);
        }
        g_status = create_static(g_main, L"这是原生桌面程序。先点击“加载示例数据”测试。", SS_LEFT | SS_CENTERIMAGE);
    }

    if (!create_main_controls(g_main)) return 2;
    layout_controls(g_main);
    ShowWindow(g_main, show);
    UpdateWindow(g_main);

    load_demo_data();
    run_analysis_and_refresh();

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_font);
    DeleteObject(g_title_font);
    DeleteObject(g_metric_font);
    return (int)msg.wParam;
}
