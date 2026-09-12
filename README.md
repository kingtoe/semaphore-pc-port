# Semaphore PC Port

POSIX 信号量（semaphore）生产者-消费者示例的 PC 移植版。

基于《Programming Embedded Systems, 2nd Edition》（O'Reilly, 2007）第 12 章的嵌入式 Linux 示例，将硬件依赖替换为键盘输入和屏幕输出，使其可在任何 x86 Linux PC 上直接编译运行。

## 快速开始

```bash
make
./semaphore
```

运行后按任意键触发 LED 翻转，按 `q` 或 `Ctrl+C` 退出。

## 文件结构

```
semaphore.c      主程序（producer-consumer + semaphore）
pc/
  input.h/c      键盘输入检测（select + termios）
  display.h/c    LED 模拟输出（printf）
Makefile         编译脚本
PORTING.md       移植文档（问题定位 + 原理讲解 + 测试用例）
LICENSE          CC BY-NC 4.0
```

## 工作原理

```
producerTask                     consumerTask
     │                                │
     ▼                                ▼
 每100ms轮询键盘                sem_timedwait等待
     │                                │
     ▼                                ▼
 select()检测stdin              收到信号量
     │                                │
     ▼                                ▼
 有按键→sem_post ────────────→ 翻转LED（printf）
```

## 原始代码的硬件依赖

| 原始（嵌入式） | 移植版（PC） |
|----------------|-------------|
| `/dev/mem` + `mmap` 映射 GPIO 寄存器 | 删除 |
| 读按钮寄存器 `0x14500000` | `select()` + `read(stdin)` |
| 移位寄存器消抖 | 不需要（键盘无物理弹跳） |
| 写 GPIO SET/CLEAR 寄存器 | `printf("[LED] ON/OFF")` |

## 文档

详见 [PORTING.md](PORTING.md)，包含：
- `select` 键盘检测原理
- `termios` 非规范模式设置
- 信号中断（EINTR）机制
- 7 个测试用例及验证结果
- 信号量计数器语义分析

## 版权

- 原始代码：Copyright (c) 2006 Anthony Massa and Michael Barr
- PC 移植：Copyright (c) 2026 Ethan Lin
- 许可证：[CC BY-NC 4.0](LICENSE)

## AI 辅助声明

本项目的 PC 移植代码、PORTING.md 中的技术讲解（select 原理、termios 设置、EINTR 机制、信号量语义分析）以及测试用例设计，部分由 AI 辅助生成。人工负责需求定义、方案设计、代码审查和测试验证。
