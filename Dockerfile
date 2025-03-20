FROM alpine
RUN apk add bash make gcc g++ cmake gtest-dev

# Create a group and user
RUN addgroup -S gitlab && adduser -S gitlab -G gitlab

# default user
USER gitlab

# default working directory
WORKDIR /home/gitlab

SHELL ["/bin/bash", "-c"]
