# 使用你本地已经有的基础镜像
FROM ubuntu:22.04

# 避免交互问答
ENV DEBIAN_FRONTEND=noninteractive

# 1. 关键：把下载好的压缩包复制进镜像，ADD指令会自动解压
ADD gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 /opt/

# 2. 安装本地极难断掉的小工具（cmake/ninja通常很快）
RUN apt-get update && apt-get install -y cmake ninja-build && rm -rf /var/lib/apt/lists/*

# 3. 设置路径
ENV PATH="/opt/gcc-arm-none-eabi-10.3-2021.10/bin:${PATH}"

WORKDIR /app