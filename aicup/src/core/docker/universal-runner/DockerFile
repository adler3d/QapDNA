FROM node:18-alpine

RUN mkdir /dokcon
COPY dokcon.js /dokcon/

WORKDIR /dokcon

CMD ["sh", "-c", "node dokcon.js server_unix && sleep infinity"]