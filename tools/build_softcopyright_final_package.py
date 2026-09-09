from __future__ import annotations
import argparse, math, os, re, zipfile
from pathlib import Path
from typing import List, Tuple
from docx import Document
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK, WD_LINE_SPACING
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Inches, Pt, RGBColor

FULL="催化剂长期稳定性评估与实验决策系统"; SHORT="智策"; VER="V1.0"
BODY="Microsoft YaHei"; CODE="Consolas"
SRC=Path("native/qt/src"); MANUAL_MD=Path("docs/软件操作说明.md"); SHOTS=Path("docs/software-copyright/screenshots")
SHOT_MAP={
"## 3. 项目管理":"09_项目_本地管理.png",
"## 6. 数据检查":"02_数据_质量检查.png",
"## 7. 首页":"01_首页_长期稳定性总览.png",
"## 8. 寿命分析":"03_分析_T90寿命指标.png",
"## 9. 实验条件检查":"11_分析_条件差异阻止比较.png",
"## 10. 同时间对比":"04_分析_同时间对比.png",
"## 11. 实验建议":"05_分析_实验建议.png",
"## 12. 资料整理":"07_资料_关联与确认.png",
"## 13. PDF 报告":"08_AI助手_资料准备.png",
"## 15. 本地文件与运行环境":"10_设置_软件信息.png",
}

def font(run,size=10.5,bold=False,name=BODY,color="000000"):
    run.font.name=name; run.font.size=Pt(size); run.font.bold=bold; run.font.color.rgb=RGBColor.from_string(color)
    rpr=run._element.get_or_add_rPr(); rf=rpr.rFonts
    if rf is None: rf=OxmlElement("w:rFonts"); rpr.append(rf)
    rf.set(qn("w:ascii"),name); rf.set(qn("w:hAnsi"),name); rf.set(qn("w:eastAsia"),BODY); rf.set(qn("w:cs"),name)

def shade(cell,fill="F2F2F2"):
    pr=cell._tc.get_or_add_tcPr(); sh=pr.find(qn("w:shd"))
    if sh is None: sh=OxmlElement("w:shd"); pr.append(sh)
    sh.set(qn("w:fill"),fill)

def margins(cell,top=80,start=95,bottom=80,end=95):
    pr=cell._tc.get_or_add_tcPr(); mar=pr.first_child_found_in("w:tcMar")
    if mar is None: mar=OxmlElement("w:tcMar"); pr.append(mar)
    for n,v in (("top",top),("start",start),("bottom",bottom),("end",end)):
        x=mar.find(qn(f"w:{n}"))
        if x is None: x=OxmlElement(f"w:{n}"); mar.append(x)
        x.set(qn("w:w"),str(v)); x.set(qn("w:type"),"dxa")

def cell_style(c,size=9.2,bold=False,center=False):
    c.vertical_alignment=WD_CELL_VERTICAL_ALIGNMENT.CENTER; margins(c)
    for p in c.paragraphs:
        p.alignment=WD_ALIGN_PARAGRAPH.CENTER if center else WD_ALIGN_PARAGRAPH.LEFT
        p.paragraph_format.space_before=Pt(0); p.paragraph_format.space_after=Pt(0); p.paragraph_format.line_spacing=1.1
        for r in p.runs: font(r,size,bold)

def field(run,code):
    a=OxmlElement("w:fldChar"); a.set(qn("w:fldCharType"),"begin")
    b=OxmlElement("w:instrText"); b.set(qn("xml:space"),"preserve"); b.text=code
    c=OxmlElement("w:fldChar"); c.set(qn("w:fldCharType"),"end")
    run._r.extend([a,b,c])

def setup(doc):
    s=doc.sections[0]; s.page_width=Cm(21); s.page_height=Cm(29.7); s.top_margin=Cm(1.8); s.bottom_margin=Cm(1.8); s.left_margin=Cm(2); s.right_margin=Cm(2)
    st=doc.styles["Normal"]; st.font.name=BODY; st.font.size=Pt(10.5); st._element.rPr.rFonts.set(qn("w:eastAsia"),BODY); st.paragraph_format.line_spacing=1.35
    hp=s.header.paragraphs[0]; hp.alignment=WD_ALIGN_PARAGRAPH.RIGHT; font(hp.add_run(f"{SHORT} {VER}"),8.2,color="666666")
    fp=s.footer.paragraphs[0]; fp.alignment=WD_ALIGN_PARAGRAPH.CENTER; r=fp.add_run("第 "); font(r,8.2,color="666666"); field(r,"PAGE"); font(fp.add_run(" 页"),8.2,color="666666")

def pb(doc): doc.add_paragraph().add_run().add_break(WD_BREAK.PAGE)
def h(doc,text,size=16):
    p=doc.add_paragraph(); p.paragraph_format.space_before=Pt(8); p.paragraph_format.space_after=Pt(6); font(p.add_run(text),size,True)
def body(doc,text,indent=True):
    p=doc.add_paragraph(); p.paragraph_format.space_after=Pt(5); p.paragraph_format.line_spacing=1.35
    if indent: p.paragraph_format.first_line_indent=Pt(21)
    font(p.add_run(text),10.5)
def bullet(doc,text):
    p=doc.add_paragraph(); p.paragraph_format.left_indent=Cm(.55); p.paragraph_format.first_line_indent=Cm(-.3); p.paragraph_format.space_after=Pt(3); font(p.add_run("• "),10.5); font(p.add_run(text),10.5)
def cover(doc,title,sub):
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER; p.paragraph_format.space_before=Pt(105); font(p.add_run(title),24,True)
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER; p.paragraph_format.space_after=Pt(30); font(p.add_run(sub),15,True)
def picture(doc,path,caption):
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER; p.add_run().add_picture(str(path),width=Inches(6.45))
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER; p.paragraph_format.space_after=Pt(6); font(p.add_run(caption),9,color="444444")

def source_order(files):
    def k(p): return (0,"",0) if p.name=="main.cpp" else (1,p.stem.lower(),0 if p.suffix==".h" else 1)
    return sorted(files,key=k)

def collect(repo):
    rows=[]; names=[]
    for p in source_order([x for x in (repo/SRC).iterdir() if x.suffix in {".cpp",".h"}]):
        rel=p.relative_to(repo).as_posix(); names.append(rel)
        for n,line in enumerate(p.read_text(encoding="utf-8-sig",errors="replace").splitlines(),1):
            if line.strip(): rows.append((rel,n,line.rstrip()))
    return rows,names

def wlen(t): return sum(.6 if ord(ch)<128 else 1 for ch in t) or 1
def csize(t): return max(3.6,min(7.0,515/wlen(t)))

def build_source(repo,out,meta):
    rows,names=collect(repo); block=1500
    selected=rows[:block]+rows[-block:] if len(rows)>=3000 else rows; mode="前30页 + 后30页" if len(rows)>=3000 else "全部源程序"; pages=math.ceil(len(selected)/50)
    d=Document(); s=d.sections[0]; s.page_width=Cm(21); s.page_height=Cm(29.7); s.top_margin=Cm(1.25); s.bottom_margin=Cm(1.2); s.left_margin=Cm(1.15); s.right_margin=Cm(1.15)
    hp=s.header.paragraphs[0]; hp.alignment=WD_ALIGN_PARAGRAPH.CENTER; font(hp.add_run(f"{FULL} {VER}  源程序鉴别材料"),8.1,True)
    fp=s.footer.paragraphs[0]; fp.alignment=WD_ALIGN_PARAGRAPH.CENTER; r=fp.add_run("第 "); font(r,8); field(r,"PAGE"); font(fp.add_run(f" 页 / 共 {pages} 页"),8)
    for pi in range(pages):
        ch=selected[pi*50:(pi+1)*50]; first,last=ch[0],ch[-1]
        p=d.add_paragraph(); p.paragraph_format.space_after=Pt(3); txt=f"代码范围：{first[0]} L{first[1]}"+(f" - L{last[1]}" if first[0]==last[0] else f" → {last[0]} L{last[1]}"); font(p.add_run(txt),7.2,True)
        for i,(_,_,line) in enumerate(ch,1):
            p=d.add_paragraph(); p.paragraph_format.space_before=Pt(0); p.paragraph_format.space_after=Pt(0); p.paragraph_format.line_spacing_rule=WD_LINE_SPACING.EXACTLY; p.paragraph_format.line_spacing=Pt(10)
            font(p.add_run(f"{pi*50+i:04d}  "),6.1,name=CODE,color="333333"); font(p.add_run(line.replace("\t","    ")),csize(line),name=CODE)
        if pi<pages-1: pb(d)
    d.core_properties.author=""; d.core_properties.last_modified_by=""; d.core_properties.title=f"{FULL} {VER} 源程序鉴别材料"; out.parent.mkdir(parents=True,exist_ok=True); d.save(out)
    meta.write_text("\n".join([f"软件全称：{FULL}",f"软件简称：{SHORT}",f"版本号：{VER}",f"交存方式：{mode}",f"源程序非空行总数：{len(rows)}",f"本鉴别材料页数：{pages}","每页源程序行数：50","第一方源码：native/qt/src 的 .cpp/.h","第三方依赖源码未纳入。","源文件顺序：",*[f"- {n}" for n in names]]),encoding="utf-8")

def add_md_inline(p,text):
    parts=re.split(r"(\*\*.*?\*\*)",text)
    for part in parts:
        if not part: continue
        if part.startswith("**") and part.endswith("**"): font(p.add_run(part[2:-2]),10.5,True)
        else: font(p.add_run(part),10.5)

def build_manual(repo,out):
    md=(repo/MANUAL_MD).read_text(encoding="utf-8"); d=Document(); setup(d); cover(d,FULL,f"软件操作说明书  {VER}")
    t=d.add_table(rows=5,cols=2); t.alignment=WD_TABLE_ALIGNMENT.CENTER
    for i,(a,b) in enumerate([("软件简称",SHORT),("版本号",VER),("软件类型","Windows 原生桌面应用"),("核心技术","C++20、Qt 6、SQLite"),("项目格式",".clrproj（SQLite）")]):
        t.cell(i,0).text=a; t.cell(i,1).text=b; shade(t.cell(i,0)); cell_style(t.cell(i,0),bold=True); cell_style(t.cell(i,1))
    pb(d); h(d,"目录")
    for line in md.splitlines():
        if line.startswith("## "): bullet(d,line[3:])
    pb(d)
    figure=1; in_table=False; table=None
    for raw in md.splitlines():
        line=raw.rstrip()
        if line.startswith("# "): continue
        if line in SHOT_MAP and (repo/SHOTS/SHOT_MAP[line]).exists():
            h(d,line[3:] if line.startswith("## ") else line); picture(d,repo/SHOTS/SHOT_MAP[line],f"图 {figure}  {line[3:]}（Windows Qt 实际运行截图）"); figure+=1; in_table=False; continue
        if line.startswith("## "): h(d,line[3:]); in_table=False; continue
        if line.startswith("### "): h(d,line[4:],13); in_table=False; continue
        if re.match(r"^\|.*\|$",line):
            vals=[x.strip() for x in line.strip("|").split("|")]
            if all(re.fullmatch(r":?-+:?",x.replace(" ","")) for x in vals): continue
            if not in_table:
                table=d.add_table(rows=1,cols=len(vals)); table.alignment=WD_TABLE_ALIGNMENT.CENTER; in_table=True
                for i,v in enumerate(vals): table.cell(0,i).text=v; shade(table.cell(0,i),"EDEDED"); cell_style(table.cell(0,i),bold=True,center=True)
            else:
                cells=table.add_row().cells
                for i,v in enumerate(vals): cells[i].text=v; cell_style(cells[i],9)
            continue
        in_table=False
        if not line: continue
        if line.startswith("- "): bullet(d,line[2:]); continue
        if re.match(r"^\d+\. ",line): bullet(d,re.sub(r"^\d+\. ","",line)); continue
        p=d.add_paragraph(); p.paragraph_format.space_after=Pt(5); p.paragraph_format.line_spacing=1.35; p.paragraph_format.first_line_indent=Pt(21); add_md_inline(p,line)
    pb(d); h(d,"附录 A  V1.0 申报一致性")
    for x in [f"软件全称：{FULL}",f"软件简称：{SHORT}",f"版本号：{VER}","开发方式：个人独立开发","核心技术：C++20、Qt 6、SQLite","第三方依赖（Qt、QXlsx、SQLite 等）不作为自主源程序申报内容。"]: bullet(d,x)
    d.core_properties.author=""; d.core_properties.last_modified_by=""; d.core_properties.title=f"{FULL} {VER} 软件操作说明书"; out.parent.mkdir(parents=True,exist_ok=True); d.save(out)

def row(tab,vals,head=False):
    cells=tab.add_row().cells
    for i,v in enumerate(vals): cells[i].text=v; cell_style(cells[i],9.1,bold=head,center=(i==2 if len(vals)>2 else False))
    return cells

def build_check(repo,out,meta):
    d=Document(); setup(d); cover(d,"软件著作权申请信息核对表",f"{FULL}  {VER}"); body(d,"本表按“个人独立开发、原始取得、全部权利”口径整理。姓名、身份证件、地址、电话和邮箱等个人敏感信息由申请人本人填写，本文件不预填。",False)
    pb(d); h(d,"一、软件基本信息"); t=d.add_table(rows=1,cols=4); t.alignment=WD_TABLE_ALIGNMENT.CENTER
    for i,v in enumerate(("核对项","建议填写","状态","说明")): t.cell(0,i).text=v; shade(t.cell(0,i),"EDEDED"); cell_style(t.cell(0,i),9.2,True,True)
    for vals in [("软件全称",FULL,"已确定","与程序、说明书、源程序材料一致"),("软件简称",SHORT,"已确定","对外产品简称"),("版本号",VER,"已确定","V1.0 申报版本"),("开发方式","独立开发","已确定","申请人已确认个人独立完成"),("权利取得方式","原始取得","已确定","非受让/继承"),("权利范围","全部权利","已确定","无共同著作权人"),("开发完成日期","【申请人确认】","待确认","以 V1.0 实际完成/冻结日为准"),("发表状态","【申请人确认】","待确认","必须与 GitHub 等公开事实一致"),("首次发表日期/地点","【如已发表则填写】","待确认","仅已发表时填写")]: row(t,vals)
    pb(d); h(d,"二、著作权人及身份信息"); body(d,"著作权人为申请人个人；开发方式为独立开发，权利取得方式为原始取得。以下个人信息只由申请人本人填写。",False)
    t=d.add_table(rows=1,cols=3); t.alignment=WD_TABLE_ALIGNMENT.CENTER
    for i,v in enumerate(("字段","填写内容","提交前核对")): t.cell(0,i).text=v; shade(t.cell(0,i),"EDEDED"); cell_style(t.cell(0,i),9.2,True,True)
    for a,b in [("姓名","与身份证完全一致"),("证件类型/号码","核对证件号码"),("通讯地址","可正常接收通知"),("邮政编码","与地址匹配"),("手机号码","登记期间可联系"),("电子邮箱","接收登记通知"),("身份证明文件","按登记系统要求上传清晰有效材料")]: row(t,(a,"【申请人填写】",b))
    pb(d); h(d,"三、技术与功能信息")
    t=d.add_table(rows=1,cols=3); t.alignment=WD_TABLE_ALIGNMENT.CENTER
    for i,v in enumerate(("项目","建议填写","依据")): t.cell(0,i).text=v; shade(t.cell(0,i),"EDEDED"); cell_style(t.cell(0,i),9.2,True,True)
    for vals in [("开发语言","C++20","native/qt 第一方源程序"),("GUI 框架","Qt 6 Widgets","Windows 原生界面"),("本地数据库","SQLite",".clrproj 项目存储"),("Excel 读取","QXlsx","第三方依赖"),("PDF 读取","Qt PDF","资料读取"),("运行平台","Windows 10/11 x64","V1.0 发布平台"),("数据输入","CSV、Excel .xlsx、内置演示数据","数据模块"),("资料输入","PDF、TXT、Markdown、CSV、TSV","资料模块"),("报告输出","PDF","分析归档")]: row(t,vals)
    h(d,"建议功能简介",13); body(d,"本软件用于催化剂长期稳定性实验数据的导入、质量检查和寿命分析，可计算保持率及 T95/T90/T80，检查不同催化剂实验条件是否可比，并在共同实测时间进行性能比较。系统可根据寿命区间、测试时长、采样间隔和条件完整度生成后续实验建议，同时支持资料整理、人工确认、本地项目保存及 PDF 报告输出。",False)
    pb(d); h(d,"四、鉴别材料核对"); body(d,"依据国家版权局《计算机软件著作权登记办法》，软件鉴别材料包括程序和文档；原则上由源程序和任何一种文档前、后各连续 30 页组成，整个程序或文档不足 60 页时提交全部，除特定情况外程序每页不少于 50 行、文档每页不少于 30 行。",False)
    t=d.add_table(rows=1,cols=4); t.alignment=WD_TABLE_ALIGNMENT.CENTER
    for i,v in enumerate(("材料","当前文件","状态","核对")): t.cell(0,i).text=v; shade(t.cell(0,i),"EDEDED"); cell_style(t.cell(0,i),9.2,True,True)
    for vals in [("源程序鉴别材料",f"{SHORT}_{VER}_源程序鉴别材料.docx","已生成","只含第一方 C++/H 源码"),("软件操作说明书",f"{SHORT}_{VER}_软件操作说明书.docx","已生成","与实际 V1.0 功能一致"),("界面材料",f"{SHORT}_{VER}_软件著作权界面材料.docx","已生成","11 张 Windows Qt 实机截图"),("身份证明","个人身份证明材料","待准备","申请人本人准备"),("在线申请表","登记系统填写","待填写","名称/版本/权利人必须一致")]: row(t,vals)
    txt=meta.read_text(encoding="utf-8",errors="replace") if meta.exists() else ""; m=re.search(r"源程序非空行总数：(\d+)",txt); p=re.search(r"本鉴别材料页数：(\d+)",txt)
    if m and p: body(d,f"本次第一方源程序共 {m.group(1)} 个非空源程序行，生成源程序鉴别材料 {p.group(1)} 页，每页 50 行。",False)
    pb(d); h(d,"五、发表状态专项核对")
    for x in ["当前项目存在公开 GitHub 仓库。公开访问代码、软件页面或安装包可能影响“软件是否已向公众发表”的事实判断。","正式申请时应按实际公开情况选择“已发表/未发表”，不要机械选择“未发表”。","如选择“已发表”，首次发表日期和地点应与可核实的公开记录保持一致。","发表与否不会使著作权消失，但申请表内容必须与事实一致。"]: body(d,x,False)
    for x in ["□ 确认 GitHub 仓库第一次公开时间。","□ 确认是否曾公开发布安装包/便携包。","□ 确认是否曾在网页、论文附件、比赛平台或网盘公开软件。","□ 根据实际情况确认发表状态和日期/地点。"]: bullet(d,x)
    pb(d); h(d,"六、最终提交前检查")
    checks=["软件全称、简称、版本号在全部材料中完全一致。","著作权人填个人，开发方式填独立开发，权利取得填原始取得，权利范围填全部权利。","个人身份与联系信息本人核对。","开发完成日期与 V1.0 实际完成事实一致。","发表状态与 GitHub/安装包公开情况一致。","源程序只含第一方代码，不使用 Qt、QXlsx、SQLite 等第三方源码凑页。","源程序前30页和后30页连续，每页50行。","说明书和界面材料均对应实际 Windows Qt 程序。","材料中无 API Key、密码、隐私或调试日志。","内置演示数据明确为模拟数据。"]
    for x in checks: bullet(d,"□ "+x)
    d.core_properties.author=""; d.core_properties.last_modified_by=""; d.core_properties.title=f"{FULL} {VER} 软件著作权申请信息核对表"; out.parent.mkdir(parents=True,exist_ok=True); d.save(out)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--repo-root",type=Path,default=Path(".")); ap.add_argument("--output-dir",type=Path,required=True); ap.add_argument("--source-sha",default=os.environ.get("GITHUB_SHA","")); a=ap.parse_args(); repo=a.repo_root.resolve(); out=a.output_dir.resolve(); out.mkdir(parents=True,exist_ok=True)
    meta=out/f"{SHORT}_{VER}_源程序生成记录.txt"; build_source(repo,out/f"{SHORT}_{VER}_源程序鉴别材料.docx",meta); build_manual(repo,out/f"{SHORT}_{VER}_软件操作说明书.docx"); build_check(repo,out/f"{SHORT}_{VER}_软件著作权申请信息核对表.docx",meta)
    (out/"README_申报材料说明.md").write_text(f"# {SHORT} {VER} 软件著作权正式材料包\n\n- 软件全称：{FULL}\n- 软件简称：{SHORT}\n- 版本号：{VER}\n- 权属口径：个人独立开发 / 原始取得 / 全部权利\n- 源代码快照：`{a.source_sha or '未提供'}`\n\n提交前仍需申请人本人确认个人身份信息、开发完成日期与发表状态。\n",encoding="utf-8")
    z=out/f"{SHORT}_{VER}_软件著作权正式材料包.zip"
    with zipfile.ZipFile(z,"w",zipfile.ZIP_DEFLATED) as zf:
        for p in sorted(out.iterdir()):
            if p.is_file() and p!=z: zf.write(p,p.name)
    for p in sorted(out.iterdir()): print(p.name,p.stat().st_size)
if __name__=="__main__": main()
