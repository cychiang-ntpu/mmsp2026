# C 語言速查表（新手版）

寫作業時放旁邊查。每段都是可以直接抄去改的最小範例。

## 程式的基本骨架

```c
#include <stdio.h>      /* 要用 printf/scanf 就要這行 */

int main(void)          /* 程式從 main 開始執行 */
{
    printf("Hello!\n");
    return 0;           /* 0 代表正常結束 */
}
```

每個敘述句結尾要 `;`，程式區塊用 `{ }` 包起來。

---

## 變數與型別

```c
int    age    = 20;       /* 整數 */
double height = 172.5;    /* 小數（浮點數） */
char   grade  = 'A';      /* 單一字元，用單引號 */
char   name[20] = "Amy";  /* 字串 = 字元陣列，用雙引號 */
```

| 型別 | 存什麼 | printf/scanf 格式 |
|------|------|------|
| `int` | 整數 | `%d` |
| `double` | 小數 | printf 用 `%f`，scanf 用 `%lf` |
| `char` | 一個字元 | `%c` |
| `char[]` | 字串 | `%s` |

---

## 輸出 printf 與輸入 scanf

```c
int age;
double h;
char name[20];

printf("請輸入名字與年齡：");
scanf("%s %d", name, &age);      /* 注意：一般變數要加 &，字串不用 */
scanf("%lf", &h);                /* double 在 scanf 要用 %lf */

printf("%s 今年 %d 歲，身高 %.1f\n", name, age, h);
```

常用技巧：`\n` 換行、`\t` 定位、`%.2f` 小數點後兩位、`%5d` 靠右對齊佔 5 格。

> 最常見的 bug：`scanf("%d", x)` 忘了 `&` → 程式閃退！

---

## 運算子

```c
int a = 7, b = 2;
a + b;   a - b;   a * b;
a / b;      /* 整數除整數 = 3（小數被丟掉！） */
a % b;      /* 餘數 = 1 */
7.0 / 2;    /* 有一邊是小數才會得到 3.5 */
a++;        /* a 加 1，等同 a = a + 1 */
a += 5;     /* 等同 a = a + 5 */
```

比較與邏輯：`==`（相等，**不是 =**）、`!=`、`<`、`<=`、`>`、`>=`、
`&&`（而且）、`||`（或者）、`!`（不是）。

---

## if / else 條件判斷

```c
if (score >= 90) {
    printf("優秀\n");
} else if (score >= 60) {
    printf("及格\n");
} else {
    printf("再加油\n");
}
```

## 迴圈

```c
/* for：知道要跑幾次 */
for (int i = 0; i < 5; i++) {
    printf("第 %d 次\n", i);       /* 印 0,1,2,3,4 共 5 次 */
}

/* while：條件成立就一直跑 */
int n = 0;
while (n < 3) {
    printf("n = %d\n", n);
    n++;                            /* 忘了這行就是無窮迴圈！ */
}
```

`break` 立刻跳出迴圈；`continue` 跳過這一圈剩下的部分，直接進下一圈。

---

## 函式

```c
/* 定義：回傳型別 名稱(參數) */
int add(int a, int b)
{
    return a + b;
}

/* 沒有回傳值就用 void */
void greet(void)
{
    printf("哈囉！\n");
}

int main(void)
{
    int sum = add(3, 5);    /* 呼叫，sum = 8 */
    greet();
    return 0;
}
```

函式定義要放在 main **前面**（或先在檔案開頭寫宣告 `int add(int, int);`）。

---

## 陣列與字串

```c
int scores[5] = {90, 85, 70, 60, 100};
scores[0];              /* 第一個元素是 [0] 不是 [1]！ */
scores[4];              /* 最後一個是 [4]（大小減 1） */

for (int i = 0; i < 5; i++) {       /* 走訪整個陣列 */
    printf("%d\n", scores[i]);
}
```

```c
#include <string.h>
char s[20] = "hello";
strlen(s);              /* 長度 = 5 */
strcpy(s, "world");     /* 字串「指定」要用 strcpy，不能 s = "world" */
strcmp(s, "world");     /* 比較：相等回傳 0（不能用 == 比字串！） */
```

---

## 新手十大地雷總整理

1. `=` 是指定、`==` 才是比較。`if (x = 5)` 永遠成立。
2. `scanf` 的一般變數要加 `&`：`scanf("%d", &x);`
3. 整數除整數會丟小數：`7 / 2` 是 `3`。
4. 陣列從 `[0]` 開始，大小 5 的最後一格是 `[4]`。
5. 字串比較用 `strcmp`，不能用 `==`。
6. `while` 迴圈裡忘了更新變數 → 無窮迴圈（按 `Ctrl+C` 停）。
7. `%d` 對 `int`、`%f` 對 `double`，配錯會印出亂數。
8. 每個敘述句結尾要 `;`，但 `if (...)` 和 `for (...)` 後面**不要**加 `;`。
9. 變數要初始化：`int sum = 0;` 沒給初始值就是垃圾值。
10. `char` 用單引號 `'A'`，字串用雙引號 `"A"`，不一樣！
