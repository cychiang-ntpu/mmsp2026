# C 語言錯誤訊息急救手冊

編譯失敗不可怕，可怕的是看不懂錯誤訊息。
這份手冊收集新手最常遇到的錯誤，教你怎麼讀、怎麼修。

## 先學會讀錯誤訊息的格式

```
hello.c:5:20: error: expected ';' before 'return'
```

從左到右讀：**檔名 : 行號 : 第幾個字 : 等級 : 說明**

- `hello.c` → 哪個檔案有問題
- `5` → **第 5 行**（最重要！先跳到這一行附近看）
- `error` → 錯誤，一定要修，否則編不出執行檔
- `warning` → 警告，能編譯但很可能是 bug，建議也要修

> 兩個黃金原則：
> 1. **永遠先修第一個 error**。後面的錯誤常常是第一個引起的連鎖反應，
>    修好第一個重編，可能一次消掉一大串。
> 2. 錯誤在第 N 行，問題常在 **N 或 N 的前一行**（例如上一行忘了分號）。

---

## 錯誤 1：`expected ';' before ...`（忘記分號）

```c
int x = 10        /* ← 這行忘了分號 */
printf("%d\n", x);
```

```
error: expected ';' before 'printf'
```

**修法**：錯誤指到 `printf` 那行，但真正的問題在**上一行**結尾少了 `;`。

---

## 錯誤 2：`expected declaration or statement at end of input`（大括號沒配對）

```c
int main(void)
{
    if (1) {
        printf("hi\n");
    return 0;
}          /* ← if 的 } 忘了寫，編譯器讀到檔案結尾發現少一個 */
```

**修法**：`{` 和 `}` 數量不相等。善用 VSCode：點一個大括號，
它會把配對的那一個亮起來；存檔前按 `Shift+Alt+F` 自動排版，
縮排亂掉的地方通常就是括號漏掉的地方。

---

## 錯誤 3：`'xxx' undeclared`（變數沒宣告或打錯字）

```c
int count = 5;
printf("%d\n", conut);   /* ← 打錯字：conut */
```

```
error: 'conut' undeclared (first use in this function)
```

**修法**：九成是打錯字（拼字、大小寫）。C 語言 `Count` 和 `count` 是不同變數。
另一種可能：變數宣告在別的大括號範圍裡（scope），外面看不到。

---

## 錯誤 4：`implicit declaration of function 'xxx'`（忘了 include）

```c
int main(void)
{                          /* ← 檔案開頭忘了 #include <stdio.h> */
    printf("hi\n");
}
```

```
warning: implicit declaration of function 'printf'
```

**修法**：用到的函式要 include 對應的標頭檔：
`printf/scanf` → `<stdio.h>`、`strlen/strcpy` → `<string.h>`、
`malloc/rand` → `<stdlib.h>`、`sqrt/pow` → `<math.h>`。

---

## 錯誤 5：`undefined reference to 'xxx'`（連結錯誤）

這種錯誤**沒有行號**，長相也不同——它不是語法錯，而是「找不到函式的實體」：

```
undefined reference to 'add'
undefined reference to 'WSAStartup'
```

**常見原因與修法**：
- 函式只有宣告沒有定義（只寫了 `int add(int, int);` 沒寫函式內容），或函式名稱打錯。
- 少連結函式庫：數學函式要加 `-lm`（Linux），
  網路程式在 Windows 要加 `-lws2_32`（我們的 chat.c 就是！），
  執行緒在 Linux 要加 `-lpthread`。函式庫參數要放在**指令最後面**：

  ```
  gcc chat.c -o chat.exe -lws2_32
  ```

- `undefined reference to 'main'`：你根本沒寫 `main` 函式，或編錯檔案了。

---

## 錯誤 6：`format '%d' expects argument of type 'int'`（printf 格式不符）

```c
double pi = 3.14;
printf("%d\n", pi);   /* ← %d 是整數用的，pi 是 double */
```

**修法**：格式化字元要和變數型別對應：
`int` 用 `%d`、`double` 用 `%f`、`char` 用 `%c`、字串用 `%s`。
印出來是 0 或亂數，常常就是這個原因。

---

## 錯誤 7：`warning: comparison ... assignment`（把 == 寫成 =）

```c
if (x = 5) { ... }    /* ← 這是「指定」，永遠成立！應該用 == */
```

```
warning: suggest parentheses around assignment used as truth value
```

**修法**：比較用 `==`，指定用 `=`。這是 warning 不是 error，
程式編得過但邏輯全錯——這就是為什麼 warning 也要看。

---

## 執行時期的錯誤（編譯過了，跑起來卻掛掉）

### 程式閃退 / Segmentation fault

編譯都沒錯，執行時卻當掉，常見原因：

```c
int arr[5];
arr[10] = 1;              /* 陣列只有 0~4，寫到 10 越界了 */

scanf("%d", x);           /* scanf 忘了 & ，應該是 &x */

char *p = NULL;
*p = 'a';                 /* 對 NULL 指標取值 */
```

**抓法**：用 debugger！按 F5 執行，程式會**停在當掉的那一行**，
直接看是哪個變數出問題（詳見《VSCode 除錯教學》）。

### 無窮迴圈（程式跑不停）

```c
int i = 0;
while (i < 10) {
    printf("%d\n", i);    /* 忘了 i++，i 永遠是 0 */
}
```

**急救**：在終端機按 `Ctrl+C` 強制停止程式，再回頭檢查迴圈變數有沒有更新。

---

## 卡住超過 15 分鐘怎麼辦

1. 把**第一個** error 訊息**完整**複製起來（不要只截一半）。
2. 貼到搜尋引擎查，或連同出錯的那段程式碼一起問助教／同學。
3. 問問題的好格式：「我想做 ○○，我寫了這段程式（附碼），
   出現這個錯誤（附完整訊息），我試過 ○○ 但沒用。」
   這樣別人才幫得了你。
