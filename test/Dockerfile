FROM alpine:3.9
RUN apk add --no-cache gcc g++ make cmake git curl-dev
COPY entrypoint.sh /
ENTRYPOINT ["/bin/sh", "/entrypoint.sh"]
