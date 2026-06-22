# NipoVPN Docker

This directory contains Docker files for building and running **NipoVPN** in either `server` or `agent` mode.

## Directory Structure

```text
nipovpn/
├── docker/
│   ├── Dockerfile
│   ├── docker-compose.yml
│   └── README.md
├── nipovpn/
│   └── etc/nipovpn/config.yaml
├── core/
├── CMakeLists.txt
└── .dockerignore
```

## Requirements

Install Docker and Docker Compose:

```bash
docker --version
docker compose version
```

For older systems:

```bash
docker-compose --version
```

## Important: `.dockerignore`

Create `.dockerignore` in the repository root:

```text
.git
.github
build
cmake-build-*
*.deb
*.tar.gz
*.log
```

This prevents local build files from being copied into the Docker image.

## Docker Compose File

The compose file is located at:

```bash
docker/docker-compose.yml
```

Example:

```yaml
services:
  nipovpn:
    container_name: nipovpn

    build:
      context: ..
      dockerfile: docker/Dockerfile

    image: nipovpn:latest

    restart: unless-stopped

    ports:
      - "443:443"
      - "8080:8080"

    volumes:
      - ../nipovpn/etc/nipovpn/config.yaml:/etc/nipovpn/config.yaml:ro
      - nipovpn_logs:/var/log/nipovpn

    command:
      - ${NIPOVPN_MODE:-server}
      - /etc/nipovpn/config.yaml

volumes:
  nipovpn_logs:
```

## Build Image

From the `docker` directory:

```bash
cd docker
docker compose build
```

Or with old Docker Compose:

```bash
cd docker
docker-compose build
```

To rebuild without cache:

```bash
docker compose build --no-cache
```

## Run in Server Mode

Using environment variable:

```bash
cd docker
NIPOVPN_MODE=server docker compose up -d
```

Or create `docker/.env`:

```env
NIPOVPN_MODE=server
```

Then run:

```bash
docker compose up -d
```

The container will run:

```bash
nipovpn server /etc/nipovpn/config.yaml
```

## Run in Agent Mode

Using environment variable:

```bash
cd docker
NIPOVPN_MODE=agent docker compose up -d
```

Or create `docker/.env`:

```env
NIPOVPN_MODE=agent
```

Then run:

```bash
docker compose up -d
```

The container will run:

```bash
nipovpn agent /etc/nipovpn/config.yaml
```

## Specify Config File

By default, this file is mounted into the container:

```bash
../nipovpn/etc/nipovpn/config.yaml
```

Inside the container it becomes:

```bash
/etc/nipovpn/config.yaml
```

To use another config file, change this line in `docker-compose.yml`:

```yaml
volumes:
  - ../nipovpn/etc/nipovpn/config.yaml:/etc/nipovpn/config.yaml:ro
```

Example:

```yaml
volumes:
  - ./server-config.yaml:/etc/nipovpn/config.yaml:ro
```

Then run:

```bash
docker compose up -d
```

## View Logs

Show live container logs:

```bash
docker compose logs -f
```

Show logs only for NipoVPN service:

```bash
docker compose logs -f nipovpn
```

Read the log file inside the container volume:

```bash
docker exec -it nipovpn cat /var/log/nipovpn/nipovpn.log
```

Follow the log file:

```bash
docker exec -it nipovpn tail -f /var/log/nipovpn/nipovpn.log
```

## Stop Container

```bash
docker compose down
```

## Restart Container

```bash
docker compose restart
```

## Rebuild and Restart

```bash
docker compose down
docker compose build --no-cache
docker compose up -d
```

## Check Container Status

```bash
docker ps
```

Or:

```bash
docker compose ps
```

## Execute Shell Inside Container

```bash
docker exec -it nipovpn bash
```

## Run Manually Without Compose

Build image from repository root:

```bash
docker build -f docker/Dockerfile -t nipovpn:latest .
```

Run server mode:

```bash
docker run --rm \
  --name nipovpn \
  -p 443:443 \
  -p 8080:8080 \
  -v ./nipovpn/etc/nipovpn/config.yaml:/etc/nipovpn/config.yaml:ro \
  nipovpn:latest server /etc/nipovpn/config.yaml
```

Run agent mode:

```bash
docker run --rm \
  --name nipovpn \
  -p 8080:8080 \
  -v ./nipovpn/etc/nipovpn/config.yaml:/etc/nipovpn/config.yaml:ro \
  nipovpn:latest agent /etc/nipovpn/config.yaml
```

## Common Problems

### CMakeCache path error

If you see an error like:

```text
CMakeCache.txt directory is different
```

your local `build/` directory was copied into Docker.

Fix:

```bash
rm -rf build
docker compose build --no-cache
```

Also make sure `.dockerignore` exists in the repository root.

### Port already in use

If port `443` or `8080` is already used, change the port mapping:

```yaml
ports:
  - "8443:443"
  - "8081:8080"
```

### Config changes are not applied

Restart the container:

```bash
docker compose restart
```

## Recommended Commands

Server:

```bash
cd docker
NIPOVPN_MODE=server docker compose up -d --build
docker compose logs -f
```

Agent:

```bash
cd docker
NIPOVPN_MODE=agent docker compose up -d --build
docker compose logs -f
```
