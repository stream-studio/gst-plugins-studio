# syntax=docker/dockerfile:1
FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Paris
RUN apt-get update && apt-get install -y tzdata

ADD . /code

RUN apt-get update && apt-get install -y libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly gstreamer1.0-libav \
  build-essential \
  python3 python3-pip python3-setuptools \
  python3-wheel ninja-build \
  git \
  && rm -rf /var/lib/apt/lists/*

RUN pip install meson
WORKDIR /code

ENTRYPOINT [ "/code/entrypoint.sh" ]