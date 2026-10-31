#!/usr/bin/env python3
"""把課程的 Markdown 印成 A4 PDF（給紙本講義、考卷用）。

用法：python tools/md2pdf.py in.md out.pdf [--title 文字] [--no-links]
需要：pip install markdown；以及 Chrome 或 Edge（用它的 headless 模式把 HTML 印成 PDF）。
--no-links：把「（wk03 1.6）」這類指向講義的括號與超連結文字留著、但不印成藍色連結（考卷用）。
"""
import argparse, os, re, shutil, subprocess, sys, tempfile

import markdown

CSS = """
@page { size: A4; margin: 15mm 15mm 15mm 15mm; }
html { font-family: "Noto Sans CJK TC", "Microsoft JhengHei", "PingFang TC", "Helvetica Neue", Arial, sans-serif; font-size: 10pt; line-height: 1.45; color: #111; }
body { max-width: none; }
h1 { font-size: 17pt; margin: 0 0 6pt; border-bottom: 2px solid #333; padding-bottom: 4pt; }
h2 { font-size: 13.5pt; margin: 14pt 0 6pt; border-bottom: 1px solid #999; page-break-after: avoid; }
h3 { font-size: 11.5pt; margin: 10pt 0 4pt; page-break-after: avoid; }
p { margin: 4pt 0; }
ol, ul { margin: 4pt 0 4pt 1.4em; padding: 0; }
li { margin: 3pt 0; page-break-inside: avoid; }
table { border-collapse: collapse; margin: 6pt 0; font-size: 9.5pt; page-break-inside: avoid; }
th, td { border: 1px solid #888; padding: 3pt 6pt; vertical-align: top; }
th { background: #eee; }
code { font-family: "Cascadia Mono", "Consolas", "Noto Sans Mono CJK TC", monospace; font-size: 9.5pt; background: #f3f3f3; padding: 0 2pt; }
pre { font-family: "Cascadia Mono", "Consolas", "Noto Sans Mono CJK TC", monospace; font-size: 8.8pt; background: #f6f6f6; border: 1px solid #ccc; padding: 6pt; white-space: pre; overflow: visible; page-break-inside: avoid; }
pre code { background: none; padding: 0; font-size: inherit; }
hr { border: 0; border-top: 1px solid #999; margin: 10pt 0; }
a { color: inherit; text-decoration: none; }
blockquote { margin: 6pt 0; padding: 2pt 10pt; border-left: 3px solid #999; color: #333; }
strong { font-weight: 700; }
.footer { position: fixed; bottom: -8mm; right: 0; font-size: 8pt; color: #666; }
"""


def find_browser():
    for c in ("chrome", "msedge", "google-chrome", "chromium"):
        p = shutil.which(c)
        if p:
            return p
    for p in (r"C:\Program Files\Google\Chrome\Application\chrome.exe",
              r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
              r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
              r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
              "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
              "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"):
        if os.path.exists(p):
            return p
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src"); ap.add_argument("dst")
    ap.add_argument("--title", default=None)
    ap.add_argument("--no-links", action="store_true")
    a = ap.parse_args()
    text = open(a.src, encoding="utf-8").read()
    if a.no_links:
        text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", text)          # [text](url) → text
    body = markdown.markdown(text, extensions=["tables", "fenced_code", "sane_lists"])
    title = a.title or re.search(r"<h1[^>]*>(.*?)</h1>", body, re.S)
    title = title if isinstance(title, str) else (title.group(1) if title else os.path.basename(a.src))
    html = f"<!doctype html><html lang='zh-Hant'><head><meta charset='utf-8'><title>{title}</title><style>{CSS}</style></head><body>{body}</body></html>"
    tmpdir = tempfile.mkdtemp()
    src_html = os.path.join(tmpdir, "page.html")
    open(src_html, "w", encoding="utf-8").write(html)
    browser = find_browser()
    if not browser:
        sys.exit("找不到 Chrome 或 Edge；請安裝其中一個，或把 page.html 用瀏覽器開啟後自己列印：" + src_html)
    dst = os.path.abspath(a.dst)
    cmd = [browser, "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--no-first-run", "--no-default-browser-check",
           f"--user-data-dir={os.path.join(tmpdir, 'profile')}", f"--print-to-pdf={dst}", "file:///" + src_html.replace("\\", "/")]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if not os.path.exists(dst):
        sys.exit("列印失敗：\n" + r.stderr[-800:])
    print(f"{dst}  ({os.path.getsize(dst):,} bytes)")


if __name__ == "__main__":
    main()
