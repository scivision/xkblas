# From https://gitlab.inria.fr/gitlabci_gallery/pipelines/complete_cpp_example/-/tree/main?ref_type=heads


FROM ubuntu:20.04

# Update and install necessary packages
RUN apt-get update
RUN apt-get install -y build-essential
RUN apt-get install -y libc6-dev
RUN apt-get install -y linux-headers-$(uname -r)
RUN apt-get install -y bash
RUN apt-get install -y clang
RUN apt-get install -y cmake
RUN apt-get install -y curl
RUN apt-get install -y g++ gcc
RUN apt-get install -y git
RUN apt-get install -y  make cmake
RUN apt-get install -y vim
RUN apt-get install -y valgrind
RUN apt-get install -y python3
RUN apt-get install -y hwloc hwloc-nox
RUN apt-get clean

# Create a group and user
RUN groupadd -r gitlab && useradd -r -g gitlab gitlab

# Create a directory where CI jobs are performed
RUN mkdir /builds && \
    chown -R gitlab:gitlab /builds && \
    chmod g+s /builds

# Default user
USER gitlab

# Default working directory
WORKDIR /builds


# FROM alpine
# 
# RUN echo "http://dl-cdn.alpinelinux.org/alpine/edge/main" >> /etc/apk/repositories && \
#     echo "http://dl-cdn.alpinelinux.org/alpine/edge/community" >> /etc/apk/repositories && \
#     echo "http://dl-cdn.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories && \
#     apk update
# 
# RUN apk add bash clang cmake curl g++ gcc git make vim valgrind python3 hwloc-dev hwloc-tools
# 
# # Create a group and user
# RUN addgroup -S gitlab && adduser -S gitlab -G gitlab
# 
# # Create a directory where ci jobs are performed
# RUN mkdir /builds && \
#     chown -R gitlab:gitlab /builds && \
#     chmod g+s /builds
# 
# # default user
# USER gitlab
# 
# # default working directory
# WORKDIR /builds
