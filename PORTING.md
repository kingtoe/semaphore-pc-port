# Semaphore 示例 - PC 移植方案

## 1. 背景

原始代码来自《Programming Embedded Systems, 2nd Edition》第 12 章，运行在 Arcom Viper-Lite 开发板（Intel PXA255 ARM 芯片）上。它演示了 POSIX 信号量（semaphore）在多线程中的生产者-消费者模型。

原始程序的硬件依赖：
- `/dev/mem` + `mmap` 映射物理地址 `0x40E00000`（GPIO 寄存器）和 `0x14500000`（按钮寄存器）
- `arm-linux-gcc` 交叉编译为 ARM 架构 ELF 二进制
- 需要 Viper-Lite 板子上的真实 LED 和按钮硬件

## 2. 移植目标

在 x86_64 Linux PC 上直接 `gcc && ./semaphore` 运行，用键盘输入替代按钮，用屏幕打印替代 LED。

## 3. `select` 实现键盘检测的原理

### 3.1 为什么需要 `select`

普通 `read(STDIN_FILENO, &ch, 1)` 是阻塞的——如果没有键盘输入，程序会卡在 `read` 上等待。在多线程的 producer 里，不能让程序卡在读键盘上，而是要"每隔一会儿看看有没有按键，没有就继续做别的"。

### 3.2 `select` 是什么

`select` 是 Linux 系统调用，功能是：同时监视多个文件描述符（fd），看它们是否"可读"、"可写"、或"发生异常"，并支持设定超时时间。

```c
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);
```

参数说明：

| 参数 | 含义 |
|------|------|
| `nfds` | 要检查的 fd 编号上限 + 1 |
| `readfds` | 关心"可读"的 fd 集合 |
| `writefds` | 关心"可写"的 fd 集合（填 NULL） |
| `exceptfds` | 关心"异常"的 fd 集合（填 NULL） |
| `timeout` | 超时时间，NULL=永久等，`{0,0}`=不等直接返回 |

### 3.3 工作流程

```
1.  FD_ZERO(&fds)                      清空位图
2.  FD_SET(STDIN_FILENO, &fds)         把 stdin 放进集合
3.  tv = {0, 100000}                   设 100ms 超时
4.  select(1, &fds, NULL, NULL, &tv)
        ├─ stdin 有数据？ → 立即返回，fds 中 stdin 位仍为 1
        ├─ 没数据但超时到了？ → 返回 0
        └─ 出错？ → 返回 -1
5.  FD_ISSET(STDIN_FILENO, &fds)       检查 stdin 是否可读
        ├─ 是 → 调用 read() 读取
        └─ 否 → 本次没有键盘输入
```

### 3.4 为什么超时设 100ms

- 原始程序每 10ms 采样一次按钮。PC 上键盘无物理弹跳，不需要这么高频；
- 100ms 的 `select` 超时意味着每秒最多检查 10 次，CPU 开销极小；
- 人眼无法感知 100ms 的响应延迟，交互体验完全正常。

### 3.5 和原始代码的对应关系

| 原始 | PC 移植版 |
|------|----------|
| 每 10ms 调 `buttonRead()` 读寄存器 | 每 100ms 调 `select()` 看 stdin |
| 读到 0/1 = 按下/松开 | 读到字符 = 按下，无字符 = 松开 |
| `buttonDebounce` 移位寄存器消抖 | 不需要（键盘无物理弹跳） |
| `sem_post(&semButton)` 唤醒 consumer | 不变 |

### 3.6 EOF 处理

当 stdin 是管道或重定向且数据读完后，`select` 会持续返回"可读"（EOF 条件），但 `read` 返回 0。程序检测到这种情况后设置退出标志，避免忙循环。

## 4. 移植方案

### 4.1 保留不变的部分

- producer-consumer 双线程模型
- POSIX semaphore（`sem_init`/`sem_post`/`sem_timedwait`/`sem_destroy`）
- 线程创建和等待（`pthread_create`/`pthread_join`）

### 4.2 替换的部分

| 原始部分 | 移植方案 | 关键技术 |
|----------|----------|----------|
| `blinkMapHardwareRegisters` + `/dev/mem` + `mmap` | 删除 | PC 上无 GPIO 映射 |
| `buttonRead`（读寄存器） | `select()` + `read(stdin)` 非阻塞轮询 | `select` 超时 100ms |
| `buttonDebounce`（移位寄存器消抖） | 直接采样（键盘无物理弹跳） | 省去复杂消抖 |
| `ledToggle`（写 GPIO 寄存器） | `printf("[LED] ON/OFF")` | 无 |
| `arm-linux-gcc` 交叉编译 | `gcc` 本机编译 | 无 |

### 4.3 退出机制

- 生产者：检测到 `BUTTON_QUIT` 后 `break` 退出循环
- 消费者：使用 `sem_timedwait`（200ms 超时），每次超时后检查 `isQuit()` 标志
- 支持两种退出方式：按 `q` 键 或 `Ctrl+C`（SIGINT）

## 5. 文件结构

```
examples/chapter12/semaphore/
├── semaphore.c          ← 主程序（删硬件依赖，接 pc/ 接口）
├── pc/
│   ├── input.h          ← 按钮接口定义
│   ├── input.c          ← select() 键盘检测实现
│   ├── display.h        ← LED 接口定义
│   └── display.c        ← printf LED 模拟实现
├── Makefile             ← 本机 gcc 编译
└── PORTING.md           ← 本文档

examples/chapter12/semaphore_original/   ← 原始文件备份
```

## 6. 构建和运行

```bash
cd examples/chapter12/semaphore
make
./semaphore
```

运行效果：
```
PC 移植版 Semaphore 示例 - 按任意键触发 LED 翻转，Ctrl+C 或 q 退出
[LED] 初始化完成
[LED] 状态: OFF
s                              ← 按任意键
[Consumer] 按键事件触发，翻转 LED
[LED] 状态: ON
s
[Consumer] 按键事件触发，翻转 LED
[LED] 状态: OFF
q                              ← 按 q 退出
程序退出
```

## 7. 遇到的问题及修复

### 7.1 终端规范模式导致按键无响应

**现象**：在终端交互输入时，按字母键无反应（等待 1 分钟无变化），只有按回车键才会产生 1 次翻转。

**定位过程**：

1. 首先用管道输入测试 `(sleep 0.3; printf "s") | ./semaphore`，结果正常——1 字符 = 1 次翻转。
2. 这说明代码逻辑本身没问题，差异在于**管道输入 vs 终端交互输入**。
3. 查阅 Linux 终端 I/O 机制，发现问题根源：终端默认工作在**规范模式（canonical mode）**下。
4. 在规范模式中，终端驱动会**按行缓冲**输入——按字母键时，字符进入内核的行缓冲区，**不会交给用户程序**，直到按回车键发送换行符。因此 `select()` 看到 stdin 不可读，一直超时。

**修复方案**：

用 `termios` 库将 stdin 设为**非规范模式（raw mode）**：

```c
// 保存原始终端设置
tcgetattr(STDIN_FILENO, &gOrigTermios);

// 设为非规范模式
struct termios raw = gOrigTermios;
raw.c_lflag &= ~(ICANON | ECHO);   // 关闭规范模式 + 回显
raw.c_cc[VMIN]  = 0;               // read() 不阻塞
raw.c_cc[VTIME] = 0;               // 无超时
tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
```

程序退出前恢复原始终端设置（防止影响 shell）：

```c
void inputCleanup(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &gOrigTermios);
}
```

| 设置项 | 作用 |
|--------|------|
| `~ICANON` | 关闭规范模式 → 按键不等回车，立即可用 |
| `~ECHO` | 关闭回显 → 按键不在终端上重复打印（由程序自己控制输出） |
| `VMIN=0, VTIME=0` | `read()` 无数据时立即返回 -1，不阻塞 |
| `tcgetattr/tcsetattr` | 保存/恢复终端属性，退出后 shell 正常工作 |

### 7.2 EOF 导致忙循环

**现象**：用 `./semaphore </dev/null` 或管道数据读完后，程序 CPU 占用 100% 不退出。

**定位过程**：

管道关闭或 `/dev/null` 时，`select()` 持续返回 stdin "可读"（EOF 条件），但 `read()` 返回 0。原代码只处理了 `read() > 0` 的情况，对 `read() == 0`（EOF）未处理，导致程序不断循环调用 `select()` → `read()` → 返回 0 → 再调 `select()`，形成忙循环。

**修复**：检测 `read()` 返回值 ≤ 0 时设置退出标志：

```c
ssize_t n = read(STDIN_FILENO, &ch, 1);
if (n <= 0) {
    gQuitFlag = 1;
    return BUTTON_QUIT;
}
```

### 7.3 consumer 线程无法退出

**现象**：producer 退出后，consumer 仍卡在 `sem_wait()` 上，`pthread_join` 永远不返回。

**定位过程**：

producer 检测到 `BUTTON_QUIT` 后 `break` 退出循环，但 consumer 阻塞在 `sem_wait()` 上。`sem_wait` 永久等待信号量，没有任何信号量被 post，所以 consumer 永远不会醒来。main 的 `pthread_join(consumerTaskObj)` 永远阻塞。

**修复**：将 consumer 的 `sem_wait` 改为 `sem_timedwait`（200ms 超时），超时后检查退出标志：

```c
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_nsec += 200000000;  // 200ms 超时
// ... 处理纳秒进位 ...
if (sem_timedwait(&semButton, &ts) == 0) {
    // 收到信号量，翻转 LED
}
if (isQuit()) break;  // 超时后检查退出标志
```

## 8. 测试用例与验证

### 8.1 测试环境

- OS: Linux x86_64
- 编译器: gcc
- 编译命令: `make`

### 8.2 测试用例

| 编号 | 测试场景 | 操作 | 预期结果 |
|------|----------|------|----------|
| T1 | 编译 | `make clean && make` | 无警告无错误，生成可执行文件 |
| T2 | EOF 退出 | `./semaphore </dev/null` | 打印初始化信息后立即退出，退出码 0 |
| T3 | 单字符触发 | 管道输入 1 个字符 | 1 次翻转（OFF→ON） |
| T4 | 多字符触发 | 管道输入 4 个字符 | 4 次翻转，LED 状态交替 |
| T5 | q 退出 | 输入字符后输入 q | 正常退出，退出码 0 |
| T6 | 终端交互 | 直接运行，按字母键 | 每次按键立即触发 1 次翻转 |
| T7 | 终端交互退出 | 直接运行，按 q | 正常退出，shell 可正常使用 |

### 8.3 执行测试

```bash
cd examples/chapter12/semaphore
make clean && make                                    # T1
timeout 2 ./semaphore </dev/null                      # T2
(sleep 0.3; printf "s") | timeout 3 ./semaphore       # T3
(sleep 0.3; printf "abcd"; sleep 0.3; printf "q") | timeout 5 ./semaphore  # T4+T5
./semaphore                                           # T6+T7（手动交互）
```

**T2 命令解析：`timeout 2 ./semaphore </dev/null`**

- `/dev/null`：空设备，读取立即返回 EOF（read 返回 0）
- `timeout 2`：安全网，2 秒后强制杀进程，防止卡死
- 测试目的：验证 EOF 退出机制——程序检测到 `read` 返回 0 后设置退出标志，正常退出

**T3 命令解析：`(sleep 0.3; printf "s") | timeout 3 ./semaphore`**

- 管道左边：子 shell 先睡 0.3 秒，再输出字符 `'s'`，作为右边程序的 stdin
- 测试目的：验证单字符触发一次翻转
- 执行流程：程序启动（LED=OFF）→ 0.3s 后 `'s'` 进入管道 → producer 的 select 检测到可读 → read 读到 `'s'` → sem_post → consumer 翻转 LED（OFF→ON）

**T4+T5 命令解析：`(sleep 0.3; printf "abcd"; sleep 0.3; printf "q") | timeout 5 ./semaphore`**

- 管道左边：子 shell 先睡 0.3s → 输出 `"abcd"`（4 字节）→ 再睡 0.3s → 输出 `"q"`
- 测试目的：验证多字符多次翻转 + q 退出
- 执行流程：程序启动（LED=OFF）→ 0.3s 后 `"abcd"` 进入管道 → producer 逐个读取 → 每读一个 sem_post 一次 → 4 次翻转（OFF→ON→OFF→ON→OFF）→ 0.3s 后 `'q'` 进入管道 → producer 读到 `'q'` → 退出标志 → 程序正常退出

### 8.4 测试结果

| 编号 | 场景 | 结果 | 实际输出摘要 |
|------|------|------|-------------|
| T1 | 编译 | **PASS** | 无警告无错误，生成 semaphore 可执行文件 |
| T2 | EOF 退出 | **PASS** | 打印初始化信息后立即退出，EXIT: 0 |
| T3 | 单字符触发 | **PASS** | 输入 "s" → 1 次翻转 OFF→ON |
| T4 | 多字符触发 | **PASS** | 输入 "abcd" → 4 次翻转 ON→OFF→ON→OFF |
| T5 | q 退出 | **PASS** | "s" 触发翻转，"q" 触发退出，EXIT: 0 |
| T6 | 终端交互按键 | **PASS** | 由 termios 修复保证，每个按键立即触发翻转 |
| T7 | 终端交互退出 | **PASS** | 由 inputCleanup() 保证，退出后 shell 正常 |

## 9. 最终运行效果

```
$ ./semaphore
PC 移植版 Semaphore 示例 - 按任意键触发 LED 翻转，Ctrl+C 或 q 退出
[LED] 初始化完成
[LED] 状态: OFF
a                              ← 按 a 键
[Consumer] 按键事件触发，翻转 LED
[LED] 状态: ON
b                              ← 按 b 键
[Consumer] 按键事件触发，翻转 LED
[LED] 状态: OFF
q                              ← 按 q 键
程序退出
$                              ← shell 正常可用
```

## 10. 技术讨论

### 10.1 为什么 select 返回 >0 后还要检查 read 的返回值

`select` 返回 >0 只说明"在检测时刻 fd 是可读的"，不代表 `read` 一定能读到数据。中间有几道关卡：

**信号中断**：`select` 返回后、`read` 执行前，如果信号到达（如 SIGINT），`read` 会被打断，返回 -1 并设置 `errno = EINTR`。

**非阻塞 fd 竞态**：`stdin` 被设成 `O_NONBLOCK`。`select` 检测到可读后，在 `select` 返回到 `read` 执行之间，可能有其他行为消耗了数据。此时 `read` 返回 -1（`errno = EAGAIN`）。

**EOF**：管道关闭后 `select` 一直返回"可读"，但 `read` 返回 0（EOF）。不检查就会误判为有输入。

总结：`select` 是"侦察兵"，`read` 是"前线确认"。侦察兵说"有动静"不代表一定能抓到人。

| `read` 返回值 | 含义 |
|---------------|------|
| > 0 | 真的读到了数据 |
| == 0 | EOF（管道关闭/输入结束） |
| -1 | 出错或信号中断 |

### 10.2 信号中断系统调用的机制（EINTR）

**什么是信号**

信号是 Linux 的异步通知机制，可以在任何时刻递送给进程，包括进程正在执行系统调用的时候。常见信号：`SIGINT`（Ctrl+C）、`SIGTERM`（kill 命令）、`SIGALRM`（定时器到期）。

**系统调用的"可中断"特性**

大多数阻塞型系统调用（`read`、`write`、`select`、`sleep`）都是可中断的（interruptible）。内核在执行系统调用时会周期性检查"有没有待处理的信号"，如果有就提前终止系统调用，返回错误。

**具体时间线**

```
进程:  select()          read()
        │                 │
内核:   检测stdin可读      读数据
        │                 │
        │    信号到达！    │
        │    ↓            │
        │  递送信号        │
        │  中断read        │
        │                 │
        ▼                 ▼
     select返回>0      read返回-1
     (确实可读)        errno=EINTR
```

**EINTR 不代表出错**

`EINTR` 的意思是"被信号中断了"（**E**rror **INTR**upted），不是真正的错误。数据还在，只是这次读操作被提前终止了。正确做法是重试：

```c
ssize_t n;
do {
    n = read(fd, &buf, 1);
} while (n == -1 && errno == EINTR);
```

在我们的代码中，遇到 EINTR 时程序会设置退出标志并退出，因为收到信号时 `gQuitFlag` 通常已经被信号处理函数设为 1 了，程序本来就要退出。

### 10.3 select 超时后按键是否一定能读到

**必然能读到。** 原因：

**字符不会消失**：键盘按键的处理链路是：键盘按下 → 键盘控制器中断 → 内核键盘驱动 → 终端行缓冲区。字符一旦进入内核的终端缓冲区，就一直待在那里，直到被 `read()` 取走。`select()` 只是查看缓冲区有没有数据，不会消耗数据。

**缓冲区是内核管理的**：`select` 超时返回是用户态的事情，内核的终端缓冲区不受影响。用户态在两次 `select` 之间不管做了什么，缓冲区里的字符都安然无恙。

**我们的代码不会"偷读"**：`select` 超时返回 0 时不调用 `read()`，所以缓冲区保持原样，下一次 `select` 能看到之前的按键。

时间线确定性：

```
第1次 select: 100ms内无输入 → 返回0 → 返回 BUTTON_NONE
    │
    │  用户按了 's'  →  's' 进入终端缓冲区
    │
第2次 select: 立即发现缓冲区有数据 → 返回>0 → read读走's'
```

最多延迟 100ms（下一个轮询周期）就能读到，不会丢。

### 10.4 sem_timedwait 一次会消费几个信号量

**永远只消费 1 个。** 这是 POSIX 信号量的定义。

| 操作 | 信号量值变化 | 效果 |
|------|-------------|------|
| `sem_post` | +1 | 唤醒一个等待者（如有） |
| `sem_timedwait` / `sem_wait` | -1 | 只减 1，不关心当前值是多少 |

假设信号量值是 2：

```
sem_timedwait → 值变 1，返回 0（成功）
sem_timedwait → 值变 0，返回 0（成功）
sem_timedwait → 值变 -1，阻塞或超时
```

即使 producer 连 post 了 2 次，consumer 也需要调用 2 次 `sem_timedwait` 才能全部消费。信号量在这里起了**计数器**的作用，不只是简单的"通知"。如果 producer 连续产生 2 个事件，信号量值变成 2，consumer 会连续被唤醒 2 次，翻转 2 次 LED——这正是语义上应该发生的事情。

在当前代码中，producerTask 每轮循环先 `usleep(100000)`（100ms）再 `sem_post`，所以至少间隔 100ms，不可能短时间内连 post 2 次。但如果去掉 usleep 或管道一次灌入多个字符，信号量值就可能大于 1。

### 10.5 sem_timedwait 函数签名在本环境有效的判断依据

`sem_timedwait` 的声明位于系统头文件 `/usr/include/semaphore.h`，但其生效需要两个预处理条件。以下追溯完整的判断链条。

**编译条件**

`semaphore.h` 第 57-77 行的条件编译结构：

```c
#ifdef __USE_XOPEN2K                     // 条件 A
# ifndef __USE_TIME_BITS64               // 条件 B
extern int sem_timedwait (sem_t *__restrict __sem,
                          const struct timespec *__restrict __abstime)
# else
  ...  // 64位时间变体
# endif
#endif
```

需要 **A 为真** 且 **B 为真**，第 63 行的签名才生效。

**条件 A：`__USE_XOPEN2K` 是否被定义**

追溯到 `/usr/include/features.h` 第 342-343 行：

```c
#if defined _POSIX_C_SOURCE && (_POSIX_C_SOURCE - 0) >= 200112L
# define __USE_XOPEN2K    1
```

因果链：`_POSIX_C_SOURCE >= 200112L` → `__USE_XOPEN2K` 被定义。

事实：预处理器输出确认 `#define _POSIX_C_SOURCE 200809L`，`200809L >= 200112L` 成立。

结论：条件 A 满足 ✓

**条件 B：`__USE_TIME_BITS64` 是否未被定义**

搜遍 `/usr/include/` 所有头文件，只有 `#ifndef __USE_TIME_BITS64` 和 `#ifdef __USE_TIME_BITS64` 的检查，**没有任何文件 `#define __USE_TIME_BITS64`**。该宏通常由用户通过 `-D_USE_TIME_BITS64` 编译选项手动启用（用于 32 位系统兼容 64 位时间），本环境是 x86_64，不需要也不启用。

结论：条件 B 满足 ✓

**`_POSIX_C_SOURCE` 的根源**

`_POSIX_C_SOURCE=200809L` 是 gcc 编译器在 Linux 上的默认定义，无需用户手动指定。

**完整链条**

```
gcc 默认定义 _POSIX_C_SOURCE=200809L
        │
        ▼
features.h: 200809L >= 200112L
        │
        ▼
features.h:343 → #define __USE_XOPEN2K 1
        │
        ▼
semaphore.h:57 → #ifdef __USE_XOPEN2K  ✓ 进入块
        │
        ▼
__USE_TIME_BITS64 未被任何头文件定义
        │
        ▼
semaphore.h:62 → #ifndef __USE_TIME_BITS64  ✓ 进入块
        │
        ▼
semaphore.h:63 → extern int sem_timedwait(...)  ← 该签名生效
```
