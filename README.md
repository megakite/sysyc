# sysyc (SysY Compiler)

> [!NOTE]  
> WIP. Prone to change.

Toy compiler based on [PKU Compiler Course](https://pku-minic.github.io/online-doc/). A simple Breakout game included.\
照着[北大编译实践](https://pku-minic.github.io/online-doc/)写的玩具编译器。有一个打砖块小游戏。

## Dependency 依赖
`qemu-system-riscv32` is required aside from dependencies specified in the course manual.\
除编译实践文档指定的依赖以外，还需要`qemu-system-riscv32`。

## Build 构建
```sh
make DEBUG=0
cd app
make run
```
