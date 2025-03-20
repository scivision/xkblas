# From https://gitlab.inria.fr/gitlabci_gallery/pipelines/complete_cpp_example/-/tree/main?ref_type=heads

FROM alpine

RUN echo "http://dl-cdn.alpinelinux.org/alpine/edge/main" >> /etc/apk/repositories && \
    echo "http://dl-cdn.alpinelinux.org/alpine/edge/community" >> /etc/apk/repositories && \
    echo "http://dl-cdn.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories && \
    apk update

RUN apk add bash clang cmake curl g++ gcc git jq make vim valgrind python3

