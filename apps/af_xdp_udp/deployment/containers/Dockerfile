#
# alpine_af_xdp_udp:latest
#

#
#
# Using standard Alpine Linux
FROM alpine:latest
#
# Identify the maintainer of an image
LABEL maintainer="PacketCord Ltd."
#
# Update the image to the latest packages
RUN apk update
#
# Install the necessary networking tools to test.
RUN apk add --no-cache iproute2
RUN apk add --no-cache ethtool
RUN apk add --no-cache tshark
RUN apk add --no-cache iperf3
RUN apk add --no-cache openssh-server
RUN apk add --no-cache shadow
RUN apk add --no-cache libc6-compat
RUN apk add --no-cache build-base
RUN apk add --no-cache meson ninja pkgconfig python3 py3-pip
RUN apk add --no-cache linux-headers numactl-dev libbsd-dev
RUN apk add --no-cache libpcap-dev zlib-dev jansson-dev openssl-dev
RUN apk add --no-cache py3-elftools
RUN apk add --no-cache libbpf-dev libxdp-dev
RUN apk add --no-cache elfutils-dev zlib-dev

WORKDIR /root

# Set password for root user
RUN echo "root:toor" | chpasswd

# Set up configuration for SSH
RUN mkdir -p /var/run/sshd && \
    ssh-keygen -A && \
    rm -f /etc/ssh/sshd_config.d/*.conf 2>/dev/null || true && \
    sed -i 's/^#PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config && \
    sed -i 's/^PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config && \
    sed -i 's/^#PasswordAuthentication.*/PasswordAuthentication yes/' /etc/ssh/sshd_config && \
    sed -i 's/^PasswordAuthentication.*/PasswordAuthentication yes/' /etc/ssh/sshd_config && \
    echo "export VISIBLE=now" >> /etc/profile

# Expose the SSH port
EXPOSE 22

# Run SSH
CMD ["/usr/sbin/sshd", "-D", "-e"]
#
#
#
