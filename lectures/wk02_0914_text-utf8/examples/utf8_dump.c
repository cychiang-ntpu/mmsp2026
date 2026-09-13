/* utf8_dump.c — 逐字元印出 UTF-8 的 bytes、code point 與長度（MP1 的第一塊積木）
 * 用法：  macOS/Linux:  ./utf8_dump < ../data/sample_zh_en.txt
 *         Windows cmd:  .\utf8_dump.exe < ..\data\sample_zh_en.txt
 *         PowerShell:   cmd /c ".\utf8_dump.exe < ..\data\sample_zh_en.txt"   （PowerShell 不支援 < 重導向）
 * 規則：前導 byte 0xxxxxxx=1 byte、110xxxxx=2、1110xxxx=3、11110xxx=4；續位元組一律 10xxxxxx
 * 非法序列（前導 byte 後面不是 10xxxxxx、檔案提早結束、overlong、代理區、超過 U+10FFFF）：
 * 只把前導 byte 當 1 byte 印出，後面的 byte 留給下一輪重新判斷。
 * 「非法就退一個 byte」與去年 MP1 滿分程式相同；RFC 3629 的三條禁令是它沒做的。 */
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
  #include <windows.h>
#endif

static int utf8_len(unsigned char b0) {
    if ((b0 & 0x80) == 0x00) return 1;
    if ((b0 & 0xE0) == 0xC0) return 2;
    if ((b0 & 0xF0) == 0xE0) return 3;
    if ((b0 & 0xF8) == 0xF0) return 4;
    return 0;                       /* 不是合法前導 byte（例如落單的續位元組） */
}

int main(void) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY); /* Windows 預設會把 \r\n 讀成 \n，改成 binary 才看得到 \r */
    SetConsoleOutputCP(65001);           /* 終端機用 UTF-8 顯示中文（等同 chcp 65001） */
#endif
    /* 先把整個輸入讀進記憶體，之後用索引往前看，不必 ungetc */
    size_t cap = 1 << 16, n = 0, got;
    unsigned char *buf = malloc(cap);
    while ((got = fread(buf + n, 1, cap - n, stdin)) > 0) {
        n += got;
        if (n == cap) buf = realloc(buf, cap *= 2);
    }

    for (size_t pos = 0, idx = 0; pos < n; idx++) {
        int len = utf8_len(buf[pos]), ok = (len > 0);
        for (int i = 1; ok && i < len; i++)                 /* 檢查續位元組都在且都是 10xxxxxx */
            if (pos + i >= n || (buf[pos + i] & 0xC0) != 0x80) ok = 0;
        if (!ok) len = 1;                                    /* 非法：只吃前導 byte */

        unsigned cp = buf[pos];
        if (ok && len > 1) {                                 /* 拼出 code point：去掉前導標記位元再串接 */
            cp = buf[pos] & (0xFF >> (len + 1));
            for (int i = 1; i < len; i++) cp = (cp << 6) | (buf[pos + i] & 0x3F);
            /* RFC 3629 的三條禁令：overlong（用太多 bytes 表示小編號）、UTF-16 代理區、超過 U+10FFFF */
            static const unsigned min_cp[5] = { 0, 0, 0x80, 0x800, 0x10000 };
            if (cp < min_cp[len] || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) { ok = 0; len = 1; cp = buf[pos]; }
        }
        printf("#%-4zu %d byte%s  hex:", idx, len, len > 1 ? "s" : " ");
        for (int i = 0; i < len; i++) printf(" %02X", buf[pos + i]);
        printf("%*s  U+%04X  ", 3 * (4 - len), "", cp);
        if (cp == '\n') puts("'\\n'"); else if (cp == '\r') puts("'\\r'");
        else if (cp == '\t') puts("'\\t'");
        else if (cp == 0xFEFF && idx == 0) puts("(BOM)");       /* 檔頭的 U+FEFF 是標記不是內容 */
        else { fwrite(buf + pos, 1, len, stdout); puts(""); }
        pos += len;
    }
    free(buf);
    return 0;
}
